#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "MT_SYS.h"
#include "MT_UART.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ezmode.h"

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
#include "zcl_ota.h"
#include "hal_ota.h"
#endif

#include "smartir_app.h"
#include "string.h"

byte zclSmartIR_TaskID;
uint8 zclSmartIRSeqNum;
devStates_t zclSmartIR_NwkState = DEV_INIT;
extern byte App_TaskID;
#ifdef ZCL_ON_OFF
afAddrType_t zclSmartIR_DstAddr;
#endif

#ifdef ZCL_EZMODE
static void zclSmartIR_ProcessZDOMsgs( zdoIncomingMsg_t *pMsg );
static void zclSmartIR_EZModeCB( zlcEZMode_State_t state, zclEZMode_CBData_t *pData );

static const zclEZMode_RegisterData_t zclSmartIR_RegisterEZModeData =
{
  &zclSmartIR_TaskID,
  SMARTIR_EZMODE_NEXTSTATE_EVT,
  SMARTIR_EZMODE_TIMEOUT_EVT,
  &zclSmartIRSeqNum,
  zclSmartIR_EZModeCB
};

// NOT ZLC_EZMODE, Use EndDeviceBind
#else

static cId_t bindingOutClusters[] =
{
  ZCL_CLUSTER_ID_GEN_ON_OFF
};
#define ZCLSMARTIR_BINDINGLIST   (sizeof(bindingOutClusters)/sizeof(bindingOutClusters[0]))
#endif  // ZLC_EZMODE

static endPointDesc_t SmartIR_TestEp =
{
  SMARTIR_ENDPOINT,                  // endpoint
  &zclSmartIR_TaskID,
  (SimpleDescriptionFormat_t *)NULL,  // No Simple description for this test endpoint
  (afNetworkLatencyReq_t)0            // No Network Latency req
};

static uint8 aProcessCmd[] = { 1, 0, 0, 0 }; // used for reset command, { length + cmd0 + cmd1 + data }

static void zclSmartIR_HandleKeys( byte shift, byte keys );
static void zclSmartIR_BasicResetCB( void );
static void zclSmartIR_IdentifyCB( zclIdentify_t *pCmd );
static void zclSmartIR_IdentifyQueryRspCB(  zclIdentifyQueryRsp_t *pRsp );
static void zclSmartIR_ProcessIdentifyTimeChange( void );
static void zclSmartIR_ProcessIncomingMsg( zclIncomingMsg_t *msg );
#ifdef ZCL_READ
static uint8 zclSmartIR_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8 zclSmartIR_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif
static uint8 zclSmartIR_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8 zclSmartIR_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclSmartIR_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclSmartIR_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg );
#endif

static zclGeneral_AppCallbacks_t zclSmartIR_CmdCallbacks =
{
  zclSmartIR_BasicResetCB,               // Basic Cluster Reset command
  zclSmartIR_IdentifyCB,                 // Identify command
  #ifdef ZCL_EZMODE
  NULL,                                   // Identify EZ-Mode Invoke command
  NULL,                                   // Identify Update Commission State command
  #endif
  NULL,                                   // Identify Trigger Effect command
  zclSmartIR_IdentifyQueryRspCB,          // Identify Query Response command
  NULL,                                   // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
  #ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
  #endif
  #ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
  #endif
  #ifdef ZCL_SCENES
  NULL,                                   // Scene Store Request command
  NULL,                                   // Scene Recall Request command
  NULL,                                   // Scene Response command
  #endif
  #ifdef ZCL_ALARMS
  NULL,                                   // Alarm (Response) commands
  #endif
  #ifdef SE_UK_EXT
  NULL,                                   // Get Event Log command
  NULL,                                   // Publish Event Log command
  #endif
  NULL,                                   // RSSI Location command
  NULL                                    // RSSI Location Response command
};

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
#define DEVICE_POLL_RATE                 8000   // Poll rate for end device
#endif

void MY_UART_Init()
{
  halUARTCfg_t uartConfig;
  App_TaskID = 0;
  uartConfig.configured           = TRUE;
  uartConfig.baudRate             = HAL_UART_BR_115200;
  uartConfig.flowControl          = false;
  uartConfig.flowControlThreshold = MT_UART_DEFAULT_THRESHOLD;
  uartConfig.rx.maxBufSize        = MT_UART_DEFAULT_MAX_RX_BUFF;
  uartConfig.tx.maxBufSize        = MT_UART_DEFAULT_MAX_TX_BUFF;
  uartConfig.idleTimeout          = MT_UART_DEFAULT_IDLE_TIMEOUT;
  uartConfig.intEnable            = TRUE;
  #if defined (ZTOOL_P1) || defined (ZTOOL_P2)
  uartConfig.callBackFunc         = MT_UartProcessZToolData;
  #elif defined (ZAPP_P1) || defined (ZAPP_P2)
  uartConfig.callBackFunc         = MT_UartProcessZAppData;
  #else
  uartConfig.callBackFunc         = NULL;
  #endif
  #if defined (MT_UART_DEFAULT_PORT)
  HalUARTOpen (MT_UART_DEFAULT_PORT, &uartConfig);
  #else
  (void)uartConfig;
  #endif
  #if defined (ZAPP_P1) || defined (ZAPP_P2)
  MT_UartMaxZAppBufLen  = 1;
  MT_UartZAppRxStatus   = MT_UART_ZAPP_RX_READY;
  #endif
}

void zclSmartIR_Init( byte task_id )
{
  zclSmartIR_TaskID = task_id;
  MY_UART_Init();
  MT_UartRegisterTaskID(task_id);
  #ifdef ZCL_ON_OFF
  // Set destination address to indirect
  zclSmartIR_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  zclSmartIR_DstAddr.endPoint = 0;
  zclSmartIR_DstAddr.addr.shortAddr = 0;
  #endif
  // This app is part of the Home Automation Profile
  zclHA_Init( &zclSmartIR_SimpleDesc );
  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( SMARTIR_ENDPOINT, &zclSmartIR_CmdCallbacks );
  // Register the application's attribute list
  zcl_registerAttrList( SMARTIR_ENDPOINT, SMARTIR_MAX_ATTRIBUTES, zclSmartIR_Attrs );
  // Register the Application to receive the unprocessed Foundation command/response messages
  zcl_registerForMsg( zclSmartIR_TaskID );
  #ifdef ZCL_DISCOVER
  // Register the application's command list
  zcl_registerCmdList( SMARTIR_ENDPOINT, zclCmdsArraySize, zclSampleLight_Cmds );
  #endif
  // Register for all key events - This app will handle all key events
  RegisterForKeys( zclSmartIR_TaskID );
  // Register for a test endpoint
  afRegister( &SmartIR_TestEp );
  #ifdef ZCL_EZMODE
  // Register EZ-Mode
  zcl_RegisterEZMode( &zclSmartIR_RegisterEZModeData );
  // Register with the ZDO to receive Match Descriptor Responses
  ZDO_RegisterForZDOMsg(task_id, Match_Desc_rsp);
  #endif
  // Register for all key events - This app will handle all key events
  RegisterForKeys( zclSmartIR_TaskID );
  // Register for a test endpoint
  afRegister( &SmartIR_TestEp );
  ZDO_RegisterForZDOMsg( zclSmartIR_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( zclSmartIR_TaskID, Match_Desc_rsp );
  #if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
  // Register for callback events from the ZCL OTA
  zclOTA_Register(zclSmartIR_TaskID);
  #endif
  uint8 *Init_finished = "Welcome to Z-Stack Home (1.2.2a.44539)\n";
  HalUARTWrite(0,Init_finished,strlen((const char*)Init_finished));
}

uint16 zclSmartIR_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  (void)task_id;  // Intentionally unreferenced parameter
  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclSmartIR_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        #ifdef ZCL_EZMODE
        case ZDO_CB_MSG:
        zclSmartIR_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
        break;
        #endif
        case ZCL_INCOMING_MSG:
        // Incoming ZCL Foundation command/response messages
        zclSmartIR_ProcessIncomingMsg( (zclIncomingMsg_t *)MSGpkt );
        break;
        case KEY_CHANGE:
        zclSmartIR_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
        break;
        case ZDO_STATE_CHANGE:
        zclSmartIR_NwkState = (devStates_t)(MSGpkt->hdr.status);
        // now on the network
        if ( (zclSmartIR_NwkState == DEV_ZB_COORD) ||
        (zclSmartIR_NwkState == DEV_ROUTER)   ||
        (zclSmartIR_NwkState == DEV_END_DEVICE) )
        {
          #ifdef ZCL_EZMODE
          zcl_EZModeAction( EZMODE_ACTION_NETWORK_STARTED, NULL );
          #endif
        }
        break;
        #if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
        case ZCL_OTA_CALLBACK_IND:
        zclSmartIR_ProcessOTAMsgs( (zclOTA_CallbackMsg_t*)MSGpkt  );
        break;
        #endif
        default:
        break;
      }
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }
    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  if ( events & SMARTIR_IDENTIFY_TIMEOUT_EVT )
  {
    zclSmartIR_IdentifyTime = 10;
    zclSmartIR_ProcessIdentifyTimeChange();
    return ( events ^ SMARTIR_IDENTIFY_TIMEOUT_EVT );
  }
  #ifdef ZCL_EZMODE
  if ( events & SMARTIR_EZMODE_NEXTSTATE_EVT )
  {
    zcl_EZModeAction ( EZMODE_ACTION_PROCESS, NULL );   // going on to next state
    return ( events ^ SMARTIR_EZMODE_NEXTSTATE_EVT );
  }
  if ( events & SMARTIR_EZMODE_TIMEOUT_EVT )
  {
    zcl_EZModeAction ( EZMODE_ACTION_TIMED_OUT, NULL ); // EZ-Mode timed out
    return ( events ^ SMARTIR_EZMODE_TIMEOUT_EVT );
  }
  #endif // ZLC_EZMODE
  // Discard unknown events
  return 0;
}

void zclSmartIR_EZModeCB( zlcEZMode_State_t state, zclEZMode_CBData_t *pData )
{

}

static void zclSmartIR_ProcessZDOMsgs( zdoIncomingMsg_t *pMsg )
{
  
}

static void zclSmartIR_HandleKeys( byte shift, byte keys )
{

}

static void zclSmartIR_BasicResetCB( void )
{

}
static void zclSmartIR_IdentifyCB( zclIdentify_t *pCmd )
{

}
static void zclSmartIR_IdentifyQueryRspCB(  zclIdentifyQueryRsp_t *pRsp )
{

}
static void zclSmartIR_ProcessIdentifyTimeChange( void )
{
  
}
static void zclSmartIR_ProcessIncomingMsg( zclIncomingMsg_t *msg )
{
  
}
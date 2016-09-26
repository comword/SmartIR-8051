#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "MT_SYS.h"
#include "MT_UART.h"
#include "MT.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ezmode.h"

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
#include "zcl_ota.h"
#include "hal_ota.h"
#endif

#include "smartir_app.h"
#include "m_protocol.h"
#include "string.h"
#include "hal_led.h"

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
#else

static cId_t bindingOutClusters[] =
{
  ZCL_CLUSTER_ID_GEN_ON_OFF
};
#define ZCLSMARTIR_BINDINGLIST   (sizeof(bindingOutClusters)/sizeof(bindingOutClusters[0]))
#endif  // ZLC_EZMODE

const SimpleDescriptionFormat_t SmartIR_SimpleDesc =
{
  SMARTIR_ENDPOINT,              //  int Endpoint;
  SMARTIR_PROFID,                //  uint16 AppProfId[2];
  SMARTIR_DEVICEID,              //  uint16 AppDeviceId[2];
  SMARTIR_DEVICE_VERSION,        //  int   AppDevVer:4;
  SMARTIR_FLAGS,                 //  int   AppFlags:4;
  ZCLSMARTIR_BINDINGLIST,          //  uint8  AppNumInClusters;
  (cId_t *)bindingOutClusters,  //  uint8 *pAppInClusterList;
  ZCLSMARTIR_BINDINGLIST,          //  uint8  AppNumInClusters;
  (cId_t *)bindingOutClusters   //  uint8 *pAppInClusterList;
};

static endPointDesc_t SmartIR_TestEp;

//static uint8 aProcessCmd[] = { 1, 0, 0, 0 }; // used for reset command, { length + cmd0 + cmd1 + data }

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
  //#if defined (ZTOOL_P1) || defined (ZTOOL_P2)
  //uartConfig.callBackFunc         = MT_UartProcessZToolData;
  //#elif defined (ZAPP_P1) || defined (ZAPP_P2)
  //uartConfig.callBackFunc         = MT_UartProcessZAppData;
  //#else
  uartConfig.callBackFunc         = MY_UartProcessZToolData;
  //#endif
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

void MY_UartProcessZToolData ( uint8 port, uint8 event )
{
  uint8 flag=0,i,j=0;
  uint8 buf[128]; 
  (void)event;
  while (Hal_UART_RxBufLen(port))
  {
    HalUARTRead (port, &buf[j], 1);
    j++;
    flag=1;
    if(flag == 1){
      /* Allocate memory for the data */
      mtOSALSerialData_t *pMsg =  (mtOSALSerialData_t *)osal_msg_allocate( sizeof( mtOSALSerialData_t )+j+1);
      if (pMsg)
      {
        /* Fill up what we can */
        pMsg->hdr.event = CMD_SERIAL_MSG;
        pMsg->msg = (uint8*)(pMsg+1);
        pMsg->msg[0] = j;
        for(i=0;i<j;i++)
          pMsg->msg [i+1]= buf[i];
        osal_msg_send( App_TaskID, (byte *)pMsg );
        /* deallocate the msg */
        osal_msg_deallocate ( (uint8 *)pMsg );
      }
      else
      {
        return;
      }
    }
  }
}

void zclSmartIR_Init( byte task_id )
{
  init_m_clock();
  zclSmartIR_TaskID = task_id;
  MY_UART_Init();
  MT_UartRegisterTaskID(task_id);
  #ifdef COORDINATOR
  zgDeviceLogicalType = ZG_DEVICETYPE_COORDINATOR;
  #else
  zgDeviceLogicalType = ZG_DEVICETYPE_ROUTER;
  #endif
  #ifdef ZCL_ON_OFF
  zclSmartIR_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  zclSmartIR_DstAddr.endPoint = SMARTIR_ENDPOINT;
  zclSmartIR_DstAddr.addr.shortAddr = 0xFFFF;
  SmartIR_TestEp.endPoint = SMARTIR_ENDPOINT;
  SmartIR_TestEp.task_id = &zclSmartIR_TaskID;
  SmartIR_TestEp.simpleDesc = (SimpleDescriptionFormat_t *) &SmartIR_SimpleDesc;
  SmartIR_TestEp.latencyReq = noLatencyReqs;
  ZDOInitDevice(0);
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
  zcl_registerCmdList( SMARTIR_ENDPOINT, zclCmdsArraySize, zclSmartIR_Cmds );
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
  zclEZMode_InvokeData_t ezModeData;
  static uint16 clusterIDs[] = { ZCL_CLUSTER_ID_GEN_ON_OFF };   // only bind on the on/off cluster
  // Invoke EZ-Mode
  ezModeData.endpoint = SMARTIR_ENDPOINT; // endpoint on which to invoke EZ-Mode
  if ( (zclSmartIR_NwkState == DEV_ZB_COORD) ||
               (zclSmartIR_NwkState == DEV_ROUTER)   ||
               (zclSmartIR_NwkState == DEV_END_DEVICE) )
  {
    ezModeData.onNetwork = TRUE;      // node is already on the network
  }
  else
  {
    ezModeData.onNetwork = FALSE;     // node is not yet on the network
  }
  ezModeData.initiator = TRUE;        // OnOffSwitch is an initiator
  ezModeData.numActiveOutClusters = 1;   // active output cluster
  ezModeData.pActiveOutClusterIDs = clusterIDs;
  ezModeData.numActiveInClusters = 0;  // no active input clusters
  ezModeData.pActiveInClusterIDs = NULL;
  zcl_InvokeEZMode( &ezModeData );
/*#else // NOT ZCL_EZMODE
    zAddrType_t dstAddr;
    //HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
    // Initiate an End Device Bind Request, this bind request will
    // only use a cluster list that is important to binding.
    dstAddr.addrMode = afAddr16Bit;
    dstAddr.addr.shortAddr = 0;   // Coordinator makes the match
    ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),
                           SMARTIR_ENDPOINT,
                           ZCL_HA_PROFILE_ID,
                           0, NULL,   // No incoming clusters to bind
                           ZCLSMARTIR_BINDINGLIST, bindingOutClusters,
                           TRUE );
  */
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
  HalLedSet(HAL_LED_1,HAL_LED_MODE_ON);
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
        case AF_INCOMING_MSG_CMD:
        SmartIR_MessageMSGCB( MSGpkt );
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
          #ifdef COORDINATOR
          osal_start_timerEx( zclSmartIR_TaskID, SMARTIR_COOR_PERIODIC_CHECK_EVT, 1000 );
          #endif
        }
        break;
        #if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
        case ZCL_OTA_CALLBACK_IND:
        zclSmartIR_ProcessOTAMsgs( (zclOTA_CallbackMsg_t*)MSGpkt);
        break;
        #endif
        case CMD_SERIAL_MSG:
          if(pro_verify_uart_mesg(((mtOSALSerialData_t *)MSGpkt)->msg,*(((mtOSALSerialData_t *)MSGpkt)->msg)) == 1){
            m_proc_uart_msg(((mtOSALSerialData_t *)MSGpkt)->msg,*(((mtOSALSerialData_t *)MSGpkt)->msg));
          }
        break; 
        default:
        break;
      }
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclSmartIR_TaskID );
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
  if ( events & SMARTIR_COOR_PERIODIC_CHECK_EVT )
  {
    
  }
  // Discard unknown events
  return 0;
}
#ifdef ZCL_EZMODE
void zclSmartIR_EZModeCB( zlcEZMode_State_t state, zclEZMode_CBData_t *pData )
{
  // time to go into identify mode
  if ( state == EZMODE_STATE_IDENTIFYING )
  {
    zclSmartIR_IdentifyTime = (EZMODE_TIME / 1000);  // convert to seconds
    zclSmartIR_ProcessIdentifyTimeChange();
  }

  // autoclosing, show what happened (success, cancelled, etc...)
  if( state == EZMODE_STATE_AUTOCLOSE )
  {
  }
  // finished, either show DstAddr/EP, or nothing (depending on success or not)
  if( state == EZMODE_STATE_FINISH )
  {
    // turn off identify mode
    zclSmartIR_IdentifyTime = 0;
    zclSmartIR_ProcessIdentifyTimeChange();
  }
}

static void zclSmartIR_ProcessZDOMsgs( zdoIncomingMsg_t *pMsg )
{
    // Let EZ-Mode know of the Match Descriptor Reponse (same as ActiveEP Response)
  if ( pMsg->clusterID == Match_Desc_rsp )
  {
    zclEZMode_ActionData_t data;
    ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( pMsg );
    data.pMatchDescRsp = pRsp;
    zcl_EZModeAction( EZMODE_ACTION_MATCH_DESC_RSP, &data );
    osal_mem_free(pRsp);
  }
}
#endif
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
static void zclSmartIR_ProcessIncomingMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      zclSmartIR_ProcessInReadRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      zclSmartIR_ProcessInWriteRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_REPORT
    // See ZCL Test Applicaiton (zcl_testapp.c) for sample code on Attribute Reporting
    case ZCL_CMD_CONFIG_REPORT:
      break;

    case ZCL_CMD_CONFIG_REPORT_RSP:
      break;

    case ZCL_CMD_READ_REPORT_CFG:
      break;

    case ZCL_CMD_READ_REPORT_CFG_RSP:
      break;

    case ZCL_CMD_REPORT:
      break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
      zclSmartIR_ProcessInDefaultRspCmd( pInMsg );
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      zclSmartIR_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      zclSmartIR_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      zclSmartIR_ProcessInDiscAttrsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      zclSmartIR_ProcessInDiscAttrsExtRspCmd( pInMsg );
      break;
#endif
    default:
      break;
  }

  if ( pInMsg->attrCmd )
    osal_mem_free( pInMsg->attrCmd );
}

void SmartIR_MessageMSGCB( afIncomingMSGPacket_t *pckt )
{
  switch ( pckt->clusterId )
  {

  }
}

#ifdef ZCL_READ
static uint8 zclSmartIR_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // Notify the originator of the results of the original read attributes
    // attempt and, for each successfull request, the value of the requested
    // attribute
  }
  return TRUE;
}
#endif
#ifdef ZCL_WRITE
static uint8 zclSmartIR_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < writeRspCmd->numAttr; i++)
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }
  return TRUE;
}
#endif

static uint8 zclSmartIR_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  (void)pInMsg;
  return TRUE;
}

#ifdef ZCL_DISCOVER
static uint8 zclSmartIR_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}

static uint8 zclSmartIR_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}

static uint8 zclSmartIR_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}
#endif // ZCL_DISCOVER

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
static void zclSmartIR_ProcessOTAMsgs( zclOTA_CallbackMsg_t* pMsg )
{
  uint8 RxOnIdle;

  switch(pMsg->ota_event)
  {
  case ZCL_OTA_START_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {
      // Speed up the poll rate
      RxOnIdle = TRUE;
      ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
      NLME_SetPollRate( 2000 );
    }
    break;

  case ZCL_OTA_DL_COMPLETE_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {
      // Reset the CRC Shadow and reboot.  The bootloader will see the
      // CRC shadow has been cleared and switch to the new image
      HalOTAInvRC();
      SystemReset();
    }
    else
    {
      // slow the poll rate back down.
      RxOnIdle = FALSE;
      ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
      NLME_SetPollRate(DEVICE_POLL_RATE);
    }
    break;

  default:
    break;
  }
}
#endif // defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
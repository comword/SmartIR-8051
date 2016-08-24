#include "ZComDef.h"
#include "hal_drivers.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"

#include "zcl.h"

extern void zclSmartIR_Init( byte task_id );
extern UINT16 zclSmartIR_event_loop( byte task_id, UINT16 events );

#if defined ( MT_TASK )
#include "MT.h"
#include "MT_TASK.h"
#endif

#include "nwk.h"
#include "APS.h"
#include "ZDApp.h"
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
#include "ZDNwkMgr.h"
#endif
#if defined ( ZIGBEE_FRAGMENTATION )
#include "aps_frag.h"
#endif

const pTaskEventHandlerFn tasksArr[] = {
  macEventLoop,
  nwk_event_loop,
  Hal_ProcessEvent,
  #if defined( MT_TASK )
  MT_ProcessEvent,
  #endif
  APS_event_loop,
  #if defined ( ZIGBEE_FRAGMENTATION )
  APSF_ProcessEvent,
  #endif
  ZDApp_event_loop,
  #if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_event_loop,
  #endif
  zcl_event_loop,
  zclSmartIR_event_loop
};

const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );
uint16 *tasksEvents;

void osalInitTasks( void )
{
  uint8 taskID = 0;

  tasksEvents = (uint16 *)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
  osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));

  macTaskInit( taskID++ );
  nwk_init( taskID++ );
  Hal_Init( taskID++ );
  #if defined( MT_TASK )
  MT_TaskInit( taskID++ );
  #endif
  APS_Init( taskID++ );
  #if defined ( ZIGBEE_FRAGMENTATION )
  APSF_Init( taskID++ );
  #endif
  ZDApp_Init( taskID++ );
  #if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_Init( taskID++ );
  #endif
  zcl_Init( taskID++ );
  zclSmartIR_Init( taskID );
}


extern void zclSmartIR_Init( byte task_id );
extern void MY_UART_Init(void);
extern UINT16 zclSmartIR_event_loop( byte task_id, UINT16 events );

#define SMARTIR_ENDPOINT           8

#define SMARTIR_MAX_ATTRIBUTES     15

// Events for the sample app
#define SMARTIR_IDENTIFY_TIMEOUT_EVT         0x0001
#define SMARTIR_POLL_CONTROL_TIMEOUT_EVT     0x0002
#define SMARTIR_EZMODE_TIMEOUT_EVT           0x0004
#define SMARTIR_EZMODE_NEXTSTATE_EVT         0x0008
#define SMARTIR_MAIN_SCREEN_EVT              0x0010

#define SMARTIR_DEVICE_VERSION     0
#define SMARTIR_FLAGS              0

#define SMARTIR_HWVERSION          0
#define SMARTIR_ZCLVERSION         0

extern CONST zclCommandRec_t zclSmartIR_Cmds[];
extern CONST uint8 zclCmdsArraySize;

const cId_t zclSmartIR_InClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY
};
#define ZCLSMARTIR_MAX_INCLUSTERS (sizeof(zclSmartIR_InClusterList) / sizeof( zclSmartIR_InClusterList[0]))

const cId_t zclSmartIR_OutClusterList[] =
{
  //  ZCL_CLUSTER_ID_GEN_ON_OFF,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  #ifdef ZCL_LEVEL_CTRL
  ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL,
  #endif
  0
};

#define ZCLSMARTIR_MAX_OUTCLUSTERS    ( ( sizeof( zclSmartIR_OutClusterList ) / sizeof( zclSmartIR_OutClusterList[0] ) ) - 1 )

SimpleDescriptionFormat_t zclSmartIR_SimpleDesc =
{
  SMARTIR_ENDPOINT,                   //  int Endpoint;
  ZCL_HA_PROFILE_ID,                  //  uint16 AppProfId[2];
  ZCL_HA_DEVICEID_REMOTE_CONTROL,     //  uint16 AppDeviceId[2];
  SMARTIR_DEVICE_VERSION,             //  int   AppDevVer:4;
  SMARTIR_FLAGS,                      //  int   AppFlags:4;
  ZCLSMARTIR_MAX_INCLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)zclSmartIR_InClusterList,  //  byte *pAppInClusterList;
  ZCLSMARTIR_MAX_OUTCLUSTERS,         //  byte  AppNumInClusters;
  (cId_t *)zclSmartIR_OutClusterList  //  byte *pAppInClusterList;
};

const uint8 zclSmartIR_HWRevision = SMARTIR_HWVERSION;
const uint8 zclSmartIR_ZCLVersion = SMARTIR_ZCLVERSION;
const uint8 zclSmartIR_ManufacturerName[] = { 16, 'T','e','x','a','s','I','n','s','t','r','u','m','e','n','t','s' };
const uint8 zclSmartIR_ModelId[] = { 16, 'T','I','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 zclSmartIR_DateCode[] = { 16, '2','0','0','6','0','8','3','1',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 zclSmartIR_PowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 zclSmartIR_LocationDescription[17] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 zclSmartIR_PhysicalEnvironment = 0;
uint8 zclSmartIR_DeviceEnable = DEVICE_ENABLED;

// Identify Cluster
uint16 zclSmartIR_IdentifyTime = 0;

CONST zclAttrRec_t zclSmartIR_Attrs[SMARTIR_MAX_ATTRIBUTES] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // Cluster IDs - defined in the foundation (ie. zcl.h)
    {  // Attribute record
      ATTRID_BASIC_HW_VERSION,            // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // Data Type - found in zcl.h
      ACCESS_CONTROL_READ,                // Variable access control - found in zcl.h
      (void *)&zclSmartIR_HWRevision  // Pointer to attribute variable
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclSmartIR_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSmartIR_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSmartIR_ModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSmartIR_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclSmartIR_PowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)zclSmartIR_LocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_UINT8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSmartIR_PhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_UINT8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSmartIR_DeviceEnable
    }
  },

  // *** Identify Cluster Attribute ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // Attribute record
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSmartIR_IdentifyTime
    }
  },
  /*
  // *** On / Off Cluster Attributes ***
  {
  ZCL_CLUSTER_ID_GEN_ON_OFF,
  { // Attribute record
  ATTRID_ON_OFF,
  ZCL_DATATYPE_BOOLEAN,
  ACCESS_CONTROL_READ,
  (void *)&zclSmartIR_OnOff
}
},

// *** On / Off Switch Configuration Cluster *** //
{
ZCL_CLUSTER_ID_GEN_ON_OFF_SWITCH_CONFIG,
{ // Attribute record
ATTRID_ON_OFF_SWITCH_TYPE,
ZCL_DATATYPE_ENUM8,
ACCESS_CONTROL_READ,
(void *)&zclSmartIR_OnOffSwitchType
}
},
{
ZCL_CLUSTER_ID_GEN_ON_OFF_SWITCH_CONFIG,
{ // Attribute record
ATTRID_ON_OFF_SWITCH_ACTIONS,
ZCL_DATATYPE_ENUM8,
ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
(void *)&zclSmartIR_OnOffSwitchActions
}
},
*/
};

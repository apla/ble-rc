#include "bcomdef.h"
#include "linkdb.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "accelerometerservice.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Accelerometer Service UUID
CONST uint8 accelServUUID[TI_UUID_SIZE] =
{
  TI_UUID(ACCELEROMETER_SERV_UUID),
};

// Accelerometer Characteristic value Data UUID
CONST uint8 accelDataUUID[TI_UUID_SIZE] =
{
  TI_UUID(ACCELEROMETER_DATA_UUID),
};

// Accelerometer Characteristic value Configuration UUID
CONST uint8 accelCfgUUID[TI_UUID_SIZE] =
{
  TI_UUID(ACCELEROMETER_CONF_UUID),
};

// Accelerometer Characteristic value Configuration UUID
CONST uint8 accelPeriodUUID[TI_UUID_SIZE] =
{
  TI_UUID(ACCELEROMETER_PERI_UUID),
};


/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static accelCBs_t *accel_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// Accelerometer  Profile Service attribute
static CONST gattAttrType_t accelService = { TI_UUID_SIZE, accelServUUID };

// Accelerometer Characteristic Properties
static uint8 accelDataProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

static uint8 accelData[ACCELEROMETER_DATA_LEN] = { 0, 0, 0};

// Accelerometer Characteristic Configuration
static gattCharCfg_t accelDataConfig[GATT_MAX_NUM_CONN];

// Accelerometer Characteristic User Description
static uint8 accelDataUserDesp[] = "Accel. Data";

// Accelerometer Characteristic Configuration Properties
static uint8 accelCfgProps = GATT_PROP_READ | GATT_PROP_WRITE;

// Accelerometer Characteristic Configuration Value
 static uint8 accelCfg = 0;

// Accelerometer Characteristic Configuration User Description
static uint8 accelCfgUserDesp[] = "Accel. Conf.";

// Accelerometer Characteristic Period Properties
static uint8 accPerProps = GATT_PROP_READ | GATT_PROP_WRITE;

// Accelerometer Characteristic Period Value
static uint8 accPer = 0;

// Accelerometer Characteristic Period User Description
static uint8 accPerUserDesp[] = "Acc. Period";

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t accelAttrTbl[] =
{
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&accelService                    /* pValue */
  },

    // Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &accelDataProps
    },

      // Characteristic Value "Data"
      {
        { TI_UUID_SIZE, accelDataUUID },
        GATT_PERMIT_READ,
        0,
        accelData
      },

      // Characteristic configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)accelDataConfig
      },

      // Characteristic User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ,
        0,
        accelDataUserDesp
      },

    // Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &accelCfgProps
    },

      // Characteristic Value "Configuration"
      {
        { TI_UUID_SIZE, accelCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        &accelCfg
      },

      // Characteristic User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ,
        0,
        accelCfgUserDesp
      },
     // Characteristic Declaration "Period"
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &accPerProps
    },

      // Characteristic Value "Period"
      {
        { TI_UUID_SIZE, accelPeriodUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        &accPer
      },

      // Characteristic User Description "Period"
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ,
        0,
        accPerUserDesp
      },
};

static bStatus_t utilExtractUuid16(gattAttribute_t *pAttr, uint16 *pUuid)
{
	bStatus_t status = SUCCESS;

	 if (pAttr->type.len == ATT_BT_UUID_SIZE)
	{
		// 16-bit UUID direct
		*pUuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
#ifdef GATT_TI_UUID_128_BIT
	}
	else if (pAttr->type.len == ATT_UUID_SIZE)
	{
		// 16-bit UUID extracted bytes 12 and 13
		*pUuid = BUILD_UINT16( pAttr->type.uuid[12], pAttr->type.uuid[13]);
#endif
	}
	else
	{
		*pUuid = 0xFFFF;
		status = FAILURE;
	}

	return status;
};




/*********************************************************************
 * LOCAL FUNCTIONS
 */
static uint8 acc_ReadAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
                            uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen );
static bStatus_t acc_WriteAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
                                 uint8 *pValue, uint8 len, uint16 offset );
static void acc_HandleConnStatusCB( uint16 connHandle, uint8 changeType );


/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
CONST gattServiceCBs_t accCBs =
{
  acc_ReadAttrCB,  // Read callback function pointer
  acc_WriteAttrCB, // Write callback function pointer
  NULL             // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      Accel_AddService
 *
 * @brief   Initializes the Sensor Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t Accel_AddService( uint32 services )
{
  uint8 status = SUCCESS;

  // Register with Link DB to receive link status change callback
  VOID linkDB_Register( acc_HandleConnStatusCB );
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, accelDataConfig );

  if (services & ACCELEROMETER_SERVICE )
  {
       // Register GATT attribute list and CBs with GATT Server App
    status = GATTServApp_RegisterService( accelAttrTbl,
                                          GATT_NUM_ATTRS( accelAttrTbl ),
                                          &accCBs );
  }

  return ( status );
}


/*********************************************************************
 * @fn      Accel_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t Accel_RegisterAppCBs( accelCBs_t *appCallbacks )
{
  if ( accel_AppCBs == NULL )
  {
    if ( appCallbacks != NULL )
    {
      accel_AppCBs = appCallbacks;
    }

    return ( SUCCESS );
  }

  return ( bleAlreadyInRequestedMode );
}

/*********************************************************************
 * @fn      Accel_SetParameter
 *
 * @brief   Set an Accelrometer parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to right
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t Accel_SetParameter( uint8 param, uint8 len, void *value )
{
  bStatus_t ret = SUCCESS;

  switch ( param )
  {
    case ACCELEROMETER_DATA:
    if ( len == ACCELEROMETER_DATA_LEN )
    {
      VOID osal_memcpy( accelData, value, ACCELEROMETER_DATA_LEN );
      // See if Notification has been enabled
      GATTServApp_ProcessCharCfg( accelDataConfig, accelData, FALSE,
                                 accelAttrTbl, GATT_NUM_ATTRS( accelAttrTbl ),
                                 INVALID_TASK_ID );
    }
    else
    {
      ret = bleInvalidRange;
    }
    break;

    case ACCELEROMETER_CONF:
      if ( len == sizeof ( uint8 ) )
      {
        accelCfg = *((uint8*)value);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case ACCELEROMETER_PERI:
      if ( len == sizeof ( uint8 ) )
      {
        accPer = *((uint8*)value);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ( ret );
}

/*********************************************************************
 * @fn      Accel_GetParameter
 *
 * @brief   Get a Sensor Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t Accel_GetParameter( uint8 param, void *value )
{
  bStatus_t ret = SUCCESS;

  switch ( param )
  {
    case ACCELEROMETER_DATA:
      VOID osal_memcpy( value, accelData, ACCELEROMETER_DATA_LEN );
      break;

    case ACCELEROMETER_CONF:
      *((uint8*)value) = accelCfg;
      break;

    case ACCELEROMETER_PERI:
      *((uint8*)value) = accPer;
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ( ret );
}

/*********************************************************************
 * @fn          acc_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 *
 * @return      Success or Failure
 */
static uint8 acc_ReadAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
                            uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen )
{
  uint16 uuid;
  bStatus_t status = SUCCESS;

  // If attribute permissions require authorization to read, return error
  if ( gattPermitAuthorRead( pAttr->permissions ) )
  {
    // Insufficient authorization
    return ( ATT_ERR_INSUFFICIENT_AUTHOR );
  }

  // Make sure it's not a blob operation (no attributes in the profile are long)
  if ( offset > 0 )
  {
    return ( ATT_ERR_ATTR_NOT_LONG );
  }

  if (utilExtractUuid16(pAttr,&uuid) == FAILURE) {
    // Invalid handle
    *pLen = 0;
    return ATT_ERR_INVALID_HANDLE;
  }

  switch ( uuid )
  {
    // No need for "GATT_SERVICE_UUID" or "GATT_CLIENT_CHAR_CFG_UUID" cases;
    // gattserverapp handles those reads
    case ACCELEROMETER_DATA_UUID:
      *pLen = ACCELEROMETER_DATA_LEN;
      VOID osal_memcpy( pValue, pAttr->pValue, ACCELEROMETER_DATA_LEN );
      break;

    case ACCELEROMETER_CONF_UUID:
    case ACCELEROMETER_PERI_UUID:
      *pLen = 1;
      pValue[0] = *pAttr->pValue;
      break;

    default:
      *pLen = 0;
      status = ATT_ERR_ATTR_NOT_FOUND;
      break;
    }

  return ( status );
}

/*********************************************************************
* @fn      acc_WriteAttrCB
*
* @brief   Validate attribute data prior to a write operation
*
* @param   connHandle - connection message was received on
* @param   pAttr - pointer to attribute
* @param   pValue - pointer to data to be written
* @param   len - length of data
* @param   offset - offset of the first octet to be written
*
* @return  Success or Failure
*/
static bStatus_t acc_WriteAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
                                           uint8 *pValue, uint8 len, uint16 offset )
{
  bStatus_t status = SUCCESS;
  uint8 notifyApp = 0xFF;
  uint16 uuid;

  // If attribute permissions require authorization to write, return error
  if ( gattPermitAuthorWrite( pAttr->permissions ) )
  {
    // Insufficient authorization
    return ( ATT_ERR_INSUFFICIENT_AUTHOR );
  }

  if (utilExtractUuid16(pAttr,&uuid) == FAILURE) {
    // Invalid handle
    return ATT_ERR_INVALID_HANDLE;
  }

  switch ( uuid )
  {
    case ACCELEROMETER_DATA_UUID:
      // Should not get here
      break;

    case ACCELEROMETER_CONF_UUID:
      // Validate the value
      // Make sure it's not a blob oper
      if ( offset == 0 )
      {
        if ( len != 1 )
        {
          status = ATT_ERR_INVALID_VALUE_SIZE;
        }
      }
      else
      {
        status = ATT_ERR_ATTR_NOT_LONG;
      }

      // Write the value
      if ( status == SUCCESS )
      {
        uint8 *pCurValue = (uint8 *)pAttr->pValue;

        *pCurValue = pValue[0];

        if( pAttr->pValue == &accelCfg )
        {
          notifyApp = ACCELEROMETER_CONF;
        }
      }
      break;

    case ACCELEROMETER_PERI_UUID:
      // Validate the value
      // Make sure it's not a blob oper
      if ( offset == 0 )
      {
        if ( len != 1 )
        {
          status = ATT_ERR_INVALID_VALUE_SIZE;
        }
      }
      else
      {
        status = ATT_ERR_ATTR_NOT_LONG;
      }
      // Write the value
      if ( status == SUCCESS )
      {
        if (pValue[0]>=(ACCELEROMETER_MIN_PERIOD/ACCELEROMETER_TIME_UNIT))
        {

          uint8 *pCurValue = (uint8 *)pAttr->pValue;
          *pCurValue = pValue[0];

          if( pAttr->pValue == &accPer )
          {
            notifyApp = ACCELEROMETER_PERI;
          }
        }
        else
        {
           status = ATT_ERR_INVALID_VALUE;
        }
      }
      break;

    case GATT_CLIENT_CHAR_CFG_UUID:
      status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                              offset, GATT_CLIENT_CFG_NOTIFY );
      break;

    default:
      // Should never get here!
      status = ATT_ERR_ATTR_NOT_FOUND;
      break;
  }

  // If a charactersitic value changed then callback function to notify application of change
  if ( (notifyApp != 0xFF ) && accel_AppCBs && accel_AppCBs->pfnAccelChange )
  {
    accel_AppCBs->pfnAccelChange( notifyApp );
  }

  return ( status );
}

/*********************************************************************
 * @fn          acc_HandleConnStatusCB
 *
 * @brief       Sensor Profile link status change handler function.
 *
 * @param       connHandle - connection handle
 * @param       changeType - type of change
 *
 * @return      none
 */
static void acc_HandleConnStatusCB( uint16 connHandle, uint8 changeType )
{
  // Make sure this is not loopback connection
  if ( connHandle != LOOPBACK_CONNHANDLE )
  {
    // Reset Client Char Config if connection has dropped
    if ( ( changeType == LINKDB_STATUS_UPDATE_REMOVED )      ||
         ( ( changeType == LINKDB_STATUS_UPDATE_STATEFLAGS ) &&
           ( !linkDB_Up( connHandle ) ) ) )
    {
      GATTServApp_InitCharCfg( connHandle, accelDataConfig );
    }
  }
}


/*********************************************************************
*********************************************************************/



#if !defined(HAL_I2C)

void HalAccInit(void)
{
}

bool HalAccRead(uint8 *pBuf )
{
  return false;
}

bool HalAccTest(void)
{
  return false;
}

#endif
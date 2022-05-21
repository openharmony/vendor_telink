/******************************************************************************
 * Copyright (c) 2022 Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include "stack/ble/ble.h"

/**
 *  @brief  GATT table descriptors enumeration
 */
typedef enum {
    ATT_H_START = 0,

    /* GAP service */
    GenericAccess_PS_H,                     // UUID: 2800, VALUE: uuid 1800
    GenericAccess_DeviceName_CD_H,          // UUID: 2803, VALUE: Prop: Read | Notify
    GenericAccess_DeviceName_DP_H,          // UUID: 2A00, VALUE: device name
    GenericAccess_Appearance_CD_H,          // UUID: 2803, VALUE: Prop: Read
    GenericAccess_Appearance_DP_H,          // UUID: 2A01, VALUE: appearance
    CONN_PARAM_CD_H,                        // UUID: 2803, VALUE: Prop: Read
    CONN_PARAM_DP_H,                        // UUID: 2A04, VALUE: connParameter

    /* GATT service */
    GenericAttribute_PS_H,                  // UUID: 2800, VALUE: uuid 1801
    GenericAttribute_ServiceChanged_CD_H,   // UUID: 2803, VALUE: Prop: Indicate
    GenericAttribute_ServiceChanged_DP_H,   // UUID: 2A05, VALUE: service change
    GenericAttribute_ServiceChanged_CCB_H,  // UUID: 2902, VALUE: serviceChangeCCC

    /* Device info service */
    DeviceInformation_PS_H,                 // UUID: 2800, VALUE: uuid 180A
    DeviceInformation_pnpID_CD_H,           // UUID: 2803, VALUE: Prop: Read
    DeviceInformation_pnpID_DP_H,           // UUID: 2A50, VALUE: PnPtrs

    ATT_END_H,
}ATT_HANDLE;

/**
 *  @brief  connect parameters structure for ATT
 */
typedef struct {
    /** Minimum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
    u16 intervalMin;
    /** Maximum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
    u16 intervalMax;
    /** Number of LL latency connection events (0x0000 - 0x03e8) */
    u16 latency;
    /** Connection Timeout (0x000A - 0x0C80 * 10 ms) */
    u16 timeout;
} gap_periConnectParams_t;

/* UUIDs */
static const u16 my_primaryServiceUUID  = GATT_UUID_PRIMARY_SERVICE;
static const u16 my_gapServiceUUID      = SERVICE_UUID_GENERIC_ACCESS;
static const u16 my_characterUUID       = GATT_UUID_CHARACTER;
static const u16 my_devNameUUID         = GATT_UUID_DEVICE_NAME;
static const u16 my_gattServiceUUID     = SERVICE_UUID_GENERIC_ATTRIBUTE;
static const u16 serviceChangeUUID      = GATT_UUID_SERVICE_CHANGE;
static const u16 clientCharacterCfgUUID = GATT_UUID_CLIENT_CHAR_CFG;
static const u16 my_devServiceUUID      = SERVICE_UUID_DEVICE_INFORMATION;
static const u16 my_PnPUUID             = CHARACTERISTIC_UUID_PNP_ID;
static const u16 my_appearanceUUID      = GATT_UUID_APPEARANCE;
static const u16 my_periConnParamUUID   = GATT_UUID_PERI_CONN_PARAM;

/* Characteristics */
static const u8 my_devNameCharVal[5] = {
    CHAR_PROP_READ | CHAR_PROP_NOTIFY,
    U16_LO(GenericAccess_DeviceName_DP_H), U16_HI(GenericAccess_DeviceName_DP_H),
    U16_LO(GATT_UUID_DEVICE_NAME), U16_HI(GATT_UUID_DEVICE_NAME)
};

static const u8 my_appearanceCharVal[5] = {
    CHAR_PROP_READ,
    U16_LO(GenericAccess_Appearance_DP_H), U16_HI(GenericAccess_Appearance_DP_H),
    U16_LO(GATT_UUID_APPEARANCE), U16_HI(GATT_UUID_APPEARANCE)
};

static const u8 my_periConnParamCharVal[5] = {
    CHAR_PROP_READ,
    U16_LO(CONN_PARAM_DP_H), U16_HI(CONN_PARAM_DP_H),
    U16_LO(GATT_UUID_PERI_CONN_PARAM), U16_HI(GATT_UUID_PERI_CONN_PARAM)
};

static const u8 my_serviceChangeCharVal[5] = {
    CHAR_PROP_INDICATE,
    U16_LO(GenericAttribute_ServiceChanged_DP_H), U16_HI(GenericAttribute_ServiceChanged_DP_H),
    U16_LO(GATT_UUID_SERVICE_CHANGE), U16_HI(GATT_UUID_SERVICE_CHANGE)
};

static const u8 my_PnCharVal[5] = {
    CHAR_PROP_READ,
    U16_LO(DeviceInformation_pnpID_DP_H), U16_HI(DeviceInformation_pnpID_DP_H),
    U16_LO(CHARACTERISTIC_UUID_PNP_ID), U16_HI(CHARACTERISTIC_UUID_PNP_ID)
};

/* Values */
static const u8 my_devName[] = {'e', 'S', 'a', 'm', 'p', 'l', 'e'};
static const u16 my_appearance = GAP_APPEARE_UNKNOWN;
static const gap_periConnectParams_t my_periConnParameters = {8, 11, 0, 1000};
static u16 serviceChangeVal[2] = {0};
static u8 serviceChangeCCC[2] = {0, 0};
static const u8 my_PnPtrs [] = {0x02, 0x8a, 0x24, 0x66, 0x82, 0x01, 0x00};

void AppBleGattInit(void)
{
    /* Define our GATT table here */
    static const attribute_t gattTable[] = {
        {
            ATT_END_H - 1,
            0,
            0,
            0,
            0,
            0
        }, // total num of attribute

        // 0001 - 0007  gap
        {
            7,
            ATT_PERMISSIONS_READ,
            2,
            2,
            (u8 *)(&my_primaryServiceUUID),
            (u8 *)(&my_gapServiceUUID),
            0
        },
        {
            0, 
            ATT_PERMISSIONS_READ,
            2, 
            sizeof(my_devNameCharVal),
            (u8 *)(&my_characterUUID),
            (u8 *)(my_devNameCharVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_devName),
            (u8 *)(&my_devNameUUID),
            (u8 *)(my_devName),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_appearanceCharVal),
            (u8 *)(&my_characterUUID),
            (u8 *)(my_appearanceCharVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_appearance),
            (u8 *)(&my_appearanceUUID),
            (u8 *)(&my_appearance),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_periConnParamCharVal),
            (u8 *)(&my_characterUUID),
            (u8 *)(my_periConnParamCharVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_periConnParameters),
            (u8 *)(&my_periConnParamUUID),
            (u8 *)(&my_periConnParameters),
            0
        },
        // 0008 - 000b gatt
        {
            4,
            ATT_PERMISSIONS_READ,
            2,
            2,
            (u8 *)(&my_primaryServiceUUID),
            (u8 *)(&my_gattServiceUUID),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_serviceChangeCharVal),
            (u8 *)(&my_characterUUID),
            (u8 *)(my_serviceChangeCharVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(serviceChangeVal),
            (u8 *)(&serviceChangeUUID),
            (u8 *)(&serviceChangeVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_RDWR,
            2,
            sizeof(serviceChangeCCC),
            (u8 *)(&clientCharacterCfgUUID),
            (u8 *)(serviceChangeCCC),
            0
        },

        // 000c - 000e  device Information Service
        {
            3,
            ATT_PERMISSIONS_READ,
            2,
            2,
            (u8 *)(&my_primaryServiceUUID),
            (u8 *)(&my_devServiceUUID),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_PnCharVal),
            (u8 *)(&my_characterUUID),
            (u8 *)(my_PnCharVal),
            0
        },
        {
            0,
            ATT_PERMISSIONS_READ,
            2,
            sizeof(my_PnPtrs),
            (u8 *)(&my_PnPUUID),
            (u8 *)(my_PnPtrs),
            0
        },
    };

    /* Set up GATT table */
    bls_att_setAttributeTable((u8 *)gattTable);
}

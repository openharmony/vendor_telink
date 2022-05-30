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
#include <assert.h>

#include <los_compiler.h>
#include <hiview_log.h>
#include <gpio_if.h>

#include <tl_common.h>
#include <drivers.h>
#include <stack/ble/ble.h>

#include <board_config.h>

#include "app_config.h"
#include "app_att.h"
#include "app.h"

#include "uni_ble.h"

#define ACL_CONN_MAX_RX_OCTETS    27
#define ACL_CONN_MAX_TX_OCTETS    27
#define ACL_TX_FIFO_SIZE          48
#define ACL_TX_FIFO_NUM           17
#define ACL_RX_FIFO_SIZE          48
#define ACL_RX_FIFO_NUM           8

#define ATT_MTU_SLAVE_RX_MAX_SIZE   23
#define	MTU_S_BUFF_SIZE_MAX			CAL_MTU_BUFF_SIZE(ATT_MTU_SLAVE_RX_MAX_SIZE)

#if TELINK_SDK_B91_BLE_SINGLE
#undef SLAVE_MAX_NUM
#define SLAVE_MAX_NUM 1
#endif /* TELINK_SDK_B91_BLE_SINGLE */

/**
 * @brief  This function do initialization of BLE advertisement
 * @param  none
 * @return return BLE_SUCCESS in case if advertisement was inited successfully
 */
static ble_sts_t AppBleAdvInit(void)
{
    ble_sts_t status = BLE_SUCCESS;

    static const u8	tbl_advData[] = {
        0x05, 0x09, 'e', 'H', 'I', 'D',
        0x02, 0x01, 0x05,                        // BLE limited discoverable mode and BR/EDR not supported
        0x03, 0x19, 0x80, 0x01,                  // 384, Generic Remote Control, Generic category
        0x05, 0x02, 0x12, 0x18, 0x0F, 0x18,      // incomplete list of service class UUIDs (0x1812, 0x180F)
    };

    static const u8	tbl_scanRsp [] = {
        0x08, 0x09, 'e', 'S', 'a', 'm', 'p', 'l', 'e',
    };

    status = uni_ble_ll_setAdvParam(ADV_INTERVAL_30MS, ADV_INTERVAL_35MS, ADV_TYPE_CONNECTABLE_UNDIRECTED,
        OWN_ADDRESS_PUBLIC, 0, NULL, BLT_ENABLE_ADV_ALL, ADV_FP_NONE);
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "uni_ble_ll_setAdvParam(): %d", status);
        return status;
    }

    status = uni_ble_ll_setAdvData((u8 *)tbl_advData, sizeof(tbl_advData));
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "uni_ble_ll_setAdvData(): %d", status);
        return status;
    }

    status = uni_ble_ll_setScanRspData((u8 *)tbl_scanRsp, sizeof(tbl_scanRsp));
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "uni_ble_ll_setScanRspData(): %d", status);
        return status;
    }

    status = uni_ble_ll_setAdvEnable(BLC_ADV_ENABLE); // Advertising enable
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "uni_ble_ll_setAdvEnable(): %d", status);
        return status;
    }

    return status;
}

static void connect(void)
{
    GpioWrite(LED_WHITE_HDF, GPIO_VAL_HIGH);
}

static void disconnect(void)
{
    GpioWrite(LED_WHITE_HDF, GPIO_VAL_LOW);
}

/**
 * @brief  This function do initialization of BLE connection mode
 * @param  none
 * @return return BLE_SUCCESS in case if connection mode was inited successfully
 */
static ble_sts_t AppBleConnInit(void)
{
    ble_sts_t status = BLE_SUCCESS;
    static u8 rxFufoBuff[ACL_RX_FIFO_SIZE * ACL_RX_FIFO_NUM] = {0};
    static u8 txFifoBuff[ACL_TX_FIFO_SIZE * ACL_TX_FIFO_NUM * SLAVE_MAX_NUM] = {0};

    status = uni_ble_ll_initAclConnTxFifo(txFifoBuff, ACL_TX_FIFO_SIZE, ACL_TX_FIFO_NUM, SLAVE_MAX_NUM);
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "uni_ble_ll_initAclConnTxFifo(): %d", status);
        return status;
    }

    status = blc_ll_initAclConnRxFifo(rxFufoBuff, ACL_RX_FIFO_SIZE, ACL_RX_FIFO_NUM);
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_initAclConnRxFifo(): %d", status);
        return status;
    }

    status = blc_controller_check_appBufferInitialization();
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_controller_check_appBufferInitialization(): %d", status);
        return status;
    }

#if TELINK_SDK_B91_BLE_MULTI
    status = blc_ll_setMaxConnectionNumber(MASTER_MAX_NUM, SLAVE_MAX_NUM);
    if (status != BLE_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setMaxConnectionNumber(): %d", status);
        return status;
    }
#endif /* TELINK_SDK_B91_BLE_MULTI */

    /* GAP initialization must be done before any other host feature initialization !!! */
    uni_ble_init();

#if TELINK_SDK_B91_BLE_MULTI
    /* ACL connection L2CAP layer MTU TX & RX data FIFO allocation, Begin */
    static u8 mtu_s_rx_fifo[SLAVE_MAX_NUM * MTU_S_BUFF_SIZE_MAX];
    static u8 mtu_s_tx_fifo[SLAVE_MAX_NUM * MTU_S_BUFF_SIZE_MAX];
    /* ACL connection L2CAP layer MTU TX & RX data FIFO allocation, End */

    /* L2CAP buffer initialization */
    blc_l2cap_initAclConnSlaveMtuBuffer(mtu_s_rx_fifo, MTU_S_BUFF_SIZE_MAX, mtu_s_tx_fifo, MTU_S_BUFF_SIZE_MAX);
#endif /* TELINK_SDK_B91_BLE_MULTI */

    AppBleGattInit();

    uni_ble_l2cap_register_data_handler();

    GpioSetDir(LED_WHITE_HDF, GPIO_DIR_OUT);

    uni_ble_register_connect_disconnect_cb(connect, disconnect);

    return status;
}

static void AppBleInit(void)
{
    ble_sts_t status = BLE_SUCCESS;
    u8 mac_public[6];
    u8 mac_random_static[6];

    /*
     * for 1M Flash, flash_sector_mac_address equals to 0xFF000
     * for 2M Flash, flash_sector_mac_address equals to 0x1FF000
     */
    blc_initMacAddress(flash_sector_mac_address, mac_public, mac_random_static);

    blc_ll_initBasicMCU();                       // Mandatory
    blc_ll_initStandby_module(mac_public);       // Mandatory
    uni_ble_ll_initAdvertising_module();         // ADV Module: Mandatory for BLE slave

    uni_ble_ll_initConnection_module();
    uni_ble_ll_initSlaveRole_module();

    status = AppBleAdvInit();
    HILOG_INFO(HILOG_MODULE_APP, "AppBleAdvInit(): %d", status);
    assert(status == BLE_SUCCESS);

    status = AppBleConnInit();
    HILOG_INFO(HILOG_MODULE_APP, "AppBleConnInit(): %d", status);
    assert(status == BLE_SUCCESS);
}

/**
 * @brief       This function do normal MCU initialization
 * @param[in]   none
 * @return      none
 */
void UserInitNormal(void)
{
    /*
     * Random number generator must be initiated here( in the beginning of user_init_nromal).
     * when deepSleep retention wakeUp, no need initialize again
     */
    random_generator_init();  // Mandatory

    /* init BLE stack */
    AppBleInit();
}

/**
 * @brief       This is main_loop function
 * @param[in]   none
 * @return      none
 */
_attribute_no_inline_ void MainLoop(void)
{
    uni_ble_sdk_main_loop();
}

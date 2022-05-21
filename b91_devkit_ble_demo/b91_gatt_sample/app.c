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

#include <board_config.h>

#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"

#include "app_config.h"
#include "app.h"
#include "app_att.h"

#define ACL_CONN_MAX_RX_OCTETS    27
#define ACL_CONN_MAX_TX_OCTETS    27
#define ACL_TX_FIFO_SIZE          48
#define ACL_TX_FIFO_NUM           17
#define ACL_RX_FIFO_SIZE          48
#define ACL_RX_FIFO_NUM           8

#define ATT_MTU_SLAVE_RX_MAX_SIZE   23
#define	MTU_S_BUFF_SIZE_MAX			CAL_MTU_BUFF_SIZE(ATT_MTU_SLAVE_RX_MAX_SIZE)

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

    status = blc_ll_setAdvParam(ADV_INTERVAL_30MS, ADV_INTERVAL_35MS,
                                ADV_TYPE_CONNECTABLE_UNDIRECTED, OWN_ADDRESS_PUBLIC,
                                0,  NULL,
                                BLT_ENABLE_ADV_ALL,
                                ADV_FP_NONE);
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setAdvParam(): %d", status);
        return status;
    }

    status = blc_ll_setAdvData((u8 *)tbl_advData, sizeof(tbl_advData));
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setAdvData(): %d", status);
        return status;
    }

    status = blc_ll_setScanRspData((u8 *)tbl_scanRsp, sizeof(tbl_scanRsp));
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setScanRspData(): %d", status);
        return status;
    }

    status = blc_ll_setAdvEnable(BLC_ADV_ENABLE);  //adv enable
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setAdvEnable(): %d", status);
        return status;
    }

    return status;
}

/**
 * @brief      BLE controller event handler call-back.
 * @param[in]  event    event type
 * @param[in]  param    Pointer point to event parameter buffer.
 * @param[in]  paramLen the length of event parameter.
 * @return
 */
int AppControllerEventCallback(u32 event, u8 *param, int paramLen)
{
    UNUSED(paramLen);

    if (event & HCI_FLAG_EVENT_BT_STD)  //Controller HCI event
    {
        u8 evtCode = event & 0xff;

        //------------ disconnect -------------------------------------
        if(evtCode == HCI_EVT_DISCONNECTION_COMPLETE)  //connection terminate
        {
            GpioWrite(LED_WHITE_HDF, GPIO_VAL_LOW);
        }
        else if(evtCode == HCI_EVT_LE_META)  //LE Event
        {
            u8 subEvt_code = param[0];

            //------hci le event: le connection complete event---------------------------------
            if (subEvt_code == HCI_SUB_EVT_LE_CONNECTION_COMPLETE)  // connection complete
            {
                GpioWrite(LED_WHITE_HDF, GPIO_VAL_HIGH);
            }
        }
    }

    return 0;
}

/**
 * @brief  This function do initialization of BLE connection mode
 * @param  none
 * @return return BLE_SUCCESS in case if connection mode was inited successfully
 */
static ble_sts_t AppBleConnInit(void)
{
    ble_sts_t status = BLE_SUCCESS;
    static u8 txFifoBuff[ACL_TX_FIFO_SIZE * ACL_TX_FIFO_NUM * SLAVE_MAX_NUM] = {0};
    static u8 rxFufoBuff[ACL_RX_FIFO_SIZE * ACL_RX_FIFO_NUM] = {0};

    /***************** ACL connection L2CAP layer MTU TX & RX data FIFO allocation, Begin ********************************/
    static 	u8 mtu_s_rx_fifo[SLAVE_MAX_NUM * MTU_S_BUFF_SIZE_MAX];
    static	u8 mtu_s_tx_fifo[SLAVE_MAX_NUM * MTU_S_BUFF_SIZE_MAX];
    /***************** ACL connection L2CAP layer MTU TX & RX data FIFO allocation, End **********************************/

    status = blc_ll_initAclConnSlaveTxFifo(txFifoBuff, ACL_TX_FIFO_SIZE, ACL_TX_FIFO_NUM, SLAVE_MAX_NUM);
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_initAclConnSlaveTxFifo(): %d", status);
        return status;
    }

    status = blc_ll_initAclConnRxFifo(rxFufoBuff, ACL_RX_FIFO_SIZE, ACL_RX_FIFO_NUM);
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_initAclConnRxFifo(): %d", status);
        return status;
    }

    status = blc_controller_check_appBufferInitialization();
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_controller_check_appBufferInitialization(): %d", status);
        return status;
    }

    status = blc_ll_setMaxConnectionNumber( MASTER_MAX_NUM, SLAVE_MAX_NUM);
    if(status != BLE_SUCCESS)
    {
        HILOG_ERROR(HILOG_MODULE_APP, "blc_ll_setMaxConnectionNumber(): %d", status);
        return status;
    }

    /* GAP initialization must be done before any other host feature initialization !!! */
    blc_gap_init();

    /* L2CAP buffer Initialization */
    blc_l2cap_initAclConnSlaveMtuBuffer(mtu_s_rx_fifo, MTU_S_BUFF_SIZE_MAX, mtu_s_tx_fifo, MTU_S_BUFF_SIZE_MAX);

    AppBleGattInit();

    //////////// HCI Initialization  Begin /////////////////////////
    blc_hci_registerControllerDataHandler (blc_l2cap_pktHandler);

    GpioSetDir(LED_WHITE_HDF, GPIO_DIR_OUT);

    blc_hci_le_setEventMask_cmd(HCI_EVT_MASK_DISCONNECTION_COMPLETE | HCI_LE_EVT_MASK_CONNECTION_COMPLETE);
    blc_hci_registerControllerEventHandler(AppControllerEventCallback); //controller hci event to host all processed in this func

    return status;
}

static void AppBleInit(void)
{
    ble_sts_t status = BLE_SUCCESS;
    u8 mac_public[6];
    u8 mac_random_static[6];

    /* for 1M   Flash, flash_sector_mac_address equals to 0xFF000
     * for 2M   Flash, flash_sector_mac_address equals to 0x1FF000 */
    blc_initMacAddress(flash_sector_mac_address, mac_public, mac_random_static);

    blc_ll_initBasicMCU();                       //mandatory
    blc_ll_initStandby_module(mac_public);       //mandatory
    blc_ll_initLegacyAdvertising_module();             //adv module: mandatory for BLE slave,

    blc_ll_initAclConnection_module();
    blc_ll_initAclSlaveRole_module();

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
    /* random number generator must be initiated here( in the beginning of user_init_nromal).
     * When deepSleep retention wakeUp, no need initialize again */
    random_generator_init();  //this is must

    /* init BLE stack */
    AppBleInit();
}

/**
 * @brief       This is main_loop function
 * @param[in]   none
 * @return      none
 */
_attribute_no_inline_ void MainLoop (void)
{
    blc_sdk_main_loop();
}

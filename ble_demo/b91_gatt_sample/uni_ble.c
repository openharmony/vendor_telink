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

#include <los_compiler.h>

#include <tl_common.h>
#include <drivers.h>
#include <stack/ble/ble.h>

#include "uni_ble.h"

struct {
    connect_cb_t connect;
    connect_cb_t disconnect;
} g_app_ble_state;

#if TELINK_SDK_B91_BLE_SINGLE

ble_sts_t uni_ble_ll_setAdvParam(u16 intervalMin, u16 intervalMax, adv_type_t advType, own_addr_type_t ownAddrType,
                                 u8 peerAddrType, u8 *peerAddr, adv_chn_map_t adv_channelMap,
                                 adv_fp_type_t advFilterPolicy)
{
    return bls_ll_setAdvParam(intervalMin, intervalMax, advType, ownAddrType, peerAddrType, peerAddr, adv_channelMap,
                              advFilterPolicy);
}

ble_sts_t uni_ble_ll_setAdvData(u8 *data, u8 len)
{
    return bls_ll_setAdvData(data, len);
}

ble_sts_t uni_ble_ll_setScanRspData(u8 *data, u8 len)
{
    return bls_ll_setScanRspData(data, len);
}

ble_sts_t uni_ble_ll_setAdvEnable(int adv_enable)
{
    return bls_ll_setAdvEnable(adv_enable);
}

ble_sts_t uni_ble_ll_initAclConnTxFifo(u8 *pTxbuf, int fifo_size, int fifo_number, int conn_number)
{
    UNUSED(conn_number);

    return blc_ll_initAclConnTxFifo(pTxbuf, fifo_size, fifo_number);
}

void uni_ble_init(void)
{
    blc_gap_peripheral_init();
}

void uni_ble_l2cap_register_data_handler(void)
{
    /* L2CAP initialization */
    blc_l2cap_register_handler((void *)blc_l2cap_packet_receive);
}

void uni_ble_ll_initAdvertising_module(void)
{
    blc_ll_initAdvertising_module();
}

void uni_ble_ll_initConnection_module(void)
{
    blc_ll_initConnection_module();
}

void uni_ble_ll_initSlaveRole_module(void)
{
    blc_ll_initSlaveRole_module();
}

void uni_ble_sdk_main_loop(void)
{
    blt_sdk_main_loop();
}

_attribute_ram_code_ void uni_ble_sdk_irq_handler(void)
{
    irq_blt_sdk_handler();
}

static void connect_cb(u8 e, u8 *p, int n)
{
    UNUSED(e);
    UNUSED(p);
    UNUSED(n);

    connect_cb_t func = g_app_ble_state.connect;
    if (func) {
        func();
    }
}

static void disconnect_cb(u8 e, u8 *p, int n)
{
    UNUSED(e);
    UNUSED(p);
    UNUSED(n);

    connect_cb_t func = g_app_ble_state.disconnect;
    if (func) {
        func();
    }
}

void uni_ble_register_connect_disconnect_cb(connect_cb_t on_connect, connect_cb_t on_disconnect)
{
    g_app_ble_state.connect = on_connect;
    g_app_ble_state.disconnect = on_disconnect;
    bls_app_registerEventCallback(BLT_EV_FLAG_CONNECT, connect_cb);
    bls_app_registerEventCallback(BLT_EV_FLAG_TERMINATE, disconnect_cb);
}

#elif TELINK_SDK_B91_BLE_MULTI

ble_sts_t uni_ble_ll_setAdvParam(u16 intervalMin, u16 intervalMax, adv_type_t advType, own_addr_type_t ownAddrType,
                                 u8 peerAddrType, u8 *peerAddr, adv_chn_map_t adv_channelMap,
                                 adv_fp_type_t advFilterPolicy)
{
    return blc_ll_setAdvParam(intervalMin, intervalMax, advType, ownAddrType, peerAddrType, peerAddr, adv_channelMap,
                              advFilterPolicy);
}

ble_sts_t uni_ble_ll_setAdvData(u8 *data, u8 len)
{
    return blc_ll_setAdvData(data, len);
}

ble_sts_t uni_ble_ll_setScanRspData(u8 *data, u8 len)
{
    return blc_ll_setScanRspData(data, len);
}

ble_sts_t uni_ble_ll_setAdvEnable(int adv_enable)
{
    return blc_ll_setAdvEnable(adv_enable);
}

ble_sts_t uni_ble_ll_initAclConnTxFifo(u8 *pTxbuf, int fifo_size, int fifo_number, int conn_number)
{
    return  blc_ll_initAclConnSlaveTxFifo(pTxbuf, fifo_size, fifo_number, conn_number);
}

void uni_ble_init(void)
{
    blc_gap_init();
}

void uni_ble_l2cap_register_data_handler(void)
{
    /* HCI initialization begin */
    blc_hci_registerControllerDataHandler(blc_l2cap_pktHandler);
}

void uni_ble_ll_initAdvertising_module(void)
{
    blc_ll_initLegacyAdvertising_module();
}

void uni_ble_ll_initConnection_module(void)
{
    blc_ll_initAclConnection_module();
}

void uni_ble_ll_initSlaveRole_module(void)
{
    blc_ll_initAclSlaveRole_module();
}

void uni_ble_sdk_main_loop(void)
{
    blc_sdk_main_loop();
}

_attribute_ram_code_ void uni_ble_sdk_irq_handler(void)
{
    blc_sdk_irq_handler();
}

/**
 * @brief      BLE controller event handler call-back.
 * @param[in]  event    event type
 * @param[in]  param    Pointer point to event parameter buffer.
 * @param[in]  paramLen the length of event parameter.
 * @return
 */
static int AppControllerEventCallback(u32 event, u8 *param, int paramLen)
{
    UNUSED(paramLen);

    if (event & HCI_FLAG_EVENT_BT_STD) {
        u8 evtCode = event & 0xff;

        // ------------ disconnect -------------------------------------
        if (evtCode == HCI_EVT_DISCONNECTION_COMPLETE) {
            connect_cb_t func = g_app_ble_state.disconnect;
            if (func) {
                func();
            }
        } else if (evtCode == HCI_EVT_LE_META) {
            u8 subEvt_code = param[0];

            // ------hci le event: le connection complete event---------------------------------
            if (subEvt_code == HCI_SUB_EVT_LE_CONNECTION_COMPLETE) {
                connect_cb_t func = g_app_ble_state.connect;
                if (func) {
                    func();
                }
            }
        }
    }

    return 0;
}

void uni_ble_register_connect_disconnect_cb(connect_cb_t on_connect, connect_cb_t on_disconnect)
{
    g_app_ble_state.connect = on_connect;
    g_app_ble_state.disconnect = on_disconnect;

    blc_hci_le_setEventMask_cmd(HCI_EVT_MASK_DISCONNECTION_COMPLETE | HCI_LE_EVT_MASK_CONNECTION_COMPLETE);

    // controller hci event to host all processed in this func
    blc_hci_registerControllerEventHandler(AppControllerEventCallback);
}

#endif /* TELINK_SDK_B91_BLE_SINGLE */

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

#ifndef UNI_BLE_H
#define UNI_BLE_H

#include <stack/ble/ble.h>

typedef void (*connect_cb_t)(void);

ble_sts_t uni_ble_ll_setAdvParam(u16 intervalMin, u16 intervalMax, adv_type_t advType, own_addr_type_t ownAddrType,
                                 u8 peerAddrType, u8 *peerAddr, adv_chn_map_t adv_channelMap,
                                 adv_fp_type_t advFilterPolicy);

ble_sts_t uni_ble_ll_setAdvData(u8 *data, u8 len);

ble_sts_t uni_ble_ll_setScanRspData(u8 *data, u8 len);

ble_sts_t uni_ble_ll_setAdvEnable(int adv_enable);

ble_sts_t uni_ble_ll_initAclConnTxFifo(u8 *pTxbuf, int fifo_size, int fifo_number, int conn_number);

void uni_ble_init(void);

void uni_ble_l2cap_register_data_handler(void);

void uni_ble_ll_initAdvertising_module(void);

void uni_ble_ll_initConnection_module(void);

void uni_ble_ll_initSlaveRole_module(void);

void uni_ble_sdk_main_loop(void);

void uni_ble_sdk_irq_handler(void);

void uni_ble_register_connect_disconnect_cb(connect_cb_t on_connect, connect_cb_t on_disconnect);

#endif // UNI_BLE_H

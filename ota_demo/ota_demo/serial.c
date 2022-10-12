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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <los_event.h>
#include <los_mux.h>
#include <los_interrupt.h>

#include <B91/clock.h>
#include <B91/plic.h>
#include <B91/uart.h>

#include <b91_irq.h>

#include "serial.h"

#define SERVICE_UART_PORT        UART1
#define SERVICE_UART_PIN_TX      UART1_TX_PE0
#define SERVICE_UART_PIN_RX      UART1_RX_PE2
#define SERVICE_UART_PARITY      UART_PARITY_NONE
#define SERVICE_UART_STOP_BITS   UART_STOP_BIT_ONE
#define SERVICE_UART_BAUDRATE    115200

#define HZ_IN_MHZ (1000 * 1000)

#define UARTS_COUNT 2
#define UART_HW_FIFO_SIZE 8

#define UART_RX_BUF 256

#define NON_VALID_MUTEX_ID UINT32_MAX

/**
 * TX:
 * - Any interface functions are thread-safe after kernel started and @c uart_init is called.
 * - If buffer is enabled then data are stored in buffer and sent on background using @c UartIdleThreadHandler.
 * - Considering that Idle thread will capture mutex, all threads that want put data to buffer will wait while Idle
 *     thread will send data to UART. To prevent it, there is mechanism of preemption. Threads that indend to put data
 *     to buffer signalize about it using @c threadWait variable and Idle function will exit after next byte printed.
 *
 * RX:
 * LiteOS Queue has very huge overhead for byte buffer where access is performed byte by byte. LiteOS Ringbuf
 * doesn't support blocking when no data available. So there is implemented simple ring buffer using Spinlock for
 * thread-safety and Event for data waiting.
 */
static struct {
    struct {
        struct {
            UINT32 mux;
#if LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE
            size_t start;
            size_t end;
    #if UART_TX_THREAD_PREEMTION_ENABLE
            bool threadWait;
    #endif /* ENABLE_UART_THREAD_PREEMTION */
#endif     /* LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE */
        } tx;
        struct {
            size_t start;
            size_t end;
            EVENT_CB_S event;
            struct {
                UINT8 *ptr;
                INT32 nbytes;
                INT32 max;
            } irq_buf;
        } rx;
    } dev[UARTS_COUNT];

    bool init;
} g_uartState = {
    .dev[0 ...(UARTS_COUNT - 1)] = {
        .tx = {
            .mux = NON_VALID_MUTEX_ID,
#if LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE
            .start = 0,
            .end = 0,
    #if UART_TX_THREAD_PREEMTION_ENABLE
            .threadWait = false,
    #endif /* ENABLE_UART_THREAD_PREEMTION */
#endif /* LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE */
        },
        .rx = {
            .start = 0,
            .end = 0,
        },
    },
    .init = false,
};

__attribute__((unused)) static struct {
#if LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE
    CHAR tx[UART_TX_BUF];
#endif /* LOSCFG_UART_BUFFERED_TX_BUFFER_ENABLE */
    CHAR rx[UART_RX_BUF];
} g_uartBuf[UARTS_COUNT];

VOID VendorUartInit(UINT32 uartNum, UINT32 baudrate);

STATIC_INLINE VOID VendorUartSendByte(UINT32 uartNum, UINT8 txData)
{
    uart_send_byte(uartNum, txData);
}

/**
 * @brief     This function serves to send data by byte with not DMA method. Returns immediately.
 * @param[in] uart_num - UART0 or UART1.
 * @param[in] tx_data  - the data to be send.
 * @return    @c true - if data is sent successfully, @c false - if fifo is full
 */
static inline bool uart_send_byte_nonblock(uart_num_e uart_num, unsigned char tx_data)
{
    if (uart_get_txfifo_num(uart_num) > (UART_HW_FIFO_SIZE - sizeof(tx_data))) {
        return false;
    }

    unsigned char index = uart_tx_byte_index[uart_num];
    reg_uart_data_buf(uart_num, index) = tx_data;
    index = (index + 1) & 0x03;
    uart_tx_byte_index[uart_num] = index;

    return true;
}

STATIC INLINE bool VendorUartSendByteNonBlock(UINT32 uartNum, UINT8 txData)
{
    return uart_send_byte_nonblock(uartNum, txData);
}

STATIC INLINE char VendorUartGetChar(UINT32 uartNum)
{
    return (char)uart_read_byte(uartNum);
}

STATIC INLINE void VendorUartTxIrqEnable(UINT32 uartNum, bool state)
{
    if (state) {
        uart_set_irq_mask(uartNum, UART_TX_IRQ_MASK);
    } else {
        uart_clr_irq_mask(uartNum, UART_TX_IRQ_MASK);
    }
}

STATIC INLINE void VendorUartRxIrqEnable(UINT32 uartNum, bool state)
{
    if (state) {
        uart_set_irq_mask(uartNum, UART_RX_IRQ_MASK);
    } else {
        uart_clr_irq_mask(uartNum, UART_RX_IRQ_MASK);
    }
}

INT32 uart_read(UINT32 timeOut)
{
    UINT32 intSave;
    INT32 rec = 0;

    intSave = LOS_IntLock();

    /* Spinlock disables interrupts. So it should be unlocked while waiting for data.
     * Data availability check is looped because another thread can consume data before Spinlock will be locked again.
     */
    while (g_uartState.dev[SERVICE_UART_PORT].rx.start == g_uartState.dev[SERVICE_UART_PORT].rx.end) {
        LOS_IntRestore(intSave);
        UINT32 ret = LOS_EventRead(&g_uartState.dev[SERVICE_UART_PORT].rx.event, 1, LOS_WAITMODE_OR | LOS_WAITMODE_CLR,
            timeOut);
        if ((ret & LOS_ERRTYPE_ERROR) != 0) {
            return -1;
        }
        intSave = LOS_IntLock();
    }

    rec = (UINT8)g_uartBuf[SERVICE_UART_PORT].rx[g_uartState.dev[SERVICE_UART_PORT].rx.start];
    g_uartState.dev[SERVICE_UART_PORT].rx.start = (g_uartState.dev[SERVICE_UART_PORT].rx.start + 1) % UART_RX_BUF;

    LOS_IntRestore(intSave);

    return rec;
}

INT32 uart_read_buf(UINT32 timeOut, UINT8 *buf, INT32 max)
{
    UINT32 intSave;
    INT32 nbytes = 0;

    intSave = LOS_IntLock();

    while ((nbytes < max) &&
           (g_uartState.dev[SERVICE_UART_PORT].rx.start != g_uartState.dev[SERVICE_UART_PORT].rx.end)) {
        buf[nbytes++] = (UINT8)g_uartBuf[SERVICE_UART_PORT].rx[g_uartState.dev[SERVICE_UART_PORT].rx.start];
        g_uartState.dev[SERVICE_UART_PORT].rx.start = (g_uartState.dev[SERVICE_UART_PORT].rx.start + 1) % UART_RX_BUF;
    }

    if ((nbytes < max) && (timeOut != 0)) {
        g_uartState.dev->rx.irq_buf.ptr = buf;
        g_uartState.dev->rx.irq_buf.nbytes = nbytes;
        g_uartState.dev->rx.irq_buf.max = max;
        LOS_EventClear(&g_uartState.dev[SERVICE_UART_PORT].rx.event, ~1);
        LOS_IntRestore(intSave);
        LOS_EventRead(&g_uartState.dev[SERVICE_UART_PORT].rx.event, 1, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, timeOut);
        intSave = LOS_IntLock();
        nbytes = g_uartState.dev->rx.irq_buf.nbytes;
        g_uartState.dev->rx.irq_buf.ptr = NULL;
        g_uartState.dev->rx.irq_buf.nbytes = 0;
        g_uartState.dev->rx.irq_buf.max = 0;
    }

    LOS_IntRestore(intSave);

    return nbytes;
}

STATIC UINT8 uart_getc_unsafe(uart_num_e uartNum)
{
    UINT8 c = VendorUartGetChar(uartNum);

    size_t nextEnd = (g_uartState.dev[uartNum].rx.end + 1) % UART_RX_BUF;
    if (nextEnd != g_uartState.dev[uartNum].rx.start) {
        g_uartBuf[uartNum].rx[g_uartState.dev[uartNum].rx.end] = c;
        g_uartState.dev[uartNum].rx.end = nextEnd;
        LOS_EventWrite(&g_uartState.dev[uartNum].rx.event, 1);
    }

    return c;
}

STATIC VOID UartIrqHandler(void *arg)
{
    uart_num_e uartNum = (uart_num_e)arg;
    while (uart_get_rxfifo_num(uartNum) > 0) {
        if (g_uartState.dev->rx.irq_buf.ptr != NULL) {
            UINT8 c = VendorUartGetChar(uartNum);
            g_uartState.dev->rx.irq_buf.ptr[g_uartState.dev->rx.irq_buf.nbytes++] = c;
            if (g_uartState.dev->rx.irq_buf.nbytes >= g_uartState.dev->rx.irq_buf.max) {
                g_uartState.dev->rx.irq_buf.ptr = NULL;
                LOS_EventWrite(&g_uartState.dev[uartNum].rx.event, 1);
            }
        } else {
            (VOID) uart_getc_unsafe(uartNum);
        }
    }
}

void SerialInit(void)
{
    LOS_EventInit(&g_uartState.dev[SERVICE_UART_PORT].rx.event);

    unsigned short div;
    unsigned char bwpc;

    uart_set_pin(SERVICE_UART_PIN_TX, SERVICE_UART_PIN_RX);
    uart_reset(SERVICE_UART_PORT);
    uart_cal_div_and_bwpc(SERVICE_UART_BAUDRATE, sys_clk.pclk * HZ_IN_MHZ, &div, &bwpc);
    telink_b91_uart_init(SERVICE_UART_PORT, div, bwpc, SERVICE_UART_PARITY, SERVICE_UART_STOP_BITS);
    uart_rx_irq_trig_level(SERVICE_UART_PORT, 1);
    uart_tx_irq_trig_level(SERVICE_UART_PORT, 0);

    B91IrqRegister(IRQ18_UART1, UartIrqHandler, SERVICE_UART_PORT);
    plic_interrupt_enable(IRQ18_UART1);
    VendorUartRxIrqEnable(SERVICE_UART_PORT, true);
}

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

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <errno.h>

#include <los_arch_interrupt.h>
#include <los_task.h>

#include <hota_updater.h>
#include <ohos_init.h>
#include <ohos_types.h>

#include <hiview_log.h>

#include <app_sha256.h>
#include <hota_verify.h>

#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>

#include <board_config.h>

#include <B91/flash.h>
#include <B91/uart.h>

#include "serial.h"

#define OTA_UART_TASK_PRIO 7

#define SOH   0x01
#define STX   0x02
#define EOT   0x04
#define ACK   0x06
#define NAK   0x15
#define CAN   0x18
#define CTRLZ 0x1A

#define DLY_1S 1000
#define MAXRETRANS 25
#define MAX_BYTE_RETRIES 16
#define FLUSH_INPUT_NBYTES 3

#define XMODEM_PACKET_SIZE 128
#define XMODEM1K_PACKET_SIZE 1024
#define XMODEM_BLOCK_NUMBER 1
#define XMODEM_BLOCK_NUMBER_INV 2
#define XMODEM_HEADER_SIZE 3

#define XMODEM_BUF_SIZE 1024
#define READ_BYTE_TIMEOUT_MS 5000

#define OTA_THREAD_STACK_SIZE (1024 * 16)

#define RADIX_16 16
#define RADIX_10 10
#define HEX_IN_BYTE 2

typedef enum {
    COMMAND_TYPE_ERROR_TIMEOUT = -1,
    COMMAND_TYPE_ERROR_PREAMBLE = -2,
    COMMAND_TYPE_ERROR_WRONG_COMMAND = -3,
    COMMAND_TYPE_ERROR_WRONG_ARGS = -4,

    COMMAND_TYPE_UPLOAD = 0,
    COMMAND_TYPE_PRINT = 1,
    COMMAND_TYPE_CANCEL = 2,
    COMMAND_TYPE_ERASE = 3,
    COMMAND_TYPE_HASH_SHA256 = 4,
    COMMAND_TYPE_RESTART = 5,
    COMMAND_TYPE_ROLLBACK = 6,
    COMMAND_TYPE_DEBUG = 7,
    COMMAND_TYPE_SIGN = 8,
} CommandType;

enum {
    XMODEM_ERROR_CANCEL = -1,
    XMODEM_ERROR_SYNC_ERROR = -2,
    XMODEM_ERROR_RETRANS_LIMIT = -3,
    XMODEM_PACKET_END = -4,
};

typedef struct {
    int sz, start;

    AppSha256Context sha256;
} xmodemReceiveCBArg;

typedef struct {
    UINT8 hash[32];
    UINT8 *workBuf;
} OTA_AppState;

unsigned short crc16_ccitt(const unsigned char *buf, int len)
{
    unsigned short crc = 0;
    int remain = len;
    const unsigned char *p = buf;
    while (remain--) {
        int i;
        crc ^= *p++ << CHAR_BIT;
        for (i = 0; i < CHAR_BIT; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static int check(bool is_crc, const unsigned char *buf, int sz)
{
    if (is_crc) {
        unsigned short crc = crc16_ccitt(buf, sz);
        unsigned short tcrc = (buf[sz] << CHAR_BIT) + buf[sz + 1];
        if (crc == tcrc) {
            return 1;
        }
    } else {
        int i;
        unsigned char cks = 0;
        for (i = 0; i < sz; ++i) {
            cks += buf[i];
        }
        if (cks == buf[sz]) {
            return 1;
        }
    }

    return 0;
}

static int _inbyte(unsigned short timeout_ms)
{
    int c = uart_read(LOS_MS2Tick(timeout_ms));
    return c;
}

STATIC int read_with_echo(UINT32 timeout_ms)
{
    int c = uart_read(LOS_MS2Tick(timeout_ms));
    if (c > 0) {
        if (c == 0x7f) {
            uart_send_byte(UART1, 0x08);
            uart_send_byte(UART1, ' ');
            c = 0x08;
        }
        uart_send_byte(UART1, (unsigned char)c);
    }
    return c;
}

static void _outbyte(int c)
{
    uart_send_byte(UART1, (unsigned char)c);
}

static void flushinput(void)
{
    while (_inbyte(((DLY_1S)*FLUSH_INPUT_NBYTES) >> 1) >= 0) {
    }
}

typedef void (*XModemReceiveCB)(uint8_t *buf, size_t addr, size_t len, UINT8 *arg);

static int get_first_byte(int *c)
{
    if ((*c = _inbyte((DLY_1S) << 1)) >= 0) {
        switch (*c) {
            case SOH: {
                return XMODEM_PACKET_SIZE;
            }
            case STX: {
                return XMODEM1K_PACKET_SIZE;
            }

            case EOT: {
                flushinput();
                _outbyte(ACK);
                return XMODEM_PACKET_END;
            }
            case CAN: {
                if ((*c = _inbyte(DLY_1S)) == CAN) {
                    flushinput();
                    _outbyte(ACK);
                    return XMODEM_ERROR_CANCEL; /* canceled by remote */
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    return 0;
}

static int packet_check(unsigned char *xbuff, int destsz, XModemReceiveCB cb, UINT8 *arg, int bufsz,
                        unsigned char *packetno, int *retrans, bool use_src, int *len)
{
    if (xbuff[XMODEM_BLOCK_NUMBER] == (unsigned char)(~xbuff[XMODEM_BLOCK_NUMBER_INV]) &&
        (xbuff[XMODEM_BLOCK_NUMBER] == *packetno || xbuff[XMODEM_BLOCK_NUMBER] == (unsigned char)*packetno - 1) &&
        check(use_src, &xbuff[XMODEM_HEADER_SIZE], bufsz)) {
        if (xbuff[1] == *packetno) {
            int count = destsz - *len;
            if (count > bufsz) {
                count = bufsz;
            }
            if (count > 0) {
                cb(&xbuff[XMODEM_HEADER_SIZE], *len, count, arg);
                *len += count;
            }
            ++*packetno;
            *retrans = MAXRETRANS + 1;
        }
        if (--*retrans <= 0) {
            flushinput();
            _outbyte(CAN);
            _outbyte(CAN);
            _outbyte(CAN);
            return XMODEM_ERROR_RETRANS_LIMIT; /* too many retry error */
        }
        _outbyte(ACK);
        return 0;
    } else {
        return 1;
    }
}

int xmodemReceive(unsigned char *xbuff, int destsz, XModemReceiveCB cb, void *arg)
{
    unsigned char *p;
    int bufsz = 0, use_src = 0;
    unsigned char trychar = 'C';
    unsigned char packetno = 1;
    int c, len = 0;
    int retry, retrans = MAXRETRANS;

    for (;;) {
        for (retry = 0; retry < MAX_BYTE_RETRIES; ++retry) {
            if (trychar) {
                _outbyte(trychar);
            }
            bufsz = get_first_byte(&c);
            if (bufsz == XMODEM_PACKET_END) {
                return len;
            } else if (bufsz < 0) {
                return bufsz;
            } else if (bufsz > 0) {
                break;
            }
        }

        if (bufsz == 0) {
            if (trychar == 'C') {
                trychar = NAK;
                continue;
            }
            flushinput();
            _outbyte(CAN);
            _outbyte(CAN);
            _outbyte(CAN);
            return XMODEM_ERROR_SYNC_ERROR; /* sync error */
        }

        if (trychar == 'C') {
            use_src = 1;
        }
        trychar = 0;
        p = xbuff;
        *p++ = c;

        INT32 to_read = (bufsz + (use_src ? 1 : 0) + 3);
        INT32 nread = uart_read_buf(LOS_MS2Tick(DLY_1S * 10), p, to_read);
        if (nread < to_read) {
            printf(" === %s:%d to_read: %d nread: %d\r\n", __func__, __LINE__, to_read, nread);
            flushinput();
            _outbyte(NAK);
        } else {
            int tmp = packet_check(xbuff, destsz, cb, arg, bufsz, &packetno, &retrans, use_src, &len);
            if (tmp == 1) {
                flushinput();
                _outbyte(NAK);
            } else if (tmp < 0) {
                return tmp;
            }
        }
    }
}

STATIC ptrdiff_t ReadLine(char *buf, size_t bufLen)
{
    ptrdiff_t i;

    for (i = 0; i < bufLen;) {
        char c;
        if ((c = read_with_echo(READ_BYTE_TIMEOUT_MS)) < 0) {
            return COMMAND_TYPE_ERROR_TIMEOUT;
        }

        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            break;
        }
        if (c == 0x08) {
            if (i > 0) {
                buf[--i] = '\0';
            }
            continue;
        }
        buf[i++] = c;
    }

    return i;
}

void OtaErrorCallBack(HotaErrorCode errorCode)
{
    printf(" === %s:%d errorCode: %d\r\n", __func__, __LINE__, errorCode);
}

void OtaStatusCallBack(HotaStatus status)
{
    printf(" === %s:%d status: %d\r\n", __func__, __LINE__, status);
}

void xmodemReceiveCB(uint8_t *buf, size_t addr, size_t len, UINT8 *_arg)
{
    xmodemReceiveCBArg *arg = (xmodemReceiveCBArg *)_arg;
    int to_print = len;
    if (to_print > 0) {
        printf("Received[%d %d %u %d]: \"", arg->sz, arg->start, addr, to_print);
        printf("\"\r\n");
        AppSha256Update(&arg->sha256, buf, len);
        unsigned int offset = (unsigned)(arg->start > 0 ? arg->start : 0) + addr;
        HotaWrite(buf, offset, len);
    }
}

STATIC ptrdiff_t ReadSign(UINT8 *sign, unsigned len)
{
    char buf[32];
    size_t nbytes = 0;
    while (nbytes < len) {
        ptrdiff_t res = ReadLine(buf, sizeof(buf));
        if (res < 0) {
            return res;
        }

        if (nbytes + (res / HEX_IN_BYTE) > len) {
            return -1;
        }

        for (size_t i = 0; i < res; i += HEX_IN_BYTE) {
            char hex[3] = {buf[i], buf[i + 1], '\0'};
            errno = ENOERR;
            UINT32 num = strtoul(hex, NULL, RADIX_16);
            if (((num == 0) || (ULONG_MAX == num)) && (ENOERR != errno)) {
                printf(" === %s:%d errno: %d \"%s\"\r\n", __func__, __LINE__, errno, hex);
                return -1;
            }

            sign[nbytes++] = num;
        }

        if (res < sizeof(buf)) {
            break;
        }
    }

    return nbytes;
}

STATIC VOID CommandRunUpload(int arg1, int arg2, OTA_AppState *state)
{
    xmodemReceiveCBArg arg = {arg1, arg2, {0}};
    AppSha256Init(&arg.sha256);
    int res = xmodemReceive(state->workBuf, arg1, xmodemReceiveCB, &arg);
    AppSha256Finish(&arg.sha256, state->hash);
    printf("sha256: ");
    for (size_t i = 0; i < sizeof(state->hash); ++i) {
        printf("%02x", state->hash[i]);
    }
    printf("\r\n");
}

STATIC VOID CommandRunPrint(int arg1, int arg2, OTA_AppState *state)
{
    unsigned start = arg1;  // & ~0xFF; //(sz / 256) * 256;
    unsigned bufLen = (unsigned)(arg2 >= 0 ? arg2 : 256);

    for (unsigned offset = 0, next = XMODEM_BUF_SIZE; next < bufLen; offset = next, next += XMODEM_BUF_SIZE) {
        if (OHOS_SUCCESS == HotaRead(start + offset, XMODEM_BUF_SIZE, state->workBuf)) {
        }
        printf("Read[%u %u %u %u]: {", start, start + offset, XMODEM_BUF_SIZE, bufLen);
        for (size_t i = 0; i < XMODEM_BUF_SIZE; ++i) {
            printf("%02x", state->workBuf[i]);
        }
        printf("}\r\n");
    }
    unsigned rem = bufLen % XMODEM_BUF_SIZE;

    if (rem != 0) {
        if (OHOS_SUCCESS == HotaRead(start + bufLen - rem, rem, state->workBuf)) {
        }
        printf("Read[%u %u %u %u]: {", start, start + bufLen - rem, rem, bufLen);
        for (size_t i = 0; i < rem; ++i) {
            printf("%02x", state->workBuf[i]);
        }
        printf("}\r\n");
    }
}

STATIC VOID CommandRunHashSha256(int arg1, int arg2, OTA_AppState *state)
{
    unsigned start = arg1;
    unsigned bufLen = (unsigned)(arg2 >= 0 ? arg2 : 256);
    uint8_t hashLocal[32];

    AppSha256Context sha256;
    AppSha256Init(&sha256);

    for (unsigned offset = 0, next = XMODEM_BUF_SIZE; next < bufLen; offset = next, next += XMODEM_BUF_SIZE) {
        if (OHOS_SUCCESS == HotaRead(start + offset, XMODEM_BUF_SIZE, state->workBuf)) {
        }
        AppSha256Update(&sha256, state->workBuf, XMODEM_BUF_SIZE);
    }

    unsigned rem = bufLen % XMODEM_BUF_SIZE;

    if (rem != 0) {
        if (OHOS_SUCCESS == HotaRead(start + bufLen - rem, rem, state->workBuf)) {
        }
        AppSha256Update(&sha256, state->workBuf, rem);
    }

    AppSha256Finish(&sha256, hashLocal);
    printf("sha256: ");
    for (size_t i = 0; i < sizeof(hashLocal); ++i) {
        printf("%02x", hashLocal[i]);
    }
    printf("\r\n");
}

STATIC VOID CommandRunSign(OTA_AppState *state)
{
    UINT8 sign[128];
    ptrdiff_t len = ReadSign(sign, sizeof(sign));
    if (len < 0) {
        printf(" === ERROR %s:%d res: %d\r\n", __func__, __LINE__, len);
    } else {
        mbedtls_pk_context context;
        uint32 length = 0;

        mbedtls_pk_init(&context);
        uint8 *keyBuf = HotaGetPubKey(&length);
        if (keyBuf == NULL) {
            printf("Get key fail\r\n");
        }

        int32 parseRet = mbedtls_pk_parse_public_key(&context, keyBuf, length);
        if (parseRet != 0) {
            printf("Parse public key failed.\r\n");
        } else {
            int ret;

            mbedtls_entropy_context entropy;
            mbedtls_rsa_set_padding(mbedtls_pk_rsa(context), MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);
            mbedtls_entropy_init(&entropy);
            if ((ret = mbedtls_rsa_pkcs1_verify(mbedtls_pk_rsa(context), MBEDTLS_MD_SHA256, sizeof(state->hash),
                state->hash, sign)) != 0) {
                printf("Sign check fail!\r\n");
            } else {
                printf("Sign check success!\r\n");
            }
            mbedtls_pk_free(&context);
        }
    }
}

STATIC VOID CommandRunCancel(OTA_AppState *state)
{
    HotaCancel();
    (VOID) HotaInit(OtaErrorCallBack, OtaStatusCallBack);
    (VOID) HotaSetPackageType(NOT_USE_DEFAULT_PKG);
}

STATIC VOID CommandRunRestart(OTA_AppState *state)
{
    int ret = HotaRestart();
    printf("HotaRestart() =  %d\r\n", ret);
}

STATIC VOID CommandRunRollback(OTA_AppState *state)
{
    int HotaHalRollback(void);
    int ret = HotaHalRollback();
    printf("HotaHalRollback() =  %d\r\n", ret);
}

STATIC int CommandRunDebug(char *buf, OTA_AppState *state)
{
    char c = buf[0];
    if (c != 'm' && c != 'f') {
        return COMMAND_TYPE_ERROR_WRONG_COMMAND;
    }
    char *p;
    errno = ENOERR;
    UINT32 addr = strtoul(buf + 1, &p, RADIX_10);
    if (((addr == 0) || (ULONG_MAX == addr)) && (ENOERR != errno)) {
        printf(" === %s:%d errno: %d\r\n", __func__, __LINE__, errno);
        return COMMAND_TYPE_ERROR_WRONG_ARGS;
    }
    UINT32 sz = strtoul(p, &p, RADIX_10);
    if (((sz == 0) || (ULONG_MAX == sz)) && (ENOERR != errno)) {
        printf(" === %s:%d errno: %d\r\n", __func__, __LINE__, errno);
        sz = 1;
    }
    if (c == 'm') {
        p = (char *)addr;
    } else if (c == 'f') {
        p = malloc(sz);
        flash_read_page(addr, sz, p);
    }
    printf("Debug(%c): ", c);
    for (size_t i = 0; i < sz; ++i) {
        printf("%02x", p[i]);
    }
    printf("\r\n");
    if (c == 'f') {
        free(p);
    }
    return COMMAND_TYPE_DEBUG;
}

STATIC CommandType CommandProcess(OTA_AppState *state)
{
    int c;
    if ((c = read_with_echo(LOS_WAIT_FOREVER)) < 0) {
        return COMMAND_TYPE_ERROR_TIMEOUT;
    }
    if (c != '{') {
        return COMMAND_TYPE_ERROR_PREAMBLE;
    }

    if ((c = read_with_echo(READ_BYTE_TIMEOUT_MS)) < 0) {
        return COMMAND_TYPE_ERROR_TIMEOUT;
    }

    CommandType cmd;

    switch (c) {
        case 'u': {
            cmd = COMMAND_TYPE_UPLOAD;
            break;
        }
        case 'p': {
            cmd = COMMAND_TYPE_PRINT;
            break;
        }
        case 'c': {
            cmd = COMMAND_TYPE_CANCEL;
            break;
        }
        case 'h': {
            cmd = COMMAND_TYPE_HASH_SHA256;
            break;
        }
        case 'r': {
            cmd = COMMAND_TYPE_RESTART;
            break;
        }
        case 'b': {
            cmd = COMMAND_TYPE_ROLLBACK;
            break;
        }
        case 'd': {
            cmd = COMMAND_TYPE_DEBUG;
            break;
        }
        case 's': {
            CommandRunSign(state);
            return COMMAND_TYPE_SIGN;
        }
        default: {
            return COMMAND_TYPE_ERROR_WRONG_COMMAND;
        }
    }

    char buf[32] = {0};

    ptrdiff_t res = ReadLine(buf, sizeof(buf));
    if (res < 0) {
        return res;
    }

    if ((COMMAND_TYPE_UPLOAD == cmd) || (COMMAND_TYPE_PRINT == cmd) || (COMMAND_TYPE_HASH_SHA256 == cmd)) {
            char *p;
            errno = ENOERR;
            UINT32 arg1 = strtoul(buf, &p, RADIX_10);
            if (((arg1 == 0) || (ULONG_MAX == arg1)) && (ENOERR != errno)) {
                printf(" === %s:%d errno: %d\r\n", __func__, __LINE__, errno);
                return COMMAND_TYPE_ERROR_WRONG_ARGS;
            }
            UINT32 arg2 = strtoul(p, &p, RADIX_10);
            if (((arg2 == 0) || (ULONG_MAX == arg2)) && (ENOERR != errno)) {
                printf(" === %s:%d errno: %d\r\n", __func__, __LINE__, errno);
                arg2 = UINT32_MAX;
            }

            if (cmd == COMMAND_TYPE_UPLOAD) {
                CommandRunUpload(arg1, arg2, state);
            } else if (cmd == COMMAND_TYPE_PRINT) {
                CommandRunPrint(arg1, arg2, state);
            } else if (cmd == COMMAND_TYPE_HASH_SHA256) {
                CommandRunHashSha256(arg1, arg2, state);
            }
    } else if (cmd == COMMAND_TYPE_CANCEL) {
        if (strcmp("ancel", buf) == 0) {
            CommandRunCancel(state);
        }
    } else if (COMMAND_TYPE_RESTART == cmd) {
        if (strcmp("estart", buf) == 0) {
            CommandRunRestart(state);
        }
    } else if (COMMAND_TYPE_ROLLBACK == cmd) {
        if (strcmp("ack", buf) == 0) {
            CommandRunRollback(state);
        }
    } else if (COMMAND_TYPE_DEBUG == cmd) {
        return CommandRunDebug(buf, state);
    }

    return cmd;
}

STATIC VOID OTA_TestThread(VOID)
{
    printf(" === %s:%d\r\n", __func__, __LINE__);

    UINT8 *workBuff = malloc(XMODEM_BUF_SIZE + 6); /* 6 bytes XModem overhead */
    if (workBuff == NULL) {
        printf("%s: Memory allocation error!\r\n", __func__);
    }

    SerialInit();

    OTA_AppState state = {
        {0},
        workBuff
    };

    while (1) {
        CommandType cmd = CommandProcess(&state);
        printf(" === %s:%d cmd: %d\r\n", __func__, __LINE__, cmd);
    }

    free(state.workBuf);
}

VOID OtaUartTestInit(VOID)
{
    (VOID) HotaInit(OtaErrorCallBack, OtaStatusCallBack);
    (VOID) HotaSetPackageType(NOT_USE_DEFAULT_PKG);

    UINT32 ret = LOS_OK;

    unsigned int task_id;
    TSK_INIT_PARAM_S task_param = {0};

    task_param.pfnTaskEntry = (TSK_ENTRY_FUNC)OTA_TestThread;
    task_param.uwStackSize = OTA_THREAD_STACK_SIZE;
    task_param.pcName = "OTA_TestThread";
    task_param.usTaskPrio = OTA_UART_TASK_PRIO;
    ret = LOS_TaskCreate(&task_id, &task_param);
    if (ret != LOS_OK) {
        printf("Create Task failed! ERROR: 0x%x\r\n", ret);
    }
}

void AppMain(void)
{
    printf("%s %s\r\n", __DATE__, __TIME__);
    printf("%s: %p\r\n", __func__, AppMain);

    OtaUartTestInit();
}

SYS_RUN(AppMain);

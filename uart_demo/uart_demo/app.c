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
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <los_task.h>
#include <los_arch_interrupt.h>

#include <ohos_init.h>
#include <ohos_types.h>

#include <hiview_log.h>
#include <uart_if.h>

#include <board_config.h>


#define UART_TASK_PRIORITY LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO
#define DELAY 2000
#define UART0 0
#define UART1 1



// STATIC VOID HelloWorldTask(VOID)
// {
//     while (1) {
//         time_t t = time(NULL);
//         printf("Hello World, time: %lld\r\n", (long long)t);
//         LOS_TaskDelay(DELAY);
//     }
// }

// STATIC VOID LedTask(VOID)
// {
//     GpioSetDir(LED_BLUE_HDF, GPIO_DIR_OUT);

//     while (1) {
//         GpioWrite(LED_BLUE_HDF, GPIO_VAL_HIGH);
//         LOS_Msleep(DELAY);

//         GpioWrite(LED_BLUE_HDF, GPIO_VAL_LOW);
//         LOS_Msleep(DELAY);
//     }
// }


STATIC VOID uart_task(VOID)
{
    LOS_TaskDelay(DELAY);
    DevHandle *uart1 = UartOpen(UART1);

}


void AppMain(void)
{
    UINT32 ret;

    UINT32 taskId = 0;

    TSK_INIT_PARAM_S taskParam = {0};
    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)uart_task;
    taskParam.uwArg = 0;
    taskParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskParam.pcName = "uart_task";
    taskParam.usTaskPrio = UART_TASK_PRIORITY;

    ret = LOS_TaskCreate(&taskId, &taskParam);
    if (ret != LOS_OK) {
        HILOG_ERROR(HILOG_MODULE_APP, "ret of LOS_TaskCreate(uart_task) = %#x", ret);
    }
}

SYS_RUN(AppMain);

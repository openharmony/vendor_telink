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
#include <gpio_if.h>

#include <board_config.h>

#define LED_TASK_PRIORITY LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO
#define PROTO_TASK_PRIORITY (OS_TASK_PRIORITY_LOWEST-1)

#define DELAY 1000

STATIC VOID HelloWorldTask(VOID)
{
    while(1) {
        time_t t = time(NULL);
        printf("Hello World, time: %lld\r\n", (long long)t);
        LOS_TaskDelay(DELAY);
    }
}

STATIC VOID LedTask(VOID)
{
    GpioSetDir(LED_BLUE_HDF, GPIO_DIR_OUT);

    while(1) {
        GpioWrite(LED_BLUE_HDF, GPIO_VAL_HIGH);
        LOS_Msleep(DELAY);

        GpioWrite(LED_BLUE_HDF, GPIO_VAL_LOW);
        LOS_Msleep(DELAY);
    }
}

#if TELINK_GPIO_IRQ_SAMPLE_ENABLE
_attribute_ram_code_ int32_t gpio_handler(uint16_t gpio, void *data)
{
    UNUSED(gpio);
    UNUSED(data);
    printf("=== %s:%d\r\n", __func__, __LINE__);

    return 0;
}
#endif /* TELINK_GPIO_IRQ_SAMPLE_ENABLE */

void AppMain(void)
{
    UINT32 ret;
    UINT32 taskId = 0;
    TSK_INIT_PARAM_S taskParam = {0};
    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)HelloWorldTask;
    taskParam.uwArg = 0;
    taskParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskParam.pcName = "HelloWorldTask";
    taskParam.usTaskPrio = PROTO_TASK_PRIORITY;
    ret = LOS_TaskCreate(&taskId, &taskParam);
    if (ret != LOS_OK) {
        HILOG_ERROR(HILOG_MODULE_APP, "ret of LOS_TaskCreate(HelloWorldTask) = %#x", ret);
    }

    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)LedTask;
    taskParam.pcName = "LedTask";
    ret = LOS_TaskCreate(&taskId, &taskParam);
    if (ret != LOS_OK) {
        HILOG_ERROR(HILOG_MODULE_APP, "ret of LOS_TaskCreate(LedTask) = %#x", ret);
    }

#if TELINK_GPIO_IRQ_SAMPLE_ENABLE
    GpioSetDir(SW1_2_GPIO_HDF, GPIO_DIR_OUT);
    GpioWrite(SW1_2_GPIO_HDF, GPIO_VAL_HIGH);

    GpioSetDir(SW1_3_GPIO_HDF, GPIO_DIR_IN);
    gpio_set_up_down_res(SW1_3_GPIO, GPIO_PIN_PULLDOWN_100K);
    GpioSetIrq(SW1_3_GPIO_HDF, GPIO_IRQ_TRIGGER_RISING, gpio_handler, NULL);
    GpioEnableIrq(SW1_3_GPIO_HDF);
#endif /* TELINK_GPIO_IRQ_SAMPLE_ENABLE */
}

SYS_RUN(AppMain);

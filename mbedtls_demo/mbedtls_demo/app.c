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

#include <stdio.h>

#include <los_task.h>

#include <ohos_init.h>
#include <ohos_types.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#define MBEDTLS_DEMO_TASK_PRIORITY 7
#define MBEDTLS_DEMO_TASK_STACK_SIZE (16*1024)

#define DEMO_START_DELAY_MS 5000
#define DEMO_DELAY_MS 1000

#define KEY_LEN 32

STATIC VOID MbedTLS_TestThread(VOID)
{
    {
        LOS_Msleep(DEMO_START_DELAY_MS);
        printf("====================\r\n");
        mbedtls_entropy_context entropy;
        mbedtls_entropy_init(&entropy);

        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_ctr_drbg_init(&ctr_drbg);

        char *personalization = "my_app_specific_string";

        int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
            (const unsigned char *) personalization, strlen(personalization));
        if (ret != 0) {
            // ERROR HANDLING CODE FOR YOUR APP
            printf(" failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret);
        }

        unsigned char key[KEY_LEN] = {0};
        if ((ret = mbedtls_ctr_drbg_random(&ctr_drbg, key, KEY_LEN)) != 0) {
            printf(" failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret);
        } else {
            printf("key: ");
            for (size_t i = 0; i < sizeof(key); ++i) {
                printf("%02x", key[i]);
            }
            printf("\r\n");
        }

        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        printf("====================\r\n");
    }

    {
        LOS_Msleep(DEMO_DELAY_MS);
        printf("====================\r\n");

        if (!mbedtls_ecp_self_test(1)) {
            printf("\tECP test ok\n");
        } else {
            printf("\tECP test failed\n");
        }

        printf("====================\r\n");
    }
}

void AppMain(void)
{
    printf("%s %s\r\n", __DATE__, __TIME__);

    unsigned int task_id;
    TSK_INIT_PARAM_S task_param = {0};

    task_param.pfnTaskEntry = (TSK_ENTRY_FUNC)MbedTLS_TestThread;
    task_param.uwStackSize = MBEDTLS_DEMO_TASK_STACK_SIZE;
    task_param.pcName = "MbedTLS_TestThread";
    task_param.usTaskPrio = MBEDTLS_DEMO_TASK_PRIORITY;
    UINT32 ret = LOS_TaskCreate(&task_id, &task_param);
    if (ret != LOS_OK) {
        printf("Create Task(MbedTLS_TestThread) failed! ERROR: 0x%x\r\n", ret);
    }
}

SYS_RUN(AppMain);

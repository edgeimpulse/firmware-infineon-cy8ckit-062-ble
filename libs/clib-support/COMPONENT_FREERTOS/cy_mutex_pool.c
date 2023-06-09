/***********************************************************************************************//**
 * \file COMPONENT_FREERTOS/cy_mutex_pool.c
 *
 * \brief
 * Mutex pool implementation for FreeRTOS
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
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
 **************************************************************************************************/

#include "cy_mutex_pool.h"
#include "cy_mutex_pool_cfg.h"
#include <task.h>
#include <stdbool.h>
#include <string.h>
#include <cmsis_compiler.h>

// The standard library requires mutexes in order to ensure thread safety for
// operations such as malloc. The mutexes must be initialized at startup.
// It is not possible to allocate these mutexes on the heap because the heap
// is not initialized until later in the startup process (and because the
// heap requires a mutex). Some of the toolchains require recursive mutexes.

// The mutex functions may be called before vTaskStartScheduler. In this case,
// the acquire/release functions will do nothing.

#if (configUSE_MUTEXES == 0) || (configUSE_RECURSIVE_MUTEXES == 0) || \
    (configSUPPORT_STATIC_ALLOCATION == 0)
#warning \
    configUSE_MUTEXES, configUSE_RECURSIVE_MUTEXES, and configSUPPORT_STATIC_ALLOCATION must be enabled and set to 1 to use clib-support

#else

//--------------------------------------------------------------------------------------------------
// cy_freertos_kernel_started
//--------------------------------------------------------------------------------------------------
static bool cy_freertos_kernel_started(void)
{
    static bool rtos_started = false;
    if (!rtos_started)
    {
        rtos_started = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
    }
    return rtos_started;
}


// The mutex functions must not be used in interrupt context.
// The create/destroy functions will not work because taskENTER_CRITICAL will
// not work in an interrupt.
// The acquire function is explicitly designed for waiting which is likely to
// cause a deadlock in an interrupt.
static void cy_freertos_check_in_isr(void)
{
    #if defined(COMPONENT_CR4) // Can work for any Cortex-A & Cortex-R
    uint32_t mode = __get_mode();
    // SVC, ABT, and UND exceptions are exempt. Device startup is in SVC context
    // and hence this needs to pass. ABT and UND context are not applicable.
    if ((mode == 0x11U /*FIQ*/) || (mode == 0x12U /*IRQ*/))
    #else // Cortex-M
    if (0 != __get_IPSR())
    #endif
    {
        __BKPT(0);
    }
}


#ifdef __ICCARM__
// For IAR, the mutexes are allocated by __iar_Initlocks before static
// data initialization. Use the __no_init attribute so the mutex data
// will not be overwritten during static data initialization.
#   define CY_ATTR_NO_INIT __no_init
#else
// The other toolchains perform mutex initialization after static data
// initialization so they do not require any special behavior.
#   define CY_ATTR_NO_INIT
#endif

static CY_ATTR_NO_INIT StaticSemaphore_t cy_mutex_pool_storage[CY_STATIC_MUTEX_MAX];
static CY_ATTR_NO_INIT SemaphoreHandle_t cy_mutex_pool_handle[CY_STATIC_MUTEX_MAX];

//--------------------------------------------------------------------------------------------------
// cy_mutex_pool_setup
//--------------------------------------------------------------------------------------------------
void cy_mutex_pool_setup(void)
{
    // Only required if the startup code does not initialize cy_mutex_pool_handle
    memset(cy_mutex_pool_handle, 0, sizeof(cy_mutex_pool_handle));
}


//--------------------------------------------------------------------------------------------------
// cy_mutex_pool_create
//--------------------------------------------------------------------------------------------------
SemaphoreHandle_t cy_mutex_pool_create(void)
{
    cy_freertos_check_in_isr();
    SemaphoreHandle_t handle = NULL;
    int16_t           found  = -1;
    taskENTER_CRITICAL();
    for (int16_t i = 0; i < CY_STATIC_MUTEX_MAX; i++)
    {
        if (NULL == cy_mutex_pool_handle[i])
        {
            found                       = i;
            cy_mutex_pool_handle[found] = (SemaphoreHandle_t)1;
            break;
        }
    }
    taskEXIT_CRITICAL();
    if (found >= 0)
    {
        handle =
            xSemaphoreCreateRecursiveMutexStatic(&cy_mutex_pool_storage[found]);
        cy_mutex_pool_handle[found] = handle;
    }
    else
    {
        __BKPT(0);  // Out of resources
    }
    return handle;
}


//--------------------------------------------------------------------------------------------------
// cy_mutex_pool_acquire
//--------------------------------------------------------------------------------------------------
void cy_mutex_pool_acquire(SemaphoreHandle_t m)
{
    cy_freertos_check_in_isr();
    if (cy_freertos_kernel_started())
    {
        static const int16_t CY_MUTEX_TIMEOUT_TICKS = 10000;
        while (xSemaphoreTakeRecursive(m, CY_MUTEX_TIMEOUT_TICKS) != pdTRUE)
        {
            // Halt here until condition is false
        }
    }
}


//--------------------------------------------------------------------------------------------------
// cy_mutex_pool_release
//--------------------------------------------------------------------------------------------------
void cy_mutex_pool_release(SemaphoreHandle_t m)
{
    cy_freertos_check_in_isr();
    if (cy_freertos_kernel_started())
    {
        xSemaphoreGiveRecursive(m);
    }
}


//--------------------------------------------------------------------------------------------------
// cy_mutex_pool_destroy
//--------------------------------------------------------------------------------------------------
void cy_mutex_pool_destroy(SemaphoreHandle_t m)
{
    cy_freertos_check_in_isr();
    vSemaphoreDelete(m);
    taskENTER_CRITICAL();
    for (int16_t i = 0; i < CY_STATIC_MUTEX_MAX; i++)
    {
        if (cy_mutex_pool_handle[i] == m)
        {
            cy_mutex_pool_handle[i] = NULL;
            break;
        }
    }
    taskEXIT_CRITICAL();
}


#endif // if configUSE_MUTEXES == 0 || configUSE_RECURSIVE_MUTEXES == 0 ||
// configSUPPORT_STATIC_ALLOCATION == 0

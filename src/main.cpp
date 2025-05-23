/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "firmware-sdk/ei_device_info_lib.h"
#include "ei_device_psoc62.h"
#include "ei_at_handlers.h"
#include "ei_flash_memory.h"
#include "ei_inertial_sensor.h"
#include "ei_environment_sensor.h"
#include "ei_microphone.h"
#include "ei_run_impulse.h"
#include "ei_bluetooth_psoc63.h"
#include "ei_eink_screen.h"


#include "cyhal_clock.h"
#include "cyhal_gpio.h"
#include "cyhal_uart.h"

#ifdef FREERTOS_ENABLED
#include <FreeRTOS.h>
#include <task.h>
#endif

/******
 *
 * @brief EdgeImpulse PSoC63 firmware. See README.md for more information
 *
 ******/

void ei_task(void* param);
/***************************************
*            Constants
****************************************/
#define UART_CLEAR_SCREEN   "\x1b[2J\x1b[;H"
/* Task parameters for Edge Impulse Task. */
#define EI_TASK_PRIORITY                   (2u)
#define EI_TASK_STACK_SIZE                 (2048)

#define EINK_TASK_PRIORITY                  (2u)
#define EINK_TASK_STACK_SIZE                (1024u)



volatile int uxTopUsedPriority; // This enables RTOS aware debugging.
static ATServer *at;
EiDevicePSoC62 *eidev;


void cy_err(int result)
{
    if(result != CY_RSLT_SUCCESS) {
        int err_type = CY_RSLT_GET_TYPE(result);
        int err_module = CY_RSLT_GET_MODULE(result);
        int err_code = CY_RSLT_GET_CODE(result);

        ei_printf("err type = 0x%X module = 0x%X code = 0x%X\n\r", err_type, err_module, err_code);
    }
}

void board_init(void)
{
    cy_rslt_t result;
    cyhal_clock_t system_clock, pll_clock;

    /* This enables RTOS aware debugging in OpenOCD */
    uxTopUsedPriority = configMAX_PRIORITIES - 1 ;

    /* Make sure firmware starts after debugger */
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) {
        Cy_SysLib_Delay(400u);
    }

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Initialize the PLL */
    cyhal_clock_reserve(&pll_clock, &CYHAL_CLOCK_PLL[1]);
    cyhal_clock_set_frequency(&pll_clock, 150000000, NULL);
    cyhal_clock_set_enabled(&pll_clock, true, true);
    cyhal_clock_free(&pll_clock);

    /* Initialize the system clock (HFCLK0) */
    cyhal_clock_reserve(&system_clock, &CYHAL_CLOCK_HF[0]);
    cyhal_clock_set_source(&system_clock, &pll_clock);
    cyhal_clock_free(&system_clock);

    /* Do not disable the FLL on PSoC63
    cyhal_clock_reserve(&fll_clock, &CYHAL_CLOCK_FLL);
    cyhal_clock_set_enabled(&fll_clock, false, true);
    cyhal_clock_free(&fll_clock);
    */

    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
}

int main(void)
{
    board_init();
    ei_inertial_sensor_init();
    ei_environment_sensor_init();
    ei_microphone_pdm_init();
    /* Make sure the environmental sensor is settled by sampling more from it */
    ei_environment_sensor_async_trigger();
    ei_printf(UART_CLEAR_SCREEN);

    eidev =  static_cast<EiDevicePSoC62*>(EiDeviceInfo::get_device());
    at = ei_at_init(eidev);
    ei_printf("Type AT+HELP to see a list of commands.\r\n");
    at->print_prompt();
    eidev->set_state(eiStateFinished);

    /* Register the Bluetooth stack and configure BLE task  */
    if(ei_bluetooth_init() != CY_RSLT_SUCCESS) {
        CY_ASSERT(0);
    }

    /* Register EI firmware's main task */
    if(pdPASS != xTaskCreate(ei_task, "EI Task", EI_TASK_STACK_SIZE,
                             NULL, EI_TASK_PRIORITY, NULL))
    {
        printf("Failed to create the ei task!\r\n");
        CY_ASSERT(0u);
    }

    vTaskStartScheduler();

    /* Should never get here */
    CY_ASSERT(0) ;
}

void ei_task(void* param)
{
    char uart_data;
    /* Suppress warning for unused parameter */
    (void)param;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    eink_screen_onetime_set();

    while(1)
    {
        if(cyhal_uart_getc(&cy_retarget_io_uart_obj, (uint8_t*)&uart_data, 5) == CY_RSLT_SUCCESS) {
            /* Controlling inference */
            if(is_inference_running() && uart_data == 'b') {
                ei_stop_impulse();
                at->print_prompt();
                continue;
            }
            at->handle(uart_data);
        }

        if(is_inference_running()) {
            ei_run_impulse();
        }
    }
}

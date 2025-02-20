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

#include <stdint.h>
#include <stdlib.h>
#include <cmath>

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_environment_sensor.h"
#include "ei_device_psoc62.h"

/***************************************
*            Constants
****************************************/
#define PIN_THERM_VDD   (CYBSP_A0)
#define PIN_THERM_OUT1  (CYBSP_A1)
#define PIN_THERM_OUT2  (CYBSP_A2)
#define PIN_THERM_GND   (CYBSP_A3)
/* Resistance of the reference resistor */
#define THERM_R_REF     (float)(10000)
/* Beta constant of the NCP18XH103F03RB thermistor is 3380 Kelvin */
#define THERM_B_CONST   (float)(3380)
/* Resistance of the thermistor is 10K at 25 degrees C
 * R0 = 10000 Ohm, and T0 = 298.15 Kelvin, which gives
 * R_INFINITY = R0 e^(-B_CONSTANT / T0) = 0.1192855
 */
#define THERM_R_INFINITY (float)(0.1192855)
/* Needed for computation */
#define ABSOLUTE_ZERO   (float)(-273.15)
/* Desired sample rate for Thermistor, as it is a very slow changing sensor,
 * we can sample faster and report the current value without quailty issues.
 */
#define THERM_SAMPLE_FREQ_HZ (uint32)(125000)
#define ADC_ISR_PRIORITY 6

/***************************************
 *        Local variables
 **************************************/
static float env_data[ENV_AXIS_SAMPLED];
cyhal_adc_t adc;
/* Flag: 0 - sample reference, 1 - sample thermistor */
static volatile bool sample_ref_voltage = false;
/* Variable to store sample between ISR and callback */
static volatile int32_t adc_sample = 0;
/* Variables to store the latest adc samples before computation */
static volatile uint32_t voltage_ref = 0;
static volatile uint32_t voltage_therm = 0;


bool ei_environment_sensor_async_trigger(void)
{
    cy_rslt_t result;
    bool ret = false;

    if(sample_ref_voltage) {
        cyhal_gpio_write(PIN_THERM_VDD, 0u);
        cyhal_gpio_write(PIN_THERM_GND, 1u);
    }
    else {
        cyhal_gpio_write(PIN_THERM_VDD, 1u);
        cyhal_gpio_write(PIN_THERM_GND, 0u);
    }

    result = cyhal_adc_read_async(&adc, 1, (int32_t*)&adc_sample);
    if(result == CY_RSLT_SUCCESS) {
        ret = true;
    }

    return ret;
}

void ei_environment_sensor_adc_cb(void *callback_arg, cyhal_adc_event_t event)
{
    if(sample_ref_voltage) {
        voltage_ref = adc_sample;
        /* We need the voltage across the thermistor too */
        ei_environment_sensor_async_trigger();
    }
    else {
        voltage_therm = adc_sample;
        /* We no longer need voltage applied */
        cyhal_gpio_write(PIN_THERM_VDD, 0u);
    }
    /* Indicate what are we sampling next */
    sample_ref_voltage = !sample_ref_voltage;
}

bool ei_environment_sensor_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool ret = false;
    static const cyhal_adc_channel_config_t ADC_CHAN_CONFIG = {
        .enabled = true,
        .enable_averaging = false,
        .min_acquisition_ns = 1000u /* 1000ns per Infineon recommendation */
    };
    cyhal_adc_channel_t adc_channel;

    ei_printf("\nInitializing Thermistor\n");

    /* GPIO for reference resistor */
    result = cyhal_gpio_init(PIN_THERM_VDD, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0u);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    result = cyhal_gpio_init(PIN_THERM_GND, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0u);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* ADC init */
    result = cyhal_adc_init(&adc, PIN_THERM_OUT1, NULL);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    result = cyhal_adc_channel_init_diff(&adc_channel, &adc, PIN_THERM_OUT1, CYHAL_ADC_VNEG, &ADC_CHAN_CONFIG);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    if (result == CY_RSLT_SUCCESS) {
        ei_printf("Thermistor is online\n");
    }
    else {
        ei_printf("ERR: Thermistor init failed (0x%04x)!\n", result);
        return false;
    }

    /* Register async event to workaround the race condition between ADC and Timer */
    cyhal_adc_register_callback(&adc, ei_environment_sensor_adc_cb, NULL);
    cyhal_adc_enable_event(&adc, CYHAL_ADC_ASYNC_READ_COMPLETE, ADC_ISR_PRIORITY, true);

    /* Register as fusion sensor */
    if(ei_add_sensor_to_fusion_list(environment_sensor) == false) {
        ei_printf("ERR: failed to register Environment sensor!\n");
    }
    else {
        ret = true;
        /* Trigger one initial thermistor read to settle the sensor */
        ei_environment_sensor_async_trigger();
    }

    return ret;
}

float *ei_fusion_environment_sensor_read_data(int n_samples)
{
    /* Calculate thermistor reference as if MTB_THERMISTOR_NTC_WIRING_VIN_R_NTC_GND is set */
    float rThermistor = (THERM_R_REF * voltage_therm) / ((float)(voltage_ref));

    /* Calculate temperature */
    float temperature = (THERM_B_CONST / (logf(rThermistor / THERM_R_INFINITY))) + ABSOLUTE_ZERO;

    /* Store for sensor fusion */
    env_data[0] = temperature;

    /* We do not know if all samples are in, thus we trigger ADC again and in
     * the worst case, the ADC samples one extra data point that is not needed
     */
    ei_environment_sensor_async_trigger();

    return env_data;
}

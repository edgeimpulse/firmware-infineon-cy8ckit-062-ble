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

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "mtb_bmi160.h"

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_inertial_sensor.h"
#include "ei_device_psoc62.h"

/***************************************
*            Constants
****************************************/
#define CONVERT_G_TO_MS2    9.80665f
#define IMU_SCALING_CONST   (16384.0)
#define I2C_CLK_FREQ_HZ     (1000000UL)

/***************************************
 *        Local variables
 **************************************/
static float imu_data[INERTIAL_AXIS_SAMPLED];
static mtb_bmi160_data_t raw_data;
static mtb_bmi160_t motion_sensor;
static cyhal_i2c_t mI2C;


bool ei_inertial_sensor_init(void)
{
    cy_rslt_t result;
    bool ret = false;

    cyhal_i2c_cfg_t mI2C_config = {
           .is_slave = false,
           .address = 0,
           .frequencyhal_hz = I2C_CLK_FREQ_HZ
       };

    ei_printf("\nInitializing BMI 160 accelerometer\n");

    /* Initialize the I2C master interface for BMI 160 motion sensor */
    result = cyhal_i2c_init(&mI2C, (cyhal_gpio_t) CYBSP_I2C_SDA, (cyhal_gpio_t) CYBSP_I2C_SCL, NULL);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Configure the I2C master interface with the desired clock frequency */
    result = cyhal_i2c_configure(&mI2C, &mI2C_config);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Initialize the BMI160 motion sensor */
    result = mtb_bmi160_init_i2c(&motion_sensor, &mI2C, MTB_BMI160_DEFAULT_ADDRESS);
    if (result != CY_RSLT_SUCCESS)
    {
        ei_printf("ERR: IMU init failed (0x%04x)!\n", ret);
    }
    else {
        ret = true;
        ei_printf("BMI 160 IMU is online\n");
    }

    /* Default IMU range is 2g, therefore no need to do IMU configuration */

    if(ei_add_sensor_to_fusion_list(inertial_sensor) == false) {
        ei_printf("ERR: failed to register Inertial sensor!\n");
        return false;
    }

    return ret;
}

bool ei_inertial_sensor_test(void)
{
    cy_rslt_t result = mtb_bmi160_read(&motion_sensor, &raw_data);
    bool ret = false;

    if(result == CY_RSLT_SUCCESS) {
        ret = true;
        ei_printf("Accel: X:%6d Y:%6d Z:%6d\n", raw_data.accel.x, raw_data.accel.y, raw_data.accel.z);
        ei_printf("Gyro : X:%6d Y:%6d Z:%6d\n\n", raw_data.gyro.x, raw_data.gyro.y, raw_data.gyro.z);
    }

    return ret;
}

float *ei_fusion_inertial_sensor_read_data(int n_samples)
{
    cy_rslt_t result;
    float temp_data[INERTIAL_AXIS_SAMPLED];

    result = mtb_bmi160_read(&motion_sensor, &raw_data);

    if(result == CY_RSLT_SUCCESS) {
        temp_data[0] = raw_data.accel.x / IMU_SCALING_CONST;
        temp_data[1] = raw_data.accel.y / IMU_SCALING_CONST;
        temp_data[2] = raw_data.accel.z / IMU_SCALING_CONST;

        imu_data[0] = temp_data[0] * CONVERT_G_TO_MS2;
        imu_data[1] = temp_data[1] * CONVERT_G_TO_MS2;
        imu_data[2] = temp_data[2] * CONVERT_G_TO_MS2;
    }
    else {
        ei_printf("ERR: no Accel data!\n");
        imu_data[0] = 0.0f;
        imu_data[1] = 0.0f;
        imu_data[2] = 0.0f;
    }

    return imu_data;
}

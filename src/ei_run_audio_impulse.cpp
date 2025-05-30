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

#include "model-parameters/model_metadata.h"
#if defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "ei_device_psoc62.h"
#include "ei_microphone.h"
#include "ei_run_impulse.h"
#include "cycfg_gatt_db.h"
#include "ei_bluetooth_psoc63.h"

typedef enum {
    INFERENCE_STOPPED = 0,
    INFERENCE_WAITING,
    INFERENCE_SAMPLING,
    INFERENCE_DATA_READY
} inference_state_t;

static int print_results;
static uint16_t samples_per_inference;
static volatile inference_state_t inference_state = INFERENCE_STOPPED;
static uint64_t last_inference_ts = 0;
static bool continuous_mode = false;
static bool debug_mode = false;
const char truncate[] = ".."; /* used to truncate long labels */

static void process_results(ei_impulse_result_t* result)
{
    static int ble_inference_settings_ready = 0;
    float max = 0.0f;
    size_t max_ix = 0;
    size_t volatile label_len = 0;

    /* Update BLE settings payload once */
    if (!ble_inference_settings_ready) {
        size_t next_total_len, total_len = 0;
        memset(app_edge_impulse_settings, 0, app_edge_impulse_class_result_len);

        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            unsigned int copylen = strlen(result->classification[ix].label);

            next_total_len = total_len + copylen + 1; /* +1 accounts for the '/' separator */
            if (next_total_len > app_edge_impulse_class_result_len) {
                if (app_edge_impulse_class_result_len - total_len > 1) {
                    /* Truncate with ".." the BLE payload showing all available chars */
                    strncat((char*)&app_edge_impulse_settings[total_len], truncate, sizeof(truncate));
                }
                break; /* avoid buffer overflow */
            }

            strncat((char*)&app_edge_impulse_settings[total_len], result->classification[ix].label, copylen);
            total_len += copylen;

            app_edge_impulse_settings[total_len] = '/';
            total_len++;
        }
        ble_inference_settings_ready++;
    }

    /* Find the label with maximum confidence */
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result->classification[ix].value > max) {
            max = result->classification[ix].value;
            max_ix = ix;
        }
    }
    /* Update BLE payload */
    label_len = strlen(result->classification[max_ix].label);
    //printf("strlen = %u, app_edge_impulse_class_result_len = %d\n", label_len, app_edge_impulse_class_result_len);
    if (label_len > app_edge_impulse_class_result_len) {
        label_len = app_edge_impulse_class_result_len;
    }
    //printf("label_len = %u\n", label_len);
    memset(app_edge_impulse_class_result, 0x00, app_edge_impulse_class_result_len);
    memcpy(app_edge_impulse_class_result, result->classification[max_ix].label, label_len);
    bt_app_send_notification(CLASS_RESULT);
}

void ei_run_impulse(void)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    switch(inference_state) {
        case INFERENCE_STOPPED:
            // nothing to do
            return;
        case INFERENCE_WAITING:
            if (ei_read_timer_ms() < (last_inference_ts + 2000)) {
                return;
            }
            ei_printf("Recording\n");
            inference_state = INFERENCE_SAMPLING;
            dev->set_state(eiStateSampling);
            ei_microphone_inference_reset_buffers();
            return;
        case INFERENCE_SAMPLING:
            // wait for data to be collected through callback
            if (ei_microphone_inference_is_recording()) {
                return;
            }
            inference_state = INFERENCE_DATA_READY;
            break;
            // nothing to do, just continue to inference provcessing below
        case INFERENCE_DATA_READY:
        default:
            break;
    }

    signal_t signal;

    signal.total_length = continuous_mode ? EI_CLASSIFIER_SLICE_SIZE : EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &ei_microphone_inference_get_data;

    // run the impulse: DSP, neural network and the Anomaly algorithm
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR ei_error;
    if (continuous_mode == true) {
        ei_error = run_classifier_continuous(&signal, &result, debug_mode);
    }
    else {
        ei_error = run_classifier(&signal, &result, debug_mode);
    }
    if (ei_error != EI_IMPULSE_OK) {
        ei_printf("Failed to run impulse (%d)", ei_error);
        return;
    }

    if (continuous_mode == true) {
        if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW >> 1)) {
            display_results(&ei_default_impulse, &result);
            process_results(&result);
            print_results = 0;
        }
    }
    else {
        display_results(&ei_default_impulse, &result);
        process_results(&result);
    }

    if (continuous_mode == true) {
        inference_state = INFERENCE_SAMPLING;
    }
    else {
        ei_printf("Starting inferencing in 2 seconds...\n");
        last_inference_ts = ei_read_timer_ms();
        inference_state = INFERENCE_WAITING;
    }
}

void ei_start_impulse(bool continuous, bool debug, bool use_max_uart_speed)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();

    continuous_mode = continuous;
    debug_mode = debug;

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.04fms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %.02f ms.", (float)(EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_INTERVAL_MS));
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                            sizeof(ei_classifier_inferencing_categories[0]));
    ei_printf("Starting inferencing, press 'b' to break\n");

    dev->set_sample_length_ms(EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_INTERVAL_MS, false);
    dev->set_sample_interval_ms(EI_CLASSIFIER_INTERVAL_MS, true);

    if (continuous == true) {
        samples_per_inference = EI_CLASSIFIER_SLICE_SIZE * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
        // In order to have meaningful classification results, continuous inference has to run over
        // the complete model window. So the first iterations will print out garbage.
        // We now use a fixed length moving average filter of half the slices per model window and
        // only print when we run the complete maf buffer to prevent printing the same classification multiple times.
        print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
        run_classifier_init();
        inference_state = INFERENCE_SAMPLING;
    }
    else {
        samples_per_inference = EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
        // it's time to prepare for sampling
        ei_printf("Starting inferencing in 2 seconds...\n");
        last_inference_ts = ei_read_timer_ms();
        inference_state = INFERENCE_WAITING;
    }

    if (ei_microphone_inference_start(continuous_mode ? EI_CLASSIFIER_SLICE_SIZE : EI_CLASSIFIER_RAW_SAMPLE_COUNT, EI_CLASSIFIER_INTERVAL_MS) == false) {
        ei_printf("ERR: Failed to setup audio sampling");
        return;
    }
}

void ei_stop_impulse(void)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();

    if (inference_state != INFERENCE_STOPPED) {
        ei_microphone_inference_end();
        inference_state = INFERENCE_STOPPED;
        ei_printf("Inferencing stopped by user\r\n");
        dev->set_state(eiStateFinished);
        run_classifier_deinit();
    }
}

bool is_inference_running(void)
{
    return (inference_state != INFERENCE_STOPPED);
}

#endif /* defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE */

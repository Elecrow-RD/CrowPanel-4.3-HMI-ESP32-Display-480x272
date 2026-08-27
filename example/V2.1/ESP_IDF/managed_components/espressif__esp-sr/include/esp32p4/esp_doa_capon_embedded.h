// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#ifndef _ESP_DOA_CAPON_EMBEDDED_H_
#define _ESP_DOA_CAPON_EMBEDDED_H_

#include <stdint.h>
#include <stddef.h>

#include "gsc_core_types.h"

/* Memory placement:
 * - The DOA internal buffers (memory pool, ~200KB for 4 mics) are allocated
 *   in PSRAM by default. To place them in internal RAM instead, define
 *   ESP_DOA_DISABLE_PSRAM before including this header (or as a compile
 *   definition of the esp_audio_processor component). */
#ifndef ESP_DOA_DISABLE_PSRAM
#define ESP_DOA_PSRAM_DEFAULT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ESP DOA Capon Embedded - Optimized for ESP32-P4
 *
 * This is a high-performance embedded implementation of the Capon/MVDR
 * direction-of-arrival (DOA) estimation algorithm.
 *
 * Features:
 *   - Optimized for real-time processing (~1.7ms/frame on ESP32-P4 @ 400MHz, 4 mics)
 *   - Arbitrary microphone array geometry (runtime mic coordinates)
 *   - Frame size: 128 samples, FFT size: 256
 *   - Frequency range: 1500-4500 Hz (speech optimized)
 *   - Angle resolution: 10 degrees (36 angles: 0°, 10°, ..., 350°)
 *   - Single precision floating point only
 *   - Zero dynamic memory allocation during processing
 *
 * Performance comparison (4-mic circular array, ESP32-P4 @ 400MHz):
 *   - This embedded version: ~1.7ms/frame, ~201KB memory
 *   - Official esp_doa_capon: ~13ms/frame, 710KB memory (default config)
 *
 * Use case: Real-time speech source localization on ESP32-P4
 */

typedef struct esp_doa_capon_embedded_handle_t esp_doa_capon_embedded_handle_t;

/**
 * @brief Initialize DOA Capon Embedded processor
 *
 * This function creates and initializes a DOA processor instance with the
 * embedded optimized configuration. Fixed processing parameters:
 *   - Frame size: 128 samples at 16kHz (8ms frame shift)
 *   - FFT size: 256 points
 *   - Processing bandwidth: 1500-4500 Hz (speech optimized)
 *   - Angle resolution: 10 degrees (36 angles from 0° to 350°)
 *
 * The microphone array geometry is arbitrary and given at runtime:
 *   - Any array shape, 2 to 8 microphones
 *   - The coordinates may be in any order, but audio channel i passed to
 *     esp_doa_capon_embedded_process() must always come from the microphone
 *     at mic_coord[i] (shuffling is fine as long as the pairing holds)
 *   - When chaining DOA with esp_gsc, pass the SAME coordinate array to both
 *
 * All memory (handle + internal memory pool) is allocated by this function
 * and released by esp_doa_capon_embedded_destroy(). Buffers are allocated
 * from PSRAM by default (see ESP_DOA_DISABLE_PSRAM above).
 *
 * @param mic_coord Array of microphone coordinates (unit: meters),
 *                  right-hand coordinate system, one entry per microphone
 * @param mic_num   Number of microphones (2 .. 8)
 *
 * @return Initialized handle on success, NULL on failure
 *         Failure can occur if:
 *         - mic_coord is NULL
 *         - mic_num is out of range
 *         - memory allocation fails
 *
 * Example:
 * @code
 *   PlaneCoord mic_coords[4] = {
 *       { 0.05f, 0.0f, 0.0f}, {0.0f,  0.05f, 0.0f},
 *       {-0.05f, 0.0f, 0.0f}, {0.0f, -0.05f, 0.0f},
 *   };
 *   esp_doa_capon_embedded_handle_t *doa =
 *       esp_doa_capon_embedded_create(mic_coords, 4);
 * @endcode
 */
esp_doa_capon_embedded_handle_t *esp_doa_capon_embedded_create(PlaneCoord *mic_coord, int mic_num);

/**
 * @brief Release all allocated resources
 *
 * This function deinitializes the DOA processor and releases all resources
 * allocated by esp_doa_capon_embedded_create(), including the internal
 * memory pool.
 *
 * @param handle DOA handle instance to be freed (can be NULL, safely ignored)
 */
void esp_doa_capon_embedded_destroy(esp_doa_capon_embedded_handle_t *handle);

/**
 * @brief Process audio frame for direction estimation
 * 
 * This function processes one frame of multi-channel audio and estimates
 * the direction of arrival (DOA) of the sound source.
 * 
 * Input data format:
 *   - mic_num-channel 16-bit PCM audio, planar layout
 *   - Layout: [ch0_0..ch0_127, ch1_0..ch1_127, ...]
 *   - Frame size: 128 samples per channel (total 128 * mic_num samples)
 *   - Sample rate: 16000 Hz
 *   - Channel i must correspond to mic_coord[i] of esp_doa_capon_embedded_create()
 * 
 * The processing includes:
 *   - FFT transform (256 points)
 *   - Covariance matrix estimation
 *   - Capon beamforming spectrum computation
 *   - Peak detection for DOA estimation
 * 
 * This function does NOT allocate any dynamic memory - all buffers are
 * pre-allocated during esp_doa_capon_embedded_create().
 * 
 * @param handle    DOA handle instance created by esp_doa_capon_embedded_create()
 * @param mic_data  mic_num-channel audio data, planar 16-bit PCM
 *                  Buffer size: 128 samples × mic_num channels
 * @param vad_result VAD result: 1 = speech detected, 0 = noise/silence.
 *                  When vad_result is 0, all adaptive state (covariance
 *                  recursion, matrix inversion, spectrum) is frozen and the
 *                  last estimated angle is returned unchanged.
 *
 * @return Estimated sound direction in degrees, range 0-360.
 *         The angle is defined in the absolute array coordinate system
 *         (0° = positive x-axis, counter-clockwise) and is independent of
 *         the microphone ordering in mic_coord.
 *
 *         Returns -1.0f on error (handle is NULL, or internal error)
 * 
 * Processing time: ~1.7ms on ESP32-P4 @ 400MHz (4 mics)
 * Real-time budget: 8ms (for 128 samples @ 16kHz)
 * CPU usage: ~21% of real-time budget
 * 
 * Example:
 * @code
 *   int16_t audio_frame[128 * 4];  // 4 channels, planar: [ch0_0..ch0_127, ch1_0..ch1_127, ...]
 *   int vad = 1;  // Speech detected
 *   float doa = esp_doa_capon_embedded_process(doa_handle, audio_frame, vad);
 *   printf("DOA: %.1f degrees\n", doa);
 * @endcode
 */
float esp_doa_capon_embedded_process(esp_doa_capon_embedded_handle_t *handle,
                                      int16_t *mic_data,
                                      int vad_result);

/**
 * @brief Reset DOA processor state
 * 
 * This function resets the internal state of the DOA processor,
 * including the covariance matrix and smoothing filters.
 * Useful when you want to restart DOA estimation (e.g., after a long pause).
 * 
 * @param handle DOA handle instance
 * @return 0 on success, -1 on error (handle is NULL)
 */
int esp_doa_capon_embedded_reset(esp_doa_capon_embedded_handle_t *handle);

/**
 * @brief Print configuration information
 *
 * This function prints configuration information about the
 * DOA processor (frame size, FFT size, frequency range, etc.).
 * Useful for debugging and verification. When handle is NULL, only the
 * static (configuration-independent) information is printed.
 *
 * @param handle DOA handle instance (can be NULL for static info)
 */
void esp_doa_capon_embedded_print_info(esp_doa_capon_embedded_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_DOA_CAPON_EMBEDDED_H_ */

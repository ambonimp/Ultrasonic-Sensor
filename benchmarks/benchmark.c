
#include "arm_math.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ultrasonic_config.h"
#include "correlation.h"
#define SAMPLE_RATE ULTRASONIC_SAMPLE_RATE_HZ
#define REF_SIZE ULTRASONIC_REFERENCE_SAMPLES
#define MAX_SAMPLES CORRELATION_MAX_SAMPLES
#define REPEATS 5
#define FFT_MAX 2048
#define FFT_VARIANTS 3

static const int fft_sizes[FFT_VARIANTS] = {512, 1024, 2048};
static float reference[REF_SIZE], received[MAX_SAMPLES];
static float result[2 * MAX_SAMPLES - 1];
static float fft_result[MAX_SAMPLES + REF_SIZE - 1];
static arm_rfft_fast_instance_f32 fft[FFT_VARIANTS];
static float reference_spectrum[FFT_VARIANTS][FFT_MAX];
static float fft_time[FFT_MAX], fft_spectrum[FFT_MAX];
static volatile float checksum;
static float setup_ms;
static correlation_context_t production;

arm_status prepare_fft(void) {
  const float start_hz = ULTRASONIC_CHIRP_CENTER_HZ - ULTRASONIC_CHIRP_BANDWIDTH_HZ / 2.0f;
  // Set the slope from the sample rate so the sweep stays 30-50 kHz
  // when the sample rate changes. The final sample reaches the upper end.
  const float duration_s = (REF_SIZE - 1) / (float)SAMPLE_RATE;
  const float slope_hz_s = ULTRASONIC_CHIRP_BANDWIDTH_HZ / duration_s;
  for (int i = 0; i < REF_SIZE; i++) {
    float t = (float)i / SAMPLE_RATE;
    reference[i] = sinf(2.0f * PI * (start_hz * t + 0.5f * slope_hz_s * t * t));
  }
  arm_status status[3];
  status[0] = arm_rfft_fast_init_512_f32(&fft[0]);
  status[1] = arm_rfft_fast_init_1024_f32(&fft[1]);
  status[2] = arm_rfft_fast_init_2048_f32(&fft[2]);
  for (int i = 0; i < 3; i++)
    if (status[i] != ARM_MATH_SUCCESS) return status[i];
  for (int v = 0; v < FFT_VARIANTS; v++) {
    memset(fft_time, 0, sizeof(fft_time));
    for (int j = 0; j < REF_SIZE; j++) {
      fft_time[j] = reference[REF_SIZE - 1 - j];
    }
    arm_rfft_fast_f32(&fft[v], fft_time, reference_spectrum[v], 0);
  }
  return correlation_init(&production, reference, REF_SIZE, 0.0f)
      ? ARM_MATH_SUCCESS : ARM_MATH_ARGUMENT_ERROR;
}

void generate_signals(int n) {
  memset(received, 0, sizeof(received));
  for (int i = 300; i < n && i < 300 + REF_SIZE; i++)
    received[i] = reference[i - 300];
}

// Compare the sound pattern at each possible starting position.
int manual_correlate(int n) {
  float best = -1e30f;
  int best_lag = 0;

  for (int lag = 0; lag <= n - REF_SIZE; lag++) {
    float sum = 0.0f;

    for (int j = 0; j < REF_SIZE; j++) {
      sum += reference[j] * received[lag + j];
    }

    if (sum > best) {
      best = sum;
      best_lag = lag;
    }
  }

  checksum = best;
  return best_lag;
}

// Use the DSP library to do the same comparison.
int cmsis_correlate(int n) {
  arm_correlate_f32(received, n, reference, REF_SIZE, result);

  // In this output, position n - 1 means no delay.
  // Positions after it mean the echo arrived later.
  int zero_lag = n - 1;
  float best = -1e30f;
  int best_lag = 0;

  for (int lag = 0; lag <= n - REF_SIZE; lag++) {
    float value = result[zero_lag + lag];

    if (value > best) {
      best = value;
      best_lag = lag;
    }
  }

  checksum = best;
  return best_lag;
}

int comparison_fft_correlate(int n, int v) {
  int size = fft_sizes[v], block = size - REF_SIZE + 1;
  memset(fft_result, 0, (n + REF_SIZE - 1) * sizeof(float));
  for (int offset = 0; offset < n; offset += block) {
    int count = n - offset;
    if (count > block) count = block;
    memset(fft_time, 0, sizeof(fft_time));
    memcpy(fft_time, received + offset, count * sizeof(float));
    arm_rfft_fast_f32(&fft[v], fft_time, fft_spectrum, 0);

    // The first two values represent zero frequency and half the sample
    // rate. Multiply them separately from the other FFT values.
    fft_spectrum[0] *= reference_spectrum[v][0];
    fft_spectrum[1] *= reference_spectrum[v][1];
    for (int k = 2; k < size; k += 2) {
      float re = fft_spectrum[k], im = fft_spectrum[k + 1];
      float rr = reference_spectrum[v][k], ri = reference_spectrum[v][k + 1];
      fft_spectrum[k] = re * rr - im * ri;
      fft_spectrum[k + 1] = re * ri + im * rr;
    }
    // CMSIS scales the inverse FFT back to the right size for us.
    arm_rfft_fast_f32(&fft[v], fft_spectrum, fft_time, 1);
    for (int j = 0; j < count + REF_SIZE - 1; j++) {
      fft_result[offset + j] += fft_time[j];
    }
  }

  float best = -1e30f;
  int best_lag = 0;
  for (int lag = 0; lag <= n - REF_SIZE; lag++) {
    float value = fft_result[lag + REF_SIZE - 1];
    if (value > best) {
      best = value;
      best_lag = lag;
    }
  }
  checksum = best;
  return best_lag;
}

int fft_correlate(int n, int v) {
  if (fft_sizes[v] == CORRELATION_FFT_SIZE) {
    correlation_result_t detected = correlation_process(&production, received, n);
    memcpy(fft_result, production.output, (n + REF_SIZE - 1) * sizeof(float));
    checksum = detected.peak_strength;
    return (int)detected.delay_samples;
  }
  return comparison_fft_correlate(n, v);
}

void benchmark(int n) {
  generate_signals(n);
  uint64_t direct_total = 0;
  int direct_peak = 0;
  for (int i = 0; i < REPEATS; i++) {
    uint64_t start = time_us_64();
    direct_peak = cmsis_correlate(n);
    direct_total += time_us_64() - start;
  }
  float direct_ms = direct_total / (1000.0f * REPEATS);
  float collection_ms = 1000.0f * n / SAMPLE_RATE;
  printf("\n--- %d samples | collection %.2f ms ---\n", n, collection_ms);
  printf("CMSIS direct: %.3f ms | peak %d\n", direct_ms, direct_peak);
  for (int v = 0; v < FFT_VARIANTS; v++) {
    uint64_t total = 0;
    int peak = 0;
    for (int i = 0; i < REPEATS; i++) {
      uint64_t start = time_us_64();
      // Time the same function the main app uses. Do not include the extra
      // copying used to check the result.
      if (fft_sizes[v] == CORRELATION_FFT_SIZE) {
        correlation_result_t detected = correlation_process(&production, received, n);
        peak = (int)detected.delay_samples;
        checksum = detected.peak_strength;
      } else {
        peak = fft_correlate(n, v);
      }
      total += time_us_64() - start;
    }
    const float *values = v == 1 ? production.output : fft_result;
    float ms = total / (1000.0f * REPEATS);
    float max_error = 0;
    for (int lag = 0; lag <= n - REF_SIZE; lag++) {
      float error = fabsf(values[lag + REF_SIZE - 1] - result[n - 1 + lag]);
      if (error > max_error) max_error = error;
    }
    printf("F32 %4d%s | %.3f ms | %.2fx | peak %d %s | max error %.5f | %s\n",
           fft_sizes[v], v == 1 ? " production" : " comparison", ms,
           direct_ms / ms, peak,
           peak == 300 && peak == direct_peak && max_error < 0.002f ? "PASS" : "FAIL",
           max_error, ms < collection_ms ? "FAST ENOUGH" : "TOO SLOW");
  }
}

#ifndef CORRELATION_HOST_TEST
int main(void) {
  stdio_init_all();
  uint64_t start = time_us_64();
  arm_status status = prepare_fft();
  setup_ms = (time_us_64() - start) / 1000.0f;
  while (!stdio_usb_connected()) sleep_ms(100);
  sleep_ms(3000);
  if (status != ARM_MATH_SUCCESS) {
    while (true) { printf("FFT initialization failed\n"); sleep_ms(2000); }
  }
  while (true) {
    printf("\nSynthetic F32 benchmark | %u Hz | cached setup %.3f ms\n",
           (unsigned)SAMPLE_RATE, setup_ms);
    printf("Test chirp: %.0f-%.0f Hz, %u samples\n",
           ULTRASONIC_CHIRP_CENTER_HZ - ULTRASONIC_CHIRP_BANDWIDTH_HZ / 2.0,
           ULTRASONIC_CHIRP_CENTER_HZ + ULTRASONIC_CHIRP_BANDWIDTH_HZ / 2.0,
           (unsigned)REF_SIZE);
    int sizes[] = {1000, 2000, 4000, 8000};
    for (int i = 0; i < 4; i++) benchmark(sizes[i]);
    printf("\nBenchmark complete. Repeating in 5 seconds.\n");
    stdio_flush();
    sleep_ms(5000);
  }
}
#endif

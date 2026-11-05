#ifndef CORRELATION_H
#define CORRELATION_H

#include "arm_math.h"
#include "ultrasonic_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORRELATION_FFT_SIZE 1024u
#define CORRELATION_MAX_SAMPLES 8000u
#define CORRELATION_MAX_REFERENCE ULTRASONIC_REFERENCE_SAMPLES

typedef struct {
    uint32_t delay_samples;
    float delay_seconds;
    float peak_strength; // Match strength, not a confidence percentage.
    bool valid;
} correlation_result_t;

// This holds the working memory. Declare it with "static" so it does not use
// the Pico's small stack. Do not share it between tasks running at the same time.
// The module does not reserve extra memory, print, or measure time.
typedef struct {
    arm_rfft_fast_instance_f32 fft;
    float reference_spectrum[CORRELATION_FFT_SIZE];
    float time[CORRELATION_FFT_SIZE];
    float spectrum[CORRELATION_FFT_SIZE];
    float output[CORRELATION_MAX_SAMPLES + CORRELATION_MAX_REFERENCE - 1];
    size_t reference_length;
    float min_peak_strength;
    bool initialized;
} correlation_context_t;

// Prepare the sound pattern once and save its FFT for later calls.
// Call again only if the pattern or minimum match strength changes.
// Values must be real numbers, not NaN or infinity. The minimum must be >= 0.
bool correlation_init(correlation_context_t *ctx, const float *reference,
                      size_t reference_length, float min_peak_strength);

// Process one set of samples. This does not remember earlier sets.
// Remove the signal's DC offset so it is centered around zero. Use the same
// scale for the sound pattern and samples, normally -1 to 1. No NaN or infinity.
// Only check positions where the whole pattern fits. Keep the first position
// if two matches are equally strong. The input samples are never changed.
// Bad inputs or a match at or below the minimum strength give valid=false.
// valid=true does not prove there is an echo; test the minimum with real noise.
// You can inspect output[] until the next processing call.
// Future DMA code must not replace these samples before this function returns.
// Keep the last reference_length-1 samples for the next set, so an echo split
// between two sets is not missed.
correlation_result_t correlation_process(correlation_context_t *ctx,
                                         const float *samples, size_t count);
#endif

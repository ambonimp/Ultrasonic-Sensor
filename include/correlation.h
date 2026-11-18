#ifndef CORRELATION_H
#define CORRELATION_H
#include "arm_math.h"
#include "ultrasonic_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORRELATION_FFT_SIZE 1024u
#define CORRELATION_MAX_SAMPLES 8000u
#define CORRELATION_MAX_REFERENCE 625u

typedef struct {
    uint32_t delay_samples;
    float delay_seconds;
    float peak_strength; // Raw matched-filter amplitude, not confidence.
    bool valid;
} correlation_result_t;

typedef struct {
    arm_rfft_fast_instance_f32 fft;
    float reference_spectrum[CORRELATION_FFT_SIZE];
    float time[CORRELATION_FFT_SIZE];
    float spectrum[CORRELATION_FFT_SIZE];
    float output[CORRELATION_MAX_SAMPLES + CORRELATION_MAX_REFERENCE - 1];
    size_t reference_length;
    float sample_rate_hz, min_peak_strength;
    bool initialized;
} correlation_fft_context_t;

typedef struct {
    arm_fir_instance_f32 fir;
    float coefficients[CORRELATION_MAX_REFERENCE];
    float state[CORRELATION_MAX_SAMPLES + CORRELATION_MAX_REFERENCE - 1];
    float output[CORRELATION_MAX_SAMPLES];
    size_t reference_length;
    float sample_rate_hz, min_peak_strength;
    bool initialized;
} correlation_fir_context_t;

// Allocate contexts statically; do not share one between concurrent calls.
// Initialize once per reference/rate/threshold. Inputs must be finite, rate > 0,
// threshold >= 0. Reference and capture must use the same sample rate and scale.
bool correlation_fft_init(correlation_fft_context_t *, const float *reference,
                          size_t length, float sample_rate_hz, float threshold);
bool correlation_fir_init(correlation_fir_context_t *, const float *reference,
                          size_t length, float sample_rate_hz, float threshold);

// Each call is an independent capture, with sample zero relative to its start.
// Remove ADC DC offset before calling. Count must be length..MAX_SAMPLES.
// Only complete-reference matches are searched; ties choose the first lag.
// Both outputs use index lag + length - 1 for a match at lag. FIR produces only
// count values; FFT also produces the convolution tail (not searched).
// Invalid input or peak <= threshold returns valid=false. Threshold must be
// calibrated with real noise; a positive peak alone does not establish an echo.
// For continuous DMA, retain length-1 samples and track capture timestamps.
correlation_result_t correlation_fft_process(correlation_fft_context_t *, const float *, size_t);
correlation_result_t correlation_fir_process(correlation_fir_context_t *, const float *, size_t);

typedef struct {
    size_t min_lag_samples;       // Ignore earlier starts (transmit/ringing).
    size_t max_lag_samples;       // Inclusive; SIZE_MAX searches to capture end.
    size_t peak_radius_samples;   // Compare nearby peaks; must be >= 1.
    float min_normalized_match;   // Absolute correlation / energy norms: (0,1].
    float min_window_rms;         // DC-removed input units; must be > 0.
} correlation_echo_options_t;

// Run the actual FIR, then select the earliest qualifying local peak of its
// absolute output inside the lag window. Handles inverted received polarity.
// peak_strength is the absolute raw correlation, not a confidence score.
// Requires BOTH waveform similarity and signal RMS, plus the init threshold.
// Radius suppresses nearby carrier/sidelobe peaks, not arbitrary multipath.
// Parameters must be calibrated for the reference, input scale and hardware.
// Lag remains capture-relative. No automatic transmit timing or distance
// calibration is inferred. Returns valid=false when nothing qualifies.
// Legacy *_process functions retain strongest-positive-peak behavior.
correlation_result_t correlation_fir_process_first_echo(
    correlation_fir_context_t *, const float *, size_t,
    const correlation_echo_options_t *);

// Assumes colocated emitter/receiver and calibrated transmit-relative delay.
// speed_m_s depends on air temperature (about 343 at 20 C).
float correlation_distance_m(correlation_result_t echo, float speed_m_s);
#endif

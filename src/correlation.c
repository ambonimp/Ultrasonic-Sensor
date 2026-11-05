#include "correlation.h"
#include <math.h>
#include <string.h>

bool correlation_init(correlation_context_t *ctx, const float *reference,
                      size_t length, float threshold) {
    if (!ctx) return false;
    ctx->initialized = false;
    if (!reference || !length || length > CORRELATION_MAX_REFERENCE ||
        !isfinite(threshold) || threshold < 0) return false;
    for (size_t i = 0; i < length; i++)
        if (!isfinite(reference[i])) return false;
    if (arm_rfft_fast_init_1024_f32(&ctx->fft) != ARM_MATH_SUCCESS) return false;
    memset(ctx->time, 0, sizeof(ctx->time));
    for (size_t j = 0; j < length; j++) ctx->time[j] = reference[length - 1 - j];
    arm_rfft_fast_f32(&ctx->fft, ctx->time, ctx->reference_spectrum, 0);
    ctx->reference_length = length;
    ctx->min_peak_strength = threshold;
    ctx->initialized = true;
    return true;
}

correlation_result_t correlation_process(correlation_context_t *ctx,
                                         const float *samples, size_t count) {
    correlation_result_t detected = {0};
    if (!ctx || !ctx->initialized || !samples || count < ctx->reference_length ||
        count > CORRELATION_MAX_SAMPLES) return detected;
    for (size_t i = 0; i < count; i++)
        if (!isfinite(samples[i])) return detected;
    const size_t length = ctx->reference_length;
    const size_t block = CORRELATION_FFT_SIZE - length + 1;
    memset(ctx->output, 0, (count + length - 1) * sizeof(float));
    for (size_t offset = 0; offset < count; offset += block) {
        size_t chunk = count - offset;
        if (chunk > block) chunk = block;
        memset(ctx->time, 0, sizeof(ctx->time));
        memcpy(ctx->time, samples + offset, chunk * sizeof(float));
        arm_rfft_fast_f32(&ctx->fft, ctx->time, ctx->spectrum, 0);
        // The first two values are special: the zero-frequency value and the
        // value at half the sample rate. Multiply each one separately.
        ctx->spectrum[0] *= ctx->reference_spectrum[0];
        ctx->spectrum[1] *= ctx->reference_spectrum[1];
        for (size_t k = 2; k < CORRELATION_FFT_SIZE; k += 2) {
            float re = ctx->spectrum[k], im = ctx->spectrum[k + 1];
            float rr = ctx->reference_spectrum[k], ri = ctx->reference_spectrum[k + 1];
            ctx->spectrum[k] = re * rr - im * ri;
            ctx->spectrum[k + 1] = re * ri + im * rr;
        }
        // CMSIS scales the inverse FFT back to the right size for us.
        // We reversed the sound pattern during setup. Adding the overlapping
        // block results now gives the same comparison as the direct method.
        arm_rfft_fast_f32(&ctx->fft, ctx->spectrum, ctx->time, 1);
        for (size_t j = 0; j < chunk + length - 1; j++)
            ctx->output[offset + j] += ctx->time[j];
    }
    float best = -1e30f;
    for (size_t lag = 0; lag <= count - length; lag++) {
        float value = ctx->output[lag + length - 1];
        if (value > best) {
            best = value;
            detected.delay_samples = (uint32_t)lag;
        }
    }
    detected.peak_strength = best;
    detected.delay_seconds = (float)detected.delay_samples / ULTRASONIC_SAMPLE_RATE_HZ;
    detected.valid = isfinite(best) && best > ctx->min_peak_strength;
    return detected;
}

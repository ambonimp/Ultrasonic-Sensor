#include "correlation.h"
#include <math.h>
#include <string.h>

static correlation_result_t find_peak(const float *output, size_t count, size_t length,
                                      float rate, float threshold) {
    correlation_result_t result = {0};
    float best = -INFINITY;
    for (size_t lag = 0; lag <= count - length; ++lag) {
        float value = output[lag + length - 1];
        if (value > best) { best = value; result.delay_samples = (uint32_t)lag; }
    }
    result.peak_strength = best;
    result.delay_seconds = result.delay_samples / rate;
    result.valid = isfinite(best) && best > threshold;
    return result;
}


bool correlation_fft_init(correlation_fft_context_t *ctx, const float *reference,
                      size_t length, float sample_rate_hz, float threshold) {
    if (!ctx) return false;
    ctx->initialized = false;
    if (!reference || !length || length > CORRELATION_MAX_REFERENCE ||
        !isfinite(sample_rate_hz) || sample_rate_hz <= 0 ||
        !isfinite(threshold) || threshold < 0) return false;
    for (size_t i = 0; i < length; i++)
        if (!isfinite(reference[i])) return false;
    if (arm_rfft_fast_init_1024_f32(&ctx->fft) != ARM_MATH_SUCCESS) return false;
    memset(ctx->time, 0, sizeof(ctx->time));
    for (size_t j = 0; j < length; j++) ctx->time[j] = reference[length - 1 - j];
    arm_rfft_fast_f32(&ctx->fft, ctx->time, ctx->reference_spectrum, 0);
    ctx->reference_length = length;
    ctx->sample_rate_hz = sample_rate_hz;
    ctx->min_peak_strength = threshold;
    ctx->initialized = true;
    return true;
}

correlation_result_t correlation_fft_process(correlation_fft_context_t *ctx,
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
    return find_peak(ctx->output, count, length, ctx->sample_rate_hz, ctx->min_peak_strength);
}

bool correlation_fir_init(correlation_fir_context_t *ctx, const float *reference,
                          size_t length, float sample_rate_hz, float threshold) {
    if (!ctx) return false;
    ctx->initialized = false;
    if (!reference || !length || length > CORRELATION_MAX_REFERENCE ||
        !isfinite(sample_rate_hz) || sample_rate_hz <= 0 ||
        !isfinite(threshold) || threshold < 0) return false;
    for (size_t i = 0; i < length; ++i)
        if (!isfinite(reference[i])) return false;
    // Matched-filter impulse response = reversed reference. CMSIS itself
    // stores FIR taps reversed, so supply the original reference order here.
    memcpy(ctx->coefficients, reference, length * sizeof(float));
    arm_fir_init_f32(&ctx->fir, (uint16_t)length, ctx->coefficients,
                     ctx->state, CORRELATION_MAX_SAMPLES);
    ctx->reference_length = length;
    ctx->sample_rate_hz = sample_rate_hz;
    ctx->min_peak_strength = threshold;
    ctx->initialized = true;
    return true;
}

correlation_result_t correlation_fir_process(correlation_fir_context_t *ctx,
                                             const float *samples, size_t count) {
    correlation_result_t invalid = {0};
    if (!ctx || !ctx->initialized || !samples || count < ctx->reference_length ||
        count > CORRELATION_MAX_SAMPLES) return invalid;
    for (size_t i = 0; i < count; ++i)
        if (!isfinite(samples[i])) return invalid;
    // Independent captures: do not leak the previous ping into this one.
    memset(ctx->state, 0, (ctx->reference_length - 1) * sizeof(float));
    arm_fir_f32(&ctx->fir, samples, ctx->output, (uint32_t)count);
    return find_peak(ctx->output, count, ctx->reference_length,
                     ctx->sample_rate_hz, ctx->min_peak_strength);
}

correlation_result_t correlation_fir_process_first_echo(
    correlation_fir_context_t *ctx, const float *samples, size_t count,
    const correlation_echo_options_t *options) {
    correlation_result_t result = {0};
    if (!ctx || !ctx->initialized || !samples || !options ||
        count < ctx->reference_length || count > CORRELATION_MAX_SAMPLES ||
        options->min_lag_samples > options->max_lag_samples ||
        options->min_lag_samples > count - ctx->reference_length ||
        !options->peak_radius_samples ||
        options->peak_radius_samples > CORRELATION_MAX_SAMPLES ||
        !isfinite(options->min_normalized_match) ||
        options->min_normalized_match <= 0 || options->min_normalized_match > 1 ||
        !isfinite(options->min_window_rms) || options->min_window_rms <= 0)
        return result;
    for (size_t i = 0; i < count; ++i)
        if (!isfinite(samples[i])) return result;
    const size_t length = ctx->reference_length;
    const size_t first = options->min_lag_samples;
    size_t last = count - length;
    if (last > options->max_lag_samples) last = options->max_lag_samples;
    double reference_energy = 0, window_energy = 0;
    for (size_t j = 0; j < length; ++j) {
        reference_energy += (double)ctx->coefficients[j] * ctx->coefficients[j];
        window_energy += (double)samples[first + j] * samples[first + j];
    }
    if (!(reference_energy > 0) || !isfinite(reference_energy)) return result;
    (void)correlation_fir_process(ctx, samples, count);
    // Fail closed on arithmetic overflow, including a peak's neighborhood.
    for (size_t i = length - 1; i < count; ++i)
        if (!isfinite(ctx->output[i])) return result;
    const double minimum_energy = (double)options->min_window_rms *
                                  options->min_window_rms * length;
    for (size_t lag = first; lag <= last; ++lag) {
        if (lag > first) {
            window_energy += (double)samples[lag + length - 1] * samples[lag + length - 1]
                           - (double)samples[lag - 1] * samples[lag - 1];
        }
        const float magnitude = fabsf(ctx->output[lag + length - 1]);
        if (window_energy < minimum_energy || magnitude <= ctx->min_peak_strength)
            continue;
        double denominator = reference_energy * window_energy;
        double needed = options->min_normalized_match;
        if ((double)magnitude * magnitude < needed * needed * denominator)
            continue;
        size_t left = lag - first > options->peak_radius_samples
                    ? lag - options->peak_radius_samples : first;
        size_t right = last - lag > options->peak_radius_samples
                     ? lag + options->peak_radius_samples : last;
        bool local_peak = true;
        for (size_t j = left; j <= right; ++j) {
            float neighbor = fabsf(ctx->output[j + length - 1]);
            // First sample wins exact plateaus; include neighbors even when
            // their signal-strength/similarity tests would fail.
            if ((j < lag && neighbor >= magnitude) || (j > lag && neighbor > magnitude)) {
                local_peak = false;
                break;
            }
        }
        if (!local_peak) continue;
        result.delay_samples = (uint32_t)lag;
        result.delay_seconds = lag / ctx->sample_rate_hz;
        result.peak_strength = magnitude;
        result.valid = true;
        return result;
    }
    return result;
}

float correlation_distance_m(correlation_result_t echo, float speed_m_s) {
    if (!echo.valid || !isfinite(echo.delay_seconds) || echo.delay_seconds < 0 ||
        !isfinite(speed_m_s) || speed_m_s <= 0) return NAN;
    return echo.delay_seconds * speed_m_s * 0.5f;
}

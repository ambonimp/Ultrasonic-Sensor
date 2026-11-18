// Both benchmark methods are the public production APIs, never local copies.
#include "correlation.h"
#include "signals.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REPEATS 3
#define SPEED_M_S 343.0f
#define MAX_DISTANCE_M 4.0f
#define PING_PERIOD_US 65000u
static correlation_fft_context_t fft;
static correlation_fir_context_t fir;
static float reference[CORRELATION_MAX_REFERENCE], received[CORRELATION_MAX_SAMPLES];
static volatile float checksum;
static unsigned failures, rows;

static void run_config(unsigned rate, unsigned length, bool burst) {
    if (burst) {
        for (unsigned i = 0; i < length; ++i)
            reference[i] = sinf(2 * PI * ULTRASONIC_CHIRP_CENTER_HZ * i / rate);
    } else benchmark_chirp(reference, length, rate);
    float threshold = 0.2f * benchmark_energy(reference, length);
    uint64_t start = time_us_64();
    bool ok_fft = correlation_fft_init(&fft, reference, length, rate, threshold);
    uint64_t fft_setup = time_us_64() - start;
    start = time_us_64();
    bool ok_fir = correlation_fir_init(&fir, reference, length, rate, threshold);
    uint64_t fir_setup = time_us_64() - start;
    if (!ok_fft || !ok_fir) { ++failures; printf("# INIT FAILED\n"); return; }
    printf("# CONFIG rate=%u taps=%u signal=%s fft_setup_us=%llu fir_setup_us=%llu\n",
        rate, length, burst ? "burst40k" : "chirp30to50k",
        (unsigned long long)fft_setup, (unsigned long long)fir_setup);
    // Capture starts at transmission; enough time for a complete 4 m echo.
    size_t count = (size_t)ceilf(2 * MAX_DISTANCE_M / SPEED_M_S * rate) + length;
    if (count > CORRELATION_MAX_SAMPLES) { ++failures; return; }
    const char *names[] = {"near_2cm", "one_m", "four_m", "fft_boundary", "weak_noise", "no_echo"};
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        float metres = scenario == 0 ? 0.02f : scenario == 2 ? MAX_DISTANCE_M : 1.0f;
        size_t lag = (size_t)lroundf(2 * metres / SPEED_M_S * rate);
        if (scenario == 3) lag = CORRELATION_FFT_SIZE - length; // cross overlap-add boundary
        float amplitude = scenario == 4 ? 0.3f : scenario == 5 ? 0.0f : 0.8f;
        benchmark_capture(received, count, reference, length, lag, amplitude,
                          scenario >= 4 ? 0.1f : 0.0f);
        // Warm caches before timing. Setup, synthetic data and printing excluded.
        correlation_fft_process(&fft, received, count);
        correlation_fir_process(&fir, received, count);
        correlation_result_t results[2];
        for (unsigned method = 0; method < 2; ++method) {
            uint64_t total = 0, worst = 0;
            bool stable = true;
            for (unsigned repeat = 0; repeat < REPEATS; ++repeat) {
                start = time_us_64();
                correlation_result_t result = method == 0 ?
                    correlation_fft_process(&fft, received, count) :
                    correlation_fir_process(&fir, received, count);
                uint64_t elapsed = time_us_64() - start;
                total += elapsed;
                if (elapsed > worst) worst = elapsed;
                bool correct = scenario == 5 ? !result.valid :
                    result.valid && result.delay_samples == lag;
                stable = stable && correct;
                results[method] = result;
                checksum = result.peak_strength;
            }
            bool detected = results[method].valid;
            float distance = correlation_distance_m(results[method], SPEED_M_S);
            float expected = lag / (float)rate * SPEED_M_S * 0.5f;
            float capture_us = 1e6f * count / rate;
            float error_mm = detected ? fabsf(distance - expected) * 1000 : NAN;
            float slack_us = PING_PERIOD_US - capture_us - worst;
            printf("%s,%u,%u,%s,%s,%u,%u,%u,%u,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.4f,%u\n",
                method == 0 ? "FFT1024" : "FIR", rate, length,
                burst ? "burst40k" : "chirp30to50k", names[scenario], (unsigned)count,
                (unsigned)lag, (unsigned)results[method].delay_samples, detected,
                total / (float)REPEATS, (float)worst, capture_us, slack_us,
                expected, distance, error_mm, stable);
            ++rows;
            if (!stable) ++failures;
        }
        float error = 0;
        for (size_t i = length - 1; i < count; ++i) {
            float delta = fabsf(fft.output[i] - fir.output[i]);
            if (!isfinite(delta)) { error = INFINITY; break; }
            if (delta > error) error = delta;
        }
        if (error > 0.002f) ++failures;
        printf("# AGREEMENT max_abs_error=%.7f pass=%u\n", error, error <= 0.002f);
    }
}
static void benchmark_run(void) {
    rows = failures = 0;
    printf("# BEGIN ultrasonic-benchmark-v2\n");
    printf("# SYNTHETIC RAW ADC EXPERIMENT. HC-SR04 TRIG/ECHO needs pulse timing, not FIR/FFT.\n");
    printf("# cpu_hz=%lu speed_m_s=343 max_distance_m=4 ping_period_us=%u repeats=%u fft_context_bytes=%u fir_context_bytes=%u\n",
           (unsigned long)clock_get_hz(clk_sys), PING_PERIOD_US, REPEATS,
           (unsigned)sizeof(fft), (unsigned)sizeof(fir));
    printf("method,sample_rate_hz,taps,signal,scenario,capture_samples,expected_lag,measured_lag,detected,mean_us,worst_us,capture_us,ping_slack_us,expected_m,measured_m,error_mm,pass\n");
    const unsigned rates[] = {125000, ULTRASONIC_SAMPLE_RATE_HZ, 250000};
    const unsigned durations_us[] = {500, 1250, 2500};
    for (unsigned r = 0; r < 3; ++r) {
        // HC-SR04-like 8-cycle fixed 40 kHz burst, NOT its digital ECHO output.
        run_config(rates[r], (unsigned)lroundf(8.0f * rates[r] / 40000), true);
        for (unsigned d = 0; d < 3; ++d)
            run_config(rates[r], (unsigned)lroundf(rates[r] * durations_us[d] / 1e6f), false);
    }
    printf("# END rows=%u failures=%u\n", rows, failures);
    stdio_flush();
}
int main(void) {
    stdio_init_all();
    while (true) {
        if (getchar_timeout_us(100000) == 'r') benchmark_run();
    }
}

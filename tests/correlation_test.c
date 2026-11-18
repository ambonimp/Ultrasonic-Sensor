#include "correlation.h"
#include "signals.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static correlation_fft_context_t fft;
static correlation_fir_context_t fir;
static float reference[CORRELATION_MAX_REFERENCE], samples[CORRELATION_MAX_SAMPLES];

int main(void) {
    const size_t lengths[] = {1, 25, 40, 63, 100, 156, 250, 313, 500, 625};
    const float rates[] = {125000, 200000, 250000};
    double worst = 0;
    unsigned cases = 0;
    for (size_t r = 0; r < 3; ++r) for (size_t t = 0; t < 10; ++t) {
        size_t length = lengths[t];
        if (length == 1) reference[0] = 1;
        else benchmark_chirp(reference, length, rates[r]);
        // Separate formula verifies physical chirp frequencies, not an old implementation.
        if (length > 1) for (size_t i = 0; i < length; ++i) {
            double time = i / (double)rates[r], duration = (length - 1) / (double)rates[r];
            double value = sin(2 * acos(-1.0) * (30000 * time + 10000 / duration * time * time));
            CHECK(fabs(reference[i] - value) < 1e-6);
        }
        float threshold = 0.2f * benchmark_energy(reference, length);
        CHECK(correlation_fft_init(&fft, reference, length, rates[r], threshold));
        CHECK(correlation_fir_init(&fir, reference, length, rates[r], threshold));
        size_t counts[] = {length, length + 1, 1024 - length + 1 > length ? 1024 - length + 1 : length,
                           1024, 2000, CORRELATION_MAX_SAMPLES};
        for (unsigned n = 0; n < 6; ++n) for (unsigned mode = 0; mode < 5; ++mode) {
            size_t count = counts[n];
            size_t lag = mode == 0 ? 0 : mode == 1 ? count - length : (count - length) / 2;
            float amplitude = mode == 4 ? 0 : mode == 3 ? 0.3f : 0.8f;
            // One-sample reference needs no noise; otherwise exact known-lag checks.
            benchmark_capture(samples, count, reference, length, lag, amplitude,
                               length > 1 && mode >= 3 ? 0.05f : 0);
            correlation_result_t a = correlation_fft_process(&fft, samples, count);
            correlation_result_t b = correlation_fir_process(&fir, samples, count);
            CHECK(a.valid == b.valid);
            if (mode == 4) CHECK(!a.valid);
            else {
                CHECK(a.valid && a.delay_samples == lag && b.delay_samples == lag);
                CHECK(fabsf(a.delay_seconds - lag / rates[r]) < 1e-7f);
                CHECK(fabsf(correlation_distance_m(a, 343) - lag / rates[r] * 343 * 0.5f) < 1e-6f);
            }
            for (size_t at = 0; at <= count - length; ++at) {
                double expected = 0;
                for (size_t j = 0; j < length; ++j) expected += (double)samples[at + j] * reference[j];
                double ea = fabs(fft.output[at + length - 1] - expected);
                double eb = fabs(fir.output[at + length - 1] - expected);
                CHECK(ea < 0.002 && eb < 0.002);
                if (ea > worst) worst = ea;
                if (eb > worst) worst = eb;
            }
            // Repeated independent captures must not retain the previous echo.
            memset(samples, 0, count * sizeof(float));
            CHECK(!correlation_fft_process(&fft, samples, count).valid);
            CHECK(!correlation_fir_process(&fir, samples, count).valid);
            ++cases;
        }
    }
    // API failures and invalid values must fail closed for both implementations.
    CHECK(!correlation_fft_init(NULL, reference, 100, 200000, 0));
    CHECK(!correlation_fir_init(NULL, reference, 100, 200000, 0));
    const float bad_rates[] = {0, -1, NAN, INFINITY};
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(!correlation_fft_init(&fft, reference, 100, bad_rates[i], 0));
        CHECK(!correlation_fir_init(&fir, reference, 100, bad_rates[i], 0));
    }
    const float bad_thresholds[] = {-1, NAN, INFINITY};
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(!correlation_fft_init(&fft, reference, 100, 200000, bad_thresholds[i]));
        CHECK(!correlation_fir_init(&fir, reference, 100, 200000, bad_thresholds[i]));
    }
    CHECK(!correlation_fft_init(&fft, NULL, 100, 200000, 0));
    CHECK(!correlation_fir_init(&fir, NULL, 100, 200000, 0));
    CHECK(!correlation_fft_init(&fft, reference, 0, 200000, 0));
    CHECK(!correlation_fir_init(&fir, reference, 0, 200000, 0));
    CHECK(!correlation_fft_init(&fft, reference, CORRELATION_MAX_REFERENCE + 1, 200000, 0));
    CHECK(!correlation_fir_init(&fir, reference, CORRELATION_MAX_REFERENCE + 1, 200000, 0));
    reference[0] = NAN;
    CHECK(!correlation_fft_init(&fft, reference, 100, 200000, 0));
    CHECK(!correlation_fir_init(&fir, reference, 100, 200000, 0));
    reference[0] = 0;
    CHECK(!correlation_fft_process(&fft, samples, 1000).valid);
    CHECK(!correlation_fir_process(&fir, samples, 1000).valid);
    CHECK(correlation_fft_init(&fft, reference, 100, 200000, 0));
    CHECK(correlation_fir_init(&fir, reference, 100, 200000, 0));
    CHECK(!correlation_fft_process(NULL, samples, 1000).valid);
    CHECK(!correlation_fir_process(NULL, samples, 1000).valid);
    CHECK(!correlation_fft_process(&fft, NULL, 1000).valid);
    CHECK(!correlation_fir_process(&fir, NULL, 1000).valid);
    CHECK(!correlation_fft_process(&fft, samples, 99).valid);
    CHECK(!correlation_fir_process(&fir, samples, 99).valid);
    CHECK(!correlation_fft_process(&fft, samples, CORRELATION_MAX_SAMPLES + 1).valid);
    CHECK(!correlation_fir_process(&fir, samples, CORRELATION_MAX_SAMPLES + 1).valid);
    samples[0] = INFINITY;
    CHECK(!correlation_fft_process(&fft, samples, 1000).valid);
    CHECK(!correlation_fir_process(&fir, samples, 1000).valid);
    CHECK(isnan(correlation_distance_m((correlation_result_t){0}, 343)));
    CHECK(isnan(correlation_distance_m((correlation_result_t){.valid=true}, 0)));
    printf("PASS %u signal cases vs independent double-precision correlation; max error %.7f\n", cases, worst);
    return 0;
}

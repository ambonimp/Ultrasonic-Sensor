#include "correlation.h"
#include "signals.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static correlation_fir_context_t fir;
static float ref[250], x[2000];
static void add_echo(size_t lag, float amplitude) {
    for (size_t j = 0; j < 250; ++j) x[lag + j] += amplitude * ref[j];
}
int main(void) {
    benchmark_chirp(ref, 250, 200000);
    CHECK(correlation_fir_init(&fir, ref, 250, 200000, 0.01f));
    correlation_echo_options_t options = {100, 1500, 20, 0.8f, 0.02f};
    memset(x, 0, sizeof(x)); add_echo(300, 0.4f); add_echo(1000, 1.0f);
    CHECK(correlation_fir_process(&fir, x, 2000).delay_samples == 1000);
    correlation_result_t r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
    CHECK(r.valid && r.delay_samples == 300);
    CHECK(fabsf(r.delay_seconds - 0.0015f) < 1e-7f);
    CHECK(fabsf(correlation_distance_m(r, 343) - 0.25725f) < 1e-6f);
    // Inverted first echo must still beat the stronger later reflection.
    memset(x, 0, sizeof(x)); add_echo(300, -0.4f); add_echo(1000, 1.0f);
    r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
    CHECK(r.valid && r.delay_samples == 300 && r.peak_strength > 0);
    // Explicit blanking removes an early transmit-like waveform.
    memset(x, 0, sizeof(x)); add_echo(0, 2); add_echo(600, 0.4f);
    options.min_lag_samples = 400;
    r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
    CHECK(r.valid && r.delay_samples == 600);
    options.max_lag_samples = 550;
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    options = (correlation_echo_options_t){100, SIZE_MAX, 20, 0.8f, 0.02f};
    // A large single-sample spike has energy but lacks the chirp pattern.
    memset(x, 0, sizeof(x)); x[150] = 5; add_echo(600, 0.4f);
    r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
    CHECK(r.valid && r.delay_samples == 600);
    // Normalization alone would accept an arbitrarily tiny exact waveform.
    memset(x, 0, sizeof(x)); add_echo(300, 0.0001f);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    // Raw correlation threshold still applies.
    CHECK(correlation_fir_init(&fir, ref, 250, 200000, 10000));
    memset(x, 0, sizeof(x)); add_echo(300, 0.4f);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    CHECK(correlation_fir_init(&fir, ref, 250, 200000, 0.01f));
    // Noisy early echo remains detectable; repeated calls reset FIR history.
    benchmark_capture(x, 2000, ref, 250, 300, 0.4f, 0.02f); add_echo(1000, 1);
    r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
    CHECK(r.valid && r.delay_samples == 300);
    memset(x, 0, sizeof(x));
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    benchmark_capture(x, 2000, ref, 250, 0, 0, 0.01f);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    // Fully captured references can be selected at both search boundaries.
    for (unsigned lag = 0; lag <= 1750; lag += 1750) {
        memset(x, 0, sizeof(x)); add_echo(lag, 0.4f);
        options.min_lag_samples = 0;
        r = correlation_fir_process_first_echo(&fir, x, 2000, &options);
        CHECK(r.valid && r.delay_samples == lag);
    }
    CHECK(!correlation_fir_process_first_echo(NULL, x, 2000, &options).valid);
    CHECK(!correlation_fir_process_first_echo(&fir, NULL, 2000, &options).valid);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, NULL).valid);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 249, &options).valid);
    CHECK(!correlation_fir_process_first_echo(&fir, x, 8001, &options).valid);
    correlation_echo_options_t bad[] = {
        {200,100,20,.8f,.02f}, {1751,SIZE_MAX,20,.8f,.02f},
        {0,SIZE_MAX,0,.8f,.02f}, {0,SIZE_MAX,SIZE_MAX,.8f,.02f},
        {0,SIZE_MAX,20,0,.02f}, {0,SIZE_MAX,20,1.1f,.02f},
        {0,SIZE_MAX,20,NAN,.02f}, {0,SIZE_MAX,20,INFINITY,.02f},
        {0,SIZE_MAX,20,.8f,0}, {0,SIZE_MAX,20,.8f,-1},
        {0,SIZE_MAX,20,.8f,NAN}, {0,SIZE_MAX,20,.8f,INFINITY}
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i)
        CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &bad[i]).valid);
    x[0] = NAN;
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    // Finite inputs whose floating-point filter output overflows fail closed.
    for (size_t i = 0; i < 250; ++i) ref[i] = FLT_MAX;
    for (size_t i = 0; i < 2000; ++i) x[i] = FLT_MAX;
    CHECK(correlation_fir_init(&fir, ref, 250, 200000, 0));
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    memset(ref, 0, sizeof(ref));
    CHECK(correlation_fir_init(&fir, ref, 250, 200000, 0));
    CHECK(!correlation_fir_process_first_echo(&fir, x, 2000, &options).valid);
    printf("PASS first-echo selection: weaker/inverted early echo, later stronger echo, "
           "blanking, range, spike/noise rejection, energy floor, boundaries and invalid inputs\n");
    return 0;
}

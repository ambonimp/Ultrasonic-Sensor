#define CORRELATION_HOST_TEST
#include "../benchmarks/benchmark.c"
#include <assert.h>

#define CHECK(condition) do { if (!(condition)) { \
  printf("FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void) {
  CHECK(prepare_fft() == ARM_MATH_SUCCESS);
  // Check the generated sweep against a separate double-precision formula.
  // A fixed old slope would fail this when the sample rate changes.
  for (int i = 0; i < REF_SIZE; i++) {
    double t = (double)i / SAMPLE_RATE;
    double duration = (double)(REF_SIZE - 1) / SAMPLE_RATE;
    double start = ULTRASONIC_CHIRP_CENTER_HZ - ULTRASONIC_CHIRP_BANDWIDTH_HZ / 2.0;
    double expected = sin(2.0 * acos(-1.0) *
        (start * t + 0.5 * ULTRASONIC_CHIRP_BANDWIDTH_HZ / duration * t * t));
    CHECK(fabs(reference[i] - expected) < 0.0002);
  }
  int sizes[] = {500, 525, 526, 1000, 2000, 4000, 8000};
  double worst = 0;
  int cases = 0;
  for (int t = 0; t < 7; t++) for (int mode = 0; mode < 5; mode++) {
    int n = sizes[t];
    generate_signals(n);
    if (mode >= 1) {
      for (int j = 0; j < n; j++)
        received[j] = mode == 4 ? ((float)rand() / (float)RAND_MAX - 0.5f) * 0.02f : 0;
      int lag = mode == 1 ? 0 : mode == 2 ? n - REF_SIZE : (n - REF_SIZE) / 2;
      for (int j = 0; j < REF_SIZE; j++)
        received[lag + j] += reference[j] * (mode == 3 ? 0.05f : 0.8f);
    }
    int expected = manual_correlate(n);
    CHECK(cmsis_correlate(n) == expected);
    if (mode == 0 && n >= 800) CHECK(expected == 300);
    for (int v = 0; v < 3; v++) {
      CHECK(fft_correlate(n, v) == expected);
      for (int lag = 0; lag <= n - REF_SIZE; lag++) {
        double direct = 0;
        for (int j = 0; j < REF_SIZE; j++) direct += reference[j] * received[lag + j];
        double error = fabs(direct - fft_result[lag + REF_SIZE - 1]);
        CHECK(error < 0.002);
        if (error > worst) worst = error;
      }
      if (v == 1) {
        correlation_result_t echo = correlation_process(&production, received, n);
        CHECK(echo.delay_samples == (unsigned)expected);
        CHECK(echo.valid == (echo.peak_strength > 0));
        CHECK(fabsf(echo.delay_seconds - (float)expected / SAMPLE_RATE) < 1e-7f);
        // Compare with the old 1024-point code to check that moving it
        // into its own module did not change the answers.
        CHECK(comparison_fft_correlate(n, v) == expected);
        for (int j = 0; j < n + REF_SIZE - 1; j++)
          CHECK(fabsf(production.output[j] - fft_result[j]) < 1e-6f);
      }
      cases++;
    }
  }
  memset(received, 0, sizeof(received));
  for (int v = 0; v < 3; v++) {
    CHECK(fft_correlate(MAX_SAMPLES, v) == 0);
    for (int i = 0; i < MAX_SAMPLES + REF_SIZE - 1; i++) CHECK(fft_result[i] == 0);
  }
  CHECK(!correlation_process(&production, received, MAX_SAMPLES).valid);
  CHECK(!correlation_process(NULL, received, MAX_SAMPLES).valid);
  CHECK(!correlation_process(&production, NULL, MAX_SAMPLES).valid);
  CHECK(!correlation_process(&production, received, REF_SIZE - 1).valid);
  CHECK(!correlation_process(&production, received, MAX_SAMPLES + 1).valid);
  received[0] = NAN;
  CHECK(!correlation_process(&production, received, MAX_SAMPLES).valid);
  CHECK(!correlation_init(&production, NULL, REF_SIZE, 0));
  CHECK(!correlation_process(&production, received, MAX_SAMPLES).valid);
  CHECK(!correlation_init(&production, reference, 0, 0));
  CHECK(!correlation_init(&production, reference, REF_SIZE + 1, 0));
  CHECK(!correlation_init(&production, reference, REF_SIZE, -1));
  CHECK(!correlation_init(&production, reference, REF_SIZE, NAN));
  CHECK(correlation_init(&production, reference, REF_SIZE, 1e6f));
  generate_signals(1000);
  CHECK(!correlation_process(&production, received, 1000).valid);
  CHECK(correlation_init(&production, reference, REF_SIZE, 0));
  correlation_result_t echo = correlation_process(&production, received, 1000);
  CHECK(echo.valid && echo.delay_samples == 300);
  CHECK(manual_correlate(1000) == 300);
  CHECK(fabsf(echo.peak_strength - checksum) < 0.002f);
  printf("PASS: %d F32 cases, production/original agreement, 300-sample delay, "
         "zero input and API checks; max absolute error %.6f\n", cases, worst);
}

#ifndef BENCHMARK_SIGNALS_H
#define BENCHMARK_SIGNALS_H
#include <stddef.h>
#include <stdint.h>
// Synthetic model only: no claim about the transducer response.
void benchmark_chirp(float *reference, size_t length, float rate);
void benchmark_capture(float *capture, size_t count, const float *reference,
                       size_t length, size_t lag, float amplitude, float noise);
float benchmark_energy(const float *reference, size_t length);
#endif

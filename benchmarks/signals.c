#include "signals.h"
#include "ultrasonic_config.h"
#include <math.h>

void benchmark_chirp(float *reference, size_t length, float rate) {
    const double pi = 3.14159265358979323846;
    double start = ULTRASONIC_CHIRP_CENTER_HZ - ULTRASONIC_CHIRP_BANDWIDTH_HZ / 2.0;
    double duration = (length - 1) / (double)rate;
    for (size_t i = 0; i < length; ++i) {
        double t = i / (double)rate;
        reference[i] = (float)sin(2 * pi * (start * t +
            0.5 * ULTRASONIC_CHIRP_BANDWIDTH_HZ / duration * t * t));
    }
}
float benchmark_energy(const float *reference, size_t length) {
    float sum = 0;
    for (size_t i = 0; i < length; ++i) sum += reference[i] * reference[i];
    return sum;
}
void benchmark_capture(float *capture, size_t count, const float *reference,
                       size_t length, size_t lag, float amplitude, float noise) {
    uint32_t seed = 408;
    for (size_t i = 0; i < count; ++i) {
        seed = seed * 1664525u + 1013904223u;
        capture[i] = noise * (2.0f * (float)(seed >> 8) / 16777215.0f - 1.0f);
        if (i >= lag && i - lag < length) capture[i] += amplitude * reference[i - lag];
    }
}

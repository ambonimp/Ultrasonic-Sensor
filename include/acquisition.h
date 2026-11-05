#ifndef ACQUISITION_H
#define ACQUISITION_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const float *samples;
    size_t count;
} acquisition_buffer_t;

// These functions are where real sensor input will be added. No ADC/DMA yet.
// init gives the app the actual sound pattern, sampled at the chosen rate.
// poll gives it a set of samples with the signal centered around zero.
// release says the app is done with the samples and they can be replaced.
// Future input code must catch dropped samples and keep enough old samples
// to find echoes split across two sets. Delay is measured from each set's start.
bool acquisition_init(acquisition_buffer_t *reference);
bool acquisition_poll(acquisition_buffer_t *capture);
void acquisition_release(const acquisition_buffer_t *capture);
#endif

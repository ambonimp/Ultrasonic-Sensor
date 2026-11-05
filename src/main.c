#include "acquisition.h"
#include "correlation.h"
#include "pico/stdlib.h"
#include <stdio.h>

static correlation_context_t correlator;

int main(void) {
    stdio_init_all();
    acquisition_buffer_t reference;
    bool ready = acquisition_init(&reference);
    if (ready) {
        ready = correlation_init(&correlator, reference.samples, reference.count,
                                 ULTRASONIC_MIN_PEAK_STRENGTH);
        acquisition_release(&reference);
    }
    while (true) {
        if (!ready) {
            printf("Ultrasonic: acquisition/reference not configured; ADC/DMA not implemented.\n");
            sleep_ms(2000);
            continue;
        }
        acquisition_buffer_t capture;
        if (!acquisition_poll(&capture)) {
            sleep_ms(1);
            continue;
        }
        correlation_result_t echo = correlation_process(&correlator, capture.samples, capture.count);
        acquisition_release(&capture);
        printf("delay=%lu samples (%.6f s), peak=%.5f, valid=%s\n",
               (unsigned long)echo.delay_samples, echo.delay_seconds,
               echo.peak_strength, echo.valid ? "yes" : "no");

        // Future distance output. Leave disabled until sample zero matches
        // the transmit time and any fixed hardware delay is accounted for.
        // Sound travels to the object and back, so divide by two.
        // 343 m/s is approximate for air at 20 degrees C.
        // A valid match still needs checking against real sensor noise.
        // if (echo.valid) {
        //     const float speed_of_sound_m_s = 343.0f;
        //     float distance_cm = echo.delay_seconds * speed_of_sound_m_s * 0.5f * 100.0f;
        //     printf("distance=%.2f cm\n", distance_cm);
        // }
    }
}

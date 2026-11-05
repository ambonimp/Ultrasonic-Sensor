#include "acquisition.h"

bool acquisition_init(acquisition_buffer_t *reference) {
    *reference = (acquisition_buffer_t){0};
    return false; // Sensor input and the sound pattern are not set up yet.
}
bool acquisition_poll(acquisition_buffer_t *capture) {
    *capture = (acquisition_buffer_t){0};
    return false;
}
void acquisition_release(const acquisition_buffer_t *capture) { (void)capture; }

#ifndef ULTRASONIC_CONFIG_H
#define ULTRASONIC_CONFIG_H

// All programs and tests use this rate. Sensor input still needs to be set up.
#define ULTRASONIC_SAMPLE_RATE_HZ 200000u
#define ULTRASONIC_REFERENCE_SAMPLES 500u
// Requested test sweep: 40 kHz center, 10 kHz below and above it.
// This describes the test signal, not a verified sensor bandwidth.
#define ULTRASONIC_CHIRP_CENTER_HZ 40000u
#define ULTRASONIC_CHIRP_BANDWIDTH_HZ 20000u
// Minimum match strength. Choose a useful value by testing the real sensor.
#define ULTRASONIC_MIN_PEAK_STRENGTH 0.0f

#endif

#ifndef ULTRASONIC_CONFIG_H
#define ULTRASONIC_CONFIG_H

// Default raw-waveform experiment rate; not the distance update rate.
// The benchmark also tests 125 and 250 kHz. Live acquisition is not implemented.
#define ULTRASONIC_SAMPLE_RATE_HZ 200000u
#define ULTRASONIC_REFERENCE_SAMPLES 500u
// Requested test sweep: 40 kHz center, 10 kHz below and above it.
// This describes the test signal, not a verified sensor bandwidth.
#define ULTRASONIC_CHIRP_CENTER_HZ 40000u
#define ULTRASONIC_CHIRP_BANDWIDTH_HZ 20000u
// Minimum match strength. Choose a useful value by testing the real sensor.
#define ULTRASONIC_MIN_PEAK_STRENGTH 0.0f

// Austin's CSV calibration: 40 kHz sine reference, DC-removed input in volts.
// Provisional: channel roles are inferred and wrong-echo cases remain.
// Apply the fixed delay only AFTER making lag relative to transmission,
// using this reference and sample rate. It is not a generic sensor offset.
#define AUSTINS_SAMPLE_RATE_HZ 200000u
#define AUSTINS_REFERENCE_FREQUENCY_HZ 40000u
#define AUSTINS_REFERENCE_SAMPLES 200u
#define AUSTINS_MIN_WINDOW_RMS_V 0.08f
#define AUSTINS_MIN_NORMALIZED_MATCH 0.6f
#define AUSTINS_PEAK_RADIUS_SAMPLES 10u
#define AUSTINS_FIXED_DELAY_SECONDS 0.000303819252f
#define AUSTINS_SOUND_SPEED_M_S 343.0f

#endif

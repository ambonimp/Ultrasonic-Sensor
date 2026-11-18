# Project description

**Goal:** detect distance using an ultrasonic emitter and receiver. **Hardware:** Raspberry Pi Pico 2 W (RP2350, Arm Cortex-M33); module identified by the user as **KY-050 / HC-SR04**.

**Current task:** callable FIR and FFT matched filters, benchmarked on the Pico with known simulated signals. No live acquisition or sensor-control implementation is requested yet. Standard TRIG/ECHO versus analog receiver access is undecided; digital ECHO uses pulse timing, while FIR/FFT applies to raw waveform data.

**Baseline experiment:** a **250-sample, 30–50 kHz chirp sampled at 200 kHz** (1.25 ms), using FIR matched filtering, with a provisional 65 ms measurement interval. Simulated echo checks passed; real recordings must still confirm detection performance. FFT remains available for comparison. Run `python3 benchmarks/run.py` for current Pico timings in the terminal; generated result dumps are not retained. This benchmark does not time the newer first-echo selector.

**Test assumptions:** 40 kHz burst or experimental 30–50 kHz chirp; 125/200/250 kHz waveform sampling; nominal 2 cm–4 m range; provisional 65 ms between pings. These are not verified hardware requirements. Chirp support, accuracy, and required update rate remain open.

**Existing recordings:** [Austin’s capture notes](AustinsChirps/README.md) report 200 kSamples/s, square-wave transmission, and roughly 60 ms between chirps. Channel roles and waveform parameters are unconfirmed; the benchmark uses simulated inputs.

**Distance:** `round-trip delay × 343 m/s / 2` for simulated air at about 20 °C. See [benchmark method and commands](benchmarks/README.md) and the [HC-SR04 datasheet](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf).

# Austin's chirp captures

These CSV files contain Austin's chirp recordings for the ultrasonic project.

## Signal and capture context

Austin's description, supplied with the recordings:

> Chirps are square wave and about 60 ms between chirps, but each file should only hold one chirp.

- The transmitted chirps use a square waveform.
- Approximately 60 ms separates chirp transmissions. This describes the spacing between transmissions, not the duration of one chirp.
- Each CSV is expected to contain one chirp capture. It may also contain the associated echo, ringing, and background noise.
- The files are named `chirp_<distance>cm_<trial>.csv`: distance labels are 10, 20, 30, 40, and 50 cm, with three trials per label (15 files total). The exact distance reference and experimental geometry have not been documented.

## CSV format

The first row labels the columns `x-axis,2,3`; the second specifies `second,Volt,Volt`. Subsequent rows contain time in seconds and two voltage channels. The channel roles (for example, transmit reference versus receiver output) have not been confirmed.

All 15 files contain 1,000 data rows, with timestamps from -0.500 ms to 4.495 ms in 5 microsecond steps: a recorded sample rate of 200 kSamples/s and an approximately 5 ms capture window. This capture window is not a measurement of the chirp duration.

## Intended project use

Compare the recorded chirps against the detector's reference waveform using cross-correlation to check whether the detector matches the actual transmitted/received signal. A single chirp per file is sufficient for that comparison; separate captures do not establish the interval between transmissions.

The original comparison used a synthetic sine-wave sweep from 30 to 50 kHz, containing 500 samples at 200 kSamples/s (approximately 2.5 ms). A 40 kHz, 1 ms sine reference fits these recordings better. See [calibration/RESULTS.md](calibration/RESULTS.md) for the final results and commands.

The selected calibration numbers are stored once in `include/ultrasonic_config.h` as `AUSTINS_*` constants. C and Python use those same values. Calibration scripts and the final result CSV are in `calibration/`; candidate attempts and duplicate JSON configurations are not retained.

The transmitter hardware, sweep endpoints, chirp duration, channel assignments, and trigger timing still need confirmation. A square-wave electrical drive can produce a smoother acoustic/received waveform because of the transducers and analog circuitry.

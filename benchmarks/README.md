# FIR versus FFT benchmarks

**Run one command and read the results in the terminal. No result files are saved.**

From the `Ultrasonic` folder, with the Pico connected and other serial monitors closed:

```sh
python3 benchmarks/run.py
```

This builds and flashes `ultrasonic_benchmark`, finds the Pico's USB serial port, starts a run, and prints pass/fail plus a 12-row timing table. It requires the existing Pico SDK, build tools, and picotool. Normal build output is hidden; build errors are printed if something fails. There is no VS Code task.

If the benchmark is already flashed:

```sh
python3 benchmarks/run.py --no-flash
```

If multiple boards are connected, add `--port /dev/cu.usbmodem...` with the actual port.

For C correctness tests on your computer, without a Pico:

```sh
python3 benchmarks/run.py --host
```

Host tests print their pass/fail results. They do not measure Pico speed.

## What the benchmark measures

Both methods call the real functions in `src/correlation.c`: CMSIS floating-point FIR and the 1024-point FFT matched filter. This comparison uses their strongest-positive-peak APIs. It does not time the newer first-echo selector used for Austin's recordings.

| Test | Purpose |
| --- | --- |
| Eight-cycle 40 kHz burst | Compare a short reference resembling the module's nominal transmission. |
| 30–50 kHz sweeps lasting 0.5, 1.25 and 2.5 ms | Compare different reference lengths. Actual module sweep support is unverified. |
| 125, 200 and 250 kSamples/s | Compare processing cost at different waveform sampling rates. |
| Simulated echoes at 2 cm, 1 m, 4 m and an FFT overlap boundary | Check known delays and block handling. |
| Weak/noisy echo and noise without an echo | Check detection and rejection for the tested synthetic noise. |

Each capture includes enough samples for a complete 4 m echo. Both methods get the same reference, samples, and threshold (20% of reference energy). Normal echo amplitude is 0.8; weak amplitude is 0.3; noisy cases use reproducible uniform noise of ±0.1.

After one warm-up, three calls per case are timed. The terminal table shows the worst processing time across the six cases, which method was faster, and whether capture plus processing fits a provisional 65 ms cycle. Input checking, FIR reset and strongest-peak selection are included. Setup, simulated signal generation, USB printing and capture time are excluded from processing time.

All 144 method/case checks and 72 FIR/FFT output-agreement checks must pass. The runner rejects incomplete output, incorrect delays and failed checks instead of displaying a success summary.

These are simulated raw-waveform tests, with no ADC or actual sound. They establish mathematical correctness and measured processing speed on the Pico, not physical distance accuracy. A standard HC-SR04 digital ECHO output instead uses pulse-width timing. Fractional delays, ringing, transmit leakage and real multipath are not modeled here.

Only normal build artifacts go in `build/`. The scripts do not create CSV/JSON reports, saved serial logs, images or generated Markdown reports. This README remains the benchmark documentation.

# Ultrasonic distance detection

Raspberry Pi Pico 2 W project. Start with the brief [project description](PROJECT.md).

The current deliverable is a **simulated comparison of callable FIR and FFT matched filters**. Both find a known waveform in a recording, return its delay, and support conversion to distance. Sensor capture remains a placeholder.

## Functions

Include `correlation.h`. Allocate the chosen context with `static`, initialize it once, then process each independent capture:

```c
static correlation_fir_context_t fir;
correlation_fir_init(&fir, reference, reference_count, sample_rate_hz, threshold);
correlation_result_t echo = correlation_fir_process(&fir, samples, sample_count);
float distance_m = correlation_distance_m(echo, 343.0f);
```

For FFT, use `correlation_fft_context_t`, `correlation_fft_init()`, and `correlation_fft_process()` with the same arguments. Check initialization success and `echo.valid`; distance is NaN for an invalid echo. Threshold is raw correlation strength and requires real-noise calibration. Output index `lag + reference_count - 1` corresponds to echo start `lag`; that filter offset is already removed from the returned delay.

## Build and run

Use Pico SDK 2.3.1, Arm compiler 15_2_Rel1, CMake and Ninja (installed by the Pico VS Code extension on this Mac).

For a one-command benchmark with results printed in the terminal:

```sh
python3 benchmarks/run.py         # Build, flash and run on the connected Pico.
python3 benchmarks/run.py --host  # C correctness tests on this computer.
```

No CSV, JSON or serial-log reports are saved. The [benchmark README](benchmarks/README.md) explains the tests and terminal table. Manual build/flash commands remain available below.

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.3.1" \
  -DPICO_BOARD=pico2_w -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ultrasonic ultrasonic_benchmark -j 8
picotool load build/ultrasonic_benchmark.uf2 -fx
```

If tools are absent from PATH, use their executables under `$HOME/.pico-sdk/`. For manual serial use, the benchmark waits for a USB serial connection at 115200 baud, starts when you send lowercase `r` and ends with `# END`. The runner handles this and displays a compact summary. `ultrasonic` remains the acquisition placeholder, using the FIR first-echo API; it does not measure physical distance yet.

- [Benchmark purpose, method, and terminal commands](benchmarks/README.md)
- [Correctness tests](tests/README.md)
- [Public API](include/correlation.h)

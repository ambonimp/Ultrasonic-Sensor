# Ultrasonic echo detection

A **Raspberry Pi Pico 2 W** project that finds a known sound pattern and reports the echo's delay and strength. The main app and benchmark share the same F32 1024-point FFT code.

**Real sensor input is not implemented yet.** The benchmark runs with simulated echoes. Distance calculation is included in `src/main.c` but commented out until real input and transmit timing are connected.

## Setup

Install the Raspberry Pi Pico VS Code extension, or set up Pico SDK **2.3.1**, Arm compiler **15_2_Rel1**, CMake, Ninja, and picotool yourself. Run the commands below from this project folder.

Download the math library version saved with the repo:

```sh
git submodule update --init --recursive
```

## Build

Adjust the SDK path if yours is different:

```sh
cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.3.1" \
  -DPICO_BOARD=pico2_w -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ultrasonic ultrasonic_benchmark -j 8
```

| Target | What it runs |
| --- | --- |
| `ultrasonic` | Main app; currently reports that sensor input is not ready. |
| `ultrasonic_benchmark` | Repeated speed and accuracy tests using simulated echoes. |

To build just one, use its name after `--target`. Release builds use `-O3` for speed.

## Upload

Close the serial monitor, then upload and start the benchmark:

```sh
picotool load build/ultrasonic_benchmark.uf2 -fx
```

Use `build/ultrasonic.uf2` instead for the main app. If automatic upload fails, hold **BOOTSEL** while connecting the Pico, then copy the chosen `.uf2` onto the **RP2350** drive.

Open the Pico's serial port at **115200 baud**. On macOS it usually starts with `/dev/cu.usbmodem`; the number may change after uploading.

If the tools are not on your terminal's PATH, the Pico extension installs them under `$HOME/.pico-sdk/`. On this Mac, the commands are:

```sh
"$HOME/.pico-sdk/cmake/v4.3.4/bin/cmake" --build build --target ultrasonic_benchmark -j 8 &&
"$HOME/.pico-sdk/picotool/2.3.1/picotool/picotool" load build/ultrasonic_benchmark.uf2 -fx
```

## Where to look

- `src/`: main app, echo-finding code, and placeholder sensor input.
- `include/ultrasonic_config.h`: sample rate (**200 kHz**) and test chirp (**30–50 kHz**).
- [Benchmarks](benchmarks/README.md): results, graphs, and explanations.
- [Tests](tests/README.md): check the math on your computer without a Pico.

# Correctness tests

These run the **same FIR and FFT functions as the Pico**, with an independent double-precision direct-correlation formula as the oracle. No benchmark code or old FFT implementation is included.

For one command with pass/fail printed in the terminal, run `python3 benchmarks/run.py --host`. The manual steps are:

```sh
cmake -S tests -B build/host-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/host-tests -j 8
ctest --test-dir build/host-tests --output-on-failure
```

Checks cover multiple reference lengths and sample rates; clean, weak/noisy, and absent echoes; first/last valid echo positions; FFT overlap boundaries; independent repeated captures; distance conversion; invalid inputs and failed initialization. Every searched output is checked against the oracle with absolute error below 0.002. Host tests establish math correctness; only the Pico benchmark establishes Pico speed. Neither establishes physical sensor accuracy.

`first_echo_test` checks the production `correlation_fir_process_first_echo()` API: an early weak echo ahead of a stronger late reflection, inverted polarity, configurable blanking/range, noise and impulse rejection, minimum signal energy, raw threshold, independent captures, boundary matches, invalid options, zero-energy reference, and arithmetic overflow. These are separate from the unchanged strongest-peak correlation tests.

The main application now calls this first-echo API. Options are provisional and read from the `AUSTINS_*` constants in `include/ultrasonic_config.h`. The main app has no transmit timestamp yet, so its minimum lag is zero; callers must set a calibrated blanking interval. The acquisition stub still returns not-ready, and distance output remains disabled. The benchmark retains the original strongest-peak APIs for mathematical/speed comparison and does not time the new selector.

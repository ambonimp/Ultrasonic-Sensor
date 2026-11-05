# Math tests

Checks the same echo-finding code used by the Pico. **No board needed.** Run from the project folder using your computer's C compiler:

```sh
git submodule update --init --recursive
cmake -S tests -B build/host-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/host-tests -j 8
ctest --test-dir build/host-tests --output-on-failure
```

Expected: **100% tests passed**.

The test chirp sweeps 30–50 kHz around a 40 kHz center. A separate formula checks that the generated samples match that sweep.

Checks cover 105 signal cases, the **300-sample echo**, weak/noisy signals, zero input, bad inputs, and matching the old 1024-point code. Allowed error: 0.002 versus direct math, 0.000001 versus the old code.

These check answers, not Pico speed or real sensor input. Generated files in `build/` are ignored by Git; keep the test source.

[Saved checks](../benchmarks/RESULTS.md) · [Pico speed tests](../benchmarks/README.md)

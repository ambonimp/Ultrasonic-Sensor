# Calibration results

## What happened?

We tested Austin's recordings using **FIR**. Here, FIR slides a known sound pattern across a recording to find where a similar sound appears.

We did not know how the two recorded channels were connected, so we guessed:

- **Channel 2:** marks when the transmitter is turned on. It shows one short on/off pulse.
- **Channel 3:** records the sound coming back. It shows a fast wave that generally arrives later when the object is farther away.

These guesses still need to be confirmed.

We then applied **calibration**: a small timing correction. Think of a ruler that starts at the wrong place—we adjusted where zero should be. In these recordings, that means subtracting about **5.21 cm** from the calculated distance.

## What did it look like?

| Actual distance | Before calibration | After calibration |
| --- | ---: | ---: |
| 10 cm | 15.26 cm | **10.05 cm — better** |
| 20 cm | 25.38 cm | **20.17 cm — better** |
| 30 cm | 35.16 cm | **29.95 cm — better** |
| 40 cm | 44.16 cm | **38.95 cm — better** |
| 50 cm | 41.59 cm | **36.38 cm — worse** |

Across these five checks, the average mistake shrank from **5.68 cm to 2.99 cm**—about half as much error.

**Why did 50 cm get worse?** The code appears to have picked the wrong returning signal. Its answer was already too small, so subtracting another 5.21 cm made it worse. Calibration can correct a timing offset; it cannot fix choosing the wrong sound.

**So calibration helped most of these checks, but we still cannot trust every distance.** We need to confirm the channel connections and fix the wrong-signal problem. These results came from saved recordings, not a live sensor test.

## Run it

From the `Ultrasonic` folder:

```sh
# Test the saved settings.
python3 lab_tests/calibration/calibrate.py --check

# Find and save the best settings.
python3 lab_tests/calibration/calibrate.py --fit
```

The settings are in **`include/ultrasonic_config.h`**, named `AUSTINS_*`. Both C and Python use those same numbers. The final answers go in `results.csv`. Rebuild the C program after changing its settings.

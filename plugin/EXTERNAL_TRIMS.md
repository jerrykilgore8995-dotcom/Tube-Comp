# External digital Input and Output controls

These host-boundary controls live in `PluginProcessor.cpp`; do not modify `TubeCompDSP.h` or the original RS124 / A-436B circuit calibration.

Audio flow: DAW input -> external Input multiplier -> **unchanged** `engine.process(left,right)` -> external Output multiplier -> DAW output.

## Input displayed value (dB)

`preGain = 10^((inputDisplayed - 12)/20)`

| Display | Actual multiplier relative to the previous plugin | Actual dB |
| --- | --- | --- |
| -12 | 0.0630957 | -24 |
| 0 (default) | 0.251189 | -12 |
| +12 | 1.0 | 0 (previous plugin unity) |

## Output displayed value (dB)

`postGain = 10^(outputDisplayed/20)`

| Display | Actual multiplier | Actual dB |
| --- | --- | --- |
| -12 | 0.251189 | -12 |
| 0 (default) | 1.0 | 0 |
| +12 | 3.981072 | +12 |

Both are sample-smoothed, automated JUCE parameters, applied equally to L and R for stereo balance. No internal compressor control code, analog-modeled input/output attenuator, load or phase model is modified. CAUTION: +12 dB output trim can exceed 0 dBFS; no limiter or clipping is intentionally added.

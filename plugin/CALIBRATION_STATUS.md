# Tube Comp V0.1 — Measured realtime model, calibration boundary

## Deliverable

A Windows x64 VST3 built from a sample-by-sample C++ compressor core, not SPICE
on the audio thread. `plugin/TubeCompDSP.h` implements signal preprocessing,
a variable-mu nonlinear gain element inspired by 6BC8 remote cutoff,
bias-dependent K2/K3, rectified 6AL5-like envelope control,
program-level-sensitive ratio and knee, a distinct modeled 6CG7 output stage,
high/low frequency coupling and real RS124 HOLD behavior.

- Default mode: RS124 reconstruction with modern stepped controls.
- Hardware switch: stock A-436B with fixed 50ms attack / 1000ms 63% recovery.
- RS124 attack switch: 9 positions (fastest 1 → slowest 9).
- RS124 recovery: 11 physical settings from 6 (slowest) to 1 (fastest), with
  five HOLD positions between them.
- BAL changes push-pull harmonic asymmetry. A centered BAL yields K3-dominant
  spectra, with much less K2 than the earlier non-SPICE-informed prototype.
- The front-panel POWER switch toggles host-visible bypass.
- Input / output / attack / recovery / balance map to real DAW automation.
- Output impedance 200/600 ohms and a *reconstructed* SuperFuse switch are in
  the bottom host UI. No internal true transformer impedance/hysteresis yet.
- No latency, no oversampling in version 0.1. Reconsider 2x on measured alias tests.

## Explicitly provisional RS124 timing

The Chandler/Abbey Road manual confirms the number, order and HOLD function
of the attack and recovery positions but **does not publish nine exact
millisecond values**. The values below came from the existing project's
provisional SPICE 6AL5/RC candidate network, not a real Chandler unit.
They are not represented as historically exact.

| Attack position | Candidate configured time (63%) | Measured on DSP at 44.1k |
|--:|--:|--:|
| 1 | 12 ms | 12.8 ms |
| 2 | 22 ms | 23.2 ms |
| 3 | 33 ms | 34.6 ms |
| 4 | 47 ms | 49.0 ms |
| 5 | 68 ms | 70.5 ms |
| 6 | 100 ms | 103.1 ms |
| 7 | 150 ms | 153.7 ms |
| 8 | 220 ms | 223.8 ms |
| 9 | 330 ms | 331.0 ms |

| Recovery label | Candidate configured time (63%) | Measured (44.1k) |
|--:|--:|--:|
| 1 / fastest | 180 ms | 180.9 ms |
| 2 | 330 ms | 330.8 ms |
| 3 | 680 ms | 681.3 ms |
| 4 | 1000 ms | 1001.8 ms |
| 5 | 1800 ms | 1803.3 ms |
| 6 / slowest | 3300 ms | 3307.9 ms |
| Interleaved HOLD ×5 | Freeze at achieved peak GR | No release |

The factory Altec reference is 50ms attack and 1-second 63% recovery.
The DSP reproduces that with its A-436B timing coefficients; full factory
compression curve and reference load/circuit gain remain uncalibrated.

## Measured harmonic behavior (44.1k, 1 kHz, exact DSP)

DSP stage levels were deliberately reduced from the excessively distorted
earlier SPICE explorer model. K3 should dominate when push-pull balance is
centered; K2 grows with mismatched triodes. The final dense test generated
160 harmonic/THD cases, including 100, 1000, 5000 and 10000 Hz, with
source input of -30/-24/-18/-12/-6 dBFS and two INPUT positions.

| Mode, balance centered | Maximum THD in dense survey |
|---|---:|
| A-436B | 1.279% |
| RS124 | 1.388% |

At balance offset +0.5, maximum observed THD was approximately 1.478%
(A-436B) and 1.575% (RS124) in the deliberately hot test. These are not
measurements of physical hardware, and no single-number THD result proves
match of harmonics and power response.

The original factory literature limits Altec THD to 1.5% at 25dB GR,
but the test conditions differ; we treat that as a *target*, not a certified
hardware equivalence. Critically, the initial SPICE exploratory circuit
gave about 9–11% THD at a 0dBu simulated signal—a symptom of uncalibrated
plate curves or excessive internal signal level, not a credible golden target.

## How to reproduce results

GitHub Actions > Fast Native DSP Regression exports
`harmonic_grid.csv`, `timing_grid.csv`, and
`DSPTestMeasurements.txt`. Each DSP test uses fresh engine state.

GitHub Actions > Build Tube Comp RS124 Windows VST3 first executes
CTest, then compiles the actual JUCE plugin from matching code.

## Required further calibration

1. Source-lock the **436B**, not 436C, factory schematic exact connections.
2. Fit 6BC8 plate curves and grid-bias transfer, 6CG7 load line, diode rectifier.
3. Compare circuit and realtime DSP harmonics K2–K20, nonlinear FR and phase
   at 100/1000/5000/10000Hz under gain reduction, across input levels.
4. Acquire hardware reference timings for nine Chandler switch positions:
   none of the publicly available manuals specifies those nine milliseconds.
5. Validate aliasing (especially 5/10k at 44.1k), check optional localized
   oversampling rather than automatically imposing 4x on all processing.
6. Test real-time CPU usage and automation clicks at 128/256 samples in Reaper.
7. Rebuild transformer loading when winding, termination and physical-response
   data are available.

Do not market the V0.1 prototype as an exact serial-number RS124 clone.

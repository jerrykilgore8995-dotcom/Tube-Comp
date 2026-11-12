# Tube Comp | RS124 + A-436B

**Historical-circuit reconstruction and measurement-first plug-in project.**

Two hardware modes are planned:

- **A-436B**: factory Altec 436B behavior, with its original fixed attack/recovery, one input gain, fixed internal threshold and no fictitious stock release control.
- **RS124**: modified Altec-derived circuit, with authentic-looking front-panel INPUT, OUTPUT, ATTACK, RECOVERY/HOLD, BAL, meter, and optional modern facilities.

### Honesty contract

1. A *component-level exploratory circuit* is **not** described as a verified Altec 436B, even when all its parts match the period hardware.
2. 436B drawing 13022-1 has a published parts list. Connectivity needs verification from a legible original scan before any claim of an exact 436B netlist.
3. Historical RS124 machines have materially different component sets and timing. Chandler's proprietary revisions and SuperFuse implementation are not published. Any reconstructed version must be marked **ENGINEERING RECONSTRUCTION**, not an exact serial-number clone.
4. Transformer transfer and loading may be parameterized proxies until real frequency/phase/THD and winding measurements become available.
5. All numerical measurements are labeled by source: HISTORICAL_DOCUMENT / NGSPICE_EXPLORATORY / CALIBRATED_SPICE / REAL_HARDWARE.
6. No VST3 will be advertised as historically accurate until the SPICE circuit passes its calibration tests and the VST3 is independently validated against out-of-sample SPICE sweeps.

### What is actually implemented in the research baseline

- Provenance-controlled parts and operating-point reference data;
- Original behavioral triode and diode *initial-guess* device cells, actual nonlinear SPICE B-sources, not polynomial static waveshapers;
- A differential 6BC8 / push-pull 6CG7 exploratory signal-path topology and a 6AL5 rectifier test cell;
- Automated batch execution of .op, .ac and .tran across multiple control/bias/drive conditions;
- Individual harmonic bins K2–K20, THD, THD+N, phase/frequency response, compression ratio/GR, DC node measurements and plots;
- Historical specification acceptance target file, but exploratory runs are **not** asserted to pass factory targets automatically.
- GitHub Actions Linux ngspice workflow and downloadable JSON/CSV/SVG/PNG/ZIP measurements.

### Current maturity

| Branch | Status |
|---|---|
| Factory A-436B | Historical parts list captured; connectivity audit pending |
| RS124 | Controls/timing documented; exact proprietary component revisions pending |
| Device models | Initial nonlinear model candidate; full measured curve fitting pending |
| Circuit measurements | Working automated ngspice research framework |
| Realtime DSP | Planned after SPICE calibration |
| Front panels | Hardware-accurate layout study / JUCE preview planned; final skin must use original drawn artwork |

### Research workflow

GitHub -> Actions -> **Circuit Simulation Laboratory** -> run. Results are uploaded as a ZIP artifact.

Local Linux: `sudo apt install ngspice python3-numpy python3-matplotlib` then `python3 scripts/run_lab.py --quick`.

Documentation: [research/source-register.md](research/source-register.md), [research/calibration.md](research/calibration.md), [research/RS124-mod-register.md](research/RS124-mod-register.md).

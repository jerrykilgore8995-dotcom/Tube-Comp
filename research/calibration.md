# Calibration gates and exactness rules

## Factory specification gates (do not mark passed until measured)

- 436B input bridging ~15kohm; output 150 or 600ohm termination.
- Gain: 56dB (15kohm source) or 42dB (bridge 600ohm source), measurement-context dependent.
- FR: +/-1.5dB, 30Hz-15kHz, specified configuration.
- Maximum compression: 30dB.
- Attack: 50ms.
- Recovery: 1s to 63% recovery.
- THD <1.5% 35-15kHz at 25dB GR; THD <2.5% 35-10kHz at 30dB GR.
- Full path 6BC8 remote-cutoff curve, 6CG7 load line, 6AL5 conduction / rectifier, bias, headroom, control interactions.
- High frequency waveform harmonics K2-K20 measured with coherent time-domain analysis and error bars.

## Required operating points

Run per-tube all active nodes in idle, low-drive, threshold vicinity, 5dB, 10dB, 20dB, 30dB GR.
At every case: plates, cathodes, grids, B+, control-voltage nodes, 6AL5 diode currents, tube plate currents and cathode currents.

## Dynamic test matrix

- Input levels -60 to +24 dBu in 1/3/6dB steps (coarse to fine)
- 20/30/35/50/100/200/500/1000/2000/5000/10000/15000/20000 Hz
- 9 attack positions, 11 RS124 recovery/HOLD positions (with serial-dependent variations documented)
- compressor GR target 0, 3, 5, 10, 15, 20, 25, 30dB
- full multi-control interaction grids with adaptive refinement
- THD / K2..K20, AC phase and group delay, stereo coupling, intermodulation, transient ramp, crest factor, release 10/50/63/90%, noise/PSRR, B+ sag.
- Unit variance through component tolerance, bias error, device mismatch and aging.

## Anti-overfitting

Fit parameters on subset of input/frequency/GR/control points; hold 25% of grid out for independent verification.
Every output row must include source model version, netlist hash, simulated state, ngspice version and provenance.
Don't use the term "exact" until historical schematic connectivity + measured hardware calibration are established.

## Reference proxies

Initially transformer loading is a fixed differential source and 600ohm output termination. These are **explicitly provisional** until input/output winding data arrives. Do not fit transformer phase or THD from a fake transformer model.

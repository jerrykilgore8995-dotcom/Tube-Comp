# 176 Live

Mono Windows x64 VST3 for AVX2/FMA processors. Copy the entire `176 Live.vst3`
directory into the Windows VST3 directory. The editor uses the 176 faceplate
artwork. The original ngspice VST3 has a separate plug-in ID.

The native audio engine compiles the averaged-supply 176 circuit equations
from LiveSPICE. For each control setting it precomputes a stiffly accurate
linear state transition over one host sample, with source and device currents
held over that interval. The nonlinear device currents are solved at the
host sample rate. The input and output pads,
detector mode, attack, release, and ratio affect the circuit. The left
vernier position blends in dry signal, from 0% dry by default to 100% dry.
The dry signal follows the modeled wet path's inverted polarity.
The right vernier position adjusts a first-order digital sidechain HPF from
0 Hz (bypass) to 350 Hz. The filter acts at the detector taps, preserving
the transformer windings and existing reflected detector-current path.
The 6AL5 current law is interpolated from a monotone table. The grid diode
junction capacitances use fixed zero-bias values. Gain reduction indication
uses the scaled control voltage. The Live ratio settings fit the two
detector parameters (T3 tap fraction and 6AL5 bias return) to the transfer
chart from the original UA 176 hardware manual. The fit uses settled 1 kHz
measurements from 5 to 19 dB of gain reduction and a shared level offset
for all four ratios. The supply retains load-dependent sag and
0B2 regulation. It averages the mains transformer and rectifier while
retaining C11D before the choke, C11A/B/C after the choke, C8B and C9B at
the T3 B+ node, C9A on the V2 feed, C10 on the regulated rail and C8A
on the detector bias feed.

At 44.1 kHz, the source response is reconstructed at the device ports
from the continuous circuit, interpolating between operating points
according to the detector control voltage. Newton solves the difference
between those reference currents and the actual nonlinear device currents.
The native calculation uses 32 samples of internal alignment at 44.1 kHz.
Right-click the faceplate background to select 1x, 2x, or 4x oversampling.
The selected mode is saved with the project. Oversampling is available at
44.1 kHz. Other rates run the native solver. The 2x and 4x modes apply a source
correction derived from the discrete solver and the continuous LiveSPICE
circuit, then run the nonlinear solve at the higher rate between half-band
resampling stages. All modes report 168 samples of latency so the switch,
Mix, and bypass can stay aligned. The detector solves its piecewise diode
law directly against its self-impedance before the coupled Newton solve.

The Windows workflow generates the circuit from the pinned LiveSPICE sources,
generates the source profiles using NumPy and SciPy, benchmarks the native
and oversampled solvers, builds the VST3 and packages it.

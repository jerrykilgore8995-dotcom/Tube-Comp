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
uses the scaled control voltage. The supply retains load-dependent sag and
0B2 regulation and averages out mains ripple and rectifier startup.

At 44.1 kHz, the source response is reconstructed at the device ports
from the continuous circuit, interpolating between operating points
according to the detector control voltage. Newton solves the difference
between those reference currents and the actual nonlinear device currents.
This uses native-rate samples and incurs 32 samples of host-reported latency.
Mix and bypass share the same delay. Other sample rates use the held-source
solver without this reconstruction. The detector solves its piecewise diode
law directly against its self-impedance before the coupled Newton solve.

The Windows workflow generates the circuit from the pinned LiveSPICE sources,
generates the source profiles using NumPy and SciPy, benchmarks the native
solver, builds the VST3 and packages it.

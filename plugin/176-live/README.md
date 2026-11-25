# 176 Live

Mono Windows x64 VST3 for AVX2/FMA processors. Copy the entire `176 Live.vst3`
directory into the Windows VST3 directory. The editor uses the 176 faceplate
artwork. The original ngspice VST3 has a separate plug-in ID.

The native audio engine compiles the averaged-supply 176 circuit equations
from LiveSPICE. It factors the linear BDF2 network once per control setting
and solves the coupled device currents per audio frame, in a mono circuit. The input and output pads,
detector mode, attack, release, and ratio affect the circuit. The left
vernier position blends in dry signal, from 0% dry by default to 100% dry.
The right vernier position adjusts a first-order digital sidechain HPF from
0 Hz (bypass) to 350 Hz. The filter acts at the detector taps, preserving
the transformer windings and existing reflected detector-current path.
The 6AL5 current law is interpolated from a monotone table. The grid diode
junction capacitances use fixed zero-bias values. Gain reduction indication
uses the scaled control voltage. The supply retains load-dependent sag and
0B2 regulation and averages out mains ripple and rectifier startup.

The Windows workflow generates the circuit from the pinned LiveSPICE sources,
benchmarks the native solver, builds the VST3 and packages it.

# 176 LiveSPICE front-end bench

This fixture ports the input pad, loaded T1, both Sylvania 6BC8 sections and
the reverse-wired A-19 T2 from `spice/176/gain.cir` to LiveSPICE's circuit
analysis and generated transient solver. T2 sees the bench's 20 kΩ and 7 pF
per side load. The rail is fixed at 108 V and the control voltage is swept
externally. The original plate law and three electrode capacitances are
included. The grid diode uses the same saturation current, ideality
factor and series resistor, with a fixed junction capacitance in this bench.
The original voltage-dependent junction capacitance and transit time need
to be added for overload validation.

The patch stamps LiveSPICE's BDF2 rule directly in the modified nodal
equations, avoiding a symbolic elimination step that fails on this circuit's
capacitive loops. It also reports Newton-iteration exhaustion as an error.
The solver's DC initialization, Jacobian generation and transient execution
come from LiveSPICE.

## Run

Use .NET 8 and a separate recursive checkout of LiveSPICE. The patch is
based on upstream commit `b2a4bb0ec4d3c4cdb158530ab4b20c13207030f7`.

```sh
git clone --recursive https://github.com/dsharlet/LiveSPICE.git /tmp/LiveSPICE
git -C /tmp/LiveSPICE checkout b2a4bb0ec4d3c4cdb158530ab4b20c13207030f7
git -C /tmp/LiveSPICE submodule update --init --recursive
git -C /tmp/LiveSPICE apply "$PWD/research/176/livespice/livespice-direct-bdf2.patch"
dotnet build research/176/livespice -c Release -p:LiveSpiceRoot=/tmp/LiveSPICE
dotnet research/176/livespice/bin/Release/net8.0/LiveSpice176.dll 1
dotnet research/176/livespice/bin/Release/net8.0/LiveSpice176.dll 2
```

The runner applies a +4 dBu matched-source 1 kHz tone with the 600 Ω source
and 20 dB input pad at 44.1 kHz. It times 256 mono blocks of 512 samples after
warm-up, for each of 0 V, −2 V and −4 V control bias. Harmonic distortion uses
K2–K20. Setup and the first block are timed separately because LiveSPICE
generates and compiles the transient function before steady processing.

This is a headless bench. The live line amplifier, T3, 6AL5 feedback,
power supply, and VST3 host integration are outside this fixture.
The existing ngspice VST3 remains the full offline reference.

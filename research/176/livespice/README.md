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

The front-end and closed-loop fixtures are headless. The existing ngspice
VST3 is the offline reference.

## Closed-loop circuit

The full fixture connects T2 directly to the 12AX7 grids, runs both 12AX7 and
12BH7 halves with their cathode returns, and models the 180 kΩ / 180 kΩ /
0.47 µF plate-to-cathode feedback paths. It uses 0.1 µF for both coupling
capacitors, the three coupled T3 windings and their reflected detector load,
the two 6AL5 sections, the ratio-selected tap and bias divider, and the timing
network back to the 6BC8 grids. The supply is configurable for comparison.

The averaged supply uses 439.417 V and 678.671 Ω in place of the T4,
GZ34 and choke section. Those parameters fit the warmed powered ngspice
deck at 55, 60 and 65 mA drawn through R44. The 60 µF filter, R44, local
decoupling, R43 and 33 Ω lit-state 0B2 resistance preserve load-dependent sag
and regulation. This model represents the warmed supply and averages out
rectifier ripple and turn-on behavior.

```sh
dotnet research/176/livespice/bin/Release/net8.0/LiveSpice176.dll 1 full ratio=2
dotnet research/176/livespice/bin/Release/net8.0/LiveSpice176.dll 1 full ratio=2 powered
dotnet research/176/livespice/bin/Release/net8.0/LiveSpice176.dll 1 full ratio=4 off
```

Use `level` for matched-source dBu, `input-db` and `output-db` for pad
attenuation, and `input-vernier`, `output-vernier`, `attack` and `release`
for normalized settings. Ratio accepts 2, 4, 8 or 12. Setpoints are fixed per
bench run. The grid diodes use fixed zero-bias capacitance as in the
front-end fixture. The test reports final steady-state voltages and audio
after 5.2 seconds of simulation, and times processing separately from
LiveSPICE equation generation and first-block compilation.

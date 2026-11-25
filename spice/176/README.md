# UA 176 input stage

This is the first SPICE bench for the original Universal Audio 176 project.
It covers the input terminals, source impedance, attenuator, and T1 up to
the two 6BC8 grids. This bench uses the factory 176T input variant so the
600 Ω T-pad can be modeled as individual resistive arms. The standard
176 switched 2 dB attenuator and 300 Ω vernier need their own transcription.

| Model element | Source and scope |
| --- | --- |
| Input and T1 | UA 176 circuit drawing identifies the position of the input attenuator and T1. The 176T variant uses a 600 Ω input T-pad. The bench uses a balanced equivalent with identical differential behavior for a balanced source and load. |
| UTC O-1 candidate | A secondary hardware source identifies the O-1 in an O-17 shield. UTC gives a nominal 500/600 Ω primary and 50 kΩ secondary, a 1:10 full-winding turns ratio, and 52 Ω / 3.9 kΩ nominal winding DCR. These impedance designations and the turns ratio are separate specifications. |
| T1 magnetics and parasitics | The primary magnetizing inductance of 3.3 H is an initial estimate anchored to the low-frequency catalog response with 600 Ω source and 50 kΩ termination. Leakage inductance, core loss, secondary capacitance and interwinding capacitance are provisional. The UTC bandwidth tolerance provides a sanity check. Hardware phase and level-dependent core behavior require measurements. |
| 6BC8 loading | The sweep compares a 50 kΩ differential resistor load with two biased Sylvania cells, their grid diodes, and their interelectrode capacitances. The latter uses two 25 kΩ grid returns, a 120 V source, a 4.7 kΩ plate resistor per half, and fixed cathode voltages as a bench fixture. The UA 6BC8 bias, balance network and interstage loading belong in the next circuit section. |

## Running the bench

From the repository root with ngspice, NumPy and Matplotlib installed:

```sh
python3 scripts/ua176_input.py --output /tmp/ua176-input
```

The AC CSV gives complex input impedance, transfer from the source to both
windings, and the transfer directly across T1. Sweeps cover two source
impedances, three pad settings and both output loads. The transient CSV
gives K2–K20 and the root-sum-square of those harmonics at the transformer
primary, differential secondary and positive grid. Source dBu means the
voltage a 600 Ω source would deliver into a matched termination. Each
harmonic spectrum is normalized to its own fundamental. Harmonics above
22.05 kHz are offline SPICE results. The THD in this bench represents the
modeled grid load and reflected loading with a linear magnetic core.

## Sources

- [Universal Audio Model 176 schematic](https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Universal-Audio-176-Compressor-Schematic.pdf)
- [UA 176 standard, T and TT input attenuators](https://analogvibes.com/wp-content/uploads/The-3-Stages-Of-Mojo.pdf)
- [UTC O-series winding and bandwidth data](https://www.ka-electronics.com/images/pdf/UTC_Transformer_Catalog_1974.pdf)
- [UTC O-series frequency-response curves](https://flurrydynelabs.com/Transformer%20Catalogs/UTC_Transformers_Catalog_1962-1963.pdf)
- [Reported UA 176 transformer identification](https://www.tapatalk.com/groups/prorecordingworkshop/what-transformers-are-in-the-ua-176-t4052333.html)

## C-10476 6BC8 and interstage bench

`gain.cir` extends the input path through the normal-position gain ladder,
the dual 6BC8, the cathode and plate balance networks, and the interstage
transformer. The 176T attenuator is carried forward at the front end.

- T1 uses 2.0 mH primary-referred leakage and 92 pF directly across the entire
  secondary. The AC matrix crosses 1.5, 2.0 and 2.5 mH with 84, 92 and 100 pF.
  These winding values are working estimates. The interwinding paths are
  separate from the differential self-capacitance.
- R2–R5 form the two 10 kΩ / 6.8 kΩ sections around the control-voltage node.
  HI selects the ends of T1 and LO selects the divider taps. The bench holds
  the test switch in normal operation and uses an external fixed voltage at C5.
- The 0B2 rail is represented by its nominal 108 V DC boundary. R12 and R14
  feed the two plates through the R13 balance pot. The T2 high-impedance winding
  is connected across the plates. The cathode return includes R6–R11, with the
  GR meter tap unloaded.
- T2 is a reverse UTC A-19 candidate. The nominal 80 kΩ to 15 kΩ ratio and
  2.9 kΩ winding resistance on the 15 kΩ side come from catalog data.
  The 4 kΩ resistance of the high-impedance winding, 240 H magnetizing branch,
  100 mH high-side leakage, loss and capacitances are provisional. The 20 kΩ
  resistor on each side and 7 pF per grid form the differential load fixture.
  The active following amplifier and its feedback will set the final load.

Run the full DC, AC and transient bench from the repository root:

```sh
python3 scripts/ua176_gain.py --output /tmp/ua176-gain
```

The AC CSV reports source-referenced magnitude and phase at the T1 winding,
6BC8 grids, 6BC8 plates and T2 secondary. Phase uses grid P minus N, plate
B minus A and T2 N minus P, with the last two oriented for midband comparison.
The fixed-CV gain changes describe the gain cell's sensitivity. The rectifier
and timing loop are outside this bench. T1 and T2 magnetic cores are linear,
so the transient sweep captures tube distortion and nonlinear grid loading.

- [UA 176 C-10476 schematic](https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Universal-Audio-176-Compressor-Schematic.pdf)
- [UTC A-19 winding data](https://www.ka-electronics.com/images/pdf/UTC_Transformer_Catalog_1974.pdf)
- [Reported A-19 orientation in the UA circuit](https://groupdiy.com/threads/universal-audio-ua-176-limiting-amplifier.56531/page-2)

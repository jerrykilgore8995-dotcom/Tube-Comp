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
- T2 follows the reverse UTC A-19 wiring. The 80 kΩ G–G winding goes
  across the plates with F–F strapped, and the 15 kΩ P–B winding feeds the
  next stage. The 2.3:1 step-down and 2.9 kΩ P–B DCR are catalog values.
  The 346 H and 10 kΩ G–G DCR are from one A-19 measured at 100 Hz.
  High-side leakage, core loss and capacitances remain estimates. The active
  next amplifier will replace the two 20 kΩ / 7 pF grid-load branches.

| Circuit item | Implementation |
| --- | --- |
| Input attenuator and R1 | Continuously variable 600 Ω 176T pad. The 300 Ω linear series vernier shown on the standard 176 is available through `VERNIER` (0–1). The stepped contact bank still needs a separate transcription. |
| R2–R5 and HI/LO | Two 10 kΩ / 6.8 kΩ ladder arms and direct connections from the selector to the 6BC8 grids. |
| R6, R7, R8 and R9 | 100 Ω cathode-balance pot, two 47 Ω shunts and 330 Ω cathode return. |
| R10 and R11 | 1 kΩ meter-zero pot and 330 Ω return. Sweep the wiper voltage with the meter tap unloaded. |
| R12–R14 | 4.7 kΩ feeds and a 1 kΩ plate-balance pot with its wiper on the regulated rail. |
| C5, S3, R31 and S4 | 0.1 µF at the control node. Normal test-switch operation is represented by its signal-path connections. The 47 kΩ R31 test injection and limiter switch and timing circuit are outside this bench. |
| 108 V rail and C10 | Nominal DC source and 0.1 µF bypass. The 0B2 and its feed network belong to the power-supply model. |
| T2 exit | R15/R17 grid-load fixture. The active amplifier bias and feedback components belong to the next stage. |

Run the DC, AC and +4 dBu/1 kHz reference bench from the repository root:

```sh
python3 scripts/ua176_gain.py --output /tmp/ua176-gain
```

Add `--drive-sweep` for the extended overload and frequency matrix. The
reference tone uses a 600 Ω source calibrated to deliver +4 dBu into 600 Ω.
The transient CSV gives the level at the actual input terminals. The passive
T2 bench checks both orientations and the trim bench checks three positions
of each pot. The reverse grid fixture uses an 8 kΩ source as a comparison
case alongside the active 6BC8 bench.

The AC CSV reports source-referenced magnitude and phase at the T1 winding,
6BC8 grids, 6BC8 plates and T2 secondary. Phase uses grid P minus N, plate
B minus A and T2 N minus P, with the last two oriented for midband comparison.
The fixed-CV gain changes describe the gain cell's sensitivity. The rectifier
and timing loop are outside this bench. T1 and T2 magnetic cores are linear,
so the transient sweep captures tube distortion and nonlinear grid loading.

- [UA 176 C-10476 schematic](https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Universal-Audio-176-Compressor-Schematic.pdf)
- [UTC A-19 winding data](https://www.ka-electronics.com/images/pdf/UTC_Transformer_Catalog_1974.pdf)
- [Reported A-19 orientation in the UA circuit](https://groupdiy.com/threads/universal-audio-ua-176-limiting-amplifier.56531/page-2)

- [UTC A-19 terminal arrangement and winding drawing P](https://www.barryrudolph.com/recall/manuals/utc_transformer_pinouts.pdf)
- [Independent A-19 winding measurements](https://groupdiy.com/threads/audio-transformer-inductance.47515/page-5)

## V2, V3 and T3 audio path

`line.cir` uses the shared input through T2 and connects the active
amplifier in place of the grid-load fixture. The C-10476 scan used for
this bench labels V2 as 12AX7 and V3 as 12BH7. Both halves of each tube
use the respective supplied current equations and interelectrode
capacitances.

| Path | Connection in the deck |
| --- | --- |
| T2 and V2 bias | T2 ends connect to the V2 grids, with R15/R17 returning to ground. The V2 cathodes use R19/R20 and the shared R16 return. |
| V2 to V3 | R23/R24 feed the V2 plates from the decoupled V2 supply. C3/C4 are 0.1 µF each and couple each plate to its V3 grid. C-10476 specifies 0.047 µF. R27/R29 are the grid returns. |
| V3 cathodes | Both cathodes use the shared R28 return. |
| Feedback | R22/R18 and R25/R21 divide the corresponding V3 plate voltages. C1/C2 return the AC signal to the V2 cathodes. |
| Main B+ | The fixed 315 V bench source connects at T3 CT, C9B and R26, following the published correction to C-10476. R26 supplies the V2 rail, with C9A at the V2 end. |
| T3 and output | Series-aiding primary halves share a center tap. The main secondary uses the Sowter 1285's nominal 9:1 plate-to-plate ratio as a proxy. DCR is estimated from the replacement's published referred DCR. Magnetizing inductance, leakage and capacitance are provisional. R30 is modeled as a series vernier. The 600 Ω load is connected at the bench output with the stepped attenuator at its zero-loss reference. |

The T3 ratio-selector taps, detector, timing network and rectified supply are
outside the audio-path bench. The transformer cores are linear. The DC
results are for the selected Koren fits and fixed bench rails. Published
scans in this circuit family disagree on the V2 plate-voltage annotation.
The sweep reports the simulated voltage without fitting to that annotation.

```sh
python3 scripts/ua176_line.py --output /tmp/ua176-line
```

The line bench measures gain and phase from the input terminals. It
also checks the closed-feedback stage against a diagnostic open-feedback
case and measures THD at +4 dBu, 1 kHz with 20, 30 and 40 dB of input
attenuation.

- [UA 176 C-10476 schematic](https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Universal-Audio-176-Compressor-Schematic.pdf)
- [C-10476 supply-junction correction](https://groupdiy.com/threads/ua-universal-audio.45039/)
- [Sowter 1285 transformer data](https://www.sowter.co.uk/acatalog/VINTAGE-COMPRESSOR-TRANSFORMERS-J-to-Z.html)
- [Published V2 voltage-annotation differences in the UA 175 drawings](https://groupdiy.com/threads/united-audio-175-different-schematic-versions.86286/)

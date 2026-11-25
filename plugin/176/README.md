# 176 SPICE VST3

This experimental target flattens and embeds `spice/176/powered.cir`. The
plug-in loads the ngspice shared library and runs a continuous transient
simulation. External voltage sources carry audio and front-panel settings
into each channel's independent circuit. Audio samples are interpolated at
host sample times from accepted SPICE steps. The control-node voltage drives
the gain-reduction meter through a provisional scale.

- Full scale represents a +18 dBu sine into the nominal input termination.
- The power supply runs with an equivalent center-tapped T4 voltage source,
  warm GZ34 and lit-state 0B2 fits, choke, filters and circuit loads.
- The embedded 6AL5 uses a monotone numerical inversion of the submitted
  diode I/V law at 27 °C. It is evaluated as an ngspice device inside the
  original capacitor and resistor network.
- Input and output controls electronically set 600 Ω pads in 2 dB steps,
  with separate series vernier controls. The stepped hardware contact bank,
  service-test switch and physical meter circuit still need transcription.
- T3 uses the existing winding proxy and measured replacement tap fractions.
- The two 0.1 µF coupling caps stay in the audio path.
- Click the VU window to cycle GR, input and output. Click the Attack legend
  to switch Limiting Action. Use the far-right power switch for delayed bypass.

The first prototype reports 8,192 samples of latency. Offline render allows
the solver to catch up. Live processing uses delayed dry audio when the solver
misses a deadline. Do not assume playback speed on the target DAW until it has
been measured. ngspice retains transient vectors during the analysis, so this
version is intended for short research renders. Each instance needs its
bundled runtime in `Contents/Resources/ngspice`. The source target and smoke
runner build with CMake and JUCE. The Windows packaging job pins the official
44.2 shared-library release.

The editor artwork was drawn from a front-facing photograph of a physical
176. The rack, labels, analog meter, handles and jacks are original vector
artwork, with manufacturer marks omitted. The artwork preview can be regenerated
using `python draw_panel.py preview.svg --preview`.

- [Hardware front-panel reference](https://analogvibes.com/wp-content/uploads/The-3-Stages-Of-Mojo.pdf)
- [UA C-10476 schematic](https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Universal-Audio-176-Compressor-Schematic.pdf)
- [GZ34 curve reference](https://hificomponents.ru/f/gz34_mullard_1.pdf)
- [ngspice shared solver API](https://ngspice.sourceforge.io/shared.html)

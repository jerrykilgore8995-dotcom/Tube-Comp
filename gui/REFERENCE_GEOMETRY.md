# Faceplate reconstruction reference lock

These are **JUCE-rendered, hardware-photo traced faceplates**—not AI-generated raster concepts, and not a third-party image pasted behind generic controls.

## RS124 front-panel master

- Chandler Limited RS124 original straight-on photograph:
  https://kmraudio.de/chandler-limited-rs124/
- Official front-panel mechanical/control reference, with printed meter and stepped selector legends:
  https://chandlerlimited.com/wp-content/uploads/2015/10/Chandler_Limited_RS124_Compressor_Manual.pdf
- The original printed **Chandler**, **EMI**, and third-party logo blocks have been replaced with **RS124 / TUBE COMP**. Physical control types, ordering, and meter type are preserved.

Front panel painted 1574 x 283 pixels in a 1600 x 350 captured component. Coordinates below are in JUCE editor pixels.

| Part | Location | Source-locked physical style |
|---|---|---|
| rack mounting slots | x31/1569, y43/279 | elongated stainless-rim rack slots |
| balance trimmer | (159,81) | black screw adjustment |
| BAL pushbutton | (169,205) | silver round momentary |
| input control | (356,166) | large tapered black pointer / molded grip |
| 9-position attack | (617,105) | small phenolic pointer with stepped numeral ring |
| 11-position recovery/HOLD | (617,217) | small phenolic pointer, alternate HOLD markings |
| meter | rectangle x744..1001, y66..237 | inset black rectangular vintage VU case, white dB compression dial |
| output attenuator | (1119,169) | large black tapered pointer knob |
| red pilot | (1294,101) | red jewel in metal bezel |
| fuse/superfuse location | (1421,101) | black fuse cap |
| power switch | (1406,225) | period metal switch |
| right identity | x1453..1571 | TYPE RS124 without copyrighted vendor logo |

*Mechanical scale caveat:* inferred from front-on photographic proportions, not certified manufacturing CAD tolerances. Pixel-for-pixel photographic equivalence depends on camera focal length, revision, illumination, and the original print font; this project follows photographed hardware geometry and hand-painted JUCE textures.

## A-436B factory front-panel master

- Factory 436B manual: https://www.manualslib.com/manual/4394475/Altec-436b.html
- Vintage chassis image and meter: https://www.retrosonic.shop/product/altec-436b-tube-mono-compressor
- Historical 436B manual schematic and factory descriptions:
  https://schematicsforfree.com/files/Audio/Products/Professional%20Audio/A/Altec%20Lansing/Altec%20436B%20Amplifier.pdf

The factory **436B** is a green hinged faceplate with only front-panel **INPUT gain, POWER, fuse, red pilot, and mechanical compression meter**. Its attack and release times are internally fixed. The front-panel release knob visible on some photographed surviving 436B units is an **aftermarket modification**, not factory stock. This is why the A-436B mode has no visible release/threshold/output rotary knobs.

| Part | Location | Original-type treatment |
|---|---|---|
| factory green panel | 1574 x 283 | dark olive-gray green matte enamel |
| input/volume | (234,181) | black tapered pointer, 0..10 marks |
| model identity replacing manufacturer | (390,105) | A-436B in cream lettering |
| model-6049 type meter | x669..904, y59..254 | round-topped black Bakelite meter housing with lower screw |
| second model / function legend | x967..1148 | 436B COMPRESSOR AMPLIFIER |
| power jewel | (1218,111) | red glass lamp in metal |
| power rotary | (1224,211) | small black fluted hardware switch |
| fuse cap | (1430,133) | black original-style fuse holder |
| lower circuit chooser | y309..339 | **outside** rack faceplate; host GUI only |

### Verification procedure

GitHub Actions > `Capture Hardware Faceplates in JUCE` compiles real JUCE 8 code on Windows and captures both panels at 1600 x 350 pixels. It fails rather than uploading a placeholder if capture has incorrect dimensions or the PNG is missing. Inspect both screenshots for visible overlapping labels before treating them as approved.

**Project status:** front-panel component previews. The original 436B and reconstructed RS124 SPICE/DSP VST3 remain separate research tasks; these interface files do not constitute a compiled audio plugin.

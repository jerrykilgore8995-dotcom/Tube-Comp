# Source / evidence register

| Item | Source | What is document-backed? | What is NOT established? |
|---|---|---|---|
| Altec 436B | Altec 436B operating instructions, drawing number 13022-1; https://schematicsforfree.com/files/Audio/Products/Professional%20Audio/A/Altec%20Lansing/Altec%20436B%20Amplifier.pdf | 6BC8, 6CG7, 6AL5; 50ms attack; 1 second 63% release; fixed threshold; 30dB maximum GR; parts values | Full confirmed connectivity (source scan presently difficult to fetch); calibrated tube transfer curves |
| Altec 436B supplemental | https://www.manualslib.com/manual/4394475/Altec-436b.html | Same factory specs and descriptive text | Schematic wiring, measurement tolerances |
| Altec 436A technical letter | https://retronik.silicium.org/DOCUMENTS/Audiovideo/Altec-Lansing/Altec-Lansing-436-A-Schematic.pdf | Closely related 6BC8/6CG7/6AL5 architecture and Altec modifications | **NOT automatically identical to 436B** |
| Altec 436C | https://aboutprosound.com/wp-content/uploads/2022/10/Altec-436C.pdf | Revised documented topology, component values and 20dB-GR node voltages | Not a replacement for 436B source |
| Abbey Road account | https://www.abbeyroad.com/news/rs124-compressor-gearthatmadeus-3174 | EMI modifications of **436B**, individual serials differed | Full original circuit values; the exact Chandler circuit |
| Chandler manual | https://www.bhphotovideo.com/lit_files/896687.pdf | 9 attack settings, 11 recovery/HOLD settings, balancing procedure, 200/600 output, SuperFuse behavior described | Internal proprietary SuperFuse circuit and secret individual schematics |
| Tube model theory | https://www.normankoren.com/Audio/Tubemodelinstr.HTM | SPICE device modeling method and known model uncertainty | Exact 6BC8 device-to-device scatter |
| RS124 caution | https://www.preservationsound.com/original-emi-modified-altec-compressor-on-ebay/ | Multiple historically circulating schematics with disputed authenticity | That any internet schematic is a certified Abbey Road production drawing |

**Rule:** numerical values taken from an original manual use VERIFIED_SPEC. Guessed topology, devices and coefficients use EXPERIMENTAL. Calibrated values only after measured validation.

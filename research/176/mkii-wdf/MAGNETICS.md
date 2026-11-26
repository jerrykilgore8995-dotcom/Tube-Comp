# 176 MK II — transformer nonlinear magnetics experiment

This branch preserves the previously validated linear-magnetic WDF reference and adds three nonlinear magnetizing current ports, one per audio transformer. The excess current is solved in the same coupled nonlinear wave-domain system as the tube stages and detector. Trial evaluations do not advance magnetic state; accepted time steps update flux linkage, play-memory hysteresis states and loss bookkeeping.

## Electrical anchors and provisional magnetic parameters

- T1 UTC O-1: 1:10, 52/3900 ohms winding DCR, nominal 3.3 H in existing model; 600 ohm primary catalog rating +8 dBm, response 30–20 kHz (1974 UTC catalog). The catalog level is converted to a provisional 50 Hz flux anchor, not claimed as a measured knee.
- T2 UTC A-19 in reverse: 80k:15k, 2.3:1, 346 H measured high winding in repository, existing winding DCR; +15 dBm catalog level in original application converted from 15k winding to high winding as a provisional 50 Hz anchor.
- T3 Sowter 1285 replacement proxy: 9:1 CT, 15k/185 ohm, 50% Mumetal core, +28 dBu output at 50 Hz, 14 ohms total secondary-referred DCR. Existing winding/leakage/coupling values preserved; 250 H full-primary linear magnetizing term. Output rating used to estimate a primary flux anchor.

The model uses a monotone odd fifth-order excess magnetizing current, three passive play/stop hysteresis elements and optional linear eddy loss. Existing linear winding/core losses retained. No measured B-H curves, coercivity, core geometry, or per-transformer distortion sweeps were found; the three initially share the same normalized curve coefficients and differ by inductance and flux anchor. Extra eddy-loss conductance defaults to zero to avoid double counting existing linear loss. These are explicitly engineering hypotheses, NOT fitted device-specific hysteresis or measured harmonic profiles. T3 uses a primary end-to-end excess branch in parallel with the existing coupled windings; leakage-flux contributions and DC flux distribution need further validation. The power transformer remains the original secondary-equivalent source. No claim of hardware accuracy or native real-time VST3.

Sources:
- https://www.ka-electronics.com/images/pdf/UTC_Transformer_Catalog_1974.pdf
- https://technicalaudio.net/pdf/UTC_United_Transformer_Corp%2BTRW%2BOPT/UTC_transformer_data_table.pdf
- https://www.sowter.co.uk/acatalog/VINTAGE-COMPRESSOR-TRANSFORMERS-J-to-Z.html
- https://www.dafx.de/paper-archive/details/S76rs5EkqeCL6m2FH0TDaQ
- https://link.springer.com/article/10.1186/s13636-023-00277-8

Tests compare nonlinear and original linear cores with identical circuit controls and mismatches. Output spectra measure complete signal-chain effects; winding spectra and nonlinear excitation current spectra identify where the change originates. Hysteresis energy is a model-internal diagnostic, not directly measured physical loss. Compare high-drive LF against low-drive LF and HF, and 600 ohm against 10k loads. No arbitrary post-transformer harmonic generators are used.

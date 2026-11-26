# 176 MK II — offline WDF engineering prototype

Source: the powered SPICE circuit from ua176-input-stage. The original plugin is untouched.

The fixed-control wave engine transcribes the active input pad, T1, 6BC8 pair, balance networks, T2, 12AX7/12BH7 stages and feedback, coupled T3 windings and reflected tap currents, 6AL5 detector, timing network, GZ34 law, 60 Hz supply, choke and 0B2 regulator. The wave adaptor is constructed once per configuration; per sample, only the coupled nonlinear device ports are solved. Capacitors and coupled inductors use trapezoidal wave memories. The mismatched SPICE deck and manifest are the first parity reference.

Known limitations: transformer cores remain linear as in source. The two grid diode types currently use constant zero-bias capacitances and omit transit charge, rather than the full ngspice depletion/diffusion charge. Diode thermal voltage is fixed at 27 C; exponential evaluation is bounded to prevent overflow. The 6AL5 is an implicit current-law inversion including its shunt. Source-deck physical controls only; no dynamic host automation, HPF, mix, output pad, GUI or VST3 integration yet. The factory ratio graph is a separate acceptance target; the initial tap fractions are the powered-deck values, not assumed calibrated. The comparison uses only in-band harmonics, never claims an offline 20th harmonic above Nyquist. Do not interpret cold-start convergence as steady-state parity.

Run: node run-powered-wave.mjs 44100 2 3.474135043921514 wave.csv 1 1000 20
Run ngspice on powered-mismatch.cir in a separate output directory and compare with compare-powered.py wave.csv powered.txt. The CI workflow runs both and uploads logs and data even when a test fails.

No real-time build is provided by this prototype.

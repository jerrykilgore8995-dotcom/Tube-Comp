#!/usr/bin/env python3
"""Generate the 33tap / 16sample PDC complex 200-ohm physical-load FIR.

Fit retains the same causal 33-tap algorithm, but uses the fitted 6.7Hz
coupling poles rather than 600ohm's 9.7Hz, since secondary load changes
low-frequency response and the physical transformer load line.
"""
from pathlib import Path
import subprocess,sys
if len(sys.argv)!=3:raise SystemExit("Use measured_load200.csv output.inc")
src,out=map(Path,sys.argv[1:])
if not src.is_file():raise FileNotFoundError(src)
subprocess.run([sys.executable,"scripts/fit_spice_phase_fir.py",
                str(src),str(out),"6.7"],check=True)
s=out.read_text()
name="namespace tubecomp::phasefit {"
if s.count(name)!=1:raise RuntimeError("Unexpected FIR namespace")
s=s.replace(name,"namespace tubecomp::phasefit200 {")
s=s.replace("// 33 causal taps at 44.1kHz.",
            "// 200-ohm secondary: 33 causal taps at 44.1kHz.")
out.write_text(s)
print("Source-locked independent 200ohm FIR",out,len(s),flush=True)

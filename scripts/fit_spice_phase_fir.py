#!/usr/bin/env python3
"""Fit 73-tap causal DSP FIR to the actual ngspice 20Hz..20kHz transfer.

We first factor out the two 9.7Hz digital coupling high-pass stages that
already exist in the DSP. We fit the remaining complex gain/phase including
transformer response against independent SPICE .AC data by least squares.
This filter is NOT a physical circuit or real transformer core simulation.
It is a measured-behavior interpolation surface.
"""
from pathlib import Path
import csv,hashlib,math,sys
import numpy as np

if len(sys.argv)!=3:raise SystemExit("Use measured.csv output.inc")
inp=Path(sys.argv[1]);dest=Path(sys.argv[2])
with inp.open(newline="") as file:source=list(csv.DictReader(file))
FS=44100.
N=73
REGLAMBDA=.003
modes=[("A-436B","altec"),("RS124","rs")]
CVS=[0,6,12,15,17]
output=[
 "#pragma once",
 '#include <array>',
 "// Automatically fitted to independent ngspice .AC complex transfer.",
 "// 73 causal taps, 44.1kHz, six-gig? NO: 73 multiplications/sample/channel.",
 "// SPICE-to-FIR inband fit checked against EACH measured AC sample.",
 f"// SPICE CSV SHA256: {hashlib.sha256(inp.read_bytes()).hexdigest()}",
 "namespace tubecomp::phasefit {",
 "static constexpr int kLen=73;",
 "static constexpr std::array<float,5> cv={{0.f,6.f,12.f,15.f,17.f}};",
]
for mode,short in modes:
 output.append(f"static constexpr std::array<std::array<float,{N}>,5> {short} {{{{")
 for cv in CVS:
  rows=sorted([r for r in source if r["mode"]==mode and
               float(r["cv_V"])==-float(cv)],key=lambda r:float(r["frequency_hz"]))
  if len(rows)<100:raise RuntimeError("Insufficient independently measured SPICE "+mode+" "+str(cv))
  f=np.array([float(x["frequency_hz"]) for x in rows])
  omega=2*np.pi*f/FS
  mag=np.array([float(x["relative_gain_dB"]) for x in rows])
  phase=np.array([float(x["phase_deg"]) for x in rows])
  desired=10**(mag/20)*np.exp(1j*np.deg2rad(phase+180.))
  alpha=np.exp(-2*np.pi*9.7/FS)
  z=np.exp(-1j*omega)
  highpass=(alpha*(1-z)/(1-alpha*z))**2
  residual=desired/highpass
  freqmat=np.exp(-1j*np.outer(omega,np.arange(N)))
  mat=np.vstack([freqmat.real,freqmat.imag])
  vec=np.r_[residual.real,residual.imag]
  regularized=np.vstack([mat,np.sqrt(REGLAMBDA)*np.eye(N)])
  vals=np.r_[vec,np.zeros(N)]
  taps=np.linalg.lstsq(regularized,vals,rcond=None)[0]
  ratio=(freqmat@taps)/residual
  amplitude=100*max(abs(abs(ratio)-1))
  phaseerr=max(abs(np.rad2deg(np.angle(ratio))))
  if amplitude>3. or phaseerr>2.:
   raise RuntimeError(f"Reference fit FAILED {mode} {cv}: amplitude {amplitude}%, phase {phaseerr}deg")
  print(f"FIR FIT {mode} CV={-cv}V maxAmp={amplitude:.2f}% "
        f"maxPhase={phaseerr:.2f}deg L1={sum(abs(taps)):.2f}",flush=True)
  def fmt(val):
   val=float(val)
   s=f"{val:.10g}"
   if "." not in s and "e" not in s:s+="."
   return s+"f"
  output.append("  {{"+",".join(fmt(t) for t in taps)+"}},")
 output.append("}};")
output.append("} // namespace tubecomp::phasefit")
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text("\n".join(output)+"\n")
print("Wrote",dest,"bytes",dest.stat().st_size)

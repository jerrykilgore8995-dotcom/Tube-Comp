#!/usr/bin/env python3
"""Fail-closed independent 66-point SPICE vs native-DSP input-pot comparison.

Both source files are real measured waveforms: 1.6s ngspice and 1.6s
44.1kHz C++ Engine output. Do not conflate a test with comprehensive
coverage over continuous knob positions, frequencies or timing.
"""
import argparse,csv,json,math
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--spice-altec",required=True)
p.add_argument("--spice-rs124",required=True)
p.add_argument("--dsp",required=True)
p.add_argument("--output",default="output/input_grid_parity")
p.add_argument("--max-error",type=float,default=8.)
a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
def read(path):
 with open(path,newline="") as fp:return list(csv.DictReader(fp))
refs=read(a.spice_altec)+read(a.spice_rs124)
dsp=read(a.dsp)
key=lambda r:(r["mode"],int(float(r["input_pct"])),int(float(r["input_dBu"])))
reference={key(r):r for r in refs}
actual={key(r):r for r in dsp}
if len(refs)!=66 or len(dsp)!=66 or len(reference)!=66 or len(actual)!=66:
 raise RuntimeError(f"Incomplete or duplicated grid: SPICE {len(refs)}/{len(reference)}, DSP {len(dsp)}/{len(actual)}; expected exactly 66.")
results=[]
problems=[]
for k in sorted(reference):
 r=reference[k]
 if k not in actual:raise RuntimeError(f"Missing actual rendering for {k}")
 d=actual[k]
 if r["status"]!="OK":raise RuntimeError(f"Unmeasured SPICE case {k}: {r['status']}")
 src=float(r["final_output_dBu"])
 test=float(d["final_output_dBu"])
 def check(name,ref,obs,kind):
  if kind=="gain":err=abs(10**((obs-ref)/20)-1)*100
  elif kind=="positive":err=100*abs(obs-ref)/max(.02,abs(ref))
  elif kind=="harmonic":err=abs(10**((obs-ref)/20)-1)*100
  else:raise RuntimeError(kind)
  passed=math.isfinite(err) and err<=a.max_error
  v=dict(mode=k[0],input_pct=k[1],input_dBu=k[2],metric=name,
         reference=ref,dsp=obs,error_pct=round(err,4),passed=passed)
  results.append(v)
  if not passed:problems.append(v)
 # For a near-muted input, compare absolute audio-voltage error instead of
 # claiming 8% relative THD precision on SPICE's numerical floor.
 if src < -65:
  audio_ref=10**((src-22)/20)
  audio_obs=10**((test-22)/20)
  abs_err=abs(audio_obs-audio_ref)
  passed=abs_err<10**((-65-22)/20)*.08
  v=dict(mode=k[0],input_pct=k[1],input_dBu=k[2],
    metric="near_mute_absolute_peak",reference=audio_ref,
    dsp=audio_obs,error_pct=abs_err/max(1e-30,abs(audio_ref))*100,
    passed=passed)
  results.append(v)
  if not passed:problems.append(v)
  continue
 check("output_amplitude",src,test,"gain")
 check("total_THD",float(r["final_THD_percent"]),
       float(d["final_THD_percent"]),"positive")
 # At the 0% input hard stop, SPICE's detector node is only 1–6mV,
 # sometimes positive rather than a gain-reduction voltage. A relative
 # error percentage about zero is mathematically ill-conditioned. Require
 # the DSP's DC control voltage to remain within a strict 50mV absolute
 # operating-point tolerance at this effectively muted setting.
 refCv=abs(float(r["final_CV_V"]))
 dspCv=float(d["CV_magnitude_V"])
 if refCv<.05:
  diff=abs(dspCv-refCv)
  v=dict(mode=k[0],input_pct=k[1],input_dBu=k[2],
   metric="near_zero_CV_absolute_50mV",reference=refCv,dsp=dspCv,
   error_pct=100.*diff,passed=diff<=.05)
  results.append(v)
  if not v["passed"]:problems.append(v)
 else:
  check("control_voltage",refCv,dspCv,"positive")
 for harmonic in range(2,21):
  name=f"K{harmonic}_dBc"
  if not r[name] or not d[name]:continue
  ref=float(r[name]);obs=float(d[name])
  if ref <=-85:continue
  check(name,ref,obs,"harmonic")
cols=["mode","input_pct","input_dBu","metric","reference","dsp","error_pct","passed"]
with (out/"all_metrics.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=cols)
 w.writeheader();w.writerows(results)
summary=dict(reference_cases=len(reference),render_cases=len(actual),
 tested_metrics=len(results),failed_metrics=len(problems),
 max_error_pct=max(v["error_pct"] for v in results),
 failed_by_input_pct={
   str(knob):sum(not x["passed"] for x in results if x["input_pct"]==knob)
   for knob in range(0,101,10)},
 failures=sorted(problems,key=lambda x:x["error_pct"],reverse=True)[:100],
 status="PASS_GRID_ONLY" if not problems else "FAIL_UNCALIBRATED")
(out/"summary.json").write_text(json.dumps(summary,indent=2))
print("INPUT GRID PARITY",json.dumps({k:v for k,v in summary.items()
    if k!="failures"},indent=2))
for r in summary["failures"][:30]:print("INPUT_GRID_MISMATCH",r)
if problems:raise SystemExit(1)

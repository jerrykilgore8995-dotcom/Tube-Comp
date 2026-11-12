#!/usr/bin/env python3
"""Validate independent SPICE complex .AC transfer vs 44.1kHz native DSP.

8% complex phasor error means amp error <=8% and angular error <=~4.59deg;
the row report preserves each metric separately. Group delay is measured by
the ±25Hz 1k native data and compared to SPICE differential group delay.
The exact 16-sample FIR delay IS reported to JUCE via setLatencySamples,\nso the frequency-domain comparison de-embeds only that host-compensated delay.
"""
import csv,json,math,sys
from pathlib import Path
import numpy as np
if len(sys.argv)!=4:raise SystemExit("Use ref.csv dsp.csv out-dir")
ref,got,dest=map(Path,sys.argv[1:])
dest.mkdir(parents=True,exist_ok=True)
def load(p):
 with p.open(newline="") as f:return list(csv.DictReader(f))
refs=load(ref);dsp=load(got)
groups={}
for row in refs:
 key=(row["mode"],float(row["cv_V"]))
 groups.setdefault(key,[]).append(row)
rows=[];failed=[]
for dr in dsp:
 key=(dr["mode"],float(dr["cv_V"]))
 r=sorted(groups[key],key=lambda p:float(p["frequency_hz"]))
 hz=float(dr["frequency_hz"])
 xs=np.log([float(v["frequency_hz"]) for v in r])
 v=np.log(hz)
 mag=np.interp(v,xs,[float(p["relative_gain_dB"]) for p in r])
 ph=np.interp(v,xs,[float(p["phase_deg"]) for p in r])
 dspmag=float(dr["relative_gain_dB"])
 # De-embed the host-reported 16-sample FIR latency, precisely.
 dspph=float(dr["phase_deg"])+360.*hz*16./44100.
 errAmp=100*abs(10**((dspmag-mag)/20)-1)
 errPhase=abs((dspph-ph+180)%360-180)
 errPhasePct=200*math.sin(math.radians(errPhase)*.5)*100/2 # 100*2*sin(delta/2)
 # Correct expression explicitly, to avoid a hidden 2x mistake.
 errPhasePct=200*math.sin(math.radians(errPhase)*.5)
 row=dict(mode=key[0],CV=key[1],Hz=hz,
  reference_mag_db=mag,dsp_mag_db=dspmag,
  amplitude_error_pct=errAmp,
  reference_phase_deg=ph,dsp_phase_deg=dspph,
  phase_error_deg=errPhase,complex_phase_error_pct=errPhasePct)
 row["passed"]=errAmp<=8. and errPhasePct<=8.
 rows.append(row)
 if not row["passed"]:failed.append(row)
# Report a 1k small-signal group delay for context. Numeric derivative
# is sensitive to interpolation; never silently suppress it.
delays=[]
for (mode,cv) in groups:
 filt={float(r["frequency_hz"]):r for r in dsp
             if r["mode"]==mode and float(r["cv_V"])==cv}
 if 975. not in filt or 1025. not in filt:continue
 a=float(filt[975.]["phase_deg"])+360.*975.*16./44100.
 b=float(filt[1025.]["phase_deg"])+360.*1025.*16./44100.
 phaseDelta=(b-a+180)%360-180
 measured=-phaseDelta/(360.*50)*1000
 r=groups[(mode,cv)]
 hzdata=np.array([float(x["frequency_hz"]) for x in r])
 expected=float(r[np.argmin(np.abs(hzdata-1000.))]["group_delay_ms"])
 error=100*abs(measured-expected)/max(0.002,abs(expected))
 delays.append(dict(mode=mode,CV=cv,expected_delay_ms=expected,
                    measured_delay_ms=measured,error_pct=error,
                    passed=error<=8.))
 if error>8:failed.append({"group_delay":delays[-1]})
with (dest/"phase_frequency_parity.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
with (dest/"group_delay_parity.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(delays[0]));w.writeheader();w.writerows(delays)
summary=dict(cases=len(rows),failure_count=len(failed),
  worst_amplitude_error_pct=max(x["amplitude_error_pct"] for x in rows),
  worst_phase_error_deg=max(x["phase_error_deg"] for x in rows),
  worst_group_delay_error_pct=max(x["error_pct"] for x in delays),
  failures=failed[:50])
(dest/"fr_phase_summary.json").write_text(json.dumps(summary,indent=2,
 default=lambda x:x.item() if hasattr(x,"item") else str(x)))
print("FR_PHASE_SUMMARY",json.dumps({k:v for k,v in summary.items()
                                       if k!="failures"},default=lambda x:x.item() if hasattr(x,"item") else str(x)),flush=True)
for x in failed[:25]:print("FR_PHASE_FAILURE",x,flush=True)
if failed:raise SystemExit(1)
print("PASS 8% frequency, complex-phase and group-delay comparisons")

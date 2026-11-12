#!/usr/bin/env python3
"""Generate *source-locked* input pot curves from real 1.6-second SPICE runs.

Keeps all 11 input positions at -18/-6/+7 dBu, both modes and all K2-K20.
Never write algorithm-generated fake SPICE "measurements".
Use:
  python3 scripts/generate_spice_input_grid.py altec.csv rs.csv \
     plugin/SpiceInputGridMeasured.inc
"""
import csv,hashlib,sys,math
from pathlib import Path

if len(sys.argv)!=4:raise SystemExit("Need Altec CSV, RS CSV, output .inc")
inputs=[
 ("measuredAltecInput",Path(sys.argv[1]),"A-436B"),
 ("measuredRSInput",Path(sys.argv[2]),"RS124"),
]
def fmt(v):
 x=float(v)
 if not math.isfinite(x):raise ValueError("Invalid SPICE input-grid datum")
 s=f"{x:.10g}"
 if "." not in s and "e" not in s:s+="."
 return s+"f"
out=[
 "// Generated automatically from ngspice 1.6-second measured 11x3 input",
 "// knob curves. Each point is an actual sine-wave SPICE simulation.",
 "// These are 1kHz points, NOT an assertion of full-band calibration.",
]
for label,path,mode in inputs:
 out.append("// "+mode+" SHA256: "+hashlib.sha256(path.read_bytes()).hexdigest())
 with path.open(newline="") as fp:rows=list(csv.DictReader(fp))
 assert len(rows)==33 and all(r["mode"]==mode and r["status"]=="OK" for r in rows)
 out.append(f"static constexpr std::array<std::array<Point,3>,11> {label} {{{{")
 for knob in range(0,101,10):
  subset=sorted((r for r in rows if int(r["input_pct"])==knob),
                key=lambda r:float(r["input_dBu"]))
  assert len(subset)==3 and [int(float(r["input_dBu"])) for r in subset]==[-18,-6,7]
  out.append(" {{")
  for r in subset:
   spectrum=",".join(fmt(r[f"K{k}_dBc"]) for k in range(2,21))
   out.append("  {"+",".join([
     fmt(r["input_dBu"]),fmt(r["final_output_dBu"]),
     fmt(-float(r["final_CV_V"])),
     fmt(r["final_THD_percent"])])+",{"+spectrum+"}},")
  out.append(" }},")
 out.append("}};")
destination=Path(sys.argv[3]);destination.parent.mkdir(exist_ok=True,parents=True)
destination.write_text("\n".join(out)+"\n")
print(f"SPICE_INPUT_GRID_OUTPUT={destination} cases=66 all K2-K20")

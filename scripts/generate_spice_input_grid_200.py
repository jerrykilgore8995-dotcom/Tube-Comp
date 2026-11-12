#!/usr/bin/env python3
"""Generate 200Ω source-locked 11x3 input control tables.

Strict validation: the CSVs must be actual status=OK ngspice results with
load_ohms=200. Reuses the same coefficient generator as the 600Ω circuit.
"""
from pathlib import Path
import csv,subprocess,sys
if len(sys.argv)!=4:raise SystemExit("Use altec200.csv rs200.csv output.inc")
altec,rs,out=map(Path,sys.argv[1:])
for path in (altec,rs):
 with path.open(newline="") as fp:rows=list(csv.DictReader(fp))
 if len(rows)!=33 or any(
       r.get("load_ohms")!="200" or r.get("status")!="OK" for r in rows):
  raise RuntimeError("Source is not complete measured 200ohm SPICE: "+str(path))
subprocess.run([sys.executable,"scripts/generate_spice_input_grid.py",
               str(altec),str(rs),str(out)],check=True)
txt=out.read_text()
for old,new in (("measuredAltecInput","measuredAltec200Input"),
                ("measuredRSInput","measuredRS200Input")):
 if txt.count(old)!=1:raise RuntimeError("Unexpected generated symbol")
 txt=txt.replace(old,new)
txt=("// Each 200ohm point includes ACTUAL loaded transformer output,\n"
     "// unlike the 600ohm line-level post-attenuator.\n"+txt)
out.write_text(txt)
print("Generated separate 200Ω physically loaded reference",out,flush=True)

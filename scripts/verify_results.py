#!/usr/bin/env python3
"""Gate on finite simulation results, provenance and expected matrix coverage.

DOES NOT claim any measured acceptance against a historical hardware unit.
"""
from pathlib import Path
import csv,json,math,sys

root=Path(sys.argv[1] if len(sys.argv)>1 else "output")
manifest=json.loads((root/"manifest.json").read_text())
rows=json.loads((root/"cases.json").read_text())
assert manifest["provenance"]=="NGSPICE_EXPLORATORY (NOT HISTORICAL REFERENCE)"
assert len(rows)==manifest["total"]
assert manifest["total"]>=12, "At least two modes and levels should be measured"
assert all(r["status"]=="OK" for r in rows), "One or more actual ngspice simulations failed"
assert set(r["mode"] for r in rows)=={"A-436B","RS124"}
for r in rows:
    for key in ["fundamental_output_dBu","fundamental_peak_volts","THD_percent",
                "control_voltage_min_V","input_to_output_gain_dB"]:
        assert isinstance(r[key],(float,int)) and math.isfinite(r[key]),(r,key)
    assert r["fundamental_peak_volts"]>0,r
    assert r["THD_percent"]>=0,r
    assert len([key for key in r if key.startswith("K") and key.endswith("_dBc")])==20
    assert r["model_sha256"]==manifest["tube_model_sha256"]
    assert "op_nodes" in r and isinstance(r["op_nodes"],dict)
    for k in ["v(pa)","v(pb)","v(p2a)","v(p2b)","v(cv)"]:
        assert k in r["op_nodes"],f"DC node not recorded: {k}"
with (root/"measurement_table.csv").open(newline="") as f:
    assert len(list(csv.DictReader(f)))==len(rows)
print(f"PASS: {len(rows)} ngspice cases, K1-K20, DC voltages, provenance and CSV")
print("CALIBRATION STATUS: UNCALIBRATED; no historic factory target is asserted yet.")

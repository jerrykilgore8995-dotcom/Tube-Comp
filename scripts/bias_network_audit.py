#!/usr/bin/env python3
"""Compare all tube DC operating points with factory operating voltages.

This operates on an *exploratory* schematic and does not relabel the user's
372V / 150uF + 47uF supply selection as a factory 436B. The schematic gives
272V at the reservoir node at idle (436C); 290V is under compression
at a DIFFERENT downstream junction. Both cases are modeled independently.

No tube characteristics, resistors, or cabinet PSU caps are adjusted here.
Measurements are real ngspice .op for each candidate, not estimated.
"""
from pathlib import Path
import csv,subprocess,re,sys,tempfile
sys.path.insert(0,str(Path(__file__).parent))
from run_lab import prepare,op_nodes
ROOT=Path("output/bias_network")
ROOT.mkdir(exist_ok=True,parents=True)
rows=[]
for bplus in (272,290,310,330,372):
 for drop in (6800,9000,11000,15000,20000):
  for bias in (-0.5,-0.8,-1.0,-1.5):
   case=dict(mode="A-436B",hz=1000,dbu=-36,attack=0,recovery=0,hold=False,input_knob=0.8)
   d=ROOT/f"b{bplus}_r{drop}_g{str(bias).replace('-','m').replace('.','p')}"
   file=prepare(case,d)
   s=file.read_text()
   s=s.replace("BPLUS=372",f"BPLUS={bplus}")
   s=s.replace("R6CG7_DROP=11000",f"R6CG7_DROP={drop}")
   s=s.replace("BIAS=-0.8",f"BIAS={bias}")
   a=s.index("\n.control\n")
   b=s.index("\n.endc",a)
   s=s[:a]+"""
.control
op
print v(bplus) v(platefeed) v(pa) v(pb) v(k1) v(ga) v(gb) v(outputfeed) v(p2a) v(p2b) v(k2) v(cv)
quit""" + s[b:]
   file.write_text(s)
   p=subprocess.run(["ngspice","-b","case.cir"],cwd=d,text=True,capture_output=True,timeout=15)
   text=p.stdout+"\n"+p.stderr
   if p.returncode!=0:
      print("OPFAIL",bplus,drop,bias,text[-400:],flush=True)
      continue
   v=op_nodes(text)
   get=lambda x: v.get("v("+x+")",float("nan"))
   platefeed,plate,pa2,k2,ga,k1=get("platefeed"),get("pa"),get("p2a"),get("k2"),get("ga"),get("k1")
   i1=(platefeed-plate)/47000.
   i2=k2/220.
   row=dict(bplus_V=bplus,output_drop_ohm=drop,grid_offset_V=bias,
            platefeed_6bc8_V=platefeed,plate_6bc8_V=plate,
            cathode_6bc8_V=k1,grid_6bc8_V=ga,grid_cathode_6bc8_V=ga-k1,
            platefeed_6cg7_V=get("outputfeed"),plate_6cg7_V=pa2,
            cathode_6cg7_V=k2,plate_current_6bc8_mA=i1*1000,
            cathode_current_6cg7_total_mA=i2*1000,
            control_V=get("cv"))
   # Targets documented on Altec 436C sheet at *idle*, separate from
   # same drawing's oval compression values (there is no 372V annotation).
   row["bias_error_V"]=(abs(platefeed-210)+abs(plate-65)+
                         2*abs(pa2-150)+10*abs(k2-2.3))
   rows.append(row)
rows.sort(key=lambda x:x["bias_error_V"])
with (ROOT/"bias_network_100_points.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
for x in rows[:24]:
 print("BIAS_OP",x,flush=True)

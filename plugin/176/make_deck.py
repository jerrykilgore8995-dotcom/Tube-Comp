"""Flatten the measured SPICE deck and expose its physical controls to the host."""
import argparse
import math
from pathlib import Path
import re


def flatten(path):
    for line in path.read_text().splitlines():
        if line.startswith(".include "):
            yield from flatten(path.parent / line.split(maxsplit=1)[1])
        else:
            yield line


def detector_cell():
    # Invert the supplied monotonic 6AL5 equation at 27 C. This is a device
    # I/V table inside SPICE, with the charging network still solved by SPICE.
    points = [(-400., -66e-12), (-.867, 0.)]
    for i in range(401):
        current = 10 ** (-13 + 13*i/400)
        voltage = .081471 * math.log1p(current / 66e-12) - .867 + (current / .0032)**.775
        points.append((voltage, current))
    pairs = "\n".join(f"+ {voltage:.12g}, {current:.12g}," for voltage, current in points)
    return (".subckt UA176_6AL5 A K\n"
            "BDIODE A K I={pwl(v(a,k),\n" + pairs + "\n+ 400, 1)}\n"
            "RLEAK A K 12.7G\nCAK A K 3.2p\n.ends UA176_6AL5")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("deck", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    lines = "\n".join(flatten(args.deck)).split("\n.control", 1)[0]
    lines = re.sub(r"(?m)^VDRIVE DRIVEP.*", "VDRIVE DRIVEP DRIVEN DC 0 EXTERNAL", lines)
    lines = lines.replace("RCOMMON DRIVEN 0 1G", "RCOMMON DRIVEN 0 100")
    lines = lines.replace("XPAD INP INN PAD_P TN UA176T_PAD LOSSDB={LOSSDB}",
                          "XPAD INP INN PAD_P TN PIN UA176_HOST_PAD")
    lines = lines.replace("R1 PAD_P TP {max(1u,300*VERNIER)}",
                          "R1 PAD_P TP R={max(.01,300*v(pinvernier))}")
    lines = lines.replace("OUTP OUTN TAPA TAPB params:", "OUTP OUTN TAPA TAPB RSEL params:")
    lines = lines.replace("T3_N TAP_A TAP_B UA176_T3", "T3_N TAP_A TAP_B RSEL UA176_T3")
    lines = lines.replace("ETAPA TAPA CT PA CT {TAPFRAC}", "ETAPA TAPA CT value={v(pa,ct)*v(rsel)}")
    lines = lines.replace("ETAPB TAPB CT PB CT {TAPFRAC}", "ETAPB TAPB CT value={v(pb,ct)*v(rsel)}")
    lines = lines.replace("FTAPA PA CT ETAPA {-TAPFRAC}", "BTAPA PA CT I={-v(rsel)*i(VTAPA)}")
    lines = lines.replace("FTAPB PB CT ETAPB {-TAPFRAC}", "BTAPB PB CT I={-v(rsel)*i(VTAPB)}")
    lines = lines.replace("ETAPA TAPA CT", "ETAPA TAPA_INT CT").replace("ETAPB TAPB CT", "ETAPB TAPB_INT CT")
    lines = lines.replace(".ends UA176_T3", "VTAPA TAPA_INT TAPA 0\nVTAPB TAPB_INT TAPB 0\n.ends UA176_T3")
    lines = lines.replace("R_S5 DET_BIAS 0 {RATIO_R}", "R_S5 DET_BIAS 0 R={v(rbias)}")
    lines = lines.replace("ternary_fcn(LIMITING,1m,1T)", "ternary_fcn(v(comp_enable)>.5,.01,1e12)")
    lines = lines.replace("ternary_fcn(LIMITING,1T,1m)", "ternary_fcn(v(comp_enable)>.5,1e12,.01)")
    lines = re.sub(r"(?m)^(R_S4_\w+ \w+ \w+) \{(.*)\}$", r"\1 R={\2}", lines)
    lines = lines.replace("{max(1m,25k*(1-ATTACK))}", "R={max(.01,25k*(1-v(atk)))}")
    lines = lines.replace("{max(1m,5meg*(1-RELEASE))}", "R={max(.01,5meg*(1-v(rel)))}")
    lines = lines.replace("{max(1m,5meg*RELEASE)}", "R={max(.01,5meg*v(rel))}")
    lines = lines.replace("R30 T3_P OUT_P {max(1m,300*OUTVERNIER)}",
                          "R30 T3_P OUT_FEED R={max(.01,300*v(poutvernier))}\n"
                          "XOUTPUT OUT_FEED T3_N OUT_P OUT_N POUT UA176_HOST_PAD")
    lines = re.sub(r"(?ms)^\.subckt UA176_6AL5 .*?^\.ends UA176_6AL5", detector_cell(), lines)
    lines = lines.replace("RLOAD OUT_P T3_N", "RLOAD OUT_P OUT_N")
    lines = lines.replace("VOUT_REF T3_N 0 0", "VOUT_REF T3_N 0 0\nEOBS AUDIO 0 OUT_P OUT_N 1")
    lines += """
.subckt UA176_HOST_PAD IP IN OP ON CONTROL
* CONTROL is the host-provided attenuation voltage ratio, K >= 1.
RIP IP MP R={max(.01,300*(v(CONTROL)-1)/(v(CONTROL)+1))}
RIN IN MN R={max(.01,300*(v(CONTROL)-1)/(v(CONTROL)+1))}
ROP MP OP R={max(.01,300*(v(CONTROL)-1)/(v(CONTROL)+1))}
RON MN ON R={max(.01,300*(v(CONTROL)-1)/(v(CONTROL)+1))}
RSH MP MN R={1200*v(CONTROL)/max(1e-9,v(CONTROL)*v(CONTROL)-1)}
.ends UA176_HOST_PAD
VPIN PIN 0 DC 10 EXTERNAL
VPOUT POUT 0 DC 3.16227766 EXTERNAL
VPINVERNIER PINVERNIER 0 DC 0 EXTERNAL
VPOUTVERNIER POUTVERNIER 0 DC 0 EXTERNAL
VRS RSEL 0 DC .29 EXTERNAL
VRB RBIAS 0 DC 6800 EXTERNAL
VATT ATK 0 DC .5 EXTERNAL
VREL REL 0 DC .8 EXTERNAL
VLIMIT COMP_ENABLE 0 DC 1 EXTERNAL
.save v(audio) v(cv)
.end
"""
    args.output.write_text('#pragma once\ninline constexpr auto poweredDeck = R"NET(' + lines + ')NET";\n')


if __name__ == "__main__":
    main()

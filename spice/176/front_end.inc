VDRIVE DRIVEP DRIVEN DC 0 AC 1 SIN(0 {AMP} {FREQ})
RCOMMON DRIVEN 0 1G
RSOURCEP DRIVEP INP {SOURCE/2}
RSOURCEN DRIVEN INN {SOURCE/2}
XPAD INP INN PAD_P TN UA176T_PAD LOSSDB={LOSSDB}
* R1 is the standard 176 series vernier. Zero ohms selects the 176T reference.
R1 PAD_P TP {max(1u,300*VERNIER)}
XT1 TP TN SEC_P SEC_N UA176_TRANSFORMER LL={LL} CS={CS}

* R2-R5 differential gain ladder. HI takes the winding ends; LO takes the taps.
R2 SEC_P TAP_P 10k
R3 TAP_P CV 6.8k
R4 CV TAP_N 6.8k
R5 TAP_N SEC_N 10k
RG_HI_P SEC_P GP {ternary_fcn(HIGH,1m,1T)}
RG_LO_P TAP_P GP {ternary_fcn(HIGH,1T,1m)}
RG_HI_N SEC_N GN {ternary_fcn(HIGH,1m,1T)}
RG_LO_N TAP_N GN {ternary_fcn(HIGH,1T,1m)}

* Control node at the R3/R4 junction in the normal test-switch position.
C5 CV 0 .1u

* Push-pull 6BC8 with the Sylvania law and supplied grid-current branch.
XV1A PA GP KA V6BC8
XV1B PB GN KB V6BC8
DGRIDP GP KA UA176_6BC8_GRID_DIODE
DGRIDN GN KB UA176_6BC8_GRID_DIODE
.model UA176_6BC8_GRID_DIODE D(IS=1n RS=2k CJO=1p N=1.5 TT=1n)

* R6 cathode balance pot, R10 meter-zero pot, unloaded GR meter tap.
R6A KA KRETURN {max(1m,100*CATHBAL)}
R6B KB KRETURN {max(1m,100*(1-CATHBAL))}
R7 KA KRETURN 47
R8 KB KRETURN 47
R9 KRETURN 0 330
R10A KRETURN METER {max(1m,1k*METERZERO)}
R10B METER R11TOP {max(1m,1k*(1-METERZERO))}
R11 R11TOP 0 330

* The 0B2-regulated rail is held at nominal DC for this stage bench.
C10 REG 0 .1u
R12 PA PLATE_BAL_A 4.7k
R13A PLATE_BAL_A REG {max(1m,1k*PLATEBAL)}
R13B REG PLATE_BAL_B {max(1m,1k*(1-PLATEBAL))}
R14 PLATE_BAL_B PB 4.7k

* T2 G-G high-impedance winding across the plates, with F-F strapped.
* Its P-B low-impedance winding drives the following grids.
XT2 PA PB NEXT_P NEXT_N UA176_T2

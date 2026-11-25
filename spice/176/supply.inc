* T4 high-voltage secondary equivalent, center-tapped and 180 degrees apart.
* Secondary voltage and winding resistance are estimates for this bench.
VT4A HV_A_OC 0 SIN(0 {HV_HALF_PEAK} 60)
VT4B 0 HV_B_OC SIN(0 {HV_HALF_PEAK} 60)
RT4A HV_A_OC HV_A 100
RT4B HV_B_OC HV_B 100
* Warm GZ34 approximation, 232 mA per plate at 17 V.
BV5A HV_A RECT I={.00331*pow(uramp(V(HV_A,RECT)),1.5)}
BV5B HV_B RECT I={.00331*pow(uramp(V(HV_B,RECT)),1.5)}
C11D RECT 0 20u
L1 RECT CHOKE_END 10
RL1 CHOKE_END FILTER 150
C11A FILTER 0 20u
C11B FILTER 0 20u
C11C FILTER 0 20u
R44 FILTER T3_CT 1k
C8B T3_CT 0 20u
R43 T3_CT REG 10k
* Lit-state 0B2 approximation. Ignition hysteresis and noise are omitted.
BV6 REG 0 I={uramp(V(REG)-107.55)/33}

# RS124 modification provenance / switching design

### Verified historically
- RS124 descended from Altec 436B; no single unique RS124 circuit due to serial-specific technician modifications. Source: Abbey Road official account.
- Chandler modern RS124 has INPUT, OUTPUT ATTENUATOR, ATTACK (9 stepped positions), RECOVERY (six times with five adjacent HOLD positions), BALANCE, SUPERFUSE and optional 200/600ohm output impedance. Source: Chandler manual.
- Original 436B lacks user attack/recovery; factory attack 50ms and recovery ~1s to 63%, fixed internal threshold. Source: Altec 436B manual.
- HOLD freezes most recently achieved compression rather than resetting the signal to unity.

### Features awaiting source-locked electrical confirmation

| Modification | Confidence | Plan |
|---|---|---|
| output attenuator | feature verified | simulate true resistive pad once impedances known |
| recovery step network | feature verified, component values unverified | fit passive RC vs measured release traces |
| HOLD switch | feature verified | model capacitor leakage / sidechain hold after schematic selection |
| attack steps | modern Chandler verified | use circuit control paths rather than generic envelope until reference emerges |
| gain-cell balance | feature verified | calibrate 6BC8 push-pull bias trim K2/K3 vs grid voltage |
| 200/600ohm output | feature verified | impedance and termination changes in eventual transformer solver |
| SuperFuse | **proprietary internal circuit unknown** | optional perceptual reconstruction clearly labeled, not an invented exact circuit |
| EMI serial 60070B/60050A/61010B | serial variation verified | do not fabricate exact internal component values |

### Implementation policy

No unverified values are silently labeled RS124 original. Maintain source-specific circuit variants and a list of **unresolved schematic nodes**.
An interface or schematic concept is not evidence that any historical audio measurements have been reproduced.

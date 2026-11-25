#!/usr/bin/env python3
"""Measure the UA 176T input and 6BC8 stage through the interstage secondary."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from itertools import product
from pathlib import Path

from matplotlib import pyplot as plt
import numpy as np

from ua176_input import (
    FREQUENCIES, LEVELS, STAGE, harmonic_columns, plot_harmonics, simulate, voltage_db, write_csv,
)

NODES = ("winding", "grids", "plates", "interstage")
BIAS_LEVELS = (0, -1, -2, -4, -6, -8, -12)
NOMINAL = (600, 20, 1, 0, 2, 92)


def run_gain(executable, condition, control, file):
    source, loss, high, bias, leak, capacitance = condition
    deck = (STAGE / "gain.cir").read_text()
    deck = deck.replace("SOURCE=600 LOSSDB=20 BIAS=0",
                        f"SOURCE={source} LOSSDB={loss} BIAS={bias}")
    deck = deck.replace("LL=2m CS=92p HIGH=1", f"LL={leak}m CS={capacitance}p HIGH={high}")
    return simulate(executable, deck, control, file, condition)


def measure_dc(executable):
    rows = []
    for bias in BIAS_LEVELS:
        case = (600, 20, 1, bias, 2, 92)
        data = run_gain(executable, case, """op
let rail_ma = -1000*i(vbplus)
let plate_a_ma = 1000*(v(reg)-v(pa))/5200
let plate_b_ma = 1000*(v(reg)-v(pb))/5200
wrdata dc.txt v(gp) v(gn) v(ka) v(kb) v(pa) v(pb) plate_a_ma plate_b_ma rail_ma""", "dc.txt")
        keys = ("grid_a_v", "grid_b_v", "cathode_a_v", "cathode_b_v", "plate_a_v",
                "plate_b_v", "plate_a_ma", "plate_b_ma", "rail_ma")
        rows.append(dict(bias_v=bias, **dict(zip(keys, data[0, 1:], strict=True))))
    return rows


def measure_trims(executable):
    deck = (STAGE / "gain.cir").read_text()
    control = """op
wrdata trims.txt v(ka) v(kb) v(pa) v(pb) v(meter)"""
    trims = ("CATHBAL", "PLATEBAL", "METERZERO", "VERNIER")
    rows = []
    for trim, position in product(trims, (0, .5, 1)):
        adjusted = deck.replace("VERNIER=0" if trim == "VERNIER" else f"{trim}=.5",
                                f"{trim}={position}")
        if trim == "VERNIER":
            data = simulate(executable, adjusted, """ac lin 1 1k 1k
let gain = db(v(next_n,next_p)/v(drivep,driven))
wrdata trim-ac.txt gain""", "trim-ac.txt", (trim, position))
            rows.append(dict(trim=trim, position=position, cathode_a_v="", cathode_b_v="",
                             plate_a_v="", plate_b_v="", meter_tap_v="", gain_at_1k_db=data[0, 1]))
        else:
            data = simulate(executable, adjusted, control, "trims.txt", (trim, position))
            ka, kb, pa, pb, meter = data[0, 1:]
            rows.append(dict(trim=trim, position=position, cathode_a_v=ka, cathode_b_v=kb,
                             plate_a_v=pa, plate_b_v=pb, meter_tap_v=meter, gain_at_1k_db=""))
    return rows


def measure_t2(executable):
    deck = (STAGE / "t2.cir").read_text()
    control = """ac dec 48 10 100k
let gain = v(out)/v(source_node)
let winding = v(out)/v(in)
let zin = v(in)/(-i(vdrive))
wrdata t2-ac.txt gain winding zin"""
    cases = (("catalog_forward", "15k", "80k", "OUT 0 IN 0"),
             ("ua_reverse_rated", "80k", "15k", "IN 0 OUT 0"),
             ("ua_reverse_grid_fixture", "8k", "40k", "IN 0 OUT 0"))
    rows = []
    for direction, source, load, ports in cases:
        adjusted = deck.replace("SOURCE=15k LOAD=80k", f"SOURCE={source} LOAD={load}")
        adjusted = adjusted.replace("OUT 0 IN 0", ports)
        data = simulate(executable, adjusted, control, "t2-ac.txt", direction)
        hz, gain_r, gain_i, winding_r, winding_i, zin_r, zin_i = data.T
        gain = gain_r + 1j * gain_i
        winding = winding_r + 1j * winding_i
        reference = np.interp(np.log(1000), np.log(hz), voltage_db(gain))
        for index, frequency in enumerate(hz):
            rows.append(dict(direction=direction, source=source, load=load,
                             frequency_hz=frequency, gain_db=voltage_db(gain[index]),
                             relative_1khz_db=voltage_db(gain[index]) - reference,
                             phase_deg=np.angle(gain[index], deg=True),
                             winding_gain_db=voltage_db(winding[index]),
                             zin_real_ohms=zin_r[index], zin_imag_ohms=zin_i[index]))
    return rows


def measure_ac(executable):
    cases = list(product((600,), (20,), (1,), (0, -4), (1.5, 2, 2.5), (84, 92, 100)))
    cases.extend((600, 20, 1, bias, 2, 92) for bias in BIAS_LEVELS if bias not in (0, -4))
    cases.extend(((600, 20, 0, 0, 2, 92), (100, 0, 1, 0, 2, 92), (600, 0, 1, 0, 2, 92)))
    rows = []
    control = """ac dec 48 10 100k
let zin = v(inp,inn)/(-i(vdrive))
let winding = v(sec_p,sec_n)/v(drivep,driven)
let grids = v(gp,gn)/v(drivep,driven)
let plates = v(pb,pa)/v(drivep,driven)
let interstage = v(next_n,next_p)/v(drivep,driven)
wrdata ac.txt zin winding grids plates interstage"""
    for case in cases:
        source, loss, high, bias, leak, capacitance = case
        data = run_gain(executable, case, control, "ac.txt")
        hz = data[:, 0]
        impedances = data[:, 1] + 1j * data[:, 2]
        transfer = data[:, 3::2] + 1j * data[:, 4::2]
        magnitudes = voltage_db(transfer)
        phases = np.angle(transfer, deg=True)
        reference = [np.interp(np.log(1000), np.log(hz), values) for values in magnitudes.T]
        for index, frequency in enumerate(hz):
            zin = impedances[index]
            row = dict(source_ohms=source, pad_loss_db=loss, gain_switch="hi" if high else "lo",
                       bias_v=bias, t1_leakage_mh=leak, t1_secondary_pf=capacitance,
                       frequency_hz=frequency, zin_real_ohms=zin.real,
                       zin_imag_ohms=zin.imag, zin_abs_ohms=abs(zin))
            for column, name in enumerate(NODES):
                row[f"{name}_gain_db"] = magnitudes[index, column]
                row[f"{name}_relative_1khz_db"] = magnitudes[index, column] - reference[column]
                row[f"{name}_phase_deg"] = phases[index, column]
            rows.append(row)
    return rows


def measure_tone(executable, frequency, level, bias=0, points=384):
    generator_peak = 2 * np.sqrt(2) * 0.775 * 10**(level / 20)
    periods = 12
    start_cycle = max(32, int(np.ceil(0.1 * frequency)))
    start = start_cycle / frequency
    stop = (start_cycle + periods + 1) / frequency
    step = 1 / (points * frequency)
    control = f"""alter @vdrive[sin] = [ 0 {generator_peak:.16g} {frequency} ]
tran {step:.16g} {stop:.16g} {start:.16g} {step:.16g}
let winding = v(sec_p,sec_n)
let grids = v(gp,gn)
let plates = v(pb,pa)
let interstage = v(next_n,next_p)
let input_terminal = v(inp,inn)
wrdata tran.txt winding grids plates interstage input_terminal"""
    condition = (600, 20, 1, bias, 2, 92)
    data = run_gain(executable, condition, control, "tran.txt")
    time = (start_cycle + 1 + np.arange(periods * points) / points) / frequency
    harmonics = harmonic_columns(data, time, periods, (*NODES, "input_terminal"))
    terminal_dbu = voltage_db(harmonics["input_terminal_fundamental_vpk"] / (np.sqrt(2) * .775))
    return dict(frequency_hz=frequency, matched_source_dbu=level, input_terminal_dbu=terminal_dbu,
                pad_loss_db=20, bias_v=bias, samples_per_period=points, **harmonics)


def plot_ac(rows, path):
    fig, axes = plt.subplots(3, 1, figsize=(10, 8.5), sharex=True, layout="constrained")
    for leak, capacitance in ((1.5, 84), (2, 92), (2.5, 100)):
        case = [row for row in rows if row["bias_v"] == 0 and row["gain_switch"] == "hi"
                and row["source_ohms"] == 600 and row["pad_loss_db"] == 20
                and row["t1_leakage_mh"] == leak and row["t1_secondary_pf"] == capacitance]
        hz = [row["frequency_hz"] for row in case]
        label = f"{leak:g} mH / {capacitance} pF"
        for axis, key in zip(axes, ("relative_1khz_db", "phase_deg", "zin_abs_ohms")):
            field = key if key.startswith("zin_") else f"interstage_{key}"
            axis.semilogx(hz, [row[field] for row in case], label=label)
    for axis, label in zip(axes, ("Gain vs 1 kHz (dB)", "Phase (degrees)", "Input |Z| (Ω)")):
        axis.grid(True, which="both", alpha=.25)
        axis.set_ylabel(label)
    axes[0].set_ylim(-10, 10)
    axes[0].legend()
    axes[-1].set(xlabel="Frequency (Hz)", xlim=(20, 20000))
    fig.suptitle("UA 176T through T2 · 6BC8 at idle bias · T1 parameter sweep")
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ngspice", default="ngspice")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--jobs", default=4, type=int)
    parser.add_argument("--drive-sweep", action="store_true", help="run the extended level and frequency sweep")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    args.output.mkdir(exist_ok=True, parents=True)
    dc = measure_dc(args.ngspice)
    ac = measure_ac(args.ngspice)
    write_csv(args.output / "UA176-6BC8-DC.csv", dc)
    write_csv(args.output / "UA176-6BC8-FR-phase-loading.csv", ac)
    write_csv(args.output / "UA176-6BC8-trims.csv", measure_trims(args.ngspice))
    write_csv(args.output / "UA176-T2-bench.csv", measure_t2(args.ngspice))
    plot_ac(ac, args.output / "UA176-6BC8-FR-phase-loading.png")
    cases = [(1000, 4, bias, 768) for bias in (0, -4)]
    if args.drive_sweep:
        cases += list(product(FREQUENCIES, LEVELS, (0,), (384,)))
        cases += [(1000, level, -4, 384) for level in LEVELS]
    frequencies, levels, biases, points = zip(*cases)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        tone = partial(measure_tone, args.ngspice)
        tones = list(pool.map(tone, frequencies, levels, biases, points))
    write_csv(args.output / "UA176-6BC8-harmonics.csv", tones)
    plot_harmonics(tones, args.output / "UA176-6BC8-SPAN.png", node="interstage",
                   title="UA 176T through T2 · 1 kHz · 20 dB pad · fundamental = 0 dBFS",
                   levels=LEVELS if args.drive_sweep else (4,))


if __name__ == "__main__":
    main()

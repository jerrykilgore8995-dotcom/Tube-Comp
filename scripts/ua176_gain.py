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
wrdata tran.txt winding grids plates interstage"""
    condition = (600, 20, 1, bias, 2, 92)
    data = run_gain(executable, condition, control, "tran.txt")
    time = (start_cycle + 1 + np.arange(periods * points) / points) / frequency
    return dict(frequency_hz=frequency, matched_source_dbu=level, pad_loss_db=20,
                bias_v=bias, samples_per_period=points,
                **harmonic_columns(data, time, periods, NODES))


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
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    args.output.mkdir(exist_ok=True, parents=True)
    dc = measure_dc(args.ngspice)
    ac = measure_ac(args.ngspice)
    write_csv(args.output / "UA176-6BC8-DC.csv", dc)
    write_csv(args.output / "UA176-6BC8-FR-phase-loading.csv", ac)
    plot_ac(ac, args.output / "UA176-6BC8-FR-phase-loading.png")
    frequencies, levels, biases, points = zip(*(
        list(product(FREQUENCIES, LEVELS, (0,), (384,)))
        + [(1000, level, -4, 384) for level in LEVELS]
        + [(1000, 4, 0, 768), (1000, 18, 0, 768)]
    ))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        tone = partial(measure_tone, args.ngspice)
        tones = list(pool.map(tone, frequencies, levels, biases, points))
    write_csv(args.output / "UA176-6BC8-harmonics.csv", tones)
    plot_harmonics(tones, args.output / "UA176-6BC8-SPAN.png", node="interstage",
                   title="UA 176T through T2 · 1 kHz · 20 dB pad · fundamental = 0 dBFS")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Measure the UA 176 audio path through the T3 secondary."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

import numpy as np

from ua176_input import STAGE, harmonic_columns, plot_harmonics, simulate, voltage_db, write_csv
from matplotlib import pyplot as plt

NODES = ("input", "t2", "v2_plate", "v3_grid", "v3_plate", "output")
VECTORS = ("v(inp,inn)", "v(next_p,next_n)", "v(v2pa,v2pb)", "v(v3ga,v3gb)",
           "v(v3pa,v3pb)", "v(out_p,t3_n)")
AC_CONTROL = """ac dec 48 10 100k
let zin = v(inp,inn)/(-i(vdrive))
wrdata ac.txt zin """ + " ".join(VECTORS)
DC_NODES = ("v2pa", "v2pb", "v2ka", "v2kb", "v2_return", "v2_supply", "v3pa",
            "v3pb", "v3ga", "v3gb", "v3k", "t3_ct")


def run_line(executable, case, control, file):
    loss, bias, feedback = case
    deck = (STAGE / "line.cir").read_text()
    deck = deck.replace("LOSSDB=20 BIAS=0", f"LOSSDB={loss} BIAS={bias}")
    if not feedback:
        deck = deck.replace("FBRES=180k", "FBRES=1T")
    return simulate(executable, deck, control, file, case)


def measure_dc(executable):
    vectors = " ".join(f"v({node})" for node in DC_NODES)
    data = run_line(executable, (20, 0, True),
                    f"op\nlet supply_ma=-1000*i(vline)\nwrdata dc.txt {vectors} supply_ma", "dc.txt")
    labels = (*[f"{node}_v" for node in DC_NODES], "line_supply_ma")
    return dict(zip(labels, data[0, 1:], strict=True))


def measure_ac(executable):
    rows = []
    for case in ((20, 0, True), (20, -4, True), (20, 0, False)):
        loss, bias, feedback = case
        data = run_line(executable, case, AC_CONTROL, "ac.txt")
        hz = data[:, 0]
        impedance = data[:, 1] + 1j * data[:, 2]
        nodes = data[:, 3::2] + 1j * data[:, 4::2]
        gains = nodes / nodes[:, :1]
        magnitudes = voltage_db(gains)
        phases = np.angle(gains, deg=True)
        reference = np.interp(np.log(1000), np.log(hz), magnitudes[:, -1])
        for i, frequency in enumerate(hz):
            row = dict(pad_loss_db=loss, bias_v=bias, feedback=feedback, frequency_hz=frequency,
                       zin_real_ohms=impedance[i].real, zin_imag_ohms=impedance[i].imag,
                       output_relative_1khz_db=magnitudes[i, -1] - reference)
            for j, name in enumerate(NODES):
                row[f"{name}_gain_db"] = magnitudes[i, j]
                row[f"{name}_phase_deg"] = phases[i, j]
            rows.append(row)
    return rows


def measure_tone(executable, loss, points=768):
    case = (loss, 0, True)
    ac = run_line(executable, case, """ac lin 1 1k 1k
wrdata input.txt v(inp,inn)""", "input.txt")
    target_peak = np.sqrt(2) * .775 * 10**(4/20)
    source_peak = target_peak / abs(complex(*ac[0, 1:]))
    periods = 16
    start = .125
    step = .001 / points
    stop = start + (periods + 1) * .001
    time = start + .001 + np.arange(periods * points) * step
    vectors = " ".join(VECTORS)
    for _ in range(4):
        control = f"""alter @vdrive[sin] = [ 0 {source_peak:.16g} 1000 ]
tran {step:.16g} {stop:.16g} {start:.16g} {step:.16g}
wrdata transient.txt {vectors}"""
        data = run_line(executable, case, control, "transient.txt")
        if data[0, 0] > time[0] or data[-1, 0] < time[-1]:
            raise RuntimeError(f"Incomplete transient for {loss} dB pad")
        harmonics = harmonic_columns(data, time, periods, NODES)
        actual_peak = harmonics["input_fundamental_vpk"]
        if abs(voltage_db(actual_peak / target_peak)) < .003:
            dbu_reference = np.sqrt(2) * .775
            return dict(frequency_hz=1000, target_input_dbu=4,
                        input_terminal_dbu=voltage_db(actual_peak / dbu_reference),
                        pad_loss_db=loss, bias_v=0, samples_per_period=points,
                        output_terminal_dbu=voltage_db(harmonics["output_fundamental_vpk"] / dbu_reference),
                        **harmonics)
        source_peak *= target_peak / actual_peak
    raise RuntimeError(f"Could not calibrate the +4 dBu input for {loss} dB pad")


def plot_ac(rows, path):
    fig, axes = plt.subplots(2, 1, figsize=(9, 6.5), sharex=True, layout="constrained")
    for bias, feedback, label in ((0, True, "Feedback connected, idle bias"),
                                  (-4, True, "Feedback connected, −4 V CV"),
                                  (0, False, "Feedback opened for comparison")):
        selected = [r for r in rows if r["bias_v"] == bias and r["feedback"] == feedback]
        hz = [r["frequency_hz"] for r in selected]
        axes[0].semilogx(hz, [r["output_relative_1khz_db"] for r in selected], label=label)
        phase = np.rad2deg(np.unwrap(np.deg2rad([r["output_phase_deg"] for r in selected])))
        phase -= 360 * np.round(np.interp(np.log(1000), np.log(hz), phase) / 360)
        axes[1].semilogx(hz, phase)
    axes[0].set(ylim=(-10, 10), ylabel="Gain vs 1 kHz (dB)")
    axes[0].legend()
    axes[1].set(xlim=(20, 20000), xlabel="Frequency (Hz)", ylabel="Phase (degrees)")
    for axis in axes:
        axis.grid(True, which="both", alpha=.25)
    fig.suptitle("UA 176 through T3 · 600 Ω output load")
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ngspice", default="ngspice")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    write_csv(args.output / "UA176-line-DC.csv", [measure_dc(args.ngspice)])
    ac = measure_ac(args.ngspice)
    write_csv(args.output / "UA176-line-AC.csv", ac)
    plot_ac(ac, args.output / "UA176-line-AC.png")
    with ThreadPoolExecutor(max_workers=3) as pool:
        rows = list(pool.map(partial(measure_tone, args.ngspice), (20, 30, 40)))
    write_csv(args.output / "UA176-line-THD.csv", rows)
    plot_harmonics(rows, args.output / "UA176-line-SPAN.png", node="output", levels=(4,),
                   title="UA 176 through T3 · +4 dBu at 1 kHz · 20 dB pad")


if __name__ == "__main__":
    main()

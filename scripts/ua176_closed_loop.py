#!/usr/bin/env python3
"""Measure the UA 176's electrical detector and closed compression loop."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from itertools import product
from pathlib import Path

import numpy as np

from ua176_input import STAGE, harmonic_columns, simulate, voltage_db, write_csv
from matplotlib import pyplot as plt

RATIOS = (2, 4, 8, 12)
LOSSES = (40, 30, 20)
VECTORS = ("v(inp,inn)", "v(out_p,t3_n)", "v(tap_a,tap_b)", "v(cv)",
           "v(det_bias)", "v(det_a)", "v(det_k1)", "v(det_k2)")


def run_closed(executable, ratio, loss, limiting, control, file):
    deck = (STAGE / "closed_loop.cir").read_text()
    deck = deck.replace("LOSSDB=20", f"LOSSDB={loss}")
    deck = deck.replace("LIMITING=1 RATIO=2", f"LIMITING={int(limiting)} RATIO={ratio}")
    return simulate(executable, deck, control, file, (ratio, loss, limiting))


def measure_dc(executable, ratio):
    data = run_closed(executable, ratio, 20, True, """op
wrdata dc.txt v(cv) v(det_bias) v(det_k1) v(det_k2)""", "dc.txt")
    columns = ("idle_cv_v", "detector_bias_v", "cathode_1_v", "cathode_2_v")
    return dict(ratio=ratio, **dict(zip(columns, data[0, 1:], strict=True)))


def measure_tap(executable, ratio):
    data = run_closed(executable, ratio, 20, True, """ac lin 1 1k 1k
let tap_fraction = v(tap_a,tap_b)/v(v3pa,v3pb)
wrdata tap.txt tap_fraction""", "tap.txt")
    return abs(complex(*data[0, 1:]))


def measure_release(executable, setting):
    deck = (STAGE / "closed_loop.cir").read_text()
    deck = deck.replace("AMP=3.474135043921514", "AMP=0")
    deck = deck.replace("RELEASE=.8", f"RELEASE={setting}")
    deck = deck.replace(".options ", ".ic v(cv)=-3\n.options ")
    data = simulate(executable, deck, """tran 50u 1.6 0 50u
wrdata release.txt v(cv)""", "release.txt", ("release", setting))
    target = -3 / np.e
    index = np.flatnonzero(data[:, 1] >= target)[0]
    elapsed = np.interp(target, data[index-1:index+1, 1], data[index-1:index+1, 0])
    return dict(release_setting=setting, target_cv_v=target, recovery_63_ms=1000*elapsed)


def measure_tone(executable, case, points=192, settle=.72):
    ratio, loss, limiting = case
    ac = run_closed(executable, *case, """ac lin 1 1k 1k
wrdata input.txt v(inp,inn)""", "input.txt")
    dbu_reference = np.sqrt(2) * .775
    target_peak = dbu_reference * 10**(4/20)
    source_peak = target_peak / abs(complex(*ac[0, 1:]))
    periods = 12
    start = settle - .07
    stop = settle + .014
    step = .001 / points
    time = settle + .001 + np.arange(periods * points) * step
    vectors = " ".join(VECTORS)
    control = f"""alter @vdrive[sin] = [ 0 {source_peak:.16g} 1000 ]
tran {step:.16g} {stop:.16g} {start:.16g} {step:.16g}
wrdata tone.txt {vectors}"""
    data = run_closed(executable, *case, control, "tone.txt")
    if data[0, 0] > time[0] - .05 or data[-1, 0] < time[-1]:
        raise RuntimeError(f"Incomplete transient for {case}")
    final = harmonic_columns(data[:, :4], time, periods, ("input", "output", "tap"))
    previous = harmonic_columns(data[:, :3], time - .05, periods, ("input", "output"))
    bias = np.interp(time, data[:, 0], data[:, 4])
    return dict(ratio=ratio, pad_loss_db=loss, limiting=limiting,
                input_terminal_dbu=voltage_db(final["input_fundamental_vpk"] / dbu_reference),
                output_terminal_dbu=voltage_db(final["output_fundamental_vpk"] / dbu_reference),
                output_drift_db=voltage_db(final["output_fundamental_vpk"] / previous["output_fundamental_vpk"]),
                control_v_mean=np.mean(bias), control_v_ripple_pp=np.ptp(bias),
                detector_bias_v_mean=np.mean(np.interp(time, data[:, 0], data[:, 5])),
                **final)


def plot_reduction(rows, destination):
    fig, ax = plt.subplots(figsize=(8.5, 4.5), layout="constrained")
    for ratio in RATIOS:
        selected = [r for r in rows if r["ratio"] == ratio and r["limiting"]]
        ax.plot([r["pad_loss_db"] for r in selected], [r["gain_reduction_db"] for r in selected],
                "o-", label=f"{ratio}:1")
    ax.set(xlabel="Input attenuation (dB)", ylabel="Gain reduction (dB)",
           title="UA 176 closed-loop SPICE bench · +4 dBu, 1 kHz")
    ax.invert_xaxis()
    ax.grid(True, alpha=.25)
    ax.legend()
    fig.savefig(destination, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ngspice", default="ngspice")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--jobs", default=6, type=int)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    args.output.mkdir(exist_ok=True, parents=True)
    dc = []
    for ratio in RATIOS:
        row = measure_dc(args.ngspice, ratio)
        row["t3_tap_fraction"] = measure_tap(args.ngspice, ratio)
        dc.append(row)
    write_csv(args.output / "UA176-detector-DC-and-taps.csv", dc)
    write_csv(args.output / "UA176-C5-release.csv",
              [measure_release(args.ngspice, x) for x in (0, .5, 1)])
    cases = list(product(RATIOS, LOSSES, (False, True)))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        jobs = pool.map(partial(measure_tone, args.ngspice), cases)
        rows = list(jobs)
    references = {(r["ratio"], r["pad_loss_db"]): r["output_terminal_dbu"]
                  for r in rows if not r["limiting"]}
    for r in rows:
        r["gain_reduction_db"] = references[r["ratio"], r["pad_loss_db"]] - r["output_terminal_dbu"]
    write_csv(args.output / "UA176-closed-loop-1kHz.csv", rows)
    plot_reduction(rows, args.output / "UA176-closed-loop-gain-reduction.png")


if __name__ == "__main__":
    main()

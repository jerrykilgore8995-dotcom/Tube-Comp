#!/usr/bin/env python3
"""Measure the UA 176T input stage with ngspice."""
import argparse
import csv
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from itertools import product
from pathlib import Path
import re
import shutil
import subprocess
from tempfile import TemporaryDirectory

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "spice/176"
FREQUENCIES = (100, 1000, 5000, 10000)
LEVELS = (-18, -12, -8, -4, 0, 4, 8, 12, 18)
HARMONICS = np.arange(2, 21)


def voltage_db(amplitude):
    return 20 * np.log10(np.maximum(np.abs(amplitude), 1e-15))


def run_case(executable, condition, control, file):
    source, loss, load, bias = condition
    with TemporaryDirectory(prefix="ua176-input-") as temp:
        stage = Path(temp) / "176"
        stage.mkdir()
        models = stage.parent / "models"
        models.mkdir()
        shutil.copyfile(ROOT / "spice/models/tube_cells.lib", models / "tube_cells.lib")
        shutil.copyfile(STAGE / "input.lib", stage / "input.lib")
        deck = (STAGE / "input.cir").read_text()
        deck = deck.replace("SOURCE=600 LOSSDB=20 BIAS=0",
                            f"SOURCE={source} LOSSDB={loss} BIAS={bias}")
        if load == "catalog_50k":
            deck = re.sub(r"(?m)^XGRID[PN].*$", "", deck)
            deck = deck.replace("VBIAS CV 0 {BIAS}",
                                "RLOADP GP 0 25k\nRLOADN GN 0 25k")
        deck = re.sub(r"(?s)(set numdgt=15\n).*?(\nquit)", r"\1" + control + r"\2", deck)
        (stage / "case.cir").write_text(deck)
        result = subprocess.run([executable, "-b", "case.cir"], cwd=stage,
                                capture_output=True, text=True, timeout=90)
        data_file = stage / file
        if result.returncode or not data_file.exists() or "error" in result.stderr.lower():
            raise RuntimeError(f"ngspice failed for {condition}\n{result.stdout}\n{result.stderr}")
        data = np.loadtxt(data_file, skiprows=1)
        if data.ndim != 2 or not np.all(np.isfinite(data)):
            raise RuntimeError(f"Invalid ngspice data for {condition}")
        return data


def measure_ac(executable):
    cases = list(product((100, 600), (0, 10, 20), ("catalog_50k", "biased_6bc8")))
    rows = []
    for source, loss, load in cases:
        bias = 0
        data = run_case(executable, (source, loss, load, bias), """ac dec 48 10 100k
let zin = v(inp,inn)/(-i(vdrive))
let primary = v(tp,tn)/v(drivep,driven)
let secondary = v(gp,gn)/v(drivep,driven)
wrdata ac.txt zin primary secondary""", "ac.txt")
        hz, zr, zi, pr, pi, sr, si = data.T
        primary = pr + 1j * pi
        secondary = sr + 1j * si
        t1_gain = secondary / primary
        reference = np.interp(np.log(1000), np.log(hz), voltage_db(secondary))
        for index, frequency in enumerate(hz):
            rows.append(dict(source_ohms=source, pad_loss_db=loss, load=load,
                             frequency_hz=frequency, zin_real_ohms=zr[index],
                             zin_imag_ohms=zi[index], zin_abs_ohms=abs(zr[index] + 1j*zi[index]),
                             generator_to_primary_db=voltage_db(primary[index]),
                             generator_to_secondary_db=voltage_db(secondary[index]),
                             secondary_relative_1khz_db=voltage_db(secondary[index]) - reference,
                             generator_to_secondary_phase_deg=np.angle(secondary[index], deg=True),
                             t1_voltage_gain_db=voltage_db(t1_gain[index]),
                             t1_phase_deg=np.angle(t1_gain[index], deg=True)))
    return rows


def measure_tone(executable, frequency, level, loss, load="biased_6bc8", bias=0, points=512):
    # Under a matched 600-ohm load, the primary source sees half the generator voltage.
    generator_peak = 2 * np.sqrt(2) * 0.775 * 10**(level / 20)
    periods = 12
    start_cycle = max(32, int(np.ceil(0.12 * frequency)))
    start = start_cycle / frequency
    stop = (start_cycle + periods + 1) / frequency
    step = 1 / (points * frequency)
    control = f"""alter @vdrive[sin] = [ 0 {generator_peak:.16g} {frequency} ]
tran {step:.16g} {stop:.16g} {start:.16g} {step:.16g}
let primary = v(tp,tn)
let secondary = v(gp,gn)
wrdata tran.txt primary secondary v(gp)"""
    data = run_case(executable, (600, loss, load, bias), control, "tran.txt")
    time = (start_cycle + 1 + np.arange(periods * points) / points) / frequency
    row = dict(frequency_hz=frequency, matched_source_dbu=level, source_ohms=600,
               pad_loss_db=loss, load=load, bias_v=bias, samples_per_period=points)
    for name, wave in zip(("primary", "secondary", "positive_grid"), data[:, 1:].T):
        samples = np.interp(time, data[:, 0], wave)
        amplitudes = 2 * np.abs(np.fft.rfft(samples)) / len(samples)
        fundamental = amplitudes[periods]
        fractions = amplitudes[HARMONICS * periods] / fundamental
        row[f"{name}_fundamental_vpk"] = fundamental
        row[f"{name}_thd_percent_k2_k20"] = 100 * np.linalg.norm(fractions)
        for harmonic, fraction in zip(HARMONICS, fractions):
            row[f"{name}_k{harmonic}_norm_dbfs"] = voltage_db(fraction)
    return row


def write_csv(path, rows):
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot_ac(rows, destination):
    fig, axes = plt.subplots(3, 1, figsize=(10.5, 8.5), sharex=True, layout="constrained")
    fields = ("secondary_relative_1khz_db", "generator_to_secondary_phase_deg", "zin_abs_ohms")
    for loss in (0, 10, 20):
        subset = [row for row in rows if row["source_ohms"] == 600
                  and row["pad_loss_db"] == loss and row["load"] == "biased_6bc8"]
        for axis, field in zip(axes, fields):
            axis.semilogx([row["frequency_hz"] for row in subset], [row[field] for row in subset],
                          label=f"{loss} dB pad")
    for axis, label in zip(axes, ("Gain vs 1 kHz (dB)", "Phase (degrees)", "Input |Z| (ohms)")):
        axis.set_ylabel(label)
        axis.grid(True, which="both", alpha=.25)
    axes[0].set_ylim(-10, 10)
    axes[0].legend()
    axes[-1].set(xlabel="Frequency (Hz)", xlim=(20, 20000))
    fig.suptitle("UA 176T input stage · 600 Ω source · biased 6BC8 load fixture")
    fig.savefig(destination, dpi=150)
    plt.close(fig)


def plot_harmonics(rows, destination):
    fig, axes = plt.subplots(3, 3, figsize=(12, 8.5), sharex=True, sharey=True, layout="constrained")
    for axis, level in zip(axes.flat, LEVELS):
        row = next(row for row in rows if row["frequency_hz"] == 1000
                   and row["matched_source_dbu"] == level and row["pad_loss_db"] == 20)
        harmonics = np.array([row[f"secondary_k{k}_norm_dbfs"] for k in HARMONICS])
        plotted = harmonics >= -100
        axis.vlines([1], -100, [0], color="#efab48", linewidth=1.8)
        axis.vlines(HARMONICS[plotted], -100, harmonics[plotted], color="#ffd385", linewidth=1.5)
        axis.set_facecolor("#1d222a")
        axis.grid(True, color="#a7b6c6", alpha=.2)
        axis.set_title(f"{level:+d} dBu")
        axis.set(xlim=(.5, 20.5), ylim=(-100, 0), xticks=[1, 2, 4, 8, 12, 16, 20])
    fig.supxlabel("Harmonic order")
    fig.supylabel("Normalized level (dBFS)")
    fig.suptitle("UA 176T T1 secondary · 1 kHz · 20 dB pad · fundamental = 0 dBFS")
    fig.savefig(destination, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ngspice", default="ngspice")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--jobs", default=4, type=int, help="concurrent transient cases")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    args.output.mkdir(exist_ok=True, parents=True)
    ac = measure_ac(args.ngspice)
    write_csv(args.output / "UA176-input-FR-phase-loading.csv", ac)
    frequency, level, loss = zip(*product(FREQUENCIES, LEVELS, (0, 20)))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        tone = partial(measure_tone, args.ngspice)
        tones = list(pool.map(tone, frequency, level, loss))
    tones.extend(measure_tone(args.ngspice, 1000, 18, loss, load, bias, points)
                 for loss, load, bias, points in ((0, "catalog_50k", 0, 512),
                                                   (0, "biased_6bc8", -3, 512),
                                                   (0, "biased_6bc8", 0, 1024),
                                                   (20, "biased_6bc8", 0, 1024)))
    write_csv(args.output / "UA176-input-harmonics.csv", tones)
    plot_ac(ac, args.output / "UA176-input-FR-phase-loading.png")
    plot_harmonics(tones, args.output / "UA176-input-SPAN.png")


if __name__ == "__main__":
    main()

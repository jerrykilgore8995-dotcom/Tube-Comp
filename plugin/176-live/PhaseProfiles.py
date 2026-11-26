"""Build native-rate source responses from the exported continuous circuit."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

os.environ["OPENBLAS_NUM_THREADS"] = "1"
import numpy as np
from scipy import linalg


def read_reference(path):
    with path.open("rb") as stream:
        nodes, states, devices, device_voltages, ports = np.fromfile(stream, np.int32, count=5)
        state_indices = np.fromfile(stream, np.int32, count=states)
        offsets = np.fromfile(stream, np.int32, count=devices + 1)

        def matrix(rows, columns):
            return np.fromfile(stream, np.float64, count=rows * columns).reshape(rows, columns)

        conductance = matrix(nodes, nodes)
        mass = np.zeros_like(conductance)
        mass[:, state_indices] = matrix(nodes, states)
        sources = matrix(nodes, devices + 3)
        observe = matrix(ports, nodes)
        slopes = matrix(device_voltages, 1).ravel()

    jacobian = np.zeros((devices, ports))
    for device in range(devices):
        region = slice(offsets[device], offsets[device + 1])
        jacobian[device, region] = slopes[region]
    conductance -= sources[:, 2:devices + 2] @ jacobian @ observe
    return conductance, mass, sources[:, 1], observe, slopes


def build_profile(path):
    rate = 44100
    conductance, mass, source, observe, slopes = read_reference(path)
    # Retain the slow poles explicitly so a short impulse response need not
    # truncate the transformer and coupling-capacitor settling.
    base = conductance + rate * mass
    transition = linalg.solve(base, rate * mass)
    excitation = linalg.solve(base, source)
    eigenvalues, left, right = linalg.eig(transition, left=True, right=True)
    finite = np.flatnonzero(np.abs(eigenvalues) > 1e-7)
    poles = 1 - 1 / eigenvalues[finite]
    slow = (np.abs(poles) < .55) & (poles.imag >= -1e-12)
    indices = finite[slow]
    poles = poles[slow]
    left, right = left[:, indices], right[:, indices]
    norm = np.einsum("ij,ij->j", left.conj(), right)
    residues = (observe @ right) * (left.conj().T @ excitation) / (norm * eigenvalues[indices])
    residues[:, poles.imag > 1e-12] *= 2
    discrete_poles = np.exp(poles)
    discrete_weights = residues * np.expm1(poles) / poles

    hz = np.r_[0., np.geomspace(5., 500., 80), np.linspace(510., 20000., 704),
               np.linspace(20020., 20900., 26)]
    exact = np.array([observe @ linalg.solve(conductance + 2j * np.pi * f * mass, source) for f in hz])
    q = np.exp(-2j * np.pi * hz[:, None] / rate)
    slow_response = .5 * (1 / (1 - q * discrete_poles) @ discrete_weights.T
                          + 1 / (1 - q * discrete_poles.conj()) @ discrete_weights.conj().T)
    remainder = exact - slow_response
    taps = np.arange(-32, 33)
    basis = np.exp(-2j * np.pi * hz[:, None] * taps / rate)
    weight = np.where(hz < 250, 2., 1.)
    weight[0] = 50
    system = np.r_[weight[:, None] * basis.real, weight[:, None] * basis.imag,
                   1e-3 * np.eye(len(taps))]
    target = np.r_[weight[:, None] * remainder.real, weight[:, None] * remainder.imag,
                   np.zeros((len(taps), observe.shape[0]))]
    impulse = linalg.lstsq(system, target)[0]
    within_band = (hz >= 30) & (hz <= 20000)
    phase_error = np.angle((slow_response + basis @ impulse)[within_band][:, [-6, -1]]
                           / exact[within_band][:, [-6, -1]], deg=True)
    if not np.all(np.isfinite(impulse)) or np.max(np.abs(phase_error)) > 1:
        raise ValueError("Circuit source reconstruction missed the one-degree design tolerance")
    return impulse.T, discrete_poles, discrete_weights.T, slopes


def initializer(values):
    if isinstance(values, np.ndarray):
        return "{" + ", ".join(initializer(value) for value in values) + "}"
    if np.iscomplexobj(values):
        return "{" + f"{values.real:.17g}, {values.imag:.17g}" + "}"
    return f"{values:.17g}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    profiles = []
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "reference.bin"
        for bias in (0, -1.75, -3.5, -4.75, -6.5):
            subprocess.run([str(args.reference.resolve()), str(path), str(bias)], check=True)
            profiles.append((bias, *build_profile(path)))
    max_modes = max(len(poles) for _, _, poles, _, _ in profiles)
    with args.output.open("w", encoding="utf-8") as out:
        print("// Generated from the continuous LiveSPICE circuit. Do not edit.", file=out)
        print(f"constexpr int maxPhaseModes = {max_modes};", file=out)
        print("""struct PhaseProfile {
    double bias;
    int modes;
    double impulse[portCount][phaseTaps];
    Complex poles[maxPhaseModes];
    Complex weights[maxPhaseModes][portCount];
    double slopes[deviceVoltages];
};""", file=out)
        print(f"constexpr std::array<PhaseProfile, {len(profiles)}> phaseProfiles{{{{", file=out)
        for bias, impulse, poles, weights, slopes in profiles:
            print(f"    {{ {bias}, {len(poles)},", file=out)
            for values in (impulse, poles, weights, slopes):
                print(f"      {initializer(values)},", file=out)
            print("    },", file=out)
        print("}};", file=out)


if __name__ == "__main__":
    main()

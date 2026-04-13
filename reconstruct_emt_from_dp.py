#!/usr/bin/env python3

import argparse
import csv
import re
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Reconstruct a real EMT waveform from dynamic phasors logged in CSV format."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("input.log"),
        help="Path to the DP log file.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("reconstructed_emt.csv"),
        help="Path to the reconstructed EMT CSV.",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=50.0,
        help="Fundamental frequency used by the DP transform in Hz.",
    )
    parser.add_argument(
        "--harmonics",
        default="0,1,3,5,7",
        help="Comma-separated harmonic orders matching the phasor columns in the log.",
    )
    parser.add_argument(
        "--mapping",
        choices=("positional", "suffix"),
        default="positional",
        help="Map columns to harmonic orders by position or by the header suffix.",
    )
    parser.add_argument(
        "--mode",
        choices=("villas-inverse", "carrier-synthesis"),
        default="villas-inverse",
        help="Use the exact inverse from VILLAS dp.cpp or a dense carrier synthesis approximation.",
    )
    parser.add_argument(
        "--dt",
        type=float,
        default=0.01,
        help="DP timestep from the hook configuration in seconds.",
    )
    parser.add_argument(
        "--emt-rate",
        type=float,
        default=5000.0,
        help="Output EMT sample rate in Hz.",
    )
    parser.add_argument(
        "--interpolation",
        choices=("zoh", "linear"),
        default="linear",
        help="How to interpolate phasor values between received DP samples.",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        help="Save a PNG plot next to the output CSV if matplotlib is available.",
    )
    return parser.parse_args()


def parse_complex(value):
    return complex(value.replace("i", "j"))


def load_dp_log(path):
    with path.open(newline="") as handle:
        reader = csv.reader(handle)
        header = None
        rows = []

        for row in reader:
            if not row:
                continue
            if row[0].lstrip().startswith("#"):
                if header is None:
                    header = row
                continue
            if header is None:
                header = row
                continue
            rows.append(row)

    if header is None:
        raise ValueError(f"{path} is empty.")

    if header[0].startswith("#"):
        header[0] = header[0].lstrip("#").strip()

    if len(header) < 5:
        raise ValueError(f"{path} does not look like a DP log file.")
    if not rows:
        raise ValueError(f"{path} is empty.")

    timestamps = np.empty(len(rows), dtype=float)
    coeffs = np.empty((len(rows), len(header) - 4), dtype=np.complex128)

    for idx, row in enumerate(rows):
        secs = int(row[0])
        nsecs = int(row[1])
        timestamps[idx] = secs + nsecs * 1e-9
        coeffs[idx, :] = [parse_complex(value) for value in row[4:]]

    timestamps -= timestamps[0]
    return header[4:], timestamps, coeffs


def infer_harmonics_from_header(column_names, configured_harmonics):
    suffix_pattern = re.compile(r"_harm(\d+)$")
    inferred = []

    for column_name in column_names:
        match = suffix_pattern.search(column_name)
        if not match:
            raise ValueError(
                f"Cannot infer harmonic index from column '{column_name}'."
            )

        harmonic_index = int(match.group(1))
        if harmonic_index >= len(configured_harmonics):
            raise ValueError(
                f"Column '{column_name}' refers to harmonic index {harmonic_index}, "
                f"but only {len(configured_harmonics)} configured harmonics were provided."
            )

        inferred.append(configured_harmonics[harmonic_index])

    return inferred


def interpolate_coefficients(sample_times, coeffs, emt_times, mode):
    if mode == "zoh":
        indices = np.searchsorted(sample_times, emt_times, side="right") - 1
        indices = np.clip(indices, 0, len(sample_times) - 1)
        return coeffs[indices]

    real_interp = np.vstack(
        [np.interp(emt_times, sample_times, coeffs[:, col].real) for col in range(coeffs.shape[1])]
    ).T
    imag_interp = np.vstack(
        [np.interp(emt_times, sample_times, coeffs[:, col].imag) for col in range(coeffs.shape[1])]
    ).T
    return real_interp + 1j * imag_interp


def reconstruct_signal(emt_times, coeffs, harmonics, f0):
    signal = np.zeros_like(emt_times, dtype=float)
    omega_t = 2.0 * np.pi * f0 * emt_times

    for column, harmonic in enumerate(harmonics):
        contribution = coeffs[:, column] * np.exp(1j * harmonic * omega_t)
        scale = 1.0 if harmonic == 0 else 2.0
        signal += scale * contribution.real

    return signal


def reconstruct_villas_inverse(sample_count, coeffs, harmonics, dt):
    times = np.arange(sample_count, dtype=float) * dt
    signal = np.zeros(sample_count, dtype=float)

    for column, harmonic in enumerate(harmonics):
        signal += (coeffs[:, column] * np.exp(1j * 2.0 * np.pi * harmonic * times)).real

    return times, signal


def write_output(path, emt_times, signal):
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["time_s", "emt"])
        for time_value, sample in zip(emt_times, signal):
            writer.writerow([f"{time_value:.12f}", f"{sample:.12f}"])


def maybe_plot(output_path, emt_times, signal, sample_times, coeffs, harmonics, f0):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    fig, (ax0, ax1) = plt.subplots(2, 1, figsize=(10, 6), sharex=False)
    ax0.plot(emt_times, signal, linewidth=1.0)
    ax0.set_title("Reconstructed EMT waveform")
    ax0.set_xlabel("Time [s]")
    ax0.set_ylabel("Amplitude")
    ax0.grid(True)

    for column, harmonic in enumerate(harmonics):
        ax1.plot(sample_times, np.abs(coeffs[:, column]), label=f"h={harmonic}")
    ax1.set_title(f"Dynamic phasor magnitudes (f0={f0:g} Hz)")
    ax1.set_xlabel("Time [s]")
    ax1.set_ylabel("|c_k|")
    ax1.grid(True)
    ax1.legend()

    fig.tight_layout()
    fig.savefig(output_path.with_suffix(".png"), dpi=150)
    plt.close(fig)


def main():
    args = parse_args()
    configured_harmonics = [int(part.strip()) for part in args.harmonics.split(",") if part.strip()]
    column_names, sample_times, coeffs = load_dp_log(args.input)

    if coeffs.shape[1] != len(configured_harmonics):
        raise ValueError(
            f"Log contains {coeffs.shape[1]} phasor columns ({column_names}), "
            f"but {len(configured_harmonics)} harmonic orders were provided."
        )

    if args.mapping == "suffix":
        harmonics = infer_harmonics_from_header(column_names, configured_harmonics)
    else:
        harmonics = configured_harmonics

    duration = sample_times[-1]
    if args.mode == "villas-inverse":
        emt_times, signal = reconstruct_villas_inverse(len(sample_times), coeffs, harmonics, args.dt)
    else:
        emt_step = 1.0 / args.emt_rate
        emt_times = np.arange(0.0, duration + 0.5 * emt_step, emt_step)
        emt_coeffs = interpolate_coefficients(sample_times, coeffs, emt_times, args.interpolation)
        signal = reconstruct_signal(emt_times, emt_coeffs, harmonics, args.f0)

    write_output(args.output, emt_times, signal)
    print(f"Wrote {len(emt_times)} EMT samples to {args.output}")
    print("Column mapping:")
    for column_name, harmonic in zip(column_names, harmonics):
        print(f"  {column_name} -> harmonic {harmonic}")

    if args.plot:
        maybe_plot(args.output, emt_times, signal, sample_times, coeffs, harmonics, args.f0)


if __name__ == "__main__":
    main()

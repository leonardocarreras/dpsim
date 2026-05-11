#!/usr/bin/env python3

import argparse
import csv
import re
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Reconstruct EMT waveforms from first-harmonic DP coefficients using a symmetric candidate inverse."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-raw.csv"),
        help="Path to the raw EMT CSV.",
    )
    parser.add_argument(
        "--dp",
        type=Path,
        default=Path("villas-conf2-threephase-dp.csv"),
        help="Path to the DP CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("candidate_first_harmonic_comparison.csv"),
        help="Output CSV path.",
    )
    parser.add_argument(
        "--plot",
        type=Path,
        default=Path("candidate_first_harmonic_comparison.png"),
        help="Output PNG path.",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=60.0,
        help="Fundamental frequency in Hz.",
    )
    parser.add_argument(
        "--dt",
        type=float,
        default=0.00005,
        help="Sample period in seconds.",
    )
    parser.add_argument(
        "--harmonic",
        type=int,
        default=1,
        help="Physical harmonic order represented by the DP column.",
    )
    return parser.parse_args()


def load_csv(path: Path):
    with path.open(newline="") as handle:
        reader = csv.reader(handle)
        header = None
        rows = []
        for row in reader:
            if not row:
                continue
            if row[0].lstrip().startswith("#"):
                cleaned = [row[0].strip()] + [field.split("#", 1)[0].strip() for field in row[1:]]
                if header is None:
                    header = cleaned
                continue
            cleaned = [field.split("#", 1)[0].strip() for field in row]
            cleaned = [field for field in cleaned if field != ""]
            if not cleaned:
                continue
            if header is None:
                header = cleaned
                continue
            if len(cleaned) < len(header):
                continue
            rows.append(cleaned)

    if header is None or not rows:
        raise ValueError(f"{path} is empty.")

    if header[0].startswith("#"):
        header[0] = header[0].lstrip("#").strip()

    return [col.strip() for col in header], rows


def find_last_segment_start(sequences):
    resets = np.flatnonzero(np.diff(sequences) < 0)
    if len(resets) == 0:
        return 0
    return int(resets[-1] + 1)


def load_raw(path: Path):
    header, rows = load_csv(path)
    phase_names = header[4:]
    time_values = []
    sequence_values = []
    values = {name: [] for name in phase_names}

    for row in rows:
        try:
            time_value = int(row[0]) + int(row[1]) * 1e-9
            sequence_value = int(row[3])
            samples = [float(row[idx]) for idx in range(4, 4 + len(phase_names))]
        except (ValueError, IndexError):
            continue
        time_values.append(time_value)
        sequence_values.append(sequence_value)
        for name, sample in zip(phase_names, samples):
            values[name].append(sample)

    times = np.asarray(time_values, dtype=float)
    sequences = np.asarray(sequence_values, dtype=int)
    start = find_last_segment_start(sequences)
    times = times[start:] - times[start]
    sequences = sequences[start:]
    for name in values:
        values[name] = np.asarray(values[name], dtype=float)[start:]
    return times, sequences, values


def parse_complex(value: str):
    return complex(value.replace("i", "j"))


def load_dp(path: Path, harmonic: int):
    header, rows = load_csv(path)
    pattern = re.compile(r"(.+)_harm(\d+)$")
    phase_names = []

    for name in header[4:]:
        match = pattern.fullmatch(name)
        if not match:
            raise ValueError(f"Unexpected DP column name: {name}")
        phase_names.append(match.group(1))

    time_values = []
    sequence_values = []
    values = {name: [] for name in phase_names}

    for row in rows:
        try:
            time_value = int(row[0]) + int(row[1]) * 1e-9
            sequence_value = int(row[3])
            samples = [parse_complex(row[idx]) for idx in range(4, 4 + len(phase_names))]
        except (ValueError, IndexError):
            continue
        time_values.append(time_value)
        sequence_values.append(sequence_value)
        for name, sample in zip(phase_names, samples):
            values[name].append(sample)

    times = np.asarray(time_values, dtype=float)
    sequences = np.asarray(sequence_values, dtype=int)
    start = find_last_segment_start(sequences)
    times = times[start:] - times[start]
    sequences = sequences[start:]
    for name in values:
        values[name] = np.asarray(values[name], dtype=np.complex128)[start:]
    return times, sequences, values, harmonic


def align(raw_times, raw_sequences, raw_values, dp_sequences, dp_values):
    common_sequences, raw_idx, dp_idx = np.intersect1d(
        raw_sequences, dp_sequences, assume_unique=False, return_indices=True
    )
    if len(common_sequences) == 0:
        raise ValueError("No common sequences found.")
    times = common_sequences.astype(float) - common_sequences[0]
    aligned_raw = {name: values[raw_idx] for name, values in raw_values.items()}
    aligned_dp = {name: values[dp_idx] for name, values in dp_values.items()}
    return times, common_sequences, aligned_raw, aligned_dp


def reconstruct_first_harmonic(coeffs, harmonic, f0, times_s):
    return 2.0 * np.real(coeffs * np.exp(-1j * 2.0 * np.pi * harmonic * f0 * times_s))


def write_output(path: Path, sequences, times_s, raw_values, recon_values):
    phase_names = list(raw_values.keys())
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        header = ["sequence", "time_s"]
        for name in phase_names:
            header.extend([name, f"{name}_reconstructed", f"{name}_error"])
        writer.writerow(header)
        for idx, seq in enumerate(sequences):
            row = [int(seq), f"{times_s[idx]:.12f}"]
            for name in phase_names:
                raw = raw_values[name][idx]
                recon = recon_values[name][idx]
                row.extend([f"{raw:.12f}", f"{recon:.12f}", f"{(recon - raw):.12f}"])
            writer.writerow(row)


def maybe_plot(path: Path, sequences, raw_values, recon_values):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    phase_names = list(raw_values.keys())
    fig, axes = plt.subplots(len(phase_names), 2, figsize=(14, 3.6 * len(phase_names)), sharex="col")
    if len(phase_names) == 1:
        axes = [axes]

    fig.suptitle("Candidate first-harmonic inverse: original vs reconstructed")

    for row_axes, name in zip(axes, phase_names):
        ax_signal, ax_error = row_axes
        error = recon_values[name] - raw_values[name]
        ax_signal.plot(sequences, raw_values[name], label="original", linewidth=1.0)
        ax_signal.plot(sequences, recon_values[name], label="candidate reconstructed", linewidth=1.0)
        ax_signal.set_title(f"{name}: original vs reconstructed")
        ax_signal.grid(True)
        ax_signal.legend(loc="upper right")
        ax_signal.set_ylabel("Amplitude")

        ax_error.plot(sequences, error, color="tab:red", linewidth=1.0)
        ax_error.set_title(f"{name}: reconstructed - original")
        ax_error.grid(True)
        ax_error.set_ylabel("Error")

    axes[-1][0].set_xlabel("Sequence")
    axes[-1][1].set_xlabel("Sequence")
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    args = parse_args()
    raw_times, raw_sequences, raw_values = load_raw(args.raw)
    _, dp_sequences, dp_values, harmonic = load_dp(args.dp, args.harmonic)
    times_idx, sequences, raw_values, dp_values = align(
        raw_times, raw_sequences, raw_values, dp_sequences, dp_values
    )
    times_s = times_idx * args.dt

    recon_values = {}
    for name in raw_values:
        recon_values[name] = reconstruct_first_harmonic(
            dp_values[name], harmonic, args.f0, times_s
        )

    write_output(args.output, sequences, times_s, raw_values, recon_values)
    print(f"Wrote comparison CSV to {args.output}")
    print(f"Candidate inverse: x_hat = 2 * Re{{ c_h * exp(-j 2*pi*h*f0*t) }} with h={harmonic}")
    for name in raw_values:
        error = recon_values[name] - raw_values[name]
        rmse = np.sqrt(np.mean(error ** 2))
        max_abs = np.max(np.abs(error))
        print(f"{name}: RMSE={rmse:.6f}, max|error|={max_abs:.6f}")

    maybe_plot(args.plot, sequences, raw_values, recon_values)
    print(f"Wrote plot to {args.plot}")


if __name__ == "__main__":
    main()

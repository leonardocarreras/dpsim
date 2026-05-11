#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot raw vs reconstructed three-phase round-trip waveforms and their errors."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-raw.csv"),
        help="Path to the raw waveform CSV.",
    )
    parser.add_argument(
        "--reconstructed",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-reconstructed.csv"),
        help="Path to the reconstructed waveform CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-comparison.png"),
        help="Output PNG path.",
    )
    parser.add_argument(
        "--x-axis",
        choices=("sequence", "time"),
        default="sequence",
        help="Use sequence number or time on the x-axis.",
    )
    return parser.parse_args()


def load_waveform_csv(path: Path):
    with path.open(newline="") as handle:
        reader = csv.reader(handle)
        header = None
        rows = []

        for row in reader:
            if not row:
                continue
            if row[0].lstrip().startswith("#"):
                if header is None:
                    header = [cell.lstrip("#").strip() for cell in row]
                continue
            rows.append(row)

    if header is None or not rows:
        raise ValueError(f"{path} is empty.")

    times = []
    sequences = []
    column_names = [name.strip() for name in header[4:]]
    values = {index: [] for index in range(len(column_names))}

    for row in rows:
        try:
            secs = int(row[0])
            nsecs = int(row[1])
            seq = int(row[3])
            sample_values = [float(row[idx]) for idx in range(4, len(header))]
        except (ValueError, IndexError):
            continue

        times.append(secs + nsecs * 1e-9)
        sequences.append(seq)
        for index, value in enumerate(sample_values):
            values[index].append(value)

    times = np.asarray(times, dtype=float)
    sequences = np.asarray(sequences, dtype=int)
    times -= times[0]
    for index in values:
        values[index] = np.asarray(values[index], dtype=float)

    return times, sequences, column_names, values


def align_waveforms(raw_times, raw_sequences, raw_values, recon_times, recon_sequences, recon_values):
    common_sequences, raw_idx, recon_idx = np.intersect1d(
        raw_sequences, recon_sequences, assume_unique=False, return_indices=True
    )
    if len(common_sequences) == 0:
        raise ValueError("No common sequence numbers found between raw and reconstructed waveforms.")

    aligned_times = raw_times[raw_idx]
    aligned_raw = {column: values[raw_idx] for column, values in raw_values.items()}
    aligned_recon = {column: values[recon_idx] for column, values in recon_values.items()}
    return aligned_times, common_sequences, aligned_raw, aligned_recon


def plot_roundtrip(output_path: Path, times, sequences, raw_column_names, raw_values, recon_values, x_axis: str):
    phases = [index for index in raw_values.keys() if index in recon_values]
    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"

    fig, axes = plt.subplots(len(phases), 2, figsize=(14, 3.6 * len(phases)), sharex="col")
    if len(phases) == 1:
        axes = [axes]

    fig.suptitle("Three-phase DP round-trip waveform comparison")

    for row_axes, phase in zip(axes, phases):
        ax_signal, ax_error = row_axes
        error = recon_values[phase] - raw_values[phase]
        phase_label = raw_column_names[phase] if phase < len(raw_column_names) else f"phase_{phase}"

        ax_signal.plot(x, raw_values[phase], label="original", linewidth=1.0)
        ax_signal.plot(x, recon_values[phase], label="reconstructed", linewidth=1.0)
        ax_signal.set_title(f"{phase_label}: original vs reconstructed")
        ax_signal.set_ylabel("Amplitude")
        ax_signal.grid(True)
        ax_signal.legend(loc="upper right")

        ax_error.plot(x, error, color="tab:red", linewidth=1.0)
        ax_error.set_title(f"{phase_label}: reconstructed - original")
        ax_error.set_ylabel("Error")
        ax_error.grid(True)

    axes[-1][0].set_xlabel(x_label)
    axes[-1][1].set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    plt.close(fig)


def main():
    args = parse_args()
    raw_times, raw_sequences, raw_column_names, raw_values = load_waveform_csv(args.raw)
    recon_times, recon_sequences, _, recon_values = load_waveform_csv(args.reconstructed)
    times, sequences, raw_values, recon_values = align_waveforms(
        raw_times, raw_sequences, raw_values, recon_times, recon_sequences, recon_values
    )
    plot_roundtrip(args.output, times, sequences, raw_column_names, raw_values, recon_values, args.x_axis)
    print(f"Wrote plot to {args.output}")


if __name__ == "__main__":
    main()

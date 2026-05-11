#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare original waveform, offline DP reconstruction, and VILLAS internal round-trip waveform."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-raw.csv"),
        help="Path to the original/raw waveform CSV.",
    )
    parser.add_argument(
        "--dp-reconstructed",
        type=Path,
        default=Path("candidate_first_harmonic_comparison.csv"),
        help="Path to the offline DP reconstruction comparison CSV.",
    )
    parser.add_argument(
        "--villas-internal",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-reconstructed.csv"),
        help="Path to the VILLAS internal round-trip waveform CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("dp_vs_villas_internal_comparison.png"),
        help="Output PNG path.",
    )
    parser.add_argument(
        "--x-axis",
        choices=("sequence", "time"),
        default="sequence",
        help="Use sequence or time on the x-axis.",
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

    phase_names = [name.strip() for name in header[4:]]
    times = []
    sequences = []
    values = {index: [] for index in range(len(phase_names))}
    for row in rows:
        try:
            secs = int(row[0])
            nsecs = int(row[1])
            seq = int(row[3])
            samples = [float(row[idx]) for idx in range(4, len(header))]
        except (ValueError, IndexError):
            continue
        times.append(secs + nsecs * 1e-9)
        sequences.append(seq)
        for index, sample in enumerate(samples):
            values[index].append(sample)

    times = np.asarray(times, dtype=float)
    sequences = np.asarray(sequences, dtype=int)
    times -= times[0]
    for index in values:
        values[index] = np.asarray(values[index], dtype=float)
    return times, sequences, phase_names, values


def load_offline_comparison_csv(path: Path):
    with path.open(newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader)
        rows = list(reader)

    col_idx = {name.strip(): idx for idx, name in enumerate(header)}
    phase_names = []
    for name in header:
        clean = name.strip()
        if clean in ("sequence", "time_s"):
            continue
        if clean.endswith("_reconstructed") or clean.endswith("_error"):
            continue
        phase_names.append(clean)

    times = np.asarray([float(row[col_idx["time_s"]]) for row in rows], dtype=float)
    sequences = np.asarray([int(row[col_idx["sequence"]]) for row in rows], dtype=int)
    values = {index: np.asarray([float(row[col_idx[name + "_reconstructed"]]) for row in rows], dtype=float)
              for index, name in enumerate(phase_names)}
    return times, sequences, phase_names, values


def align_three(raw_times, raw_sequences, raw_values, dp_sequences, dp_values, villas_sequences, villas_values):
    common_sequences = np.intersect1d(np.intersect1d(raw_sequences, dp_sequences), villas_sequences)
    if len(common_sequences) == 0:
        raise ValueError("No common sequence numbers found across the three inputs.")

    raw_pos = np.searchsorted(raw_sequences, common_sequences)
    dp_pos = np.searchsorted(dp_sequences, common_sequences)
    villas_pos = np.searchsorted(villas_sequences, common_sequences)

    times = raw_times[raw_pos]
    aligned_raw = {index: values[raw_pos] for index, values in raw_values.items()}
    aligned_dp = {index: values[dp_pos] for index, values in dp_values.items()}
    aligned_villas = {index: values[villas_pos] for index, values in villas_values.items()}
    return times, common_sequences, aligned_raw, aligned_dp, aligned_villas


def maybe_plot(path: Path, times, sequences, phase_names, raw_values, dp_values, villas_values, x_axis: str):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"
    phase_indices = list(raw_values.keys())

    fig, axes = plt.subplots(len(phase_indices), 4, figsize=(24, 4.0 * len(phase_indices)), sharex="col")
    if len(phase_indices) == 1:
        axes = [axes]

    fig.suptitle("Original vs DP reconstruction and VILLAS internal waveform")

    for row_axes, index in zip(axes, phase_indices):
        ax_dp_signal, ax_dp_err, ax_villas_signal, ax_villas_err = row_axes
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        dp_error = dp_values[index] - raw_values[index]
        villas_error = villas_values[index] - raw_values[index]

        ax_dp_signal.plot(x, raw_values[index], label="original", linewidth=1.0, color="black")
        ax_dp_signal.plot(x, dp_values[index], label="reconstructed from DP", linewidth=1.0, color="tab:orange")
        ax_dp_signal.set_title(f"{label}: original vs DP reconstruction")
        ax_dp_signal.grid(True)
        ax_dp_signal.legend(loc="upper right")
        ax_dp_signal.set_ylabel("Amplitude")

        ax_dp_err.plot(x, dp_error, color="tab:orange", linewidth=1.0)
        ax_dp_err.set_title(f"{label}: DP reconstruction - original")
        ax_dp_err.grid(True)
        ax_dp_err.set_ylabel("Error")

        ax_villas_signal.plot(x, raw_values[index], label="original", linewidth=1.0, color="black")
        ax_villas_signal.plot(x, villas_values[index], label="VILLAS internal signal", linewidth=1.0, color="tab:blue")
        ax_villas_signal.set_title(f"{label}: original vs VILLAS internal")
        ax_villas_signal.grid(True)
        ax_villas_signal.legend(loc="upper right")
        ax_villas_signal.set_ylabel("Amplitude")

        ax_villas_err.plot(x, villas_error, color="tab:red", linewidth=1.0)
        ax_villas_err.set_title(f"{label}: VILLAS internal - original")
        ax_villas_err.grid(True)
        ax_villas_err.set_ylabel("Error")

    for ax in axes[-1]:
        ax.set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def print_metrics(phase_names, raw_values, dp_values, villas_values):
    for index in raw_values:
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        dp_error = dp_values[index] - raw_values[index]
        villas_error = villas_values[index] - raw_values[index]
        print(
            f"{label}: "
            f"DP RMSE={np.sqrt(np.mean(dp_error ** 2)):.6f}, max|error|={np.max(np.abs(dp_error)):.6f}; "
            f"VILLAS RMSE={np.sqrt(np.mean(villas_error ** 2)):.6f}, max|error|={np.max(np.abs(villas_error)):.6f}"
        )


def main():
    args = parse_args()
    raw_times, raw_sequences, raw_phase_names, raw_values = load_waveform_csv(args.raw)
    dp_times, dp_sequences, _, dp_values = load_offline_comparison_csv(args.dp_reconstructed)
    villas_times, villas_sequences, _, villas_values = load_waveform_csv(args.villas_internal)

    times, sequences, raw_values, dp_values, villas_values = align_three(
        raw_times, raw_sequences, raw_values, dp_sequences, dp_values, villas_sequences, villas_values
    )
    maybe_plot(args.output, times, sequences, raw_phase_names, raw_values, dp_values, villas_values, args.x_axis)
    print(f"Wrote plot to {args.output}")
    print_metrics(raw_phase_names, raw_values, dp_values, villas_values)


if __name__ == "__main__":
    main()

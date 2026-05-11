#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Low-pass filter the reconstructed round-trip waveform to extract its slow component."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-raw.csv"),
        help="Path to the raw waveform CSV. Used only for phase labels and alignment.",
    )
    parser.add_argument(
        "--reconstructed",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-reconstructed.csv"),
        help="Path to the reconstructed round-trip waveform CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-lowfreq.csv"),
        help="Output CSV path.",
    )
    parser.add_argument(
        "--plot",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-lowfreq.png"),
        help="Output PNG path.",
    )
    parser.add_argument(
        "--dt",
        type=float,
        default=0.00005,
        help="Sample time in seconds.",
    )
    parser.add_argument(
        "--cutoff",
        type=float,
        default=10.0,
        help="Low-pass cutoff frequency in Hz for extracting the slow component.",
    )
    parser.add_argument(
        "--x-axis",
        choices=("sequence", "time"),
        default="sequence",
        help="Use sequence or time on the plot x-axis.",
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

    column_names = [name.strip() for name in header[4:]]
    times = []
    sequences = []
    values = {index: [] for index in range(len(column_names))}

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

    return times, sequences, column_names, values


def align_waveforms(raw_times, raw_sequences, raw_values, recon_times, recon_sequences, recon_values):
    common_sequences, raw_idx, recon_idx = np.intersect1d(
        raw_sequences, recon_sequences, assume_unique=False, return_indices=True
    )
    if len(common_sequences) == 0:
        raise ValueError("No common sequence numbers found between raw and reconstructed waveforms.")

    aligned_times = raw_times[raw_idx]
    aligned_raw = {index: values[raw_idx] for index, values in raw_values.items()}
    aligned_recon = {index: values[recon_idx] for index, values in recon_values.items()}
    return aligned_times, common_sequences, aligned_raw, aligned_recon


def lowpass_fft(signal: np.ndarray, dt: float, cutoff_hz: float):
    freqs = np.fft.rfftfreq(len(signal), d=dt)
    spectrum = np.fft.rfft(signal)
    spectrum[freqs > cutoff_hz] = 0
    return np.fft.irfft(spectrum, n=len(signal))


def write_output(path: Path, times, sequences, phase_names, reconstructed, lowfreq):
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        header = ["sequence", "time_s"]
        for index, name in enumerate(phase_names):
            phase_label = name if index < len(phase_names) else f"phase_{index}"
            header.extend(
                [
                    f"{phase_label}_reconstructed",
                    f"{phase_label}_lowfreq",
                    f"{phase_label}_highfreq",
                ]
            )
        writer.writerow(header)

        for idx, seq in enumerate(sequences):
            row = [int(seq), f"{times[idx]:.12f}"]
            for index in reconstructed:
                recon = reconstructed[index][idx]
                low = lowfreq[index][idx]
                row.extend([f"{recon:.12f}", f"{low:.12f}", f"{(recon - low):.12f}"])
            writer.writerow(row)


def maybe_plot(path: Path, times, sequences, phase_names, reconstructed, lowfreq, x_axis: str, cutoff_hz: float):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"
    phase_indices = list(reconstructed.keys())

    fig, axes = plt.subplots(len(phase_indices), 2, figsize=(16, 3.8 * len(phase_indices)), sharex="col")
    if len(phase_indices) == 1:
        axes = [axes]

    fig.suptitle(f"Round-trip reconstructed waveform split into low/high-frequency parts (cutoff={cutoff_hz:g} Hz)")

    for row_axes, index in zip(axes, phase_indices):
        ax_signal, ax_resid = row_axes
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        highfreq = reconstructed[index] - lowfreq[index]

        ax_signal.plot(x, reconstructed[index], label="reconstructed", linewidth=1.0)
        ax_signal.plot(x, lowfreq[index], label="low-frequency component", linewidth=1.0)
        ax_signal.set_title(f"{label}: reconstructed vs low-frequency component")
        ax_signal.grid(True)
        ax_signal.legend(loc="upper right")
        ax_signal.set_ylabel("Amplitude")

        ax_resid.plot(x, highfreq, color="tab:red", linewidth=1.0)
        ax_resid.set_title(f"{label}: removed high-frequency component")
        ax_resid.grid(True)
        ax_resid.set_ylabel("Amplitude")

    axes[-1][0].set_xlabel(x_label)
    axes[-1][1].set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    args = parse_args()
    raw_times, raw_sequences, raw_names, raw_values = load_waveform_csv(args.raw)
    recon_times, recon_sequences, _, recon_values = load_waveform_csv(args.reconstructed)
    times, sequences, _, reconstructed = align_waveforms(
        raw_times, raw_sequences, raw_values, recon_times, recon_sequences, recon_values
    )

    lowfreq = {
        index: lowpass_fft(signal, args.dt, args.cutoff)
        for index, signal in reconstructed.items()
    }

    write_output(args.output, times, sequences, raw_names, reconstructed, lowfreq)
    maybe_plot(args.plot, times, sequences, raw_names, reconstructed, lowfreq, args.x_axis, args.cutoff)

    print(f"Wrote low-frequency CSV to {args.output}")
    print(f"Wrote plot to {args.plot}")
    for index, signal in reconstructed.items():
        highfreq = signal - lowfreq[index]
        low_rms = np.sqrt(np.mean(lowfreq[index] ** 2))
        high_rms = np.sqrt(np.mean(highfreq ** 2))
        label = raw_names[index] if index < len(raw_names) else f"phase_{index}"
        print(f"{label}: lowfreq RMS={low_rms:.6f}, highfreq RMS={high_rms:.6f}")


if __name__ == "__main__":
    main()

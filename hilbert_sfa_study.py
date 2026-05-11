#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Study the SFA dynamic phasor definition via the analytic signal / Hilbert transform."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-raw.csv"),
        help="Path to the original/raw waveform CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("hilbert_sfa_study.csv"),
        help="Output CSV path.",
    )
    parser.add_argument(
        "--plot",
        type=Path,
        default=Path("hilbert_sfa_study.png"),
        help="Output PNG path.",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=60.0,
        help="Fundamental frequency in Hz.",
    )
    parser.add_argument(
        "--x-axis",
        choices=("sequence", "time"),
        default="sequence",
        help="Use sequence or time on the x-axis.",
    )
    parser.add_argument(
        "--waveform-plot",
        type=Path,
        default=Path("hilbert_sfa_waveform.png"),
        help="Output PNG path for waveform-only plots.",
    )
    parser.add_argument(
        "--envelope-plot",
        type=Path,
        default=Path("hilbert_sfa_envelope.png"),
        help="Output PNG path for envelope/baseband-only plots.",
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


def analytic_signal(x: np.ndarray):
    n = len(x)
    spectrum = np.fft.fft(x)
    h = np.zeros(n)
    if n % 2 == 0:
        h[0] = 1.0
        h[n // 2] = 1.0
        h[1:n // 2] = 2.0
    else:
        h[0] = 1.0
        h[1:(n + 1) // 2] = 2.0
    return np.fft.ifft(spectrum * h)


def sfa_first_harmonic(x: np.ndarray, times: np.ndarray, f0: float):
    z = analytic_signal(x)
    carrier = np.exp(-1j * 2.0 * np.pi * f0 * times)
    # For a real sinusoid x(t) = 2 Re{X1(t) e^{j w0 t}}, the baseband phasor is 0.5 * z * e^{-j w0 t}
    x1 = 0.5 * z * carrier
    xhat = 2.0 * np.real(x1 * np.exp(1j * 2.0 * np.pi * f0 * times))
    return z, x1, xhat


def write_output(path: Path, times, sequences, phase_names, raw_values, x1_values, recon_values):
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        header = ["sequence", "time_s"]
        for index, name in enumerate(phase_names):
            phase_label = name if index < len(phase_names) else f"phase_{index}"
            header.extend(
                [
                    phase_label,
                    f"{phase_label}_x1_real",
                    f"{phase_label}_x1_imag",
                    f"{phase_label}_reconstructed",
                    f"{phase_label}_error",
                ]
            )
        writer.writerow(header)
        for idx, seq in enumerate(sequences):
            row = [int(seq), f"{times[idx]:.12f}"]
            for index in raw_values:
                raw = raw_values[index][idx]
                x1 = x1_values[index][idx]
                recon = recon_values[index][idx]
                row.extend(
                    [
                        f"{raw:.12f}",
                        f"{x1.real:.12f}",
                        f"{x1.imag:.12f}",
                        f"{recon:.12f}",
                        f"{(recon - raw):.12f}",
                    ]
                )
            writer.writerow(row)


def maybe_plot(path: Path, times, sequences, phase_names, raw_values, x1_values, recon_values, x_axis: str):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"
    phase_indices = list(raw_values.keys())

    fig, axes = plt.subplots(len(phase_indices), 3, figsize=(20, 4.0 * len(phase_indices)), sharex="col")
    if len(phase_indices) == 1:
        axes = [axes]

    fig.suptitle("Hilbert/SFA first-harmonic definition: baseband phasor and reconstruction")

    for row_axes, index in zip(axes, phase_indices):
        ax_signal, ax_error, ax_env = row_axes
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        error = recon_values[index] - raw_values[index]
        x1 = x1_values[index]

        ax_signal.plot(x, raw_values[index], label="original", linewidth=1.0)
        ax_signal.plot(x, recon_values[index], label="Hilbert/SFA reconstructed", linewidth=1.0)
        ax_signal.set_title(f"{label}: original vs Hilbert/SFA reconstruction")
        ax_signal.grid(True)
        ax_signal.legend(loc="upper right")
        ax_signal.set_ylabel("Amplitude")

        ax_error.plot(x, error, color="tab:red", linewidth=1.0)
        ax_error.set_title(f"{label}: reconstructed - original")
        ax_error.grid(True)
        ax_error.set_ylabel("Error")

        ax_env.plot(x, np.abs(x1), label="|X1|", linewidth=1.0)
        ax_env.plot(x, np.unwrap(np.angle(x1)), label="angle(X1)", linewidth=1.0)
        ax_env.set_title(f"{label}: first-harmonic baseband phasor")
        ax_env.grid(True)
        ax_env.legend(loc="upper right")
        ax_env.set_ylabel("Value")

    axes[-1][0].set_xlabel(x_label)
    axes[-1][1].set_xlabel(x_label)
    axes[-1][2].set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_waveform_only(path: Path, times, sequences, phase_names, raw_values, recon_values, x_axis: str):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping waveform plot generation.")
        return

    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"
    phase_indices = list(raw_values.keys())

    fig, axes = plt.subplots(len(phase_indices), 2, figsize=(14, 3.8 * len(phase_indices)), sharex="col")
    if len(phase_indices) == 1:
        axes = [axes]

    fig.suptitle("Hilbert/SFA waveform reconstruction only")

    for row_axes, index in zip(axes, phase_indices):
        ax_signal, ax_error = row_axes
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        error = recon_values[index] - raw_values[index]

        ax_signal.plot(x, raw_values[index], label="original", linewidth=1.0, color="black")
        ax_signal.plot(x, recon_values[index], label="Hilbert/SFA reconstructed", linewidth=1.0, color="tab:blue")
        ax_signal.set_title(f"{label}: original vs reconstructed")
        ax_signal.grid(True)
        ax_signal.legend(loc="upper right")
        ax_signal.set_ylabel("Amplitude")

        ax_error.plot(x, error, color="tab:red", linewidth=1.0)
        ax_error.set_title(f"{label}: reconstructed - original")
        ax_error.grid(True)
        ax_error.set_ylabel("Error")

    axes[-1][0].set_xlabel(x_label)
    axes[-1][1].set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_envelope_only(path: Path, times, sequences, phase_names, x1_values, x_axis: str):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping envelope plot generation.")
        return

    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"
    phase_indices = list(x1_values.keys())

    fig, axes = plt.subplots(len(phase_indices), 2, figsize=(14, 3.8 * len(phase_indices)), sharex="col")
    if len(phase_indices) == 1:
        axes = [axes]

    fig.suptitle("Hilbert/SFA baseband first-harmonic phasor only")

    for row_axes, index in zip(axes, phase_indices):
        ax_mag, ax_phase = row_axes
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        x1 = x1_values[index]

        ax_mag.plot(x, np.abs(x1), linewidth=1.0, color="tab:green")
        ax_mag.set_title(f"{label}: |X1|")
        ax_mag.grid(True)
        ax_mag.set_ylabel("Magnitude")

        ax_phase.plot(x, np.unwrap(np.angle(x1)), linewidth=1.0, color="tab:purple")
        ax_phase.set_title(f"{label}: angle(X1)")
        ax_phase.grid(True)
        ax_phase.set_ylabel("Phase [rad]")

    axes[-1][0].set_xlabel(x_label)
    axes[-1][1].set_xlabel(x_label)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    args = parse_args()
    times, sequences, phase_names, raw_values = load_waveform_csv(args.raw)

    x1_values = {}
    recon_values = {}
    for index, signal in raw_values.items():
        _, x1, xhat = sfa_first_harmonic(signal, times, args.f0)
        x1_values[index] = x1
        recon_values[index] = xhat

    write_output(args.output, times, sequences, phase_names, raw_values, x1_values, recon_values)
    maybe_plot(args.plot, times, sequences, phase_names, raw_values, x1_values, recon_values, args.x_axis)
    plot_waveform_only(args.waveform_plot, times, sequences, phase_names, raw_values, recon_values, args.x_axis)
    plot_envelope_only(args.envelope_plot, times, sequences, phase_names, x1_values, args.x_axis)
    print(f"Wrote study CSV to {args.output}")
    print(f"Wrote plot to {args.plot}")
    print(f"Wrote waveform-only plot to {args.waveform_plot}")
    print(f"Wrote envelope-only plot to {args.envelope_plot}")
    for index, signal in raw_values.items():
        error = recon_values[index] - signal
        label = phase_names[index] if index < len(phase_names) else f"phase_{index}"
        print(f"{label}: RMSE={np.sqrt(np.mean(error ** 2)):.12f}, max|error|={np.max(np.abs(error)):.12f}")


if __name__ == "__main__":
    main()

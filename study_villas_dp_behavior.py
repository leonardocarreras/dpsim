#!/usr/bin/env python3

"""Study the exact VILLAS dp.cpp behavior against a literature-style DP inverse.

This script reproduces the current VILLAS forward and inverse algorithms from:
  /home/leo/git/github/my/villas-node/lib/hooks/dp.cpp

It then compares:
  1. Exact VILLAS forward -> exact VILLAS inverse
  2. Exact VILLAS forward -> literature-style Fourier synthesis

Literature reference point:
  Dynamic phasors are moving Fourier coefficients over a sliding window, and a
  real waveform is reconstructed from them through Fourier synthesis at the
  fundamental frequency. See:
    - Sanders et al., "Generalized averaging method for power conversion circuits" (1991)
    - "Assessment of dynamic phasor extraction methods for power system co-simulation applications" (2021)
"""

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Reproduce VILLAS dp.cpp forward/inverse behavior and compare it to literature-style reconstruction."
    )
    parser.add_argument(
        "--raw",
        type=Path,
        default=Path("villas-conf2-threephase-roundtrip-raw.csv"),
        help="Path to the raw waveform CSV.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("study_villas_dp_behavior.csv"),
        help="Output comparison CSV.",
    )
    parser.add_argument(
        "--plot",
        type=Path,
        default=Path("study_villas_dp_behavior.png"),
        help="Output plot PNG.",
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
        help="Sample time in seconds.",
    )
    parser.add_argument(
        "--harmonics",
        default="1",
        help="Comma-separated harmonic orders.",
    )
    parser.add_argument(
        "--x-axis",
        choices=("sequence", "time"),
        default="sequence",
        help="Use sequence or time on the plot x-axis.",
    )
    return parser.parse_args()


def parse_harmonics(spec: str):
    return [int(part.strip()) for part in spec.split(",") if part.strip()]


def load_csv(path: Path):
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
    values = {name.strip(): [] for name in header[4:]}
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
        for name, sample in zip(header[4:], samples):
            values[name.strip()].append(sample)

    times = np.asarray(times, dtype=float)
    sequences = np.asarray(sequences, dtype=int)
    times -= times[0]
    for name in values:
        values[name] = np.asarray(values[name], dtype=float)
    return times, sequences, values


@dataclass
class VillasDPResult:
    coeffs: dict[str, np.ndarray]
    villas_inverse: dict[str, np.ndarray]
    sfa_end_inverse: dict[str, np.ndarray]
    sfa_center_inverse: dict[str, np.ndarray]
    sfa_start_inverse: dict[str, np.ndarray]
    corr_compensated_inverse: dict[str, np.ndarray]


def villas_forward_inverse(samples: np.ndarray, f0: float, dt: float, harmonics: list[int]):
    """Exact reproduction of dp.cpp behavior for one scalar signal."""
    n_window = int((1.0 / f0) / dt)
    if n_window <= 0:
        raise ValueError("Invalid window size computed from f0 and dt.")

    window = np.zeros(n_window, dtype=float)
    coeffs = np.zeros((len(samples), len(harmonics)), dtype=np.complex128)
    villas_inverse = np.zeros(len(samples), dtype=float)
    sfa_end_inverse = np.zeros(len(samples), dtype=float)
    sfa_center_inverse = np.zeros(len(samples), dtype=float)
    sfa_start_inverse = np.zeros(len(samples), dtype=float)
    corr_compensated_inverse = np.zeros(len(samples), dtype=float)
    window_period = 1.0 / f0

    time = 0.0
    steps = 0

    for idx, sample in enumerate(samples):
        # window.update(newest): push_back, read oldest at [0], pop_front
        window = np.concatenate((window[1:], np.array([sample], dtype=float)))

        step_coeffs = np.zeros(len(harmonics), dtype=np.complex128)

        for hk, harmonic in enumerate(harmonics):
            om_k = 2j * np.pi * harmonic / n_window
            corr = np.exp(-om_k * (steps - (n_window + 1)))
            x_k = 0j
            for n, x_n in enumerate(window):
                x_k += x_n * np.exp(om_k * n)
            step_coeffs[hk] = x_k / (corr * n_window)

            # Exact VILLAS inverse from dp.cpp
            villas_inverse[idx] += np.real(step_coeffs[hk] * np.exp(2j * np.pi * harmonic * time))

            # SFA / moving-Fourier synthesis candidates with different reference instants
            if harmonic == 0:
                contribution = np.real(step_coeffs[hk])
                sfa_end_inverse[idx] += contribution
                sfa_center_inverse[idx] += contribution
                sfa_start_inverse[idx] += contribution
            else:
                sfa_end_inverse[idx] += 2.0 * np.real(
                    step_coeffs[hk] * np.exp(2j * np.pi * harmonic * f0 * time)
                )
                sfa_center_inverse[idx] += 2.0 * np.real(
                    step_coeffs[hk]
                    * np.exp(2j * np.pi * harmonic * f0 * (time - 0.5 * window_period))
                )
                sfa_start_inverse[idx] += 2.0 * np.real(
                    step_coeffs[hk]
                    * np.exp(2j * np.pi * harmonic * f0 * (time - window_period))
                )
                # Explicitly undo the corr rotating frame and then remodulate at the carrier.
                stationary_coeff = step_coeffs[hk] * corr
                corr_compensated_inverse[idx] += 2.0 * np.real(
                    stationary_coeff * np.exp(2j * np.pi * harmonic * f0 * time)
                )

        coeffs[idx, :] = step_coeffs
        time += dt
        steps += 1

    return (
        coeffs,
        villas_inverse,
        sfa_end_inverse,
        sfa_center_inverse,
        sfa_start_inverse,
        corr_compensated_inverse,
    )


def run_study(values: dict[str, np.ndarray], f0: float, dt: float, harmonics: list[int]):
    coeffs = {}
    villas_inverse = {}
    sfa_end_inverse = {}
    sfa_center_inverse = {}
    sfa_start_inverse = {}
    corr_compensated_inverse = {}

    for phase, samples in values.items():
        (
            phase_coeffs,
            phase_villas,
            phase_sfa_end,
            phase_sfa_center,
            phase_sfa_start,
            phase_corr_comp,
        ) = villas_forward_inverse(samples, f0, dt, harmonics)
        coeffs[phase] = phase_coeffs
        villas_inverse[phase] = phase_villas
        sfa_end_inverse[phase] = phase_sfa_end
        sfa_center_inverse[phase] = phase_sfa_center
        sfa_start_inverse[phase] = phase_sfa_start
        corr_compensated_inverse[phase] = phase_corr_comp

    return VillasDPResult(
        coeffs,
        villas_inverse,
        sfa_end_inverse,
        sfa_center_inverse,
        sfa_start_inverse,
        corr_compensated_inverse,
    )


def write_output(path: Path, sequences, times, raw_values, result: VillasDPResult):
    phase_names = list(raw_values.keys())
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        header = ["sequence", "time_s"]
        for phase in phase_names:
            header.extend(
                [
                    phase,
                    f"{phase}_villas_inverse",
                    f"{phase}_villas_error",
                    f"{phase}_sfa_end_inverse",
                    f"{phase}_sfa_end_error",
                    f"{phase}_sfa_center_inverse",
                    f"{phase}_sfa_center_error",
                    f"{phase}_sfa_start_inverse",
                    f"{phase}_sfa_start_error",
                    f"{phase}_corr_comp_inverse",
                    f"{phase}_corr_comp_error",
                ]
            )
        writer.writerow(header)

        for idx, seq in enumerate(sequences):
            row = [int(seq), f"{times[idx]:.12f}"]
            for phase in phase_names:
                raw = raw_values[phase][idx]
                villas = result.villas_inverse[phase][idx]
                sfa_end = result.sfa_end_inverse[phase][idx]
                sfa_center = result.sfa_center_inverse[phase][idx]
                sfa_start = result.sfa_start_inverse[phase][idx]
                corr_comp = result.corr_compensated_inverse[phase][idx]
                row.extend(
                    [
                        f"{raw:.12f}",
                        f"{villas:.12f}",
                        f"{(villas - raw):.12f}",
                        f"{sfa_end:.12f}",
                        f"{(sfa_end - raw):.12f}",
                        f"{sfa_center:.12f}",
                        f"{(sfa_center - raw):.12f}",
                        f"{sfa_start:.12f}",
                        f"{(sfa_start - raw):.12f}",
                        f"{corr_comp:.12f}",
                        f"{(corr_comp - raw):.12f}",
                    ]
                )
            writer.writerow(row)


def maybe_plot(path: Path, sequences, times, raw_values, result: VillasDPResult, x_axis: str):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return

    phase_names = list(raw_values.keys())
    x = sequences if x_axis == "sequence" else times
    x_label = "Sequence" if x_axis == "sequence" else "Time [s]"

    fig, axes = plt.subplots(len(phase_names), 5, figsize=(26, 3.8 * len(phase_names)), sharex="col")
    if len(phase_names) == 1:
        axes = [axes]

    fig.suptitle("Exact VILLAS dp.cpp behavior vs SFA-style reconstruction references")

    for row_axes, phase in zip(axes, phase_names):
        ax_wave, ax_villas_err, ax_sfa_end_err, ax_sfa_center_err, ax_corr_err = row_axes

        ax_wave.plot(x, raw_values[phase], label="original", linewidth=1.0)
        ax_wave.plot(x, result.villas_inverse[phase], label="VILLAS inverse", linewidth=1.0)
        ax_wave.plot(x, result.sfa_end_inverse[phase], label="SFA end-ref", linewidth=1.0)
        ax_wave.plot(x, result.sfa_center_inverse[phase], label="SFA center-ref", linewidth=1.0)
        ax_wave.plot(x, result.sfa_start_inverse[phase], label="SFA start-ref", linewidth=1.0)
        ax_wave.plot(x, result.corr_compensated_inverse[phase], label="corr-compensated", linewidth=1.0)
        ax_wave.set_title(f"{phase}: waveform comparison")
        ax_wave.grid(True)
        ax_wave.legend(loc="upper right")
        ax_wave.set_ylabel("Amplitude")

        ax_villas_err.plot(x, result.villas_inverse[phase] - raw_values[phase], color="tab:red", linewidth=1.0)
        ax_villas_err.set_title(f"{phase}: VILLAS inverse - original")
        ax_villas_err.grid(True)
        ax_villas_err.set_ylabel("Error")

        ax_sfa_end_err.plot(x, result.sfa_end_inverse[phase] - raw_values[phase], color="tab:orange", linewidth=1.0)
        ax_sfa_end_err.set_title(f"{phase}: SFA end-ref - original")
        ax_sfa_end_err.grid(True)
        ax_sfa_end_err.set_ylabel("Error")

        ax_sfa_center_err.plot(
            x, result.sfa_center_inverse[phase] - raw_values[phase], color="tab:green", linewidth=1.0
        )
        ax_sfa_center_err.plot(
            x, result.sfa_start_inverse[phase] - raw_values[phase], color="tab:purple", linewidth=1.0, alpha=0.7
        )
        ax_sfa_center_err.set_title(f"{phase}: SFA center/start refs - original")
        ax_sfa_center_err.grid(True)
        ax_sfa_center_err.set_ylabel("Error")

        ax_corr_err.plot(
            x, result.corr_compensated_inverse[phase] - raw_values[phase], color="tab:brown", linewidth=1.0
        )
        ax_corr_err.set_title(f"{phase}: corr-compensated - original")
        ax_corr_err.grid(True)
        ax_corr_err.set_ylabel("Error")

    for ax in axes[-1]:
        ax.set_xlabel(x_label)

    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def print_metrics(raw_values, result: VillasDPResult):
    for phase in raw_values:
        villas_error = result.villas_inverse[phase] - raw_values[phase]
        sfa_end_error = result.sfa_end_inverse[phase] - raw_values[phase]
        sfa_center_error = result.sfa_center_inverse[phase] - raw_values[phase]
        sfa_start_error = result.sfa_start_inverse[phase] - raw_values[phase]
        corr_comp_error = result.corr_compensated_inverse[phase] - raw_values[phase]
        villas_rmse = np.sqrt(np.mean(villas_error ** 2))
        sfa_end_rmse = np.sqrt(np.mean(sfa_end_error ** 2))
        sfa_center_rmse = np.sqrt(np.mean(sfa_center_error ** 2))
        sfa_start_rmse = np.sqrt(np.mean(sfa_start_error ** 2))
        corr_comp_rmse = np.sqrt(np.mean(corr_comp_error ** 2))
        villas_max = np.max(np.abs(villas_error))
        sfa_end_max = np.max(np.abs(sfa_end_error))
        sfa_center_max = np.max(np.abs(sfa_center_error))
        sfa_start_max = np.max(np.abs(sfa_start_error))
        corr_comp_max = np.max(np.abs(corr_comp_error))
        print(
            f"{phase}: "
            f"VILLAS RMSE={villas_rmse:.6f}, max|error|={villas_max:.6f}; "
            f"SFA end RMSE={sfa_end_rmse:.6f}, max|error|={sfa_end_max:.6f}; "
            f"SFA center RMSE={sfa_center_rmse:.6f}, max|error|={sfa_center_max:.6f}; "
            f"SFA start RMSE={sfa_start_rmse:.6f}, max|error|={sfa_start_max:.6f}; "
            f"corr-comp RMSE={corr_comp_rmse:.6f}, max|error|={corr_comp_max:.6f}"
        )


def main():
    args = parse_args()
    harmonics = parse_harmonics(args.harmonics)
    times, sequences, raw_values = load_csv(args.raw)
    result = run_study(raw_values, args.f0, args.dt, harmonics)
    write_output(args.output, sequences, times, raw_values, result)
    print(f"Wrote study CSV to {args.output}")
    print("Exact VILLAS reproduction: forward DFT with correction term and dp.cpp inverse.")
    print("SFA-style references: remodulation at harmonic * f0 with end/center/start anchors plus explicit corr compensation.")
    print_metrics(raw_values, result)
    maybe_plot(args.plot, sequences, times, raw_values, result, args.x_axis)
    print(f"Wrote plot to {args.plot}")


if __name__ == "__main__":
    main()

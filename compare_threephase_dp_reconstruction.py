#!/usr/bin/env python3

import argparse
import csv
import re
from pathlib import Path

import numpy as np
from reconstruct_emt_from_dp import (
    interpolate_coefficients,
    reconstruct_signal,
    reconstruct_villas_inverse,
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Reconstruct three-phase EMT signals from DP CSV data and compare them to raw EMT samples."
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
        default=Path("threephase_dp_comparison.csv"),
        help="Path to the comparison CSV.",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=60.0,
        help="Fundamental frequency in Hz.",
    )
    parser.add_argument(
        "--harmonics",
        default="1",
        help="Comma-separated physical harmonic orders matching the DP columns.",
    )
    parser.add_argument(
        "--mapping",
        choices=("positional", "suffix"),
        default="positional",
        help="Interpret DP columns by configured position or by the _harmN suffix in the header.",
    )
    parser.add_argument(
        "--dt",
        type=float,
        default=0.00005,
        help="Simulation timestep in seconds used to convert sequence numbers to time.",
    )
    parser.add_argument(
        "--time-base",
        choices=("sequence", "timestamp"),
        default="sequence",
        help="Align and reconstruct using either common sequence numbers or logged timestamps. Use sequence for this fixed-step DP test.",
    )
    parser.add_argument(
        "--mode",
        choices=("villas-inverse", "carrier-synthesis"),
        default="villas-inverse",
        help="Use the same reconstruction mode as reconstruct_emt_from_dp.py.",
    )
    parser.add_argument(
        "--interpolation",
        choices=("linear", "zoh"),
        default="linear",
        help="How to interpolate DP coefficients onto the raw EMT timestamps.",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        help="Save a PNG plot next to the output CSV if matplotlib is available.",
    )
    return parser.parse_args()


def parse_complex(value: str) -> complex:
    return complex(value.replace("i", "j"))


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
                row = [row[0].strip()] + [field.split("#", 1)[0].strip() for field in row[1:]]
                if header is None:
                    header = row
                continue
            row = [field.split("#", 1)[0].strip() for field in row]
            row = [field for field in row if field != ""]
            if not row:
                continue
            if header is None:
                header = row
                continue
            if len(row) < len(header):
                continue
            rows.append(row)

    if header is None or not rows:
        raise ValueError(f"{path} is empty.")

    if header[0].startswith("#"):
        header[0] = header[0].lstrip("#").strip()

    return [col.strip() for col in header], rows


def load_raw(path: Path):
    header, rows = load_csv(path)
    if len(header) < 7:
        raise ValueError(f"{path} does not look like the expected raw EMT CSV.")

    phase_columns = header[4:]
    time_values = []
    sequence_values = []
    values = {name: [] for name in phase_columns}

    for row in rows:
        try:
            time_value = int(row[0]) + int(row[1]) * 1e-9
            sequence_value = int(row[3])
            sample_values = [float(row[col_idx]) for col_idx in range(4, 4 + len(phase_columns))]
        except (ValueError, IndexError):
            continue

        time_values.append(time_value)
        sequence_values.append(sequence_value)
        for name, sample_value in zip(phase_columns, sample_values):
            values[name].append(sample_value)

    times = np.asarray(time_values, dtype=float)
    sequences = np.asarray(sequence_values, dtype=int)
    for name in values:
        values[name] = np.asarray(values[name], dtype=float)

    start = find_last_segment_start(sequences)
    times = times[start:]
    sequences = sequences[start:]
    for name in values:
        values[name] = values[name][start:]

    times -= times[0]
    return times, sequences, values


def load_dp(path: Path, configured_harmonics, mapping):
    header, rows = load_csv(path)
    if len(header) < 5:
        raise ValueError(f"{path} does not look like the expected DP CSV.")

    grouped = {}
    pattern = re.compile(r"(.+)_harm(\d+)$")
    column_info = []

    for name in header[4:]:
        match = pattern.fullmatch(name)
        if not match:
            raise ValueError(f"Cannot parse DP column name '{name}'. Expected '<signal>_harmN'.")
        base_name = match.group(1)
        header_index = int(match.group(2))
        if mapping == "suffix":
            if header_index >= len(configured_harmonics):
                raise ValueError(
                    f"Column '{name}' refers to harmonic index {header_index}, "
                    f"but only {len(configured_harmonics)} configured harmonics were provided."
                )
            physical_harmonic = configured_harmonics[header_index]
        else:
            physical_harmonic = None
        column_info.append((name, base_name, header_index, physical_harmonic))
        grouped.setdefault(base_name, [])

    if mapping == "positional":
        per_phase_position = {}
        resolved = []
        for name, base_name, header_index, _ in column_info:
            position = per_phase_position.get(base_name, 0)
            if position >= len(configured_harmonics):
                raise ValueError(
                    f"Phase '{base_name}' has more DP columns than provided harmonics {configured_harmonics}."
                )
            physical_harmonic = configured_harmonics[position]
            per_phase_position[base_name] = position + 1
            resolved.append((name, base_name, header_index, physical_harmonic))
        column_info = resolved

    for _, base_name, _, physical_harmonic in column_info:
        grouped[base_name].append(physical_harmonic)

    data_lists = {
        base: {harmonic: [] for harmonic in harmonics}
        for base, harmonics in grouped.items()
    }
    time_values = []
    sequence_values = []

    for row in rows:
        try:
            time_value = int(row[0]) + int(row[1]) * 1e-9
            sequence_value = int(row[3])
            parsed_values = [parse_complex(row[col_idx]) for col_idx in range(4, len(header))]
        except (ValueError, IndexError):
            continue

        time_values.append(time_value)
        sequence_values.append(sequence_value)

        for value, (_, base_name, _, physical_harmonic) in zip(parsed_values, column_info):
            data_lists[base_name][physical_harmonic].append(value)

    times = np.asarray(time_values, dtype=float)
    sequences = np.asarray(sequence_values, dtype=int)
    data = {
        base_name: {
            harmonic: np.asarray(samples, dtype=np.complex128)
            for harmonic, samples in harmonic_map.items()
        }
        for base_name, harmonic_map in data_lists.items()
    }

    start = find_last_segment_start(sequences)
    times = times[start:]
    sequences = sequences[start:]
    for base_name in data:
        for harmonic in data[base_name]:
            data[base_name][harmonic] = data[base_name][harmonic][start:]

    times -= times[0]
    return times, sequences, data, column_info


def find_last_segment_start(sequences):
    resets = np.flatnonzero(np.diff(sequences) < 0)
    if len(resets) == 0:
        return 0
    return int(resets[-1] + 1)


def reconstruct_phase(reference_times, dp_times, harmonic_map, harmonics, f0, dt, interpolation, mode):
    coeffs = np.column_stack([harmonic_map[harmonic] for harmonic in harmonics])

    if mode == "villas-inverse":
        _, reconstructed = reconstruct_villas_inverse(len(dp_times), coeffs, harmonics, dt)
        if len(reconstructed) != len(reference_times):
            raise ValueError("Reconstructed and reference signals have mismatched lengths.")
        return reconstructed

    interpolated = interpolate_coefficients(dp_times, coeffs, reference_times, interpolation)
    return reconstruct_signal(reference_times, interpolated, harmonics, f0)


def align_by_sequence(raw_times, raw_sequences, raw_values, dp_times, dp_sequences, dp_values, dt):
    common_sequences, raw_idx, dp_idx = np.intersect1d(
        raw_sequences, dp_sequences, assume_unique=False, return_indices=True
    )
    if len(common_sequences) == 0:
        raise ValueError("No common sequence numbers found between raw and DP logs.")

    common_times = (common_sequences - common_sequences[0]) * dt
    aligned_raw = {phase: values[raw_idx] for phase, values in raw_values.items()}
    aligned_dp = {
        phase: {harmonic: coeffs[dp_idx] for harmonic, coeffs in harmonic_map.items()}
        for phase, harmonic_map in dp_values.items()
    }
    return common_times, common_sequences, aligned_raw, aligned_dp


def write_output(path: Path, sequences, times, comparison):
    fieldnames = ["sequence", "time_s"]
    for phase in comparison:
        fieldnames.extend([phase, f"{phase}_reconstructed", f"{phase}_error"])

    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(fieldnames)
        for idx, time_value in enumerate(times):
            row = [int(sequences[idx]), f"{time_value:.12f}"]
            for phase in comparison:
                row.extend(
                    [
                        f"{comparison[phase]['raw'][idx]:.12f}",
                        f"{comparison[phase]['reconstructed'][idx]:.12f}",
                        f"{comparison[phase]['error'][idx]:.12f}",
                    ]
                )
            writer.writerow(row)


def maybe_plot(output_path: Path, times, comparison):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipping plot generation.")
        return False

    phases = list(comparison.keys())
    fig, axes = plt.subplots(len(phases), 1, figsize=(11, 8), sharex=True)
    if len(phases) == 1:
        axes = [axes]

    fig.suptitle("Raw EMT vs reconstructed EMT from dynamic phasors")

    for ax, phase in zip(axes, phases):
        ax.plot(times, comparison[phase]["raw"], label=f"{phase} raw", linewidth=1.0)
        ax.plot(
            times,
            comparison[phase]["reconstructed"],
            label=f"{phase} reconstructed",
            linewidth=1.0,
        )
        ax.plot(times, comparison[phase]["error"], label=f"{phase} error", linewidth=0.8)
        ax.set_ylabel("Amplitude")
        ax.grid(True)
        ax.legend(loc="upper right")

    axes[-1].set_xlabel("Time [s]")
    fig.tight_layout()
    fig.savefig(output_path.with_suffix(".png"), dpi=150)
    plt.close(fig)
    return True


def main():
    args = parse_args()
    configured_harmonics = parse_harmonics(args.harmonics)
    raw_times, raw_sequences, raw_values = load_raw(args.raw)
    dp_times, dp_sequences, dp_values, column_info = load_dp(
        args.dp, configured_harmonics, args.mapping
    )

    if args.time_base == "sequence":
        comparison_times, comparison_sequences, raw_values, dp_values = align_by_sequence(
            raw_times, raw_sequences, raw_values, dp_times, dp_sequences, dp_values, args.dt
        )
        dp_times_for_reconstruction = comparison_times
        raw_times_for_reconstruction = comparison_times
    else:
        comparison_times = raw_times
        comparison_sequences = raw_sequences
        dp_times_for_reconstruction = dp_times
        raw_times_for_reconstruction = raw_times

    comparison = {}
    harmonics_seen = set()

    for phase_name, raw_signal in raw_values.items():
        if phase_name not in dp_values:
            raise ValueError(f"No matching DP columns found for raw phase '{phase_name}'.")

        harmonics = sorted(dp_values[phase_name].keys())
        reconstructed = reconstruct_phase(
            raw_times_for_reconstruction,
            dp_times_for_reconstruction,
            dp_values[phase_name],
            harmonics,
            args.f0,
            args.dt,
            args.interpolation,
            args.mode,
        )
        comparison[phase_name] = {
            "raw": raw_signal,
            "reconstructed": reconstructed,
            "error": reconstructed - raw_signal,
        }
        harmonics_seen.update(dp_values[phase_name].keys())

    write_output(args.output, comparison_sequences, comparison_times, comparison)
    print(f"Wrote comparison CSV to {args.output}")
    print(f"Alignment mode: {args.time_base}")
    if args.time_base == "timestamp":
        print("Warning: timestamp mode uses logged wall-clock times and is less reliable than sequence mode for this fixed-step test.")
    if args.time_base == "sequence":
        print(
            f"Matched {len(comparison_sequences)} common sequence numbers "
            f"from {int(comparison_sequences[0])} to {int(comparison_sequences[-1])}"
        )
    print(f"Reconstructed phases: {', '.join(comparison.keys())}")
    print(f"Harmonics present in DP log: {sorted(harmonics_seen)}")
    print("Column mapping:")
    for name, base_name, header_index, physical_harmonic in column_info:
        print(
            f"  {name} -> {base_name}, header_index={header_index}, physical_harmonic={physical_harmonic}"
        )

    if harmonics_seen == {0}:
        print(
            "Warning: only harmonic 0 is present, so the reconstruction is the DC/average component,"
            " not the full sinusoidal waveform."
        )

    for phase_name, series in comparison.items():
        rmse = np.sqrt(np.mean(series["error"] ** 2))
        max_abs = np.max(np.abs(series["error"]))
        print(f"{phase_name}: RMSE={rmse:.6f}, max|error|={max_abs:.6f}")

    if args.plot:
        if maybe_plot(args.output, comparison_times, comparison):
            print(f"Wrote plot to {args.output.with_suffix('.png')}")


if __name__ == "__main__":
    main()

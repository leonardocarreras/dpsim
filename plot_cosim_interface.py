"""Plot interface voltages and currents from cosim-9bus-4order CSV log."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def plot(csv_path: Path, output: Path | None = None) -> None:
    df = pd.read_csv(csv_path, skipinitialspace=True)
    df.columns = df.columns.str.strip()

    t = df["time"]

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    fig.suptitle(f"Co-sim interface – {csv_path.stem}")

    # Voltages
    ax_v = axes[0]
    ax_v.plot(t, df["a"] / 1e3, label="Phase A")
    ax_v.plot(t, df["b"] / 1e3, label="Phase B")
    ax_v.plot(t, df["c"] / 1e3, label="Phase C")
    ax_v.set_ylabel("Voltage (kV)")
    ax_v.legend(loc="upper right")
    ax_v.grid(True)

    # Currents
    ax_i = axes[1]
    ax_i.plot(t, df["a_i"], label="Phase A")
    ax_i.plot(t, df["b_i"], label="Phase B")
    ax_i.plot(t, df["c_i"], label="Phase C")
    ax_i.set_ylabel("Current (A)")
    ax_i.set_xlabel("Time (s)")
    ax_i.legend(loc="upper right")
    ax_i.grid(True)

    plt.tight_layout()

    if output is None:
        output = csv_path.parent.parent / (csv_path.stem + "_interface.png")
    fig.savefig(output, dpi=150)
    print(f"Saved: {output}")
    import matplotlib
    if matplotlib.get_backend().lower() != "agg":
        plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot co-sim interface V/I.")
    parser.add_argument(
        "csv",
        nargs="?",
        default="logs/cosim-9bus-4order.csv",
        help="Path to the CSV log file (default: logs/cosim-9bus-4order.csv)",
    )
    parser.add_argument("-o", "--output", default=None, help="Output PNG path")
    args = parser.parse_args()

    plot(Path(args.csv), Path(args.output) if args.output else None)

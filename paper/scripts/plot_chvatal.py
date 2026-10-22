#!/usr/bin/env python3
"""Three-panel scatter for the Chvatal use case.

Panels: solve time of Chvatal Tactix against Default Tactix, solve time against
Satsuma+Kissat, and proof-checking time against Satsuma+Kissat.  A run without
an answer is drawn on the solving-limit border; a certificate whose check did
not succeed is drawn on the checking-limit border.  Points below the diagonal
favour Chvatal Tactix. Crosses denote UNSAT results.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import matplotlib.pyplot as plt

SOLVE_LIMIT = 5000.0
CHECK_LIMIT = 7200.0
CANDIDATE = "Chvatal Tactix"
INK = "#c2410c"
PANELS = (
    ("Default Tactix", "solve", "Tactix solve time (s)",
     "Tactix$_{\\mathrm{ch}}$ solve time (s)"),
    ("Satsuma+Kissat", "solve", "Satsuma+Kissat solve time (s)", None),
    ("Satsuma+Kissat", "check", "Satsuma+Kissat check time (s)",
     "Tactix$_{\\mathrm{ch}}$ check time (s)"),
)


def setup() -> None:
    plt.rcParams.update({
        "font.size": 7.4, "font.family": "serif", "pdf.fonttype": 42,
        "ps.fonttype": 42, "axes.linewidth": 0.6,
        "xtick.major.width": 0.6, "ytick.major.width": 0.6,
    })


def value(row: dict[str, str] | None, kind: str) -> float:
    """Measured time, or the limit (border) when there is no result."""
    if kind == "solve":
        if row is None or row["verdict"] not in ("SAT", "UNSAT"):
            return SOLVE_LIMIT
        return float(row["solve_seconds"])
    if row is None or row["verified"] != "1" or not row["check_seconds"]:
        return CHECK_LIMIT
    return float(row["check_seconds"])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=Path(__file__).resolve().parents[1] / "data/chvatal-results.csv")
    parser.add_argument("--output", default=Path(__file__).resolve().parents[1] / "figures/chvatal-scatter.pdf")
    parser.add_argument("--limits", type=Path, help="limits.json of the run (make_data.py)")
    args = parser.parse_args()
    global SOLVE_LIMIT, CHECK_LIMIT
    if args.limits and args.limits.exists():
        limits = json.loads(args.limits.read_text()).get("chvatal", {})
        SOLVE_LIMIT = float(limits.get("seconds", SOLVE_LIMIT))
        CHECK_LIMIT = float(limits.get("check_seconds", CHECK_LIMIT))
    setup()
    rows = list(csv.DictReader(Path(args.data).open(newline="", encoding="utf-8")))
    by = {(r["configuration"], r["node"]): r for r in rows}
    nodes = sorted({r["node"] for r in rows if r["configuration"] == CANDIDATE})

    fig, axes = plt.subplots(1, 3, figsize=(7.15, 2.45))
    for axis, (baseline, kind, xlabel, ylabel) in zip(axes, PANELS):
        limit = SOLVE_LIMIT if kind == "solve" else CHECK_LIMIT
        low = 100.0
        high = limit * 1.35
        xs = [value(by.get((baseline, n)), kind) for n in nodes]
        ys = [value(by.get((CANDIDATE, n)), kind) for n in nodes]
        axis.scatter(xs, ys, s=10, marker="x", color=INK,
                     linewidths=0.55, alpha=0.72, zorder=3)
        axis.plot([low, limit], [low, limit], color="#4b5563", lw=0.7, zorder=1)
        axis.axhline(limit, color="#9ca3af", linestyle=":", linewidth=0.55)
        axis.axvline(limit, color="#9ca3af", linestyle=":", linewidth=0.55)
        axis.set_xscale("log"); axis.set_yscale("log")
        axis.set_xlim(low, high); axis.set_ylim(low, high)
        ticks = [t for t in (100, 1000) if t < limit / 1.5] + [limit]
        tick_labels = [str(int(t)) for t in ticks]
        axis.set_xticks(ticks, tick_labels)
        axis.set_yticks(ticks, tick_labels)
        axis.set_aspect("equal")
        axis.grid(which="major", color="#e2e5e9", linewidth=0.4)
        axis.spines[["top", "right"]].set_visible(False)
        axis.set_xlabel(xlabel)
        if ylabel:
            axis.set_ylabel(ylabel)
    fig.tight_layout(w_pad=0.55)
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()

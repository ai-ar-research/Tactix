#!/usr/bin/env python3
"""Draw the paper's tierd, interface and milestone figures.

With --limits (the limits.json that make_data.py writes next to the data),
the axes follow the time limits of the run being drawn, e.g. a reduced rerun;
without it they use the paper's limits."""
from __future__ import annotations

import argparse
import csv
import json
import statistics
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D


SOLVE_LIMIT = 5_000.0      # tierd solving limit (replaced by --limits)
MILESTONE_LIMIT = 5_000.0
PAPER_SCALE = True         # the paper's fixed axis ranges; False for other runs
ORDER = (
    "Tactix",
    "Satsuma+Kissat",
    "Kissat 4.0.4",
    "Kissat-sc2024",
    "AE-Kissat-MAB",
    "LymphoSAT",
)
COLORS = {
    "Tactix": "#005f73",
    "Satsuma+Kissat": "#d97706",
    "Kissat 4.0.4": "#6b7280",
    "Kissat-sc2024": "#8b5cf6",
    "AE-Kissat-MAB": "#0ea5a4",
    "LymphoSAT": "#be185d",
}
STYLES = {
    "Tactix": "-",
    "Satsuma+Kissat": "--",
    "Kissat 4.0.4": "-.",
    "Kissat-sc2024": ":",
    "AE-Kissat-MAB": (0, (5, 1, 1, 1)),
    "LymphoSAT": (0, (1, 1)),
}


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def setup() -> None:
    plt.rcParams.update(
        {
            "font.size": 7.4,
            "font.family": "serif",
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
            "axes.linewidth": 0.6,
            "xtick.major.width": 0.6,
            "ytick.major.width": 0.6,
        }
    )


def cumulative_counts(times: list[float], limit: float | None = None) -> tuple[list[float], list[int]]:
    """Step coordinates for the number of solves completed by each time.

    Extend the final plateau to the limit, so every curve exposes its coverage
    at the same deadline. Counts stay unnormalized, as requested for the paper.
    """
    limit = SOLVE_LIMIT if limit is None else limit
    completed = sorted(time for time in times if 0 <= time <= limit)
    return [0.0, *completed, limit], [0, *range(1, len(completed) + 1), len(completed)]


def cactus(rows: list[dict[str, str]], output: Path) -> None:
    """Draw cumulative checked-solve counts (legacy output/CLI name retained)."""
    fig, axes = plt.subplots(1, 3, figsize=(7.15, 2.35), sharey=True)
    top = 400 if PAPER_SCALE else max(1, max(
        sum(row["configuration"] == "Tactix" and row["SC20" + y[-2:]] == "1" for row in rows)
        for y in ("SC24", "SC25", "SC26")))
    for axis, year in zip(axes, ("SC24", "SC25", "SC26")):
        flag = "SC20" + year[-2:]
        for name in ORDER:
            times = sorted(
                float(row["solve_seconds"])
                for row in rows
                if row["configuration"] == name and row[flag] == "1" and row["accepted"] == "1"
            )
            axis.step(
                *cumulative_counts(times), where="post",
                color=COLORS[name],
                linestyle=STYLES[name],
                linewidth=1.25 if name == "Tactix" else 0.9,
            )
        axis.set_title(year, pad=3, fontweight="bold")
        axis.set_xlabel("solve time (s)")
        axis.set_xscale("log")
        axis.set_xlim(0.02, SOLVE_LIMIT)
        axis.set_ylim(0, top)
        axis.grid(which="major", color="#d9dee3", linewidth=0.45)
        axis.spines[["top", "right"]].set_visible(False)
    axes[0].set_ylabel("instances solved (checked)")
    handles = [Line2D([0], [0], color=COLORS[name], linestyle=STYLES[name], lw=1.3, label=name) for name in ORDER]
    fig.legend(handles=handles, loc="upper center", ncol=3, frameon=False, bbox_to_anchor=(0.5, 1.04), columnspacing=1.25)
    fig.tight_layout(rect=(0, 0, 1, 0.84), w_pad=0.65)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight")
    plt.close(fig)


def scatter(
    rows: list[dict[str, str]],
    output: Path,
    reference_configurations: tuple[str, ...],
    reference_label: str,
) -> None:
    by_key = {(row["configuration"], row["instance"]): row for row in rows}
    fig, axes = plt.subplots(1, 3, figsize=(7.15, 2.45), sharex=True, sharey=True)
    for axis, year in zip(axes, ("SC24", "SC25", "SC26")):
        flag = "SC20" + year[-2:]
        members = [
            row["instance"]
            for row in rows
            if row["configuration"] == "Tactix" and row[flag] == "1"
        ]
        shown = []
        for instance in members:
            tactix = by_key[("Tactix", instance)]
            accepted_reference_arms = [
                by_key[(configuration, instance)]
                for configuration in reference_configurations
                if by_key[(configuration, instance)]["accepted"] == "1"
            ]
            reference = min(
                accepted_reference_arms,
                key=lambda row: float(row["solve_seconds"]),
            ) if accepted_reference_arms else None
            ta = tactix["accepted"] == "1"
            ra = reference is not None
            if not ta and not ra:
                continue
            verdict = tactix["verdict"] if ta else reference["verdict"]
            shown.append(
                (
                    float(reference["solve_seconds"]) if ra else SOLVE_LIMIT,
                    float(tactix["solve_seconds"]) if ta else SOLVE_LIMIT,
                    verdict,
                )
            )
        for verdict, marker, color in (("SAT", "o", "#2563a6"), ("UNSAT", "x", "#c2410c")):
            points = [(x, y) for x, y, value in shown if value == verdict]
            axis.scatter(
                [point[0] for point in points],
                [point[1] for point in points],
                marker=marker,
                color=color,
                facecolors="none" if marker == "o" else color,
                s=10,
                linewidths=0.55,
                alpha=0.72,
            )
        axis.plot([0.02, SOLVE_LIMIT], [0.02, SOLVE_LIMIT], color="#4b5563", linewidth=0.7, zorder=0)
        axis.axhline(SOLVE_LIMIT, color="#9ca3af", linestyle=":", linewidth=0.55)
        axis.axvline(SOLVE_LIMIT, color="#9ca3af", linestyle=":", linewidth=0.55)
        axis.set_xscale("log")
        axis.set_yscale("log")
        axis.set_xlim(0.02, SOLVE_LIMIT * 1.35)
        axis.set_ylim(0.02, SOLVE_LIMIT * 1.35)
        ticks = [t for t in (0.1, 1, 10, 100) if t < SOLVE_LIMIT / 2] + [SOLVE_LIMIT]
        tick_labels = [f"{t:g}" for t in ticks]
        axis.set_xticks(ticks, tick_labels)
        axis.set_yticks(ticks, tick_labels)
        axis.set_aspect("equal")
        axis.grid(which="major", color="#e2e5e9", linewidth=0.4)
        axis.spines[["top", "right"]].set_visible(False)
        axis.set_title(year, fontweight="bold", pad=3)
        axis.set_xlabel(f"{reference_label} (s)")
    axes[0].set_ylabel("Tactix (s)")
    handles = [
        Line2D([0], [0], marker="o", color="none", markeredgecolor="#2563a6", markersize=4, label="SAT"),
        Line2D([0], [0], marker="x", color="#c2410c", markersize=4, linewidth=0, label="UNSAT"),
    ]
    fig.legend(handles=handles, loc="upper center", ncol=2, frameon=False, bbox_to_anchor=(0.5, 1.02))
    fig.tight_layout(rect=(0, 0, 1, 0.88), w_pad=0.55)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight")
    plt.close(fig)


def milestones(rows: list[dict[str, str]], output: Path) -> None:
    """Plot cumulative solver-answer counts separately for SAT and UNSAT."""
    # Preserve archived CSV keys; display the evaluated versions as 1--5.
    labels = [f"Milestone {letter}" for letter in "ABCDE"] + ["Current"]
    colors = ["#6b7280", "#8b5cf6", "#2563a6", "#d97706", "#be185d", "#005f73"]
    styles = [":", "--", "-.", (0, (5, 1, 1, 1)), (0, (1, 1)), "-"]
    fig, axes = plt.subplots(1, 2, figsize=(7.15, 2.65), sharex=True, sharey=True)
    top = 550 if PAPER_SCALE else max(1, max(
        sum(row["label"] == label and row["verdict"] == verdict for row in rows)
        for label in labels for verdict in ("SAT", "UNSAT")) + 1)
    for axis, verdict in zip(axes, ("SAT", "UNSAT")):
        for number, (label, color, style) in enumerate(zip(labels, colors, styles), 1):
            display_label = "Current" if label == "Current" else f"Version {number}"
            times = sorted(
                float(row["solve_seconds"])
                for row in rows
                if row["label"] == label and row["verdict"] == verdict
            )
            axis.step(
                *cumulative_counts(times, MILESTONE_LIMIT), where="post", label=display_label,
                color=color, linestyle=style,
                linewidth=1.4 if label == "Current" else 1.0,
            )
        axis.set_title(verdict, pad=3, fontweight="bold")
        axis.set_xlabel("solving time (s)")
        axis.set_xlim(0, MILESTONE_LIMIT)
        axis.set_ylim(0, top)
        axis.grid(color="#d9dee3", linewidth=0.45)
        axis.spines[["top", "right"]].set_visible(False)
    axes[0].set_ylabel("instances solved")
    handles, legend_labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, legend_labels, loc="lower center", ncol=6,
               frameon=False, handlelength=2.6, columnspacing=1.3)
    fig.tight_layout(rect=(0, 0.12, 1, 1), w_pad=1.5)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight")
    plt.close(fig)


INTERFACE_SETS = ("Tier A", "B28+C5", "Rank 1-50", "WV100")


def interface_scatter(rows: list[dict[str, str]], output: Path) -> None:
    """Per-instance solve time of the default strategy over the C interface
    against the same strategy compiled directly, on instances both solved."""
    fig, axes = plt.subplots(1, 4, figsize=(7.15, 2.15), sharex=True, sharey=True)
    for axis, name in zip(axes, INTERFACE_SETS):
        members = [row for row in rows if row["set"] == name]
        for verdict, marker, color in (("SAT", "o", "#2563a6"), ("UNSAT", "x", "#c2410c")):
            points = [row for row in members if row["verdict"] == verdict]
            axis.scatter(
                [float(row["cpp_seconds"]) for row in points],
                [float(row["c_api_seconds"]) for row in points],
                marker=marker,
                color=color,
                facecolors="none" if marker == "o" else color,
                s=10,
                linewidths=0.55,
                alpha=0.72,
            )
        ratios = [float(row["c_api_seconds"]) / max(float(row["cpp_seconds"]), 1e-3) for row in members]
        median = statistics.median(ratios) if ratios else None
        axis.plot([0.005, 2_000], [0.005, 2_000], color="#4b5563", linewidth=0.7, zorder=0)
        axis.set_xscale("log")
        axis.set_yscale("log")
        axis.set_xlim(0.005, 2_000)
        axis.set_ylim(0.005, 2_000)
        axis.set_aspect("equal")
        axis.grid(which="major", color="#e2e5e9", linewidth=0.4)
        axis.spines[["top", "right"]].set_visible(False)
        axis.set_title({"Rank 1-50": "Tier R", "WV100": "Tier W"}.get(name, name),
                       fontweight="bold", pad=3)
        axis.text(0.04, 0.96, "no formula" if median is None else f"median {100 * (median - 1):+.1f}%",
                  transform=axis.transAxes, ha="left", va="top")
        axis.set_xlabel("C++ (s)")
    axes[0].set_ylabel("C interface (s)")
    handles = [
        Line2D([0], [0], marker="o", color="none", markeredgecolor="#2563a6", markersize=4, label="SAT"),
        Line2D([0], [0], marker="x", color="#c2410c", markersize=4, linewidth=0, label="UNSAT"),
    ]
    fig.legend(handles=handles, loc="upper center", ncol=2, frameon=False, bbox_to_anchor=(0.5, 1.02))
    fig.tight_layout(rect=(0, 0, 1, 0.88), w_pad=0.55)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=Path, default=Path(__file__).resolve().parents[1] / "data/tierd-times.csv")
    parser.add_argument("--cactus", type=Path)
    parser.add_argument("--scatter", type=Path)
    parser.add_argument("--scatter-satsuma", type=Path)
    parser.add_argument("--scatter-lympho", type=Path)
    parser.add_argument("--milestone-data", type=Path)
    parser.add_argument("--milestones", type=Path)
    parser.add_argument("--interface-data", type=Path)
    parser.add_argument("--interface", type=Path)
    parser.add_argument("--limits", type=Path, help="limits.json of the run (make_data.py)")
    args = parser.parse_args()
    global SOLVE_LIMIT, MILESTONE_LIMIT, PAPER_SCALE
    if args.limits and args.limits.exists():
        limits = json.loads(args.limits.read_text())
        PAPER_SCALE = limits.get("profile", "full") == "full"
        SOLVE_LIMIT = float(limits.get("tierd", {}).get("seconds", SOLVE_LIMIT))
        MILESTONE_LIMIT = float(limits.get("milestones", {}).get("seconds", MILESTONE_LIMIT))
    setup()
    tierd_figures = (args.cactus, args.scatter, args.scatter_satsuma, args.scatter_lympho)
    rows = read_rows(args.data) if any(tierd_figures) else []  # needed only by the tierd figures
    if args.cactus:
        cactus(rows, args.cactus)
    if args.scatter:
        scatter(
            rows,
            args.scatter,
            ("Satsuma+Kissat", "LymphoSAT"),
            "SK-Lym Virtual Portfolio",
        )
    if args.scatter_satsuma:
        scatter(rows, args.scatter_satsuma, ("Satsuma+Kissat",), "Satsuma+Kissat")
    if args.scatter_lympho:
        scatter(rows, args.scatter_lympho, ("LymphoSAT",), "LymphoSAT")
    if args.milestone_data and args.milestones:
        milestones(read_rows(args.milestone_data), args.milestones)
    if args.interface_data and args.interface:
        interface_scatter(read_rows(args.interface_data), args.interface)


if __name__ == "__main__":
    main()

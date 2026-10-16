#!/usr/bin/env python3
"""Draw the Tier-D and CruxMiter figures used in the paper."""
from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch


SOLVE_LIMIT = 5_000.0
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


def cumulative_counts(times: list[float], limit: float = 5000.0) -> tuple[list[float], list[int]]:
    """Step coordinates for the number of solves completed by each time.

    Extend the final plateau to the limit, so every curve exposes its coverage
    at the same deadline. Counts stay unnormalized, as requested for the paper.
    """
    completed = sorted(time for time in times if 0 <= time <= limit)
    return [0.0, *completed, limit], [0, *range(1, len(completed) + 1), len(completed)]


def cactus(rows: list[dict[str, str]], output: Path) -> None:
    """Draw cumulative checked-solve counts (legacy output/CLI name retained)."""
    fig, axes = plt.subplots(1, 3, figsize=(7.15, 2.35), sharey=True)
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
        axis.set_xlim(0.02, 5000)
        axis.set_ylim(0, 400)
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
        ticks = [0.1, 1, 10, 100, SOLVE_LIMIT]
        tick_labels = ["0.1", "1", "10", "100", "5000"]
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


def validation(rows: list[dict[str, str]], output: Path) -> None:
    """Show validation loss across solvers and checking cost for Tactix UNSAT proofs."""
    fig, axes = plt.subplots(1, 2, figsize=(7.15, 2.55))

    accepted = []
    unaccepted = []
    for name in ORDER:
        selected = [row for row in rows if row["configuration"] == name]
        solved = sum(row["solved"] == "1" for row in selected)
        checked = sum(row["accepted"] == "1" for row in selected)
        accepted.append(checked)
        unaccepted.append(solved - checked)
    positions = list(range(len(ORDER)))
    axes[0].barh(positions, accepted, color="#005f73", label="validated")
    axes[0].barh(positions, unaccepted, left=accepted, color="#cbd5e1", label="answer not validated")
    axes[0].set_yticks(positions, ORDER)
    axes[0].invert_yaxis()
    axes[0].set_xlabel("instances")
    axes[0].set_title("Answers and validation", fontweight="bold", pad=3)
    axes[0].grid(axis="x", color="#d9dee3", linewidth=0.45)
    axes[0].spines[["top", "right"]].set_visible(False)
    axes[0].legend(
        frameon=False,
        loc="upper center",
        bbox_to_anchor=(0.5, -0.18),
        ncol=2,
        fontsize=6.8,
    )

    tactix = [
        row
        for row in rows
        if row["configuration"] == "Tactix"
        and row["verdict"] == "UNSAT"
        and row["accepted"] == "1"
        and row["check_seconds"]
    ]
    solve = [float(row["solve_seconds"]) for row in tactix]
    check = [float(row["check_seconds"]) for row in tactix]
    axes[1].scatter(solve, check, s=9, color="#005f73", alpha=0.58, linewidths=0)
    lo, hi = 0.01, 100_000
    axes[1].plot([lo, hi], [lo, hi], color="#4b5563", linewidth=0.7, linestyle="--")
    axes[1].set_xscale("log")
    axes[1].set_yscale("log")
    axes[1].set_xlim(lo, 10_000)
    axes[1].set_ylim(lo, hi)
    axes[1].set_xlabel("solve time (s)")
    axes[1].set_ylabel("proof-check time (s)")
    axes[1].set_title("Tactix verified UNSAT", fontweight="bold", pad=3)
    axes[1].grid(which="major", color="#d9dee3", linewidth=0.45)
    axes[1].spines[["top", "right"]].set_visible(False)
    ratios = [y / x for x, y in zip(solve, check) if x > 0]
    axes[1].text(
        0.04,
        0.94,
        f"median check/solve = {statistics.median(ratios):.2f}",
        transform=axes[1].transAxes,
        va="top",
        fontsize=6.8,
    )
    fig.tight_layout(rect=(0, 0.08, 1, 1), w_pad=1.25)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight")
    plt.close(fig)


def coverage(rows: list[dict[str, str]], output: Path) -> None:
    """Decompose Tactix/Satsuma coverage and their mean PAR-2 difference."""
    by_key = {(row["configuration"], row["instance"]): row for row in rows}
    instances = [
        row["instance"] for row in rows if row["configuration"] == "Tactix"
    ]
    both = tactix_only = satsuma_only = neither = 0
    common_delta = tactix_only_delta = satsuma_only_delta = 0.0
    for instance in instances:
        tactix = by_key[("Tactix", instance)]
        satsuma = by_key[("Satsuma+Kissat", instance)]
        ta = tactix["accepted"] == "1"
        sa = satsuma["accepted"] == "1"
        if ta and sa:
            both += 1
            common_delta += float(satsuma["par2_seconds"]) - float(tactix["par2_seconds"])
        elif ta:
            tactix_only += 1
            tactix_only_delta += float(satsuma["par2_seconds"]) - float(tactix["par2_seconds"])
        elif sa:
            satsuma_only += 1
            satsuma_only_delta += float(satsuma["par2_seconds"]) - float(tactix["par2_seconds"])
        else:
            neither += 1

    fig, axes = plt.subplots(1, 2, figsize=(7.15, 2.65))
    labels = ["both", "Tactix only", "Satsuma only", "neither"]
    values = [both, tactix_only, satsuma_only, neither]
    colors = ["#005f73", "#0ea5a4", "#d97706", "#cbd5e1"]
    positions = list(range(len(labels)))
    axes[0].barh(positions, values, color=colors)
    axes[0].set_yticks(positions, labels)
    axes[0].invert_yaxis()
    axes[0].set_xlabel("formulas")
    axes[0].set_title("Validated coverage", fontweight="bold", pad=3)
    axes[0].grid(axis="x", color="#d9dee3", linewidth=0.45)
    axes[0].spines[["top", "right"]].set_visible(False)
    for position, value in zip(positions, values):
        axes[0].text(value + 12, position, str(value), va="center", fontsize=7)
    axes[0].set_xlim(0, max(values) * 1.12)

    count = len(instances)
    contributions = [
        common_delta / count,
        tactix_only_delta / count,
        satsuma_only_delta / count,
    ]
    names = ["shared solves", "Tactix only", "Satsuma only"]
    bar_colors = ["#005f73", "#0ea5a4", "#d97706"]
    axes[1].bar(range(3), contributions, color=bar_colors, width=0.65)
    axes[1].axhline(0, color="#4b5563", linewidth=0.7)
    net = sum(contributions)
    axes[1].axhline(net, color="#4b5563", linestyle="--", linewidth=0.8)
    axes[1].text(2.48, net + 8, f"net {net:.1f} s", ha="right", va="bottom", fontsize=7)
    axes[1].set_xticks(range(3), names, rotation=15, ha="right")
    axes[1].set_ylabel("mean PAR-2 contribution (s)")
    axes[1].set_title("Source of Tactix advantage", fontweight="bold", pad=3)
    axes[1].grid(axis="y", color="#d9dee3", linewidth=0.45)
    axes[1].spines[["top", "right"]].set_visible(False)
    for position, value in enumerate(contributions):
        axes[1].text(
            position,
            value + (8 if value >= 0 else -8),
            f"{value:+.1f}",
            ha="center",
            va="bottom" if value >= 0 else "top",
            fontsize=7,
        )
    fig.tight_layout(w_pad=1.5)
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
    for axis, verdict in zip(axes, ("SAT", "UNSAT")):
        for number, (label, color, style) in enumerate(zip(labels, colors, styles), 1):
            display_label = "Current" if label == "Current" else f"Version {number}"
            times = sorted(
                float(row["solve_seconds"])
                for row in rows
                if row["label"] == label and row["verdict"] == verdict
            )
            axis.step(
                *cumulative_counts(times), where="post", label=display_label,
                color=color, linestyle=style,
                linewidth=1.4 if label == "Current" else 1.0,
            )
        axis.set_title(verdict, pad=3, fontweight="bold")
        axis.set_xlabel("solving time (s)")
        axis.set_xlim(0, 5000)
        axis.set_ylim(0, 550)
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


def crux(rows: list[dict[str, str]], output: Path) -> None:
    by_key = {(row["configuration"], int(row["seed"])): row for row in rows}
    seeds = list(range(10))
    default_speedup = [
        float(by_key[("Default Tactix", seed)]["solve_seconds"])
        / float(by_key[("Public policy", seed)]["solve_seconds"])
        for seed in seeds
    ]
    kissat_speedup = [
        float(by_key[("Kissat 4.0.4", seed)]["solve_seconds"])
        / float(by_key[("Public policy", seed)]["solve_seconds"])
        for seed in seeds
    ]
    default_proof = [
        float(by_key[("Default Tactix", seed)]["proof_bytes"])
        / float(by_key[("Public policy", seed)]["proof_bytes"])
        for seed in seeds
    ]
    kissat_proof = [
        float(by_key[("Kissat 4.0.4", seed)]["proof_bytes"])
        / float(by_key[("Public policy", seed)]["proof_bytes"])
        for seed in seeds
    ]

    fig, axes = plt.subplots(1, 2, figsize=(7.15, 2.35), sharex=True)
    width = 0.36
    positions = list(range(10))
    for axis, first, second, ylabel in (
        (axes[0], default_speedup, kissat_speedup, "solve-time ratio to policy"),
        (axes[1], default_proof, kissat_proof, "proof-size ratio to policy"),
    ):
        axis.bar([x - width / 2 for x in positions], first, width, color="#005f73", label="Default Tactix / policy")
        axis.bar([x + width / 2 for x in positions], second, width, color="#d97706", label="Kissat / policy")
        axis.axhline(1.0, color="#4b5563", linewidth=0.7)
        axis.set_xticks(positions, [str(seed) for seed in seeds])
        axis.set_xlabel("CruxMiter seed")
        axis.set_ylabel(ylabel)
        axis.grid(axis="y", color="#d9dee3", linewidth=0.45)
        axis.spines[["top", "right"]].set_visible(False)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=2, frameon=False, bbox_to_anchor=(0.5, 1.03))
    fig.tight_layout(rect=(0, 0, 1, 0.88), w_pad=1.1)
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
        ratios = [float(row["c_api_seconds"]) / float(row["cpp_seconds"]) for row in members]
        median = statistics.median(ratios)
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
        axis.text(0.04, 0.96, f"median {100 * (median - 1):+.1f}%", transform=axis.transAxes,
                  ha="left", va="top")
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
    parser.add_argument("--validation", type=Path)
    parser.add_argument("--coverage", type=Path)
    parser.add_argument("--milestone-data", type=Path)
    parser.add_argument("--milestones", type=Path)
    parser.add_argument("--crux-data", type=Path)
    parser.add_argument("--crux", type=Path)
    parser.add_argument("--interface-data", type=Path)
    parser.add_argument("--interface", type=Path)
    args = parser.parse_args()
    setup()
    rows = read_rows(args.data)
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
    if args.validation:
        validation(rows, args.validation)
    if args.coverage:
        coverage(rows, args.coverage)
    if args.milestone_data and args.milestones:
        milestones(read_rows(args.milestone_data), args.milestones)
    if args.crux_data and args.crux:
        crux(read_rows(args.crux_data), args.crux)
    if args.interface_data and args.interface:
        interface_scatter(read_rows(args.interface_data), args.interface)


if __name__ == "__main__":
    main()

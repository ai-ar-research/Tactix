#!/usr/bin/env python3
"""Draw the milestone roadmap from the retained repository-activity data."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


# Colors match the milestone cumulative-distribution plots.
MILESTONES = (
    ("A", "#6b7280"),
    ("B", "#8b5cf6"),
    ("C", "#2563a6"),
    ("D", "#d97706"),
    ("E", "#be185d"),
    ("Current", "#005f73"),
)


def roadmap(data: Path, output: Path) -> None:
    with data.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert [row["interval"].split("-", 1)[1] for row in rows] == [
        milestone[0] for milestone in MILESTONES
    ]
    days = [int(row["end_day"]) for row in rows]
    assert days == sorted(days) and int(rows[0]["start_day"]) == 0

    plt.rcParams.update({"font.family": "serif", "font.size": 8,
                         "pdf.fonttype": 42, "ps.fonttype": 42})
    fig, axis = plt.subplots(figsize=(7.15, 0.85))
    axis.set_xlim(-4, days[-1] + 8)
    axis.set_ylim(-0.7, 0.45)
    axis.axis("off")
    axis.annotate("", xy=(days[-1] + 7, 0), xytext=(0, 0),
                  arrowprops={"arrowstyle": "->", "color": "#4b5563",
                              "lw": 1.1}, zorder=1)
    axis.text(days[-1] / 2, -0.58, "Days since project start",
              ha="center", va="center", color="#4b5563")

    for day, (label, color) in zip([0, *days], [("Start", "#4b5563"), *MILESTONES]):
        axis.plot([day, day], [-0.09, 0.09], color=color, linewidth=1.4)
        display_label = str("ABCDE".index(label) + 1) if label in "ABCDE" else label
        axis.text(day, 0.18, display_label, ha="center", va="bottom", color=color)
        axis.text(day, -0.18, str(day), ha="center", va="top")

    fig.subplots_adjust(left=0.025, right=0.975, bottom=0.02, top=0.97)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", metadata={"CreationDate": None})
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path(__file__).resolve().parents[1] / "data/milestone-activity.csv")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    roadmap(args.data, args.output)

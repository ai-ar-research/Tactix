#!/usr/bin/env python3
"""Compare exact first-schedule members and physical occurrence scores."""

import json
from pathlib import Path


HERE = Path(__file__).resolve().parent


def parse(path: Path):
    summary = None
    members = {}
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        if not line.startswith("member\t"):
            summary = line
            continue
        fields = {}
        for field in line.split("\t")[1:]:
            key, value = field.split("=", 1)
            fields[key] = value
        external = int(fields.pop("external"))
        members[external] = {
            "position": int(fields["position"]),
            "positive": int(fields["positive"]),
            "negative": int(fields["negative"]),
            "score": fields["score"],
        }
    return summary, members


def main():
    tactix_summary, tactix = parse(HERE / "traces/tactix.tsv")
    kissat_summary, kissat = parse(HERE / "traces/kissat.tsv")
    tactix_set, kissat_set = set(tactix), set(kissat)
    only_tactix = sorted(tactix_set - kissat_set)
    only_kissat = sorted(kissat_set - tactix_set)
    common = sorted(tactix_set & kissat_set)
    occurrence_differences = [
        external for external in common
        if (tactix[external]["positive"], tactix[external]["negative"]) !=
           (kissat[external]["positive"], kissat[external]["negative"])
    ]
    score_differences = [
        external for external in common
        if tactix[external]["score"] != kissat[external]["score"]
    ]
    position_differences = sum(
        tactix[external]["position"] != kissat[external]["position"]
        for external in common)
    comparison = {
        "schema": "tactix-rank17-bve-schedule-member-comparison-v1",
        "tactix_summary": tactix_summary,
        "kissat_summary": kissat_summary,
        "tactix_members": len(tactix),
        "kissat_members": len(kissat),
        "common_members": len(common),
        "only_tactix_count": len(only_tactix),
        "only_kissat_count": len(only_kissat),
        "only_tactix": only_tactix,
        "only_kissat": only_kissat,
        "common_occurrence_difference_count": len(occurrence_differences),
        "common_occurrence_differences": occurrence_differences,
        "common_score_difference_count": len(score_differences),
        "common_score_differences": score_differences,
        "common_position_difference_count": position_differences,
    }
    (HERE / "comparison.json").write_text(
        json.dumps(comparison, indent=2, sort_keys=True) + "\n")
    with (HERE / "schedule-delta.tsv").open("w") as output:
        output.write("external\tmembership\ttactix\tkissat\n")
        for external in only_tactix:
            output.write(f"{external}\tonly-tactix\t{json.dumps(tactix[external], sort_keys=True)}\t-\n")
        for external in only_kissat:
            output.write(f"{external}\tonly-kissat\t-\t{json.dumps(kissat[external], sort_keys=True)}\n")
        for external in occurrence_differences:
            output.write(f"{external}\toccurrence-difference\t{json.dumps(tactix[external], sort_keys=True)}\t{json.dumps(kissat[external], sort_keys=True)}\n")
    print(json.dumps({
        key: comparison[key] for key in (
            "tactix_members", "kissat_members", "common_members",
            "only_tactix_count", "only_kissat_count",
            "common_occurrence_difference_count",
            "common_score_difference_count",
            "common_position_difference_count")
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compute the paper's tables and plot inputs from the normalized results.

    python3 paper/scripts/make_data.py [--logs paper/logs] [--out paper/data]

Reads <logs>/<experiment>/results.csv (written by normalize.py from the
paper's logs, or by bench.py from a new run) and writes the CSV files
under <out>/ that the tables and figures of the paper are drawn from.
"""
from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
YEARS = ("SC2024", "SC2025", "SC2026")
TIERD = ("Tactix", "Satsuma+Kissat", "Kissat 4.0.4", "Kissat-sc2024", "AE-Kissat-MAB", "LymphoSAT")
TABLE = ("Tactix", "Satsuma+Kissat", "Kissat 4.0.4", "LymphoSAT", "Kissat-sc2024", "AE-Kissat-MAB")


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write(path: Path, rows: list[dict], terminator: str = "\n") -> None:
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator=terminator)
        writer.writeheader()
        writer.writerows(rows)


def true(value: str) -> bool:
    return value == "True"


def total(values) -> float:
    """Left-to-right float sum. Python 3.12's sum() compensates rounding, which
    can change the last printed digit; this keeps the files identical across
    Python versions and equal to the paper's."""
    result = 0.0
    for value in values:
        result += value
    return result


def tierd(logs: Path, out: Path) -> None:
    detail = []
    for configuration in TIERD:
        for row in (r for r in read(logs / "tierd/results.csv") if r["configuration"] == configuration):
            memberships = set(row["competition"].split("+"))
            detail.append({
                "configuration": configuration,
                "instance": row["instance"],
                **{year: int(year in memberships) for year in YEARS},
                "verdict": row["verdict"],
                "solved": int(row["verdict"] in {"SAT", "UNSAT"} and row["status"] != "wrong-answer"),
                "accepted": int(true(row["verified"])),
                "solve_seconds": row["solve_seconds"],
                "check_seconds": row["check_seconds"],
                "proof_bytes": row["proof_bytes"],
                "status": row["status"],
                "par2_seconds": row["par2_seconds"],
            })

    def summarize(rows, benchmark):
        if not rows:  # a reduced or smoke run may have no formula of a year
            return None
        solved = [r for r in rows if r["solved"]]
        accepted = [r for r in rows if r["accepted"]]
        return {
            "benchmark": benchmark,
            "configuration": rows[0]["configuration"],
            "instances": len(rows),
            "solved": len(solved),
            "accepted": len(accepted),
            "sat": sum(r["verdict"] == "SAT" for r in solved),
            "unsat": sum(r["verdict"] == "UNSAT" for r in solved),
            "unsat_verified": sum(r["verdict"] == "UNSAT" for r in accepted),
            "accepted_solve_mean_s": (f'{total(float(r["solve_seconds"]) for r in accepted) / len(accepted):.3f}'
                                      if accepted else "nan"),
            "par2_mean_s": f'{total(float(r["par2_seconds"]) for r in rows) / len(rows):.3f}',
        }

    by = {c: [r for r in detail if r["configuration"] == c] for c in TIERD}
    table = []
    for benchmark, year in (("SC24", "SC2024"), ("SC25", "SC2025"), ("SC26", "SC2026"), ("All", None)):
        for configuration in TABLE:
            rows = by[configuration]
            result = summarize(rows if year is None else [r for r in rows if r[year]], benchmark)
            if result is not None:
                table.append(result)
    write(out / "tierd-times.csv", detail, "\r\n")
    write(out / "tierd-table.csv", table)


def milestones(logs: Path, out: Path) -> None:
    raw = read(logs / "milestones/results.csv")
    labels = list(dict.fromkeys(r["configuration"] for r in raw))
    ladder, times, solved_sets = [], [], []
    for label in labels:
        rows = [r for r in raw if r["configuration"] == label]
        commit = rows[0]["commit"]
        times.extend({"label": label, "commit": commit, "instance": r["instance"],
                      "verdict": r["verdict"], "solve_seconds": r["solve_seconds"]} for r in rows)
        solved = [r for r in rows if r["verdict"] in {"SAT", "UNSAT"}]
        solved_sets.append({r["instance"]: r["verdict"] for r in solved})
        ladder.append({"label": label, "commit": commit,
                       "sat": sum(r["verdict"] == "SAT" for r in solved),
                       "unsat": sum(r["verdict"] == "UNSAT" for r in solved),
                       "solved": len(solved)})
    transitions = []
    for index, (before, after) in enumerate(zip(solved_sets, solved_sets[1:])):
        gained = {k: v for k, v in after.items() if k not in before}
        lost = {k: v for k, v in before.items() if k not in after}
        transitions.append({"from": labels[index], "to": labels[index + 1],
                            "gained_sat": sum(v == "SAT" for v in gained.values()),
                            "gained_unsat": sum(v == "UNSAT" for v in gained.values()),
                            "lost_sat": sum(v == "SAT" for v in lost.values()),
                            "lost_unsat": sum(v == "UNSAT" for v in lost.values())})
    write(out / "milestone-ladder.csv", ladder)
    write(out / "milestone-times.csv", times)
    write(out / "milestone-transitions.csv", transitions)


def interface(logs: Path, out: Path) -> None:
    rows = read(logs / "interface/results.csv")
    pairs = {}
    for row in rows:
        pairs.setdefault((row["set"], row["instance"]), {})[row["configuration"]] = row
    plot = []
    for (name, instance), arms in pairs.items():
        cpp, api = arms.get("C++"), arms.get("C interface")
        if not cpp or not api:
            continue
        if any(r["verdict"] not in ("SAT", "UNSAT") or true(r["timed_out"]) for r in (cpp, api)):
            continue
        plot.append({"set": name, "instance": instance, "verdict": api["verdict"],
                     "cpp_seconds": float(cpp["solve_seconds"]),
                     "c_api_seconds": float(api["solve_seconds"])})
    plot.sort(key=lambda r: (("Tier A", "B28+C5", "Rank 1-50", "WV100").index(r["set"]), r["instance"]))
    for row in plot:
        row["instance"] = row["instance"].split("/")[-1]
    write(out / "interface-overhead.csv", plot, "\r\n")


def cruxmiter(logs: Path, out: Path) -> None:
    rows = [{"configuration": r["configuration"],
             "seed": int(Path(r["instance"]).stem.replace("cruxmiter32seed", "")),
             "verdict": r["verdict"],
             "solve_seconds": f'{float(r["solve_seconds"]):.3f}',
             "check_seconds": f'{float(r["check_seconds"]):.3f}' if r["check_seconds"] else "",
             "proof_bytes": int(r["proof_bytes"]) if r["proof_bytes"] else "",
             "verified": int(true(r["verified"]))}
            for r in read(logs / "cruxmiter/results.csv")]
    write(out / "crux-results.csv", rows, "\r\n")


def chvatal(logs: Path, out: Path) -> None:
    rows = [{"configuration": r["configuration"],
             "node": Path(r["instance"]).stem.removeprefix("n7-"),
             "verdict": r["verdict"],
             "solve_seconds": f'{float(r["solve_seconds"]):.3f}',
             "check_seconds": "" if r["check_seconds"] == "" else f'{float(r["check_seconds"]):.3f}',
             "proof_bytes": r["proof_bytes"],
             "verified": int(true(r["verified"])),
             "kissat_screen_seconds": r["kissat_screen_seconds"]}
            for r in read(logs / "chvatal/results.csv")]
    rows.sort(key=lambda r: (r["node"], r["configuration"]))
    write(out / "chvatal-results.csv", rows)


# The paper's limits, for logs that do not record them (the archived campaigns).
PAPER_LIMITS = {"tierd": {"seconds": 5000, "check_seconds": 28800},
                "milestones": {"seconds": 5000, "check_seconds": 28800},
                "interface": {"seconds": 1200, "check_seconds": 1800},
                "cruxmiter": {"seconds": 43200, "check_seconds": 259200},
                "chvatal": {"seconds": 5000, "check_seconds": 7200}}


def limits(logs: Path, out: Path) -> None:
    """limits.json: each experiment's solving and checking limits and the
    profile of the run, for the figures' axes and the PAR-2 penalties."""
    import json
    result = {"profile": "full"}
    for name, default in PAPER_LIMITS.items():
        path = logs / name / "results.csv"
        if not path.exists():
            continue
        rows = read(path)
        if rows and rows[0].get("solve_limit_seconds"):
            # Interface Tier A has its own, shorter limit; report the main one.
            result[name] = {"seconds": max(float(r["solve_limit_seconds"]) for r in rows),
                            "check_seconds": max(float(r["check_limit_seconds"]) for r in rows),
                            "memory_gib": max(float(r["memory_limit_gib"]) for r in rows)}
            if rows[0].get("profile", "full") != "full":
                result["profile"] = rows[0]["profile"]
        else:
            result[name] = dict(default)
    (out / "limits.json").write_text(json.dumps(result, indent=1) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--logs", type=Path, default=PAPER / "logs")
    parser.add_argument("--out", type=Path, default=PAPER / "data")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for name, step in (("tierd", tierd), ("milestones", milestones), ("interface", interface),
                       ("cruxmiter", cruxmiter), ("chvatal", chvatal)):
        if (args.logs / name / "results.csv").exists():
            step(args.logs, args.out)
            print(f"{name}: done")
    limits(args.logs, args.out)


if __name__ == "__main__":
    main()

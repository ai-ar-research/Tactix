#!/usr/bin/env python3
"""Reproduce milestone activity from a read-only Tactix Git repository.

Counts all reachable commits in each previous..next range, with merge counts
reported separately. Cumulative line changes sum non-merge commit diffs;
endpoint changes compare the two trees. Renames are deliberately treated as
deletion/addition, making the measure explicit and independent of similarity
thresholds. Binary files have no line count and are counted separately.

These are repository activity measures, not labor or productivity measures.
The category policy below separates source from archived data and vendor code.
Code stored under benchmarks/ is reported separately: this directory includes
copied source trees, proposed patches, and per-experiment replay scripts.
Day offsets use commit calendar dates in their recorded timezone; day 0 is the
initial project commit. The working tree is never read or modified.
"""

import argparse
import csv
import datetime as dt
from collections import defaultdict
from pathlib import Path
import subprocess


MILESTONES = [
    ("A", "dcc3ecbd6", 625),
    ("B", "546298924", 735),
    ("C", "6f44f23d6", 818),
    ("D", "f50e46c0c", 977),
    ("E", "0a9063387", 979),
    ("Current", "0f7c10174", 1006),
]
ROOT = "30a812e30"
CODE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".py", ".sh", ".bash", ".sbatch", ".slurm", ".cmake"}
DOC_SUFFIXES = {".md", ".tex", ".bib", ".rst"}


def category(path):
    p = Path(path)
    code = p.suffix in CODE_SUFFIXES or p.name == "CMakeLists.txt"
    if path.startswith("third_party/"):
        return "third_party"
    if path.startswith(".claude/worktrees/"):
        return "archived_worktrees"
    if p.suffix in DOC_SUFFIXES or path.startswith(("docs/", "report-to-andrew/", ".claude/")):
        return "documentation" if p.suffix in DOC_SUFFIXES or p.suffix == "" else "report_assets"
    if path.startswith(("include/", "src/api/", "examples/")) and code:
        return "public_api"
    if path.startswith("src/") and code:
        return "solver"
    if path.startswith(("tests/", "tools/fuzz/", "corpus/")):
        return "tests_fuzz_code" if code else "test_assets"
    if path.startswith("benchmarks/"):
        return "experiment_archive_code" if code else "experiment_assets"
    if path.startswith(("tools/bench/", "tools/slurm/", "tools/checkers/", "tools/solvers/", "tools/compete/")):
        return "experiment_code" if code or (path.startswith("tools/") and not p.suffix) else "experiment_assets"
    return "build_other"


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True)


def summarize_numstat(text):
    groups = defaultdict(lambda: [0, 0, 0])
    for line in text.splitlines():
        if not line:
            continue
        added, deleted, path = line.split("\t", 2)
        values = groups[category(path)]
        if added == "-":
            values[2] += 1
        else:
            values[0] += int(added)
            values[1] += int(deleted)
    return groups


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parents[1] / "data")
    args = parser.parse_args()
    start = dt.datetime.fromisoformat(git(args.repo, "show", "-s", "--format=%cI", ROOT).strip()).date()
    summary, categories = [], []
    previous, previous_name, previous_day, previous_solved = None, "Start", 0, None
    for name, revision, solved in MILESTONES:
        full_revision = git(args.repo, "rev-parse", revision).strip()
        commit_date = dt.datetime.fromisoformat(git(args.repo, "show", "-s", "--format=%cI", revision).strip()).date()
        day = (commit_date - start).days
        commit_range = f"{previous}..{revision}" if previous else revision
        if previous:
            subprocess.run(["git", "-C", str(args.repo), "merge-base", "--is-ancestor", previous, revision], check=True)
        commits = int(git(args.repo, "rev-list", "--count", commit_range))
        merges = int(git(args.repo, "rev-list", "--count", "--merges", commit_range))
        cumulative = summarize_numstat(git(args.repo, "log", "--no-merges", "--numstat", "--no-renames", "--format=", commit_range))
        empty_tree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904"
        endpoint = summarize_numstat(git(args.repo, "diff", "--numstat", "--no-renames", previous or empty_tree, revision))
        interval = f"{previous_name}-{name}"
        row = dict(interval=interval, revision=full_revision, start_day=previous_day,
                   end_day=day, interval_days=day-previous_day, commits=commits,
                   merge_commits=merges, nonmerge_commits=commits-merges,
                   solved=solved, solved_gain="" if previous_solved is None else solved-previous_solved)
        for measure, groups in [("cumulative", cumulative), ("endpoint", endpoint)]:
            row[f"{measure}_added"] = sum(v[0] for v in groups.values())
            row[f"{measure}_deleted"] = sum(v[1] for v in groups.values())
            row[f"{measure}_binary_changes"] = sum(v[2] for v in groups.values())
            for label, (added, deleted, binaries) in sorted(groups.items()):
                categories.append(dict(interval=interval, measure=measure, category=label,
                                       added=added, deleted=deleted, binary_changes=binaries))
        summary.append(row)
        previous, previous_name, previous_day, previous_solved = revision, name, day, solved
        print(interval, "day", day, "commits", commits, "merges", merges,
              "cumulative", row["cumulative_added"], row["cumulative_deleted"], flush=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(args.output_dir / "milestone-activity.csv", summary)
    write_csv(args.output_dir / "milestone-activity-categories.csv", categories)


if __name__ == "__main__":
    main()

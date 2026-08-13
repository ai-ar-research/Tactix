#!/usr/bin/env python3
"""Render the durable frozen-baseline tables as a compact Andrew report."""

import argparse
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CAMPAIGN = ROOT / "benchmarks/baselines/sc24-26-32g-5000s-v1"
DEFAULT_OUTPUT = (ROOT / "report-to-andrew" /
                  "2026-08-13-tier-d-frozen-baselines" / "report.tex")


def esc(value):
    out = str(value)
    for old, new in (("\\", r"\textbackslash{}"), ("_", r"\_"),
                     ("%", r"\%"), ("&", r"\&"), ("#", r"\#")):
        out = out.replace(old, new)
    return out


def median(values):
    return f"{statistics.median(values):.2f}" if values else "--"


def maximum(values, divisor=1):
    return f"{max(values) / divisor:.2f}" if values else "--"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--campaign", type=Path, default=DEFAULT_CAMPAIGN)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = ap.parse_args()
    campaign = args.campaign.resolve()
    manifest = json.loads((campaign / "manifest.json").read_text())
    results = []
    for path in sorted((campaign / "results").glob("*.json")):
        results.append(json.loads(path.read_text()))
    years = {}
    with open(campaign / "year-membership.tsv", newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            years[row["filename"]] = [year for year in ("2024", "2025", "2026")
                                       if row[f"SC{year}"] == "1"]
    by_solver = defaultdict(list)
    for result in results:
        by_solver[result["solver"]].append(result)

    coverage_rows = []
    resource_rows = []
    for solver in manifest["solvers"]:
        name = solver["name"]
        rows = by_solver[name]
        cells = []
        for year in ("2024", "2025", "2026"):
            subset = [r for r in rows if year in years.get(Path(r["instance"]).name, [])]
            eligible = ([r for r in subset if r.get("expected") == "SAT"]
                        if solver["vbs_eligible"] == "SAT-only" else subset)
            cells.append(f"{sum(r.get('valid') is True for r in eligible)}/{len(eligible)}")
        eligible = ([r for r in rows if r.get("expected") == "SAT"]
                    if solver["vbs_eligible"] == "SAT-only" else rows)
        cells.append(f"{sum(r.get('valid') is True for r in eligible)}/{len(eligible)}")
        coverage_rows.append((name, *cells))
        checks = [r["proof_check"] for r in rows if r.get("proof_ok") is True]
        resource_rows.append((
            name,
            median([r["solve"]["wall_seconds"] for r in rows]),
            maximum([r["solve"]["peak_rss_kb"] for r in rows
                     if r["solve"].get("peak_rss_kb")], 1024 * 1024),
            median([c["wall_seconds"] for c in checks]),
            maximum([c["peak_rss_kb"] for c in checks if c.get("peak_rss_kb")],
                    1024 * 1024),
            sum(r.get("valid") is False for r in rows),
        ))

    hashes = [
        ("dataset.json", manifest["dataset_sha256"]),
        ("instances.list", manifest["instances_list_sha256"]),
        ("cache manifest", manifest["cache_manifest_sha256"]),
        ("campaign manifest", __import__("hashlib").sha256(
            (campaign / "manifest.json").read_bytes()).hexdigest()),
    ]
    hashes.extend((name, item["sha256"]) for name, item in manifest["provenance"].items())
    complete = len(results) == len(manifest["tasks"])
    tex = [r"\documentclass[10pt]{article}", r"\usepackage[margin=0.7in]{geometry}",
           r"\usepackage{booktabs}", r"\usepackage[T1]{fontenc}",
           r"\begin{document}", r"\section*{Frozen SC24--26 Tier-D baselines}",
           ("This is the durable summary of the once-only campaign at "
            r"\texttt{" + esc(campaign.relative_to(ROOT)) + "}. "
            f"Status: {'complete' if complete else 'running'} ({len(results)}/"
            f"{len(manifest['tasks'])} tasks). Solvers receive 5000 s and 32 GiB; "
            "checker pipelines receive 45000 s. Successful certificates are deleted "
            "only after their classification, hashes, and resource measurements are durable."),
           r"\subsection*{Validated coverage}",
           r"\begin{tabular}{lrrrr}\toprule Solver & SC24 & SC25 & SC26 & Union\\\midrule"]
    tex += [" & ".join(map(esc, row)) + r"\\" for row in coverage_rows]
    tex += [r"\bottomrule\end{tabular}", r"\subsection*{Resources and failures}",
            r"\begin{tabular}{lrrrrr}\toprule Solver & Solve med. (s) & Solve max (GiB) & Check med. (s) & Check max (GiB) & Invalid\\\midrule"]
    tex += [" & ".join(map(esc, row)) + r"\\" for row in resource_rows]
    tex += [r"\bottomrule\end{tabular}",
            r"\subsection*{SC2026 checking contract}",
            "UNSAT is counted only after the submission-selected certificate path reaches "
            "a formally verified backend: DRAT to LRAT to cake\_lpr; DSR (the Satsuma "
            "SR selection) to text DSR to DRAT to LRAT to cake\_lpr; or VeriPB PBP to "
            "kernel PBP to cake\_pb\_cnf. SAT output follows SC2026: non-contradictory "
            "partial assignments are allowed, the final value line ends in zero, and every "
            "clause must contain an explicitly assigned satisfying literal. LymphoSAT is a "
            "SAT-only target: its validated UNSAT answers are reported but excluded from VBS "
            "and coverage comparisons.",
            r"\subsection*{Replay limits}",
            "Lower solve-time limits are exact truncations of recorded wall times. Lower "
            "memory limits are only high-water simulations from peak RSS and must be labelled "
            "limited; a peak does not reveal when it occurred or whether allocation failure "
            "would change search. Checker time is reported separately from solve time.",
            r"\subsection*{Immutable hashes}",
            r"\begin{tabular}{lp{0.72\linewidth}}\toprule Artifact & SHA-256\\\midrule"]
    tex += [esc(name) + " & \texttt{" + esc(digest) + r"}\\" for name, digest in hashes]
    tex += [r"\bottomrule\end{tabular}", r"\end{document}", ""]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(tex))
    print(args.output)


if __name__ == "__main__":
    main()

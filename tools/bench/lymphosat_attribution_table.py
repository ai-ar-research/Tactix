#!/usr/bin/env python3
"""Render the LymphoSAT per-instance attribution deliverables.

Consumes a completed attribution campaign (results.json from
lymphosat_attribution.py --aggregate) and emits deterministic renderings:

  --tsv       per-instance table: instance, verdict, runtime, peak RSS,
              responsible module, ordered attempt sequence with outcomes
  --markdown  frozen-report body: summary tables plus the per-instance table
  --latex     Andrew-report fragment: summary + longtable of all instances

No solver execution and no comparison arms: this is a pure rendering of the
recorded attribution data.
"""

import argparse
import json
from collections import Counter
from pathlib import Path


def outcome_label(outcome):
    if outcome == "solved":
        return "solved"
    if outcome == "returned-unknown":
        return "returned UNKNOWN"
    if outcome == "launched":
        return "still running at limit"
    if outcome == "matcher-threw":
        return "matcher threw"
    if outcome.startswith("exited-"):
        return f"exited {outcome.split('-', 1)[1]}"
    return outcome


def attempt_sequence(record):
    parts = [f"{a['solver']} ({outcome_label(a['outcome'])})"
             for a in record.get("attribution", {}).get("attempts", [])]
    return " -> ".join(parts) if parts else "(no sub-solver launched)"


def instance_label(rel):
    name = Path(rel).name
    for suffix in (".xz", ".cnf", ".sanitized"):
        if name.endswith(suffix):
            name = name[: -len(suffix)]
    return name


def load(campaign):
    summary = json.loads((campaign / "results.json").read_text())
    rows = [r for r in summary["results"] if r.get("outcome") != "MISSING_INPUT"]
    rows.sort(key=lambda r: r["task"])
    return summary, rows


def render_tsv(rows):
    out = ["instance\tverdict\twall_seconds\tpeak_rss_kb\tresponsible\tattempt_sequence"]
    for r in rows:
        out.append("\t".join([
            instance_label(r["instance"]),
            r["verdict"],
            f"{r['solve']['wall_seconds']:.3f}",
            str(r["solve"]["peak_rss_kb"]),
            r["attribution"]["responsible"] or "-",
            attempt_sequence(r),
        ]))
    return "\n".join(out) + "\n"


def module_summary(rows):
    per_module = {}
    fallback_reason = Counter()
    cascade_lengths = Counter()
    for r in rows:
        attempts = r["attribution"]["attempts"]
        cascade_lengths[len(attempts)] += 1
        if r["verdict"] not in ("SAT", "UNSAT"):
            continue
        who = r["attribution"]["responsible"] or "UNATTRIBUTED"
        m = per_module.setdefault(who, {"SAT": 0, "UNSAT": 0, "walls": []})
        m[r["verdict"]] += 1
        m["walls"].append(r["solve"]["wall_seconds"])
        if who == "fallback":
            specialist_tried = any(a["via"] == "matcher" for a in attempts)
            fallback_reason["specialist gave up first" if specialist_tried
                            else "no matcher matched"] += 1
    table = []
    for name, m in sorted(per_module.items(),
                          key=lambda kv: -(kv[1]["SAT"] + kv[1]["UNSAT"])):
        walls = sorted(m["walls"])
        table.append({
            "module": name, "SAT": m["SAT"], "UNSAT": m["UNSAT"],
            "total": m["SAT"] + m["UNSAT"],
            "median_wall": walls[len(walls) // 2],
            "max_wall": walls[-1],
        })
    return table, fallback_reason, cascade_lengths


def render_markdown(summary, rows):
    table, fallback_reason, cascades = module_summary(rows)
    solved = summary["solved"]
    lines = [
        "## Module distribution (solved instances)",
        "",
        "| module | SAT | UNSAT | total | median wall (s) | max wall (s) |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for t in table:
        lines.append(f"| {t['module']} | {t['SAT']} | {t['UNSAT']} | "
                     f"{t['total']} | {t['median_wall']:.1f} | {t['max_wall']:.1f} |")
    lines += [
        "",
        f"Solved {solved} of {summary['result_count']} instances; "
        f"{summary['unknown']} UNKNOWN.",
        "",
        "Fallback wins by cause: " + (", ".join(
            f"{v} {k}" for k, v in sorted(fallback_reason.items())) or "none"),
        "",
        "Attempt-cascade lengths: " + ", ".join(
            f"{n} attempt(s): {c} instances"
            for n, c in sorted(cascades.items())),
        "",
        "## Per-instance attribution",
        "",
        "| instance | verdict | wall (s) | sub-solvers tried (in order) |",
        "|---|---|---:|---|",
    ]
    for r in rows:
        lines.append(f"| {instance_label(r['instance'])} | {r['verdict']} | "
                     f"{r['solve']['wall_seconds']:.1f} | {attempt_sequence(r)} |")
    return "\n".join(lines) + "\n"


def latex_escape(text):
    for a, b in (("\\", r"\textbackslash{}"), ("_", r"\_"), ("%", r"\%"),
                 ("&", r"\&"), ("#", r"\#"), ("$", r"\$"), ("{", r"\{"),
                 ("}", r"\}"), ("^", r"\^{}"), ("~", r"\~{}")):
        text = text.replace(a, b)
    return text


def render_latex(summary, rows):
    table, fallback_reason, cascades = module_summary(rows)
    lines = [
        r"\begin{longtable}{lrrrrr}",
        r"\caption{LymphoSAT module distribution over solved Tier-D instances.}\\",
        r"\toprule",
        r"module & SAT & UNSAT & total & median wall (s) & max wall (s)\\",
        r"\midrule",
        r"\endhead",
    ]
    for t in table:
        lines.append(f"{latex_escape(t['module'])} & {t['SAT']} & {t['UNSAT']} & "
                     f"{t['total']} & {t['median_wall']:.1f} & {t['max_wall']:.1f}\\\\")
    lines += [r"\bottomrule", r"\end{longtable}", ""]
    lines += [
        r"\begin{longtable}{p{0.34\textwidth}lrp{0.42\textwidth}}",
        r"\caption{Per-instance verdict, runtime, and ordered sub-solver attempts.}\\",
        r"\toprule",
        r"instance & verdict & wall (s) & sub-solvers tried (in order)\\",
        r"\midrule",
        r"\endhead",
        r"\midrule\multicolumn{4}{r}{\emph{continued on next page}}\\",
        r"\endfoot",
        r"\endlastfoot",
    ]
    for r in rows:
        lines.append(
            f"{latex_escape(instance_label(r['instance']))} & {r['verdict']} & "
            f"{r['solve']['wall_seconds']:.1f} & "
            f"{latex_escape(attempt_sequence(r))}\\\\")
    lines += [r"\bottomrule", r"\end{longtable}"]
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--campaign", required=True, type=Path)
    ap.add_argument("--out", type=Path, help="output file (default: stdout)")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--tsv", action="store_true")
    g.add_argument("--markdown", action="store_true")
    g.add_argument("--latex", action="store_true")
    args = ap.parse_args()
    summary, rows = load(args.campaign.resolve())
    if args.tsv:
        text = render_tsv(rows)
    elif args.markdown:
        text = render_markdown(summary, rows)
    else:
        text = render_latex(summary, rows)
    if args.out:
        args.out.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()

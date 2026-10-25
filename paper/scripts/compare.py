#!/usr/bin/env python3
"""Put a rerun of the experiments next to the paper's archived logs.

    python3 paper/scripts/compare.py --run paper/run/results --out DIR

For every experiment in the rerun (bench.py's output directory), the paper's
archived results (paper/logs/) for the same configurations and formulas are
taken as recorded, at the paper's limits (no cut-off at the rerun's limits).
Both go through the paper's analysis scripts:

  DIR/<experiment>.md                  a per-formula table and the paper's tables
  DIR/<experiment>-per-instance.csv    each run next to the archived one
  DIR/comparison.md                    all of the above
  DIR/artifact/{logs,data,figures}     the rerun
  DIR/paper-logs/{logs,data,figures}   the paper's logs on the same runs

    --show EXPERIMENT...   print only these experiments (all are written)
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
PAPER = HERE.parent
sys.path.insert(0, str(HERE))
import report  # noqa: E402
sys.path.insert(0, str(PAPER / "run"))
from experiments import EXPERIMENTS as FULL  # noqa: E402  (the paper's limits)

EXPERIMENTS = ("tierd", "interface", "cruxmiter", "chvatal", "milestones")


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(dict.fromkeys(k for row in rows for k in row))
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def key(instance: str) -> str:
    """The formula's name, whatever directory, compression or hash prefix."""
    name = Path(instance).name.removesuffix(".xz")
    return re.sub(r"^[0-9a-f]{32}-", "", name)


def true(value) -> bool:
    return str(value) == "True"


def paper_view(run_logs: Path, out_logs: Path) -> list[dict]:
    """Write the archived results of the rerun's configurations and formulas;
    return the per-instance comparison rows."""
    pairs = []
    for name in EXPERIMENTS:
        run_path = run_logs / name / "results.csv"
        if not run_path.exists():
            continue
        archived = {(r["configuration"], key(r["instance"])): r
                    for r in read(PAPER / "logs" / name / "results.csv")}
        rows = []
        for run in read(run_path):
            paper = archived.get((run["configuration"], key(run["instance"])))
            if paper is None:
                print(f"{name}: no archived result for {run['configuration']} on {run['instance']}",
                      file=sys.stderr)
                continue
            # As recorded; the rerun's spelling of the instance, so the tables align.
            rows.append(dict(paper, instance=run["instance"]))
            answered = {side: row["verdict"] in ("SAT", "UNSAT") for side, row in (("run", run), ("paper", paper))}
            pairs.append({
                "experiment": name, "set": run.get("set", ""), "configuration": run["configuration"],
                "formula": key(run["instance"]),
                "rerun_verdict": run["verdict"], "rerun_solve_seconds": run["solve_seconds"],
                # a verified check: the checkers' own time, as the paper's records give it
                "rerun_check_seconds": run.get("checker_seconds") if true(run.get("verified"))
                and run.get("checker_seconds") else run.get("check_seconds", ""),
                "rerun_status": run.get("status", ""),
                "rerun_verified": run.get("verified", ""),
                "rerun_proof_bytes": run.get("proof_bytes", ""),
                "paper_verdict": paper["verdict"], "paper_solve_seconds": paper["solve_seconds"],
                # the checkers' own time where the campaign also timed its input preparation
                "paper_check_seconds": paper.get("check_stage_seconds") or paper.get("check_seconds", ""),
                "paper_status": paper.get("status", ""),
                "paper_verified": paper.get("verified", ""),
                "paper_proof_bytes": paper.get("proof_bytes", ""),
                "solve_limit_seconds": run["solve_limit_seconds"],
                "check_limit_seconds": run["check_limit_seconds"],
                "same_outcome": answered["run"] == answered["paper"]
                and (not answered["run"] or run["verdict"] == paper["verdict"]),
            })
        if rows:
            write(out_logs / name / "results.csv", rows)
    return pairs


def current_from_tierd(logs: Path) -> None:
    """The milestones' current version is the tierd experiment's Tactix (the
    same build and command), as in the paper: the reduced and smoke profiles
    run only the five earlier versions, and the current version's rows are
    taken from the tierd runs on the same formulas."""
    milestones, tierd = logs / "milestones/results.csv", logs / "tierd/results.csv"
    if not milestones.exists() or not tierd.exists():
        return
    rows = read(milestones)
    if any(r["configuration"] == "Current" for r in rows):
        return
    formulas = {key(r["instance"]) for r in rows}
    current = [dict(r, configuration="Current", commit="0f7c10174") for r in read(tierd)
               if r["configuration"] == "Tactix" and key(r["instance"]) in formulas]
    write(milestones, rows + current)


# The paper's figures, by the data file they are drawn from.
FIGURES = {"tierd-times.csv": ("tierd-cactus.pdf", "tierd-scatter.pdf", "tierd-scatter-satsuma.pdf",
                               "tierd-scatter-lympho.pdf"),
           "interface-overhead.csv": ("interface-overhead.pdf",),
           "milestone-times.csv": ("milestone-ladder.pdf",),
           "chvatal-results.csv": ("chvatal-scatter.pdf", "chvatal-table.tex")}


def build(logs: Path, base: Path) -> None:
    """data/ from logs/, then the figures of the experiments that have data,
    with the paper's Makefile."""
    make = ["make", "-s", "-C", str(PAPER), f"LOGS={logs}", f"DATA={base / 'data'}",
            f"FIGURES={base / 'figures'}"]
    subprocess.run(make + ["data"], check=True, stdout=subprocess.DEVNULL)
    targets = [str(base / "figures" / f) for data, figures in FIGURES.items()
               if (base / "data" / data).exists() for f in figures]
    if targets:
        subprocess.run(make + targets, check=True, stdout=subprocess.DEVNULL)


@dataclass
class Paired:
    """A paper table with, next to each value column, the same column from the
    paper's runs of the same formulas."""
    name: str
    title: str
    header: list[str]               # the key columns, then the value columns
    keys: int
    paired: list[bool]              # per value column
    rows: list[list[str]]           # keys, then per value column one cell, or two if paired
    notes: list[str]


def pair(rerun: report.Table, paper: report.Table | None) -> Paired:
    lookup = {tuple(r[:rerun.keys]): r for r in (paper.rows if paper else [])}
    paired = [True] * (len(rerun.header) - rerun.keys)
    rows = []
    for row in rerun.rows:
        other = lookup.get(tuple(row[:rerun.keys]))
        line = row[:rerun.keys]
        for i, both in enumerate(paired, rerun.keys):
            line += [row[i], other[i] if other else "-"] if both else [row[i]]
        rows.append(line)
    return Paired(rerun.name, rerun.title, rerun.header, rerun.keys, paired, rows, rerun.notes)


def render(table: Paired, markdown: bool) -> str:
    """Markdown: one header row ("Solved", "Paper solved"). Text: the paper's
    column names above a Rerun and a Paper column each."""
    keys, values = table.header[:table.keys], table.header[table.keys:]
    if markdown:
        header = keys + [c for h, both in zip(values, table.paired)
                         for c in ([h, paper_header(h)] if both else [h])]
        return report.render(report.Table(table.name, table.title, header, table.rows, table.keys,
                                          table.notes), True)
    top, sub = [""] * len(keys), list(keys)
    groups = [(i, 1) for i in range(len(keys))]  # (first cell, cells) per printed group
    cell = len(keys)
    for h, both in zip(values, table.paired):
        if both:
            top += [h]; sub += ["Rerun", "Paper"]; groups.append((cell, 2)); cell += 2
        else:
            top += [""]; sub += [h]; groups.append((cell, 1)); cell += 1
    width = [max(len(r[i]) for r in [sub] + table.rows) for i in range(len(sub))]
    for (first, n), label in zip(groups, top):  # a group is as wide as its label
        extra = len(label) - (width[first] + 2 + width[first + 1]) if n == 2 else 0
        if extra > 0:
            width[first] += extra // 2
            width[first + 1] += extra - extra // 2
    def line(cells, left):
        return "  " + "  ".join(c.ljust(width[i]) if i < left else c.rjust(width[i])
                                for i, c in enumerate(cells))
    head = "  " + "  ".join(label.rjust(sum(width[first:first + n]) + 2 * (n - 1))
                            for (first, n), label in zip(groups, top))
    text = [f"\n{table.title}\n", head.rstrip(), line(sub, table.keys),
            "  " + "  ".join("-" * w for w in width)]
    text += [line(r, table.keys) for r in table.rows]
    return "\n".join(text + [f"  {n}" for n in table.notes])


def paper_header(header: str) -> str:
    first = header.split()[0]
    return "Paper " + (header if first.isupper() else header[0].lower() + header[1:])


READING = """\
Values: the rerun. "Paper" columns: the paper's runs of the same configurations and formulas,
as recorded, at the paper's limits (README.md, section 3.2).
*: a paper time above the rerun's limit. timeout: no answer within the limit.
>N: checking stopped after N s. rejected: a checker did not accept the certificate.
model not ok: an assignment printed in a nonstandard format, not counted (as in the paper).
-: no answer, or not checked."""

# Per experiment: title, the configuration column's name, whether certificates
# are checked, the paper's tables, the figures. README.md section 3.2 states each
# experiment's claim and how to read these tables.
GUIDE = {
    "tierd": ("tierd: SAT Competition 2024-2026 main-track formulas (Figures 3 and 4)",
              "Solver", True, ("figure4",),
              ("tierd-cactus.pdf (Figure 3)", "tierd-scatter.pdf (Figure 4)",
               "tierd-scatter-satsuma.pdf, tierd-scatter-lympho.pdf (appendix)")),
    "milestones": ("milestones: earlier versions of Tactix on the tierd formulas (Table 6, Figure 7)",
                   "Version", False, (), ("milestone-ladder.pdf (Figure 7)",)),
    "interface": ("interface: the default strategy through the C interface and in C++ (Figure 5)",
                  "Configuration", False, ("figure5",), ("interface-overhead.pdf (Figure 5)",)),
    "cruxmiter": ("cruxmiter: case study 1, arithmetic circuit verification (Table 4)",
                  "Configuration", True, (), ()),
    "chvatal": ("chvatal: case study 2, subproblems of Chvatal's conjecture (Table 5, Figure 6)",
                "Configuration", True, (),
                ("chvatal-scatter.pdf (Figure 6)", "chvatal-table.tex (Table 5 in LaTeX)")),
}
PROOF_SIZE = ("cruxmiter",)  # Table 4 also reports certificate sizes
ORDER = {"tierd": report.TIERD_ORDER, "cruxmiter": report.CRUX_ORDER, "chvatal": report.CHVATAL_ORDER,
         "milestones": tuple(m for m, _ in report.MILESTONES)}
VERSION = {m: ("Current" if v == "Now" else f"Version {v}") for m, v in report.MILESTONES}


def solve_cell(verdict: str, seconds: str, status: str, limit: float) -> str:
    if verdict in ("SAT", "UNSAT"):
        return f"{float(seconds):.1f}" + ("*" if float(seconds) > limit else "")
    return "memory-out" if "memout" in status or "memory" in status else \
        "timeout" if status in ("timeout", "") else "no answer"


def check_cell(verdict: str, seconds: str, status: str, verified: str, limit: float,
               stop: float) -> str:
    """limit: the rerun's checking limit (* above it); stop: the limit the run was checked with."""
    if verdict == "SAT":
        return "model ok" if true(verified) else \
            "model not ok" if "nonprotocol" in status or "nonstandard" in status else "rejected"
    if verdict != "UNSAT" or not seconds:
        return "-"
    if true(verified):
        return f"{float(seconds):.1f}" + ("*" if float(seconds) > limit else "")
    return f">{stop:g}" if float(seconds) >= 0.98 * stop else "rejected"


def per_formula(name: str, pairs: list[dict]) -> report.Table:
    _, label, checks, _, _ = GUIDE[name]
    order = ORDER.get(name, ())
    pairs = sorted(pairs, key=lambda p: (p["formula"], order.index(p["configuration"])
                                         if p["configuration"] in order else len(order)))
    header = ["Formula", label, "Verdict", "Runtime (s)", "Paper runtime (s)"]
    header += ["Check (s)", "Paper check (s)"] if checks else []
    header += ["Proof (GB)", "Paper proof (GB)"] if name in PROOF_SIZE else []
    paper_check = FULL[name]["limits"]["check_seconds"]
    rows, previous = [], None
    for p in pairs:
        solve, check = float(p["solve_limit_seconds"]), float(p["check_limit_seconds"])
        verdict = p["rerun_verdict"] if p["rerun_verdict"] in ("SAT", "UNSAT") else "-"
        formula = p["formula"].removesuffix(".cnf")
        row = [(formula if len(formula) <= 32 else formula[:29] + "...") if p["formula"] != previous else "",
               VERSION.get(p["configuration"], p["configuration"]), verdict,
               solve_cell(p["rerun_verdict"], p["rerun_solve_seconds"], p["rerun_status"], solve),
               solve_cell(p["paper_verdict"], p["paper_solve_seconds"], p["paper_status"], float("inf"))
               if p["paper_verdict"] not in ("SAT", "UNSAT") else
               solve_cell(p["paper_verdict"], p["paper_solve_seconds"], p["paper_status"], solve)]
        if checks:
            row += [check_cell(p["rerun_verdict"], p["rerun_check_seconds"], p["rerun_status"],
                               p["rerun_verified"], check, check),
                    check_cell(p["paper_verdict"], p["paper_check_seconds"], p["paper_status"],
                               p["paper_verified"], check, paper_check)]
        if name in PROOF_SIZE:
            row += [f"{int(b) / 1e9:.2f}" if v == "UNSAT" and b else "-"
                    for v, b in ((p["rerun_verdict"], p["rerun_proof_bytes"]),
                                 (p["paper_verdict"], p["paper_proof_bytes"]))]
        rows.append(row)
        previous = p["formula"]
    limits = f"limits: {solve:g} s to solve" + (f", {check:g} s to check" if checks else "")
    return report.Table(f"{name}-per-formula", f"Per formula ({limits})", header, rows, keys=3)


def section(name: str, pairs: list[dict], tables: dict[str, report.Table], out: Path,
            markdown: bool) -> str:
    title, _, _, names, figures = GUIDE[name]
    shown = ([per_formula(name, pairs)] if name != "interface" else []) + \
        [tables[t] for t in names if t in tables]
    if markdown:
        text = [f"## {title}", ""]
        text += [render(t, True) if isinstance(t, Paired) else report.render(t, True) for t in shown]
        if figures:
            text += ["**Figures:** " + "; ".join(figures) + " in artifact/figures/ (the rerun) and "
                     "paper-logs/figures/ (the paper's logs on the same runs).", ""]
        return "\n".join(text)
    text = [f"\n== {title}"]
    text += [render(t, False) if isinstance(t, Paired) else report.render(t) for t in shown]
    if figures:
        text += ["", "Figures (rerun; the paper's logs in paper-logs/figures/): " + "; ".join(figures),
                 f"  in {out / 'artifact/figures'}"]
    return "\n".join(text)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run", type=Path, default=PAPER / "run/results")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--show", nargs="*", choices=EXPERIMENTS, help="experiments to print (default: all)")
    args = parser.parse_args()
    run, out = args.run.resolve(), args.out.resolve()
    for side in ("artifact", "paper-logs"):
        shutil.rmtree(out / side, ignore_errors=True)
    for name in EXPERIMENTS:
        if (run / name / "results.csv").exists():
            (out / "artifact/logs" / name).mkdir(parents=True, exist_ok=True)
            shutil.copy(run / name / "results.csv", out / "artifact/logs" / name / "results.csv")
    current_from_tierd(out / "artifact/logs")
    pairs = paper_view(out / "artifact/logs", out / "paper-logs/logs")
    if not pairs:
        sys.exit(f"no results.csv under {run}; run bench.py first")
    for side in ("artifact", "paper-logs"):
        build(out / side / "logs", out / side)
    rerun = report.tables(out / "artifact/data")
    paper = {t.name: t for t in report.tables(out / "paper-logs/data")}
    tables = {t.name: pair(t, paper.get(t.name)) for t in rerun}
    limits = json.loads((out / "artifact/data/limits.json").read_text())
    present = [n for n in EXPERIMENTS if any(p["experiment"] == n for p in pairs)]
    same = sum(p["same_outcome"] for p in pairs)
    header = (f"# Rerun next to the paper's logs ({limits.get('profile', 'full')} profile)\n\n"
              f"Same outcome (answered or not) as the paper's logs on {same} of {len(pairs)} runs.\n\n"
              f"{READING}\n")
    documents = []
    for name in present:
        mine = [p for p in pairs if p["experiment"] == name]
        write(out / f"{name}-per-instance.csv", mine)
        document = section(name, mine, tables, out, True)
        (out / f"{name}.md").write_text(f"{header}\n{document}")
        documents.append(document)
    (out / "per-instance.csv").unlink(missing_ok=True)  # replaced by <experiment>-per-instance.csv
    (out / "comparison.md").write_text(header + "\n" + "\n".join(documents))
    print(header)
    for name in present:
        if args.show is None or name in args.show:
            print(section(name, [p for p in pairs if p["experiment"] == name], tables, out, False))
    print(f"\nwrote {out}/: <experiment>.md (tables), <experiment>-per-instance.csv, "
          f"comparison.md (all experiments), artifact/figures/, paper-logs/figures/")


if __name__ == "__main__":
    main()

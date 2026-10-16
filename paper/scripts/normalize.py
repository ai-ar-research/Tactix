#!/usr/bin/env python3
"""Turn the archived campaigns in paper/logs/*/raw/ into one table per experiment.

    python3 paper/scripts/normalize.py [--out DIR]

Writes paper/logs/<experiment>/results.csv with one row per (configuration,
instance). The campaigns were recorded by several harness versions with
different record layouts; this script applies each experiment's merge rule
and writes a common set of columns:

  configuration, instance, verdict, solve_seconds, status, verified,
  check_seconds, proof_bytes, par2_seconds

plus experiment-specific columns. A recheck record (a re-solve with more
memory or a longer checking window) replaces the original only where the
rule below says so.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import tarfile
from pathlib import Path

LOGS = Path(__file__).resolve().parents[1] / "logs"
OUT = LOGS
PENALTY = 10000


class Campaign:
    """Read-only view of one archived campaign."""

    def __init__(self, experiment: str, name: str):
        self.name = name
        self.path = LOGS / experiment / "raw" / f"{name}.tar.xz"
        self.files: dict[str, bytes] = {}
        with tarfile.open(self.path, "r:xz") as archive:
            for member in archive:
                if member.isfile():
                    relative = member.name.split("/", 1)[1]
                    self.files[relative] = archive.extractfile(member).read()

    def json(self, relative: str):
        return json.loads(self.files[relative])

    def text(self, relative: str) -> str:
        return self.files[relative].decode()

    def records(self) -> list[tuple[str, dict]]:
        """(file name, record) for every per-task record in results/."""
        return sorted((relative.split("/")[-1], json.loads(data))
                      for relative, data in self.files.items()
                      if relative.startswith("results/") and relative.endswith(".json"))

    def by_instance(self) -> dict[str, dict]:
        return {record["instance"]: record for _, record in self.records()}


def write(experiment: str, rows: list[dict]) -> None:
    fields = list(dict.fromkeys(key for row in rows for key in row))
    (OUT / experiment).mkdir(parents=True, exist_ok=True)
    with (OUT / experiment / "results.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"{experiment:11} {len(rows):6d} rows")


# --- Tier D: SAT Competition 2024-2026 main tracks --------------------------
#
# Tactix: the 20261010-tierd-fastel campaign, with the 64 GB recheck record
# substituted where it verifies. Satsuma+Kissat: its 8 h checking campaign.
# The four other reference solvers: the frozen 32 GB campaign. For the
# reference solvers too, a 64 GB / 8 h recheck record replaces the original
# where it verifies. A SAT answer counts when its model checks (and, for the
# reference solvers, the output follows the competition protocol); an UNSAT
# answer counts when its proof verifies. Unverified rows count 10,000 s.

TIERD_SOLVERS = {
    "tactix-0f7c10174": "Tactix",
    "satsuma-kissat": "Satsuma+Kissat",
    "kissat-sc2024": "Kissat-sc2024",
    "kissat-4.0.4": "Kissat 4.0.4",
    "ae-kissat-mab": "AE-Kissat-MAB",
    "lymphosat": "LymphoSAT",
}


def _answered(record):
    return record.get("verdict") in ("SAT", "UNSAT") and record.get("expected_match", True)


def _verified(record):
    if not _answered(record):
        return False
    if "model_ok" in record and record["verdict"] == "SAT":
        return record["model_ok"] is True
    return bool(record.get("valid"))


def _tierd_row(record):
    solve = record.get("solve") or {}
    model = record.get("model_check") or {}
    check = record.get("proof_check") or {}
    ok = _verified(record)
    if record.get("verdict") in ("SAT", "UNSAT") and not record.get("expected_match", True):
        status = "wrong-answer"
    elif ok:
        status = "verified"
    elif record.get("verdict") == "SAT":
        status = "model-ok-nonprotocol" if model.get("exit_code") == 0 else "model-rejected"
    elif record.get("verdict") == "UNSAT":
        status = "unverified"
    elif solve.get("timed_out"):
        status = "timeout"
    elif solve.get("exit_code") == 137:
        status = "solver-memout"
    else:
        status = f"no-answer (exit {solve.get('exit_code')})"
    wall = solve.get("wall_seconds")
    return dict(verdict=record.get("verdict"), solve_seconds=wall, status=status, verified=ok,
                check_seconds=check.get("wall_seconds"),
                proof_bytes=record.get("certificate_bytes"),
                par2_seconds=round(wall, 3) if ok else PENALTY,
                proof_pipeline=check.get("pipeline"))


def tierd():
    tactix = Campaign("tierd", "20261010-tierd-fastel").by_instance()
    rechecks = {"tactix-0f7c10174": {
        k: ("20261011-tierd-fastel-recheck-64g", v)
        for k, v in Campaign("tierd", "20261011-tierd-fastel-recheck-64g").by_instance().items()}}

    def baseline(name):
        campaign = Campaign("tierd", name)
        out: dict[str, dict] = {}
        for entry in campaign.json("results.json")["results"]:
            out.setdefault(entry["solver"], {})[entry["instance"]] = entry
        return campaign, out

    frozen_campaign, frozen = baseline("sc24-26-32g-5000s")
    _, satsuma = baseline("sc24-26-satsuma-fixed64-32g-8h")
    for name in ("sc24-26-recheck-64g-8h", "sc24-26-satsuma-recheck-64g-8h"):
        for solver, entries in baseline(name)[1].items():
            for instance, entry in entries.items():
                rechecks.setdefault(solver, {})[instance] = (name, entry)
    arms = {"tactix-0f7c10174": (tactix, "20261010-tierd-fastel"),
            "satsuma-kissat": (satsuma["satsuma-kissat"], "sc24-26-satsuma-fixed64-32g-8h")}
    for solver in ("kissat-sc2024", "kissat-4.0.4", "ae-kissat-mab", "lymphosat"):
        arms[solver] = (frozen[solver], "sc24-26-32g-5000s")

    years = {}
    for line in csv.DictReader(io.StringIO(frozen_campaign.text("year-membership.tsv")), delimiter="\t"):
        years[line["hash"]] = "+".join(c for c in ("SC2024", "SC2025", "SC2026") if line[c] == "1")
    rows = []
    for instance in sorted(tactix):
        content_hash = instance.split("/")[-1].split("-", 1)[0]
        for solver, label in TIERD_SOLVERS.items():
            records, campaign = arms[solver]
            record = records[instance]
            recheck = rechecks.get(solver, {}).get(instance)
            if recheck and _verified(recheck[1]):
                campaign, record = recheck
            rows.append(dict(configuration=label, instance=instance,
                             competition=years[content_hash], **_tierd_row(record),
                             campaign=campaign))
    write("tierd", rows)


# --- Milestone ladder ---------------------------------------------------------
#
# The first commit that met each development milestone, run on Tier D without
# proofs (SAT models are checked, UNSAT answers are not), beside the current
# version's proof-checked Tier D run.

MILESTONES = [
    ("Milestone A", "dcc3ecbd6", "20261011-tierd-noproof-1-tierA-dcc3ecbd6"),
    ("Milestone B", "546298924", "20261011-tierd-noproof-2-tierB14-546298924"),
    ("Milestone C", "6f44f23d6", "20261011-tierd-noproof-3-tierC-6f44f23d6"),
    ("Milestone D", "f50e46c0c", "20261011-tierd-noproof-4-rank1-50-f50e46c0c"),
    ("Milestone E", "0a9063387", "20261011-tierd-noproof-5-wv100-0a9063387"),
]


def milestones():
    arms = [(label, commit, Campaign("milestones", name).by_instance(), True)
            for label, commit, name in MILESTONES]
    current = Campaign("tierd", "20261010-tierd-fastel").by_instance()
    for instance, record in Campaign("tierd", "20261011-tierd-fastel-recheck-64g").by_instance().items():
        if record.get("valid"):
            current[instance] = record
    arms.append(("Current", "0f7c10174", current, False))
    rows = []
    for instance in sorted(current):
        for label, commit, records, proof_free in arms:
            record = records[instance]
            solve = record.get("solve") or {}
            answered = _answered(record)
            counted = answered and ((record["verdict"] == "UNSAT" and proof_free)
                                    or bool(record.get("valid")))
            rows.append(dict(configuration=label, commit=commit, instance=instance,
                             verdict=record.get("verdict"),
                             solve_seconds=solve.get("wall_seconds"),
                             proof_mode="none" if proof_free else "proof",
                             counted=counted,
                             proof_check_result=(record.get("proof_check") or {}).get("result"),
                             par2_seconds=round(solve["wall_seconds"], 3) if counted else PENALTY))
    write("milestones", rows)


# --- Overhead of the C interface ---------------------------------------------
#
# The default strategy with every operation through tactix.h (tactix-public)
# against the production C++ build, in the same time window. Tier A ran both
# arms in one campaign; each regression set ran one campaign per arm.

INTERFACE_SETS = [("B28+C5", "b28c5"), ("Rank 1-50", "rank1-50"), ("WV100", "wv100")]


def interface():
    rows = []
    tier_a = Campaign("interface", "20261015-120822-A-tactix-public")
    manifest = tier_a.json("manifest.json")
    names = [solver["name"] for solver in manifest["solvers"]]
    labels = {"tactix-public": "C interface", "tactix-prod": "C++"}
    for index, task in enumerate(manifest["tasks"]):
        record = tier_a.json(f"results/{index:05d}.json")
        answered = record["verdict"] in ("SAT", "UNSAT")
        rows.append(dict(set="Tier A", configuration=labels[names[task["solver"]]],
                         instance=task["rel"], verdict=record["verdict"],
                         solve_seconds=record["time"], timed_out=not answered,
                         proof_bytes=record.get("proof_bytes"),
                         certificate_sha256=None, instructions=None))
    for name, key in INTERFACE_SETS:
        for label, campaign in (("C interface", f"20261015-public-port-v3-regression-{key}"),
                                ("C++", f"20261015-prod-samewindow-regression-{key}")):
            for instance, record in sorted(Campaign("interface", campaign).by_instance().items()):
                solve = record["solve"]
                instructions = None
                for line in (record.get("runtime_context") or {}).get("perf_statistics", []):
                    fields = line.split("\t")
                    if len(fields) > 2 and fields[2] == "instructions":
                        instructions = int(fields[0])
                rows.append(dict(set=name, configuration=label, instance=instance,
                                 verdict=record["verdict"], solve_seconds=solve["wall_seconds"],
                                 timed_out=bool(solve.get("timed_out")),
                                 proof_bytes=record.get("certificate_bytes"),
                                 certificate_sha256=record.get("certificate_sha256"),
                                 instructions=instructions))
    write("interface", rows)


# --- Case study: cruxmiter ----------------------------------------------------

CRUX_LABELS = {"rebased-default": "Default Tactix", "rebased-policy": "Public policy",
               "kissat": "Kissat 4.0.4"}


def cruxmiter():
    results = Campaign("cruxmiter", "20261010-213743-crux-fixed-family-verify").json("results.json")
    rows = []
    for arm, records in results["results"].items():
        for record in records:
            rows.append(dict(configuration=CRUX_LABELS[arm], instance=record["instance"],
                             verdict=record["verdict"], solve_seconds=record["time"],
                             verified=bool(record["proof_ok"]),
                             check_seconds=record["proof_check_time"],
                             proof_bytes=record["proof_bytes"]))
    write("cruxmiter", rows)


# --- Case study: Chvatal's conjecture -------------------------------------------
#
# The 23 formulas of case n = 7 that Kissat 4.0.4 solves in 300-1,800 s
# (selection in paper-hard.json). Where a certificate check failed for a
# resource reason (checker error, memory), the recheck campaign's check
# outcome replaces the original one; the original solve time is kept.

CHVATAL_LABELS = {"tactix-default": "Default Tactix", "chvatal": "Chvatal Tactix",
                  "chvatal-stable": "Chvatal Tactix (stable)",
                  "kissat-4.0.4": "Kissat 4.0.4", "satsuma-kissat": "Satsuma+Kissat"}
CHVATAL_RUNS = ["20261015-144648-chvatal-band-baselines", "20261015-144649-chvatal-band-satsuma",
                "20261015-145107-chvatal-band-candidates"]
CHVATAL_RECHECKS = ["20261015-150949-chvatal-band-recheck48-1",
                    "20261015-160036-chvatal-hard-recheck48-2",
                    "20261015-171709-chvatal-hard-recheck48-3",
                    "20261015-173818-chvatal-hard-satsuma-recheck64"]


def _chvatal_records(name):
    campaign = Campaign("chvatal", name)
    manifest = campaign.json("manifest.json")
    for file_name, record in campaign.records():
        if "tasks" in manifest:
            task = manifest["tasks"][int(Path(file_name).stem)]
            yield manifest["solvers"][task["solver"]]["name"], task["rel"], record
        else:
            yield record["solver"], record["instance"], record


def chvatal():
    selection = json.loads((LOGS / "chvatal" / "paper-hard.json").read_text())
    screen = {entry["file"]: entry["kissat_seconds"] for entry in selection["selected"]}
    recheck = {(arm, rel): record for name in CHVATAL_RECHECKS
               for arm, rel, record in _chvatal_records(name)}
    rows = []
    for name in CHVATAL_RUNS:
        for arm, rel, record in _chvatal_records(name):
            if rel not in screen or arm not in CHVATAL_LABELS:
                continue
            rechecked = False
            resource_failure = (record.get("proof_check_kind") in ("checker-error", "resource")
                                or "exit -9" in (record.get("proof_msg") or ""))
            if resource_failure and (arm, rel) in recheck and recheck[(arm, rel)]["verdict"] == record["verdict"]:
                again = recheck[(arm, rel)]
                record = dict(record, proof_ok=again["proof_ok"],
                              proof_check_time=again["proof_check_time"],
                              proof_bytes=again.get("proof_bytes") or record.get("proof_bytes"))
                rechecked = True
            answered = record["verdict"] in ("SAT", "UNSAT") and not record.get("mismatch")
            rows.append(dict(configuration=CHVATAL_LABELS[arm], instance=rel,
                             verdict=record["verdict"], solve_seconds=record["time"],
                             verified=bool(record.get("proof_ok")),
                             solved_checked=answered and bool(record.get("proof_ok")),
                             check_seconds=record.get("proof_check_time"),
                             proof_bytes=record.get("proof_bytes"),
                             kissat_screen_seconds=screen[rel], rechecked=rechecked))
    write("chvatal", rows)


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=LOGS,
                        help="directory for <experiment>/results.csv (default: paper/logs)")
    OUT = parser.parse_args().out
    tierd()
    milestones()
    interface()
    cruxmiter()
    chvatal()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Proof/model-checked random-3-SAT n=400 policy campaign.

The split is frozen in corpus/random3sat-n400/manifest.json. `train-screen`
never schedules validation instances. `validate` opens the odd-index holdout
only after a winner is named explicitly on the command line.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
from types import SimpleNamespace

import run


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORPUS = os.path.join(ROOT, "corpus", "random3sat-n400")
CORPUS_MANIFEST = os.path.join(CORPUS, "manifest.json")
ORACLE = os.path.join(CORPUS, "oracle.json")
PIN_SOURCE_COMMIT = "af8110c60"
PIN = os.path.join(ROOT, "build", "random3sat-af8110c")
POLICY_PIN = os.path.join(ROOT, "build", "random3sat-policy-af8110c")
STOCK = os.path.join(PIN, "tactix")
POLICY = os.path.join(POLICY_PIN, "tactix-random3sat-policy")
KISSAT = os.path.join(PIN, "kissat")
ENV = "/usr/bin/env"


PROFILES = (
    "focused-saved-32",
    "focused-saved-128",
    "focused-negative-32",
    "focused-negative-128",
    "focused-positive-32",
    "stable-saved",
    "stable-negative",
    "stable-positive",
    "hybrid-negative",
    "walk0-100k",
    "walk0-1m",
    "walk1-1m",
    "walk2-1m",
    "walk0-10m",
)

SCREEN_PROFILES = (
    "walk0-1m",
    "walk1-1m",
    "walk2-1m",
    "walk0-10m",
)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_entries(split):
    with open(CORPUS_MANIFEST) as source:
        manifest = json.load(source)
    expected = {}
    if os.path.isfile(ORACLE):
        with open(ORACLE) as source:
            expected = json.load(source).get("verdicts", {})
    records = manifest["instances"]
    if split != "all":
        records = [record for record in records if record["split"] == split]
    entries = []
    paths = {}
    for record in records:
        rel = record["path"]
        entries.append((rel, expected.get(rel, "UNKNOWN"), 0.0))
        paths[rel] = os.path.join(ROOT, rel)
    return entries, paths, manifest, expected


def policy_command(profile):
    return [POLICY, f"--profile={profile}", "--drat"]


def solvers_for(stage, winner):
    stock = ("stock-tactix", [STOCK, "--drat"], True, True, True)
    kissat = ("kissat", [KISSAT], True, True, True)
    if stage == "baseline":
        return [stock, kissat]
    if stage == "train-screen":
        return [stock, kissat] + [
            (profile, policy_command(profile), True, True, True)
            for profile in SCREEN_PROFILES
        ]
    selected = (winner, policy_command(winner), True, True, True)
    return [stock, kissat, selected]


def record_oracle(run_dir):
    path = os.path.join(run_dir, "results.json")
    with open(path) as source:
        results = json.load(source)
    rows = results["results"]
    if "stock-tactix" not in rows or "kissat" not in rows:
        raise SystemExit("baseline result needs stock-tactix and kissat arms")
    verdicts = {}
    evidence = {}
    for arm in ("stock-tactix", "kissat"):
        for row in rows[arm]:
            rel = row["instance"]
            if not row["solved"]:
                continue
            if row["verdict"] == "SAT" and row.get("model_ok") is not True:
                raise SystemExit(f"unchecked SAT model: {arm} {rel}")
            if row["verdict"] == "UNSAT" and row.get("proof_ok") is not True:
                raise SystemExit(f"unchecked UNSAT proof: {arm} {rel}")
            prior = verdicts.setdefault(rel, row["verdict"])
            if prior != row["verdict"]:
                raise SystemExit(f"baseline disagreement on {rel}")
            evidence.setdefault(rel, {})[arm] = {
                "verdict": row["verdict"],
                "time": row["time"],
                "model_ok": row.get("model_ok"),
                "proof_ok": row.get("proof_ok"),
            }
    common = set.intersection(*[
        {row["instance"] for row in rows[arm] if row["solved"]}
        for arm in ("stock-tactix", "kissat")
    ])
    if set(verdicts) != common:
        # A one-arm solve is useful evidence, but a frozen family label is
        # admitted only when the two independent solvers agree.
        verdicts = {rel: verdict for rel, verdict in verdicts.items()
                    if rel in common}
        evidence = {rel: record for rel, record in evidence.items()
                    if rel in common}
    with open(ORACLE, "w") as target:
        json.dump({
            "source_run": os.path.relpath(run_dir, ROOT),
            "verdicts": verdicts,
            "evidence": evidence,
        }, target, indent=2, sort_keys=True)
        target.write("\n")
    print(f"wrote {ORACLE} with {len(verdicts)} independently agreed, checked "
          "labels; timeouts remain UNKNOWN")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("baseline", "train-screen",
                                          "validate", "final"))
    parser.add_argument("--winner", choices=PROFILES)
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--record-oracle", metavar="RUN_DIR")
    args = parser.parse_args()

    if args.record_oracle:
        record_oracle(os.path.abspath(args.record_oracle))
        return 0
    if args.stage in ("validate", "final") and not args.winner:
        parser.error(f"{args.stage} requires --winner selected on training")
    if args.stage != "baseline" and not os.path.isfile(ORACLE):
        raise SystemExit("run baseline and --record-oracle before policy tuning")
    required = [STOCK, KISSAT]
    if args.stage != "baseline":
        required.append(POLICY)
    for binary in required:
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            raise SystemExit(f"missing executable pin: {binary}")

    split = "all" if args.stage in ("baseline", "final") else (
        "train" if args.stage == "train-screen" else "validation")
    entries, paths, corpus, expected = load_entries(split)
    solvers = solvers_for(args.stage, args.winner)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"random3sat400-{args.stage}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    metadata = {
        "stage": args.stage,
        "split": split,
        "split_rule": corpus["split_rule"],
        "winner": args.winner,
        "timeout_seconds": args.timeout,
        "partition": args.partition,
        "allocation": {"cpus_per_task": 4, "memory_gib": 16},
        "pin_source_commit": PIN_SOURCE_COMMIT,
        "campaign_wrapper_commit": subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
            check=True, capture_output=True, text=True).stdout.strip(),
        "campaign_wrapper_sha256": sha256(__file__),
        "corpus_manifest_sha256": sha256(CORPUS_MANIFEST),
        "binary_sha256": {os.path.basename(path): sha256(path)
                           for path in required},
        "labels_available": len(expected),
        "arms": {name: command for name, command, _, _, _ in solvers},
        "correctness": "SAT models and UNSAT proofs checked for every arm",
        "proof_lifecycle": (
            "task-local mkdtemp; removed after solve/check and by SIGTERM or "
            "SIGINT handlers on timeout/cancellation"
        ),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(
        partition=args.partition, throttle=None, dry_run=args.dry_run,
        wait=args.wait, tier=label)
    print(f"corpus: {len(entries)} instances ({split})")
    print(f"pin source commit: {PIN_SOURCE_COMMIT}")
    return run.slurm_submit(
        submit, entries, solvers, paths, args.timeout, 16,
        outdir, stamp, PIN_SOURCE_COMMIT, label)


if __name__ == "__main__":
    raise SystemExit(main())

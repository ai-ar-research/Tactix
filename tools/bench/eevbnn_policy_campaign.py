#!/usr/bin/env python3
"""Checked SLURM campaigns for the EEVBNN public-interface policy."""

import argparse
import datetime
import hashlib
import json
import os
import subprocess
from types import SimpleNamespace

import run

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORPUS = os.path.join(ROOT, "corpus", "eevbnn")
DEFAULT = os.path.join(CORPUS, "bin", "tactix-default")
POLICY = os.path.join(CORPUS, "bin", "tactix-eevbnn-policy")
POLICY_LUCKY = os.path.join(CORPUS, "bin", "tactix-eevbnn-policy-lucky")
POLICY_FINAL = os.path.join(CORPUS, "bin", "tactix-eevbnn-policy-final")
KISSAT = os.path.join(CORPUS, "bin", "kissat-4.0.4")
SCREEN_RANKS = (4, 6, 9, 19, 29, 34)
LUCKY_RANKS = (29, 34, 44)
PROFILES = (
    "stable-negative", "stable-positive", "stable-saved",
    "gate-default", "bnn-default", "gate-stable-negative",
    "bnn-stable-negative",
)
LUCKY_PROFILES = (
    "lucky-forward-false", "lucky-forward-true",
    "lucky-backward-false", "lucky-backward-true", "lucky-bnn-cascade",
)
INPUT_PROFILES = ("input-stable-negative", "input-stable-positive", "input-auto")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_rows(stage):
    with open(os.path.join(CORPUS, "manifest.json"), encoding="utf-8") as source:
        rows = json.load(source)["instances"]
    with open(os.path.join(CORPUS, "split.json"), encoding="utf-8") as source:
        split = json.load(source)
    if stage == "screen":
        rows = [row for row in rows if row["rank"] in SCREEN_RANKS]
    elif stage == "proof":
        rows = [row for row in rows if row["rank"] == 48]
    elif stage in ("lucky", "input", "lock"):
        rows = [row for row in rows if row["rank"] in LUCKY_RANKS]
    elif stage == "train":
        allowed = set(split["train"])
        rows = [row for row in rows if row["candidate_id"] in allowed]
    elif stage == "validate":
        allowed = set(split["validation"])
        rows = [row for row in rows if row["candidate_id"] in allowed]
    elif stage in ("validate-d", "validate-final"):
        allowed = set(split["validation"])
        rows = [row for row in rows
                if row["candidate_id"] in allowed and row["family"] == "d"]
    return rows


def solver_arms(stage, winner):
    default = ("default", [DEFAULT, "--dsr"], True, True, True)
    if stage == "baseline":
        return [default]
    if stage == "lucky":
        return [default] + [
            (profile, [POLICY_LUCKY, f"--profile={profile}", "--drat"],
             True, True, True) for profile in LUCKY_PROFILES
        ]
    if stage == "input":
        return [default] + [
            (profile, [POLICY_LUCKY, f"--profile={profile}", "--drat"],
             True, True, True) for profile in INPUT_PROFILES
        ]
    if stage in ("lock", "validate-d", "proof"):
        return [default, ("input-auto",
                          [POLICY_FINAL, "--profile=input-auto", "--drat"],
                          True, True, True)]
    if stage == "validate-final":
        return [default,
                ("input-auto", [POLICY_FINAL, "--profile=input-auto", "--drat"],
                 True, True, True),
                ("kissat", [KISSAT], True, True, False)]
    profiles = PROFILES if stage == "screen" else (winner,)
    policy_binary = (POLICY_FINAL
                     if winner in LUCKY_PROFILES + INPUT_PROFILES else POLICY)
    return [default] + [
        (profile, [policy_binary, f"--profile={profile}", "--drat"],
         True, True, True)
        for profile in profiles
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "proof", "lucky", "input", "lock",
                                          "train", "validate", "validate-d",
                                          "validate-final", "baseline"))
    parser.add_argument("--winner", choices=PROFILES + LUCKY_PROFILES + INPUT_PROFILES,
                        default="stable-negative")
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if args.partition != "cpu-dense-hwulab-q":
        raise SystemExit("EEVBNN campaigns are restricted to cpu-dense-hwulab-q")
    for executable in (DEFAULT, POLICY,
                       POLICY_LUCKY if args.stage in ("lucky", "input")
                       else POLICY_FINAL if args.stage in ("proof", "lock", "validate-d", "validate-final")
                       else POLICY,
                       KISSAT if args.stage == "validate-final" else DEFAULT):
        if not os.path.isfile(executable) or not os.access(executable, os.X_OK):
            raise SystemExit(f"missing executable pin: {executable}")
    rows = load_rows(args.stage)
    entries = []
    paths = {}
    for row in rows:
        basename = os.path.basename(row["file"])[:-3]
        rel = os.path.join("eevbnn", basename)
        entries.append((rel, row["kissat_status"], row["kissat_wall_seconds"]))
        paths[rel] = os.path.join(CORPUS, "plain", basename)
        if not os.path.isfile(paths[rel]) or os.path.getsize(paths[rel]) != row["raw_bytes"]:
            raise SystemExit(
                f"missing plain checker input: {paths[rel]}\n"
                f"run: python3 tools/bench/eevbnn_stage.py "
                f"{'all' if args.stage == 'baseline' else args.stage}")
    arms = solver_arms(args.stage, args.winner)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"eevbnn-{args.stage}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    revision = subprocess.run(
        ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], check=True,
        capture_output=True, text=True).stdout.strip()
    metadata = {
        "stage": args.stage,
        "winner": args.winner if args.stage in ("train", "validate") else None,
        "split": "corpus/eevbnn/split.json",
        "screen_ranks": list(SCREEN_RANKS) if args.stage == "screen" else None,
        "timeout_seconds": args.timeout,
        "partition": args.partition,
        "cpus_per_task": 4,
        "memory_gib": 16,
        "revision": revision,
        "campaign_sha256": sha256(__file__),
        "executables": {
            "default": {"path": DEFAULT, "sha256": sha256(DEFAULT)},
            "policy": {"path": POLICY, "sha256": sha256(POLICY)},
            "policy_lucky": {"path": POLICY_LUCKY,
                             "sha256": sha256(POLICY_LUCKY)},
            "policy_final": {"path": POLICY_FINAL,
                             "sha256": sha256(POLICY_FINAL)},
            "kissat": {"path": KISSAT, "sha256": sha256(KISSAT)},
        },
        "proof_cleanup": "run.py task-local scratch with signal and timeout cleanup",
        "instances": [{"candidate_id": row["candidate_id"],
                       "rank": row["rank"], "expected": row["kissat_status"],
                       "kissat_seconds": row["kissat_wall_seconds"]}
                      for row in rows],
    }
    with open(os.path.join(outdir, "campaign.json"), "w", encoding="utf-8") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(partition=args.partition, throttle=None,
                             dry_run=args.dry_run, wait=args.wait, tier=label)
    return run.slurm_submit(submit, entries, arms, paths, args.timeout, 16,
                            outdir, stamp, revision, label)


if __name__ == "__main__":
    raise SystemExit(main())

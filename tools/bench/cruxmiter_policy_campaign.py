#!/usr/bin/env python3
"""Checked SLURM campaign for the public-API cruxmiter policy.

This is deliberately separate from the frozen benchmark tiers. Development
uses the unique size-20 inputs; the target stage contains only
cruxmiter32seed4 and should be submitted after a profile is locked.
"""

import argparse
import datetime
import hashlib
import json
import os
import subprocess
from types import SimpleNamespace

import run

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CRUX = os.path.join(ROOT, "cruxmiter")
PIN = os.path.join(ROOT, "build", "pin-cruxmiter-8887df22c")
PIN_SOURCE_COMMIT = "8887df22c"
DEFAULT = os.path.join(PIN, "tactix")
POLICY = os.path.join(PIN, "examples", "tactix-cruxmiter-policy")


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def instances(stage, development):
    if stage == "screen":
        # seed1 is byte-identical to seed0 at every size.
        names = [f"cruxmiter20seed{seed}.cnf"
                 for seed in (0, 2, 3, 4, 5, 6, 7, 8, 9)]
    elif stage == "tune":
        names = [development]
    else:
        names = ["cruxmiter32seed4.cnf"]
    entries = [(f"cruxmiter/{name}", "UNKNOWN", 0.0) for name in names]
    paths = {rel: os.path.join(ROOT, rel) for rel, _, _ in entries}
    missing = [path for path in paths.values() if not os.path.isfile(path)]
    if missing:
        raise SystemExit("missing cruxmiter input(s): " + ", ".join(missing))
    return entries, paths


def arms(stage, winner):
    if stage == "screen":
        return [("default", [DEFAULT], True, True, True)]
    selected = ("search", "positive", "saved", "gate", "binary", "full") \
        if stage == "tune" \
        else (winner,)
    result = [("default", [DEFAULT], True, True, True)]
    for profile in selected:
        result.append((
            profile,
            [POLICY, f"--profile={profile}", "--drat"],
            True, True, True))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "tune", "target"))
    parser.add_argument("--development", default="cruxmiter20seed4.cnf")
    parser.add_argument("--winner", choices=("search", "positive", "saved",
                                             "gate", "binary", "full"),
                        default="gate")
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--timeout", type=float, default=1200.0)
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    for binary in (DEFAULT, POLICY):
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            raise SystemExit(f"missing executable pin: {binary}")
    entries, paths = instances(args.stage, args.development)
    solvers = arms(args.stage, args.winner)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"crux-{args.stage}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    wrapper_commit = subprocess.run(
        ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
        check=True, capture_output=True, text=True).stdout.strip()
    metadata = {
        "stage": args.stage,
        "development": args.development if args.stage == "tune" else None,
        "winner": args.winner if args.stage == "target" else None,
        "partition": args.partition,
        "timeout_seconds": args.timeout,
        "pin_source_commit": PIN_SOURCE_COMMIT,
        "campaign_wrapper_commit": wrapper_commit,
        "campaign_wrapper_sha256": file_sha256(__file__),
        "executables": {
            "default": {"path": DEFAULT, "sha256": file_sha256(DEFAULT)},
            "policy": {"path": POLICY, "sha256": file_sha256(POLICY)},
        },
        "inputs": {
            rel: {"path": paths[rel], "sha256": file_sha256(paths[rel])}
            for rel, _, _ in entries
        },
        "proof_lifecycle": (
            "task-local scratch; removed by normal, exception, SIGTERM, and "
            "SIGINT paths"
        ),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(
        partition=args.partition, throttle=None, dry_run=args.dry_run,
        wait=args.wait, tier=label)
    print(f"pin source commit: {PIN_SOURCE_COMMIT}")
    print(f"campaign wrapper commit: {wrapper_commit}")
    print(f"default sha256: {metadata['executables']['default']['sha256']}")
    print(f"policy sha256:  {metadata['executables']['policy']['sha256']}")
    return run.slurm_submit(
        submit, entries, solvers, paths, args.timeout, 32, outdir,
        stamp, PIN_SOURCE_COMMIT, label)


if __name__ == "__main__":
    raise SystemExit(main())

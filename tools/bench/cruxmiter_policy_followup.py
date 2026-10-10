#!/usr/bin/env python3
"""Scaling and stable-mode follow-up for the cruxmiter public policy.

The original campaign kept size 32 untouched while selecting a structural
prelude on size 20.  This follow-up first measures that locked prelude and
DefaultPolicy, with and without DefaultPolicy's existing forced-stable mode,
on the size-21..30 seed-4 ladder.  A separate family stage can confirm a
choice across all unique seeds of one non-target size.  Only the final stage
touches the nine unique size-32 instances.
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
PIN = os.path.join(ROOT, "build", "pin-cruxmiter-255f5ade0")
PIN_SOURCE_COMMIT = "255f5ade0"
DEFAULT = os.path.join(PIN, "tactix")
POLICY = os.path.join(PIN, "examples", "tactix-cruxmiter-policy")
ENV = "/usr/bin/env"
UNIQUE_SEEDS = (0, 2, 3, 4, 5, 6, 7, 8, 9)


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def instances(stage, size):
    if stage == "ladder":
        names = [f"cruxmiter{width}seed4.cnf" for width in range(21, 31)]
    elif stage == "family":
        if not 20 <= size <= 31:
            raise SystemExit("family --size must be between 20 and 31")
        names = [f"cruxmiter{size}seed{seed}.cnf" for seed in UNIQUE_SEEDS]
    else:
        names = [f"cruxmiter32seed{seed}.cnf" for seed in UNIQUE_SEEDS]
    entries = [(f"cruxmiter/{name}", "UNKNOWN", 0.0) for name in names]
    paths = {rel: os.path.join(ROOT, rel) for rel, _, _ in entries}
    missing = [path for path in paths.values() if not os.path.isfile(path)]
    if missing:
        raise SystemExit("missing cruxmiter input(s): " + ", ".join(missing))
    return entries, paths


def arm(name):
    table = {
        "default": [DEFAULT, "--dsr"],
        "default-stable": [ENV, "TACTIX_FORCE_MODE=stable", DEFAULT, "--dsr"],
        "backbone": [POLICY, "--profile=backbone", "--drat"],
        "backbone-stable": [ENV, "TACTIX_FORCE_MODE=stable", POLICY,
                            "--profile=backbone", "--drat"],
    }
    return (name, table[name], True, True, True)


def arms(stage, winner):
    if stage != "target32":
        selected = ("default", "default-stable", "backbone", "backbone-stable")
    else:
        selected = ("default", winner)
    return [arm(name) for name in selected]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("ladder", "family", "target32"))
    parser.add_argument("--size", type=int, default=28)
    parser.add_argument("--winner", choices=("default-stable", "backbone",
                                             "backbone-stable"),
                        default="backbone")
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    for binary in (DEFAULT, POLICY):
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            raise SystemExit(f"missing executable pin: {binary}")
    entries, paths = instances(args.stage, args.size)
    solvers = arms(args.stage, args.winner)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"crux-followup-{args.stage}"
    if args.stage == "family":
        label += f"-{args.size}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    wrapper_commit = subprocess.run(
        ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
        check=True, capture_output=True, text=True).stdout.strip()
    metadata = {
        "stage": args.stage,
        "size": args.size if args.stage == "family" else None,
        "winner": args.winner if args.stage == "target32" else None,
        "partition": args.partition,
        "timeout_seconds": args.timeout,
        "pin_source_commit": PIN_SOURCE_COMMIT,
        "campaign_wrapper_commit": wrapper_commit,
        "campaign_wrapper_sha256": file_sha256(__file__),
        "stable_mode": "TACTIX_FORCE_MODE=stable (existing DefaultPolicy control)",
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

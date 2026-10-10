#!/usr/bin/env python3
"""Proof-checked aggressive in-processing screen for cruxmiter seed 4."""

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
PIN_SOURCE_COMMIT = "9a6ad1f23"
PIN = os.path.join(ROOT, "build", f"pin-cruxmiter-{PIN_SOURCE_COMMIT}")
POLICY = os.path.join(PIN, "examples", "tactix-cruxmiter-policy")
ENV = "/usr/bin/env"
UNIQUE_SEEDS = (0, 2, 3, 4, 5, 6, 7, 8, 9)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command(profile="backbone", *settings):
    return [ENV, "TACTIX_FORCE_MODE=stable", *settings, POLICY,
            f"--profile={profile}", "--drat"]


ARMS = {
    "backbone-stable": command(),
    "backbone-cce": command("backbone", "TACTIX_SLOT_ENABLE=CCE"),
    "backbone-cce-max1": command(
        "backbone", "TACTIX_SLOT_ENABLE=CCE",
        "TACTIX_SLOT_MAX_APPLIES=CCE:1"),
    "symmetry-sandwich": command("symmetry-sandwich"),
    "symmetry-sandwich-cce": command(
        "symmetry-sandwich", "TACTIX_SLOT_ENABLE=CCE"),
    "full-stable": command("full"),
    "full-stable-cce": command("full", "TACTIX_SLOT_ENABLE=CCE"),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "family", "target"))
    parser.add_argument("--size", type=int, default=26)
    parser.add_argument("--winner", choices=tuple(ARMS),
                        default="backbone-cce")
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not os.path.isfile(POLICY) or not os.access(POLICY, os.X_OK):
        raise SystemExit(f"missing executable pin: {POLICY}")
    if args.stage == "family" and not 20 <= args.size <= 31:
        raise SystemExit("family --size must be between 20 and 31")
    width = args.size if args.stage == "family" else (
        26 if args.stage == "screen" else 32)
    timeout = args.timeout if args.timeout is not None else (
        1200.0 if args.stage == "target" else 180.0)
    seeds = UNIQUE_SEEDS if args.stage == "family" else (4,)
    entries = [
        (f"cruxmiter/cruxmiter{width}seed{seed}.cnf", "UNKNOWN", 0.0)
        for seed in seeds
    ]
    paths = {rel: os.path.join(ROOT, rel) for rel, _, _ in entries}
    missing = [path for path in paths.values() if not os.path.isfile(path)]
    if missing:
        raise SystemExit("missing cruxmiter input(s): " + ", ".join(missing))
    selected = tuple(ARMS) if args.stage == "screen" else (
        "backbone-stable", args.winner)
    solvers = [(name, ARMS[name], True, True, True) for name in selected]
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"crux-aggressive-{args.stage}-seed4"
    if args.stage == "family":
        label = f"crux-aggressive-family-{width}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    metadata = {
        "stage": args.stage,
        "targets": [rel for rel, _, _ in entries],
        "winner": args.winner if args.stage != "screen" else None,
        "timeout_seconds": timeout,
        "partition": args.partition,
        "pin_source_commit": PIN_SOURCE_COMMIT,
        "campaign_wrapper_commit": subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
            check=True, capture_output=True, text=True).stdout.strip(),
        "campaign_wrapper_sha256": sha256(__file__),
        "policy_sha256": sha256(POLICY),
        "input_sha256": {rel: sha256(path) for rel, path in paths.items()},
        "arms": {name: ARMS[name] for name in selected},
        "proof_lifecycle": (
            "task-local scratch; removed by normal, exception, SIGTERM, "
            "and SIGINT paths"),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(
        partition=args.partition, throttle=None, dry_run=args.dry_run,
        wait=args.wait, tier=label)
    print(f"pin source commit: {PIN_SOURCE_COMMIT}")
    print(f"policy sha256: {metadata['policy_sha256']}")
    return run.slurm_submit(
        submit, entries, solvers, paths, timeout, 32,
        outdir, stamp, PIN_SOURCE_COMMIT, label)


if __name__ == "__main__":
    raise SystemExit(main())

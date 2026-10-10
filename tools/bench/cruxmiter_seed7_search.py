#!/usr/bin/env python3
"""Seed-7-only policy search for the cruxmiter size-32 family.

The development target is deliberately fixed to cruxmiter32seed7.cnf.  Other
size-32 seeds remain untouched until a policy has been selected.  The screen
writes certificates (so solve timings include proof production) but defers the
expensive checker; the final stage verifies every selected arm and Kissat.
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
PIN_COMMIT = "cd10a2e1e"
PIN = os.path.join(ROOT, "build", f"pin-cruxmiter-{PIN_COMMIT}")
OLD_PIN = os.path.join(ROOT, "build", "pin-cruxmiter-255f5ade0")
DEFAULT = os.path.join(PIN, "tactix")
POLICY = os.path.join(PIN, "examples", "tactix-cruxmiter-policy")
OLD_POLICY = os.path.join(OLD_PIN, "examples", "tactix-cruxmiter-policy")
NEXT_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-d2f139f0b", "examples",
    "tactix-cruxmiter-policy")
KISSAT = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
ENV = "/usr/bin/env"
REL = "cruxmiter/cruxmiter32seed7.cnf"


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def cli(*settings):
    return [ENV, *settings, DEFAULT, "--dsr"]


def policy(profile="backbone", *settings, binary=POLICY):
    return [ENV, *settings, binary, f"--profile={profile}", "--drat"]


# All arms are formula-independent settings or public-rule sequences.  No arm
# branches on the benchmark name or reads private Kernel state.
ARMS = {
    "default": cli(),
    "default-stable": cli("TACTIX_FORCE_MODE=stable"),
    "default-focused": cli("TACTIX_FORCE_MODE=focused"),
    "known-132": policy("backbone", "TACTIX_FORCE_MODE=stable",
                        binary=OLD_POLICY),
    "backbone-stable": policy("backbone", "TACTIX_FORCE_MODE=stable"),
    "backbone-auto": policy("backbone"),
    "backbone-focused": policy("backbone", "TACTIX_FORCE_MODE=focused"),
    "gate-stable": policy("gate", "TACTIX_FORCE_MODE=stable"),
    "backbone-close-stable": policy("backbone-close",
                                    "TACTIX_FORCE_MODE=stable"),
    "backbone-heavy-stable": policy("backbone-heavy",
                                    "TACTIX_FORCE_MODE=stable"),
    "symmetry-backbone-stable": policy("symmetry-backbone",
                                       "TACTIX_FORCE_MODE=stable"),
    "backbone-symmetry-stable": policy("backbone-symmetry",
                                       "TACTIX_FORCE_MODE=stable"),
    "symmetry-sandwich-stable": policy("symmetry-sandwich",
                                       "TACTIX_FORCE_MODE=stable"),
    "full-stable": policy("full", "TACTIX_FORCE_MODE=stable"),
    # Direct public-policy interpretations of stable=2,target=0 and the three
    # phase choices.  These deliberately bypass DefaultPolicy search.
    "target0-negative": policy("search"),
    "target0-positive": policy("positive"),
    "target0-saved": policy("saved"),
    # Dedicated public clients combining the structural prelude with the exact
    # stable=2,target=0,phase=0 search interpretation.
    "gate-target0": policy("gate-search", binary=NEXT_POLICY),
    "backbone-target0": policy("backbone-search", binary=NEXT_POLICY),
    "backbone-close-target0": policy("backbone-close-search",
                                     binary=NEXT_POLICY),
    "target0-no-repeat": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=0",
        binary=NEXT_POLICY),
    "target0-repeat1k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=1000",
        binary=NEXT_POLICY),
    "target0-repeat5k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=5000",
        binary=NEXT_POLICY),
    "target0-repeat50k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=50000",
        binary=NEXT_POLICY),
    "target0-reluctant64": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=64",
        binary=NEXT_POLICY),
    "target0-reluctant256": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=256",
        binary=NEXT_POLICY),
    "target0-reluctant4096": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=4096",
        binary=NEXT_POLICY),
    "target0-reduce500": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=500",
        binary=NEXT_POLICY),
    "target0-reduce1000": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=1000",
        binary=NEXT_POLICY),
    "target0-reduce5000": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=5000",
        binary=NEXT_POLICY),
    "target0-focus100": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=100",
        binary=NEXT_POLICY),
    "target0-focus500": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=500",
        binary=NEXT_POLICY),
    "target0-focus1000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=1000",
        binary=NEXT_POLICY),
    "target0-focus2000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=2000",
        binary=NEXT_POLICY),
    "target0-focus5000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=5000",
        binary=NEXT_POLICY),
    "target0-focus10000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=10000",
        binary=NEXT_POLICY),
    "target0-focus1000-no-repeat": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=1000",
        "TACTIX_CRUX_INPROCESS_INTERVAL=0", binary=NEXT_POLICY),
    # Search and in-processing ablations around the known stable winner.
    "stable-randec0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                             "TACTIX_RANDEC=0"),
    "stable-chrono0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                             "TACTIX_CHRONO_LEVELS=0"),
    "stable-chrono20": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_CHRONO_LEVELS=20"),
    "stable-chrono1000": policy("backbone", "TACTIX_FORCE_MODE=stable",
                                "TACTIX_CHRONO_LEVELS=1000"),
    "stable-no-conflict-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_CONFLICT_REUSE=0"),
    "stable-no-jump-reasons": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_JUMP_REASONS=0"),
    "stable-no-reason-bump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_NO_BUMP_REASONS=1"),
    "stable-reduce-arith": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0"),
    "stable-reorder0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_REORDER=0"),
    "stable-walk0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                           "TACTIX_WALK_PERMILLE=0"),
    "stable-no-factor": policy("backbone", "TACTIX_FORCE_MODE=stable",
                               "TACTIX_NO_FACTOR=1"),
    "stable-no-sweep": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_NO_SWEEP=1"),
    "stable-no-bve": policy("backbone", "TACTIX_FORCE_MODE=stable",
                            "TACTIX_NO_BVE=1"),
    "stable-preprocess-only": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_PREPROCESS_ONLY=1"),
    "stable-eager0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                            "TACTIX_EAGER_SUBSUME=0"),
    "stable-no-closure-delay": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_NO_CLOSURE_DELAY=1"),
    "stable-backbone100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Backbone:100"),
    "stable-vivify300": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Vivify:300"),
    "stable-substitute100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Substitute:100"),
    "stable-aggressive-slots": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Backbone:100,Vivify:300,Substitute:100"),
    "stable-bve64": policy("backbone", "TACTIX_FORCE_MODE=stable",
                           "TACTIX_BVE_RAMP_MAX=64"),
    "stable-cce1": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_SLOT_ENABLE=CCE",
        "TACTIX_SLOT_MAX_APPLIES=CCE:1"),
    # Coordinate search around the first seed-7 winner (Reorder disabled).
    "default-stable-reorder0": cli("TACTIX_FORCE_MODE=stable",
                                   "TACTIX_REORDER=0"),
    "gate-stable-reorder0": policy("gate", "TACTIX_FORCE_MODE=stable",
                                   "TACTIX_REORDER=0"),
    "reorder-randec0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0"),
    "reorder-reduce-arith": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0"),
    "reorder-no-factor": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_FACTOR=1"),
    "reorder-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1"),
    "reorder-no-closure-delay": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_CLOSURE_DELAY=1"),
    "reorder-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_JUMP_REASONS=0"),
    "reorder-no-conflict-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_CONFLICT_REUSE=0"),
    "reorder-walk0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_WALK_PERMILLE=0"),
    "reorder-reduce-no-factor": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_FACTOR=1"),
    "reorder-reduce-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_BVE=1"),
    "reorder-no-factor-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1"),
    "reorder-randec-reduce": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0"),
    "reorder-reduce-no-factor-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1"),
    "reorder-core": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0", "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1",
        "TACTIX_NO_CLOSURE_DELAY=1"),
    "reorder-core-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0", "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1",
        "TACTIX_NO_CLOSURE_DELAY=1", "TACTIX_JUMP_REASONS=0"),
    "kissat": [KISSAT],
}


DEFAULT_SCREEN = tuple(name for name in ARMS if name != "kissat")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "verify"))
    parser.add_argument("--arms", default="")
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    requested = tuple(filter(None, args.arms.split(",")))
    selected = requested or (DEFAULT_SCREEN if args.stage == "screen" else (
        "default", "known-132", "backbone-stable", "kissat"))
    unknown = sorted(set(selected) - set(ARMS))
    if unknown:
        raise SystemExit("unknown arm(s): " + ", ".join(unknown))
    timeout = args.timeout if args.timeout is not None else (
        240.0 if args.stage == "screen" else 600.0)
    cnf = os.path.join(ROOT, REL)
    for binary in (DEFAULT, POLICY, OLD_POLICY, NEXT_POLICY, KISSAT):
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            raise SystemExit(f"missing executable pin: {binary}")
    if not os.path.isfile(cnf):
        raise SystemExit(f"missing input: {cnf}")

    # Screen timings include proof writing but defer verification.  Verify is
    # submission-grade: every SAT model or UNSAT proof is independently checked.
    verify = args.stage == "verify"
    solvers = [(name, ARMS[name], True, verify, verify) for name in selected]
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"crux-seed7-{args.stage}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    metadata = {
        "stage": args.stage,
        "development_instance": REL,
        "held_out_size32_seeds": [0, 2, 3, 4, 5, 6, 8, 9],
        "target_speedup": "5x-10x versus faster of DefaultPolicy and Kissat",
        "timeout_seconds": timeout,
        "partition": args.partition,
        "cpus_per_task": 4,
        "memory_gib": 16,
        "pin_source_commit": PIN_COMMIT,
        "campaign_wrapper_commit": subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], check=True,
            capture_output=True, text=True).stdout.strip(),
        "campaign_wrapper_sha256": digest(__file__),
        "input_sha256": digest(cnf),
        "executables": {path: digest(path) for path in
                        (DEFAULT, POLICY, OLD_POLICY, NEXT_POLICY, KISSAT)},
        "arms": {name: ARMS[name] for name in selected},
        "proof_policy": ("write and verify" if verify else
                         "write for timed parity; delete without checking"),
        "proof_lifecycle": (
            "task-local mkdtemp; removed on normal return, exception, "
            "SIGTERM, and SIGINT"),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(partition=args.partition, throttle=None,
                             dry_run=args.dry_run, wait=args.wait, tier=label)
    print(f"development target: {REL} ({metadata['input_sha256']})")
    print(f"arms: {', '.join(selected)}")
    # run.slurm_submit maps 16 GiB to two physical / four logical CPUs.
    return run.slurm_submit(
        submit, [(REL, "UNKNOWN", 0.0)], solvers, {REL: cnf}, timeout, 16,
        outdir, stamp, PIN_COMMIT, label)


if __name__ == "__main__":
    raise SystemExit(main())

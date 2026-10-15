#!/usr/bin/env python3
"""Chvatal (symbreak n=7) subproblem campaigns: baselines and public policies.

Instances are symbreak SAT-partition nodes staged under corpus/chvatal/.
Every arm writes its certificate for timing parity; baseline certificates are
not checked, while `--verify` checks the candidate arms' certificates.
"""

import argparse
import datetime
import glob
import hashlib
import json
import os
import subprocess
from types import SimpleNamespace

import run


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PIN_COMMIT = "903ec8bb4"
PIN = os.path.join(ROOT, "build", f"pin-chvatal-{PIN_COMMIT}", "build")
TACTIX = os.path.join(PIN, "tactix")
SATSUMA_KISSAT = os.path.join(
    ROOT, "tools", "solvers", "variants", "satsuma-kissat-proof-file.sh")
SATSUMA_RUNNER = os.path.join(
    ROOT, "tools", "solvers", "variants", "satsuma-kissat-sc2026",
    "run_satsuma_kissat.sh")
POLICY_PINS = os.path.join(ROOT, "build")
KISSAT = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
ENV = "/usr/bin/env"

BASELINES = {
    "tactix-default": [ENV, TACTIX, "--dsr"],
    "satsuma-kissat": [SATSUMA_KISSAT],
    "kissat-4.0.4": [KISSAT],
}


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def candidate_arms(specs):
    """NAME=PIN_DIR:ARG1,ARG2 ... -> a policy arm on a pinned client."""
    arms = {}
    for spec in specs:
        name, _, rest = spec.partition("=")
        pin, _, args = rest.partition(":")
        binary = os.path.join(POLICY_PINS, pin, "build", "examples",
                              "tactix-chvatal-policy")
        tokens = list(filter(None, args.split(",")))
        env = [t for t in tokens if "=" in t and not t.startswith("-")]
        flags = [t for t in tokens if t not in env]
        if "--dsr" not in flags and "--drat" not in flags:
            flags.append("--drat")
        arms[name] = [ENV, *env, binary, *flags]
    return arms


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--arms", default="tactix-default,satsuma-kissat")
    parser.add_argument("--candidate", action="append", default=[],
                        help="NAME=PIN_DIR:ARG,ARG (verified arm)")
    parser.add_argument("--env-arm", action="append", default=[],
                        help="NAME=K=V,K=V: Tactix CLI under DefaultPolicy "
                             "environment settings (baseline-style arm)")
    parser.add_argument("--instances", required=True,
                        help="file listing repository-relative CNF paths")
    parser.add_argument("--timeout", type=float, required=True)
    parser.add_argument("--mem", type=int, default=8)
    parser.add_argument("--partition", default="cpu-q")
    parser.add_argument("--label", default="chvatal")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--verify-baselines", action="store_true",
                        help="also check baseline certificates (recorded, "
                             "and scored like candidate arms)")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    rels = [line.strip() for line in open(args.instances) if line.strip()]
    paths = {rel: os.path.join(ROOT, rel) for rel in rels}
    missing = [p for p in paths.values() if not os.path.isfile(p)]
    if missing:
        raise SystemExit("missing input(s): " + ", ".join(missing[:5]))
    candidates = candidate_arms(args.candidate)
    solvers = []
    for name in filter(None, args.arms.split(",")):
        solvers.append((name, BASELINES[name], True, args.verify_baselines,
                        args.verify_baselines))
    for spec in args.env_arm:
        name, _, settings = spec.partition("=")
        solvers.append((name, [ENV, *filter(None, settings.split(",")),
                               TACTIX, "--dsr"], True, args.verify_baselines,
                        args.verify_baselines))
    for name, cmd in candidates.items():
        solvers.append((name, cmd, True, args.verify, args.verify))
    executables = {TACTIX, SATSUMA_KISSAT, SATSUMA_RUNNER, KISSAT}
    executables |= {next(c for c in cmd[1:] if "=" not in c)
                    for cmd in candidates.values()}
    for path in executables:
        if not os.access(path, os.X_OK):
            raise SystemExit(f"missing executable pin: {path}")

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    outdir = os.path.join(run.RUNS, f"{stamp}-{args.label}")
    os.makedirs(outdir, exist_ok=False)
    metadata = {
        "instances": rels,
        "instance_list": os.path.relpath(args.instances, ROOT),
        "timeout_seconds": args.timeout,
        "partition": args.partition,
        "memory_gib": args.mem,
        "pin_source_commit": PIN_COMMIT,
        "campaign_wrapper_commit": subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], check=True,
            capture_output=True, text=True).stdout.strip(),
        "campaign_wrapper_sha256": digest(__file__),
        "input_sha256": {rel: digest(path) for rel, path in paths.items()},
        "executables": {path: digest(path) for path in sorted(executables)},
        "arms": {name: cmd for name, cmd, *_ in solvers},
        "verified_arms": sorted(candidates) if args.verify else [],
        "partition_reason": ("cpu-dense-hwulab-q fully allocated by gate "
                             "jobs; plan permits cpu-q for screens and "
                             "development, all arms in one window"),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(partition=args.partition, throttle=None,
                             dry_run=args.dry_run, wait=False, tier=args.label)
    return run.slurm_submit(
        submit, [(rel, "UNKNOWN", 0.0) for rel in rels], solvers, paths,
        args.timeout, args.mem, outdir, stamp, PIN_COMMIT, args.label)


if __name__ == "__main__":
    raise SystemExit(main())

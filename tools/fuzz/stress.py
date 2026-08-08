#!/usr/bin/env python3
"""Validate a stress scenario and print fuzz-harness arguments for it."""

import argparse
import json
import shlex
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("--solver", default="build/debug/tactix")
    args = ap.parse_args()
    data = json.loads(Path(args.scenario).read_text())
    required = {"schema_version", "name", "prefer", "normal_every",
                "required_target_fires", "minimum_verdicts"}
    missing = required - data.keys()
    if missing:
        raise SystemExit("missing scenario keys: " + ", ".join(sorted(missing)))
    if data["schema_version"] != 1 or not data["prefer"]:
        raise SystemExit("unsupported schema or empty prefer list")
    env = {
        "TACTIX_STRESS_RULES": ",".join(data["prefer"]),
        "TACTIX_STRESS_NORMAL_EVERY": str(data["normal_every"]),
    }
    prefix = " ".join(f"{key}={shlex.quote(value)}" for key, value in env.items())
    solver = f"env {prefix} {shlex.quote(args.solver)} --stress-policy"
    required = ",".join(data["prefer"])
    print("--solver " + shlex.quote(solver))
    print("--require-reached " + shlex.quote(required))
    print("--minimum-verdicts " + str(data["minimum_verdicts"]))
    print("--minimum-target-fires " + str(data["required_target_fires"]))


if __name__ == "__main__":
    main()

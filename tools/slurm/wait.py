#!/usr/bin/env python3
"""Wait for one explicit SLURM job and summarize its terminal state."""

import argparse
import subprocess
import time
from pathlib import Path


def output(*argv):
    return subprocess.run(argv, text=True, capture_output=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--job", required=True)
    ap.add_argument("--log")
    ap.add_argument("--poll", type=int, default=30)
    args = ap.parse_args()
    while output("squeue", "-h", "-j", args.job, "-o", "%i"):
        time.sleep(max(1, args.poll))
    state = output("sacct", "-n", "-X", "-j", args.job, "-o", "State", "-P")
    states = [line.split("|")[0].strip() for line in state.splitlines() if line.strip()]
    final = states[0] if states else "UNKNOWN"
    print(f"SLURM job {args.job}: {final}")
    if args.log:
        path = Path(args.log)
        if path.exists():
            lines = path.read_text(errors="replace").splitlines()
            print("\n".join(lines[-40:]))
        else:
            print(f"log not found: {path}")
    return 0 if final.startswith("COMPLETED") else 1


if __name__ == "__main__":
    raise SystemExit(main())

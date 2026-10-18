#!/usr/bin/env python3
"""Record home-quota usage while one Slurm job remains live.

This monitor is observational.  It never holds, releases, suspends, or cancels
jobs.  Each sample is appended to a JSONL log in the supplied directory.
"""

import argparse
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import time

UNITS = {"": 1 / 2**30, "B": 1 / 2**30, "K": 1 / 2**20,
         "M": 1 / 2**10, "G": 1.0, "T": 2**10, "P": 2**20}


def parse_size(value):
    match = re.fullmatch(
        r"\s*([0-9]+(?:\.[0-9]+)?)\s*([KMGTPB]?)i?B?\s*", value or "")
    return (float(match.group(1)) * UNITS[match.group(2)]
            if match else None)


def usage_from_table(table, user):
    for line in table.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == user:
            return parse_size(fields[1])
    return None


def read_usage(user, zfs_file=Path("/home/.zfs-quota"), run=subprocess.run):
    try:
        usage = usage_from_table(Path(zfs_file).read_text(), user)
        if usage is not None:
            return usage, str(zfs_file)
    except OSError:
        pass
    try:
        result = run(["quota", "-s"], capture_output=True, text=True, timeout=60)
        usage = usage_from_table(result.stdout, user)
        if usage is not None:
            return usage, "quota -s"
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None, "unavailable"


def job_live(job, run=subprocess.run):
    result = run(["squeue", "-h", "-j", str(job), "-o", "%i"],
                 capture_output=True, text=True, timeout=120)
    if result.returncode and "Invalid job id" not in result.stderr + result.stdout:
        return None
    return bool(result.stdout.strip())


def sample(directory, job, user, run=subprocess.run, usage_reader=read_usage,
           now=None):
    usage, source = usage_reader(user)
    live = job_live(job, run)
    record = {
        "array_job_id": str(job),
        "job_live": live,
        "observed_utc": now or dt.datetime.now(dt.timezone.utc).isoformat(),
        "usage_gib": usage,
        "usage_source": source,
    }
    directory = Path(directory)
    with (directory / "quota.log.jsonl").open("a") as output:
        output.write(json.dumps(record, sort_keys=True) + "\n")
    return live is False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", type=Path, required=True)
    parser.add_argument("--job", required=True)
    parser.add_argument("--user", required=True)
    parser.add_argument("--interval", type=int, default=600)
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("--interval must be positive")
    while True:
        if sample(args.dir, args.job, args.user):
            return
        time.sleep(args.interval)


if __name__ == "__main__":
    main()

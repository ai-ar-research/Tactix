#!/usr/bin/env python3
"""Hold and release PENDING tasks of owned Slurm arrays on home-quota usage.

Policy (user, 2026-10-08): every ``--interval`` seconds read the user's home
usage (the user's row of /home/.zfs-quota; ``quota -s`` as fallback).  Above
``--hold-above`` units, ``scontrol hold`` the PENDING tasks of the owned
arrays.  While held, keep checking (and hold newly pending tasks of owned
arrays); ``scontrol release`` only the tasks this governor held, at most
``--release-batch`` per check, and only while usage is below
``--release-below``. Running tasks are never held, suspended or cancelled.
Usage values are in the units the quota tools print (G = 2**30 B).

Owned arrays live in ``<dir>/jobs.json``: {"jobs": [{"id": "123", "label": ..}],
"expect_more": bool}.  Campaign submitters append to it.  The governor exits
once every listed job has left the queue and ``expect_more`` is false.
Every check and action is appended to ``<dir>/governor.log.jsonl``; the hold
set and state persist in ``<dir>/state.json``.
"""
import argparse
import datetime as dt
import getpass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

UNITS = {"": 1 / 2**30, "B": 1 / 2**30, "K": 1 / 2**20, "M": 1 / 2**10,
         "G": 1.0, "T": 2**10, "P": 2**20}
HOLD_CHUNK = 100


def parse_size(text):
    """'526G' -> 526.0 (GiB-units, as printed); None if unparsable."""
    m = re.fullmatch(r"\s*([0-9]+(?:\.[0-9]+)?)\s*([KMGTPB]?)i?B?\s*", text or "")
    if not m:
        return None
    return float(m.group(1)) * UNITS[m.group(2)]


def usage_from_table(text, user):
    """USED column of ``user``'s row in a NAME USED QUOTA ... table."""
    for line in text.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == user:
            return parse_size(fields[1])
    return None


def read_usage(user, zfs_file=Path("/home/.zfs-quota"), run=subprocess.run):
    try:
        value = usage_from_table(Path(zfs_file).read_text(), user)
        if value is not None:
            return value, str(zfs_file)
    except OSError:
        pass
    try:
        p = run(["quota", "-s"], capture_output=True, text=True, timeout=60)
        value = usage_from_table(p.stdout, user)
        if value is not None:
            return value, "quota -s"
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None, "unavailable"


def decide(state, usage, hold_above, release_below):
    """Hysteresis: returns (new_state, action) with action in
    {None, 'hold', 'hold-new', 'release'}."""
    if usage is None:
        return state, ("hold-new" if state == "held" else None)
    if state == "released":
        return ("held", "hold") if usage > hold_above else ("released", None)
    if usage < release_below:
        return "released", "release"
    return "held", "hold-new"


def pending_tasks(job_ids, run=subprocess.run):
    """{task_id: reason} for PENDING tasks of the owned jobs (array-expanded)."""
    if not job_ids:
        return {}
    p = run(["squeue", "-h", "-r", "-t", "PD", "-j", ",".join(job_ids),
             "-o", "%i|%r"], capture_output=True, text=True, timeout=120)
    if p.returncode != 0:
        # Purged ids make a multi-id query fail; ask one by one.
        out = {}
        for job in job_ids:
            q = run(["squeue", "-h", "-r", "-t", "PD", "-j", job, "-o", "%i|%r"],
                    capture_output=True, text=True, timeout=120)
            if q.returncode == 0:
                out.update(parse_pending(q.stdout))
        return out
    return parse_pending(p.stdout)


def parse_pending(text):
    out = {}
    for line in text.splitlines():
        if "|" in line:
            task, reason = line.split("|", 1)
            out[task.strip()] = reason.strip()
    return out


def job_live(job, run=subprocess.run):
    """True while squeue lists any task of ``job``; None if unknown."""
    p = run(["squeue", "-h", "-j", job, "-o", "%i"], capture_output=True,
            text=True, timeout=120)
    if p.returncode != 0:
        return False if "Invalid job id" in (p.stderr + p.stdout) else None
    return bool(p.stdout.strip())


def owns(task, job_ids):
    return task.split("_", 1)[0] in job_ids


def scontrol(verb, tasks, run=subprocess.run):
    done, errors = [], []
    tasks = list(tasks)
    for i in range(0, len(tasks), HOLD_CHUNK):
        chunk = tasks[i:i + HOLD_CHUNK]
        p = run(["scontrol", verb, ",".join(chunk)], capture_output=True,
                text=True, timeout=300)
        if p.returncode == 0:
            done.extend(chunk)
        else:
            # Retry one by one so a task that just started cannot block others.
            for task in chunk:
                q = run(["scontrol", verb, task], capture_output=True, text=True,
                        timeout=120)
                if q.returncode == 0:
                    done.append(task)
                else:
                    errors.append({"task": task,
                                   "error": (q.stderr or q.stdout).strip()[:200]})
    return done, errors


def cycle(directory, args, run=subprocess.run, usage_reader=None, now=None):
    """One check: returns True when the governor should exit."""
    directory = Path(directory)
    jobs = json.loads((directory / "jobs.json").read_text())
    job_ids = [str(j["id"]) for j in jobs["jobs"]]
    state_path = directory / "state.json"
    state = (json.loads(state_path.read_text()) if state_path.exists()
             else {"state": "released", "held": []})
    usage, source = (usage_reader or (lambda: read_usage(args.user, run=run)))()
    new_state, action = decide(state["state"], usage, args.hold_above,
                               args.release_below)
    record = {"time": now or dt.datetime.now().astimezone().isoformat(timespec="seconds"),
              "usage": usage, "usage_source": source, "state": state["state"],
              "new_state": new_state, "action": action, "jobs": job_ids}
    if action in ("hold", "hold-new"):
        pending = pending_tasks(job_ids, run)
        targets = [t for t, reason in pending.items()
                   if owns(t, job_ids) and t not in state["held"] and
                   "JobHeld" not in reason]
        done, errors = scontrol("hold", targets, run) if targets else ([], [])
        state["held"] = sorted(set(state["held"]) | set(done))
        record.update({"pending": len(pending), "held_now": len(done),
                       "hold_errors": errors[:20], "held_total": len(state["held"])})
    elif action == "release":
        pending = pending_tasks(job_ids, run)
        eligible = [t for t in state["held"]
                    if t in pending and owns(t, job_ids)]
        targets = eligible[:args.release_batch]
        done, errors = scontrol("release", targets, run) if targets else ([], [])
        record.update({"release_eligible": len(eligible),
                       "released_now": len(done),
                       "release_remaining": len(eligible) - len(done),
                       "release_errors": errors[:20],
                       "held_not_pending": len(set(state["held"]) - set(eligible))})
        state["held"] = sorted(set(eligible) - set(done))
        if state["held"]:
            # Admit another bounded batch, or retry a failed release, on the
            # next below-threshold tick.
            new_state = "held"
    state["state"] = new_state
    live = {job: job_live(job, run) for job in job_ids}
    record["live_jobs"] = [j for j, v in live.items() if v is not False]
    finished = (not jobs.get("expect_more", False) and job_ids and
                all(v is False for v in live.values()))
    record["exit"] = bool(finished)
    tmp = state_path.with_name(".state.json.tmp")
    tmp.write_text(json.dumps(state, indent=2) + "\n")
    os.replace(tmp, state_path)
    with open(directory / "governor.log.jsonl", "a") as log:
        log.write(json.dumps(record) + "\n")
        log.flush()
        os.fsync(log.fileno())
    return bool(finished)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, required=True)
    ap.add_argument("--interval", type=int, default=600)
    ap.add_argument("--hold-above", type=float, default=750.0)
    ap.add_argument("--release-below", type=float, default=700.0)
    ap.add_argument("--release-batch", type=int, default=40)
    ap.add_argument("--user", default=getpass.getuser())
    ap.add_argument("--once", action="store_true")
    args = ap.parse_args(argv)
    if args.release_below > args.hold_above:
        ap.error("--release-below must not exceed --hold-above")
    if args.release_batch <= 0:
        ap.error("--release-batch must be positive")
    while True:
        if cycle(args.dir, args) or args.once:
            return 0
        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())

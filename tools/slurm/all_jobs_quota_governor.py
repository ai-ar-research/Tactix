#!/usr/bin/env python3
"""Monitor all of one user's Slurm jobs and govern only pending work.

Every sample records global home usage and every queued job owned by ``--user``.
Above ``--hold-above``, newly pending tasks are held. Below
``--release-below``, only tasks held by this process are released, in bounded
batches. Running jobs are never hold, suspend, resume, requeue, or cancel
targets. The governor excludes its own Slurm job and exits only when no other
job owned by the user remains in the queue.
"""

import argparse
from collections import Counter, defaultdict
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import time

try:
    from .quota_governor import read_usage
except ImportError:  # Direct execution: python3 tools/slurm/this_file.py
    from quota_governor import read_usage


HOLD_CHUNK = 100


def scheduler_states(user, run=subprocess.run):
    result = run(
        ["squeue", "-h", "-u", user, "-o", "%F|%T"],
        capture_output=True, text=True, timeout=120, check=True)
    states = defaultdict(Counter)
    for line in result.stdout.splitlines():
        parent, state = line.split("|", 1)
        states[parent.strip()][state.strip()] += 1
    return {job: dict(counts) for job, counts in states.items()}


def pending_tasks(user, excluded, run=subprocess.run):
    """Return expanded pending task IDs, excluding the governor's own job."""
    result = run(
        ["squeue", "-h", "-r", "-t", "PD", "-u", user, "-o", "%i|%r"],
        capture_output=True, text=True, timeout=120, check=True)
    pending = {}
    for line in result.stdout.splitlines():
        if "|" not in line:
            continue
        task, reason = (field.strip() for field in line.split("|", 1))
        if task.split("_", 1)[0] not in excluded:
            pending[task] = reason
    return pending


def control_pending(verb, tasks, run=subprocess.run):
    """Apply a control verb to bounded lists of IDs known to be pending."""
    done, errors = [], []
    tasks = list(tasks)
    for offset in range(0, len(tasks), HOLD_CHUNK):
        chunk = tasks[offset:offset + HOLD_CHUNK]
        result = run(["scontrol", verb, ",".join(chunk)], capture_output=True,
                     text=True, timeout=300)
        if result.returncode == 0:
            done.extend(chunk)
            continue
        # A task can leave PENDING between squeue and scontrol. Retrying IDs
        # separately keeps that race from blocking control of the other tasks.
        for task in chunk:
            retry = run(["scontrol", verb, task], capture_output=True,
                        text=True, timeout=120)
            if retry.returncode == 0:
                done.append(task)
            else:
                errors.append({"task": task,
                               "error": (retry.stderr or retry.stdout).strip()[:200]})
    return done, errors


def load_state(path):
    if path.exists():
        return json.loads(path.read_text())
    return {"mode": "released", "held": []}


def save_state(path, state):
    temporary = path.with_name(".quota-all-jobs-state.json.tmp")
    temporary.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n")
    os.replace(temporary, path)


def sample(directory, user, excluded, hold_above, release_below,
           release_batch, run=subprocess.run, usage_reader=None, now=None):
    directory = Path(directory)
    usage, source = ((usage_reader or
                      (lambda: read_usage(user, run=run)))())
    all_states = scheduler_states(user, run)
    states = {job: counts for job, counts in all_states.items()
              if job not in excluded}
    pending = pending_tasks(user, excluded, run)
    state_path = directory / "quota-all-jobs-state.json"
    state = load_state(state_path)
    previous_mode = state["mode"]
    action = None
    errors = []
    changed = []

    if usage is not None and usage > hold_above:
        state["mode"] = "held"
        action = "hold" if previous_mode == "released" else "hold-new"
        targets = [task for task, reason in pending.items()
                   if task not in state["held"] and "JobHeld" not in reason]
        changed, errors = control_pending("hold", targets, run)
        state["held"] = sorted(set(state["held"]) | set(changed))
    elif previous_mode == "held" and usage is not None and usage < release_below:
        action = "release"
        eligible = [task for task in state["held"] if task in pending]
        targets = eligible[:release_batch]
        changed, errors = control_pending("release", targets, run)
        state["held"] = sorted(set(eligible) - set(changed))
        state["mode"] = "held" if state["held"] else "released"
    elif previous_mode == "held":
        # Fail closed on an unreadable quota and keep discovering new pending
        # work while the governor is in held mode.
        action = "hold-new"
        targets = [task for task, reason in pending.items()
                   if task not in state["held"] and "JobHeld" not in reason]
        changed, errors = control_pending("hold", targets, run)
        state["held"] = sorted(set(state["held"]) | set(changed))

    save_state(state_path, state)
    record = {
        "observed_utc": now or dt.datetime.now(dt.timezone.utc).isoformat(),
        "usage_gib": usage,
        "usage_source": source,
        "hold_above_gib": hold_above,
        "release_below_gib": release_below,
        "mode_before": previous_mode,
        "mode_after": state["mode"],
        "action": action,
        "changed_tasks": changed,
        "control_errors": errors,
        "held_by_governor": state["held"],
        "all_user_job_states": states,
        "excluded_job_ids": sorted(excluded),
    }
    with (directory / "quota-all-jobs.log.jsonl").open("a") as output:
        output.write(json.dumps(record, sort_keys=True) + "\n")
        output.flush()
        os.fsync(output.fileno())
    return not states


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", type=Path, required=True)
    parser.add_argument("--user", required=True)
    parser.add_argument("--exclude-job", action="append", default=[])
    parser.add_argument("--interval", type=int, default=600)
    parser.add_argument("--hold-above", type=float, default=750.0)
    parser.add_argument("--release-below", type=float, default=700.0)
    parser.add_argument("--release-batch", type=int, default=40)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("--interval must be positive")
    if args.release_below > args.hold_above:
        parser.error("--release-below must not exceed --hold-above")
    if args.release_batch <= 0:
        parser.error("--release-batch must be positive")
    excluded = {str(job) for job in args.exclude_job}
    while True:
        finished = sample(args.dir, args.user, excluded, args.hold_above,
                          args.release_below, args.release_batch)
        if args.once or finished:
            return
        time.sleep(args.interval)


if __name__ == "__main__":
    main()

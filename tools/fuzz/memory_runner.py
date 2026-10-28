#!/usr/bin/env python3
"""Run one fuzz child under an explicit resident-memory and wall-time limit.

Address-space limits are unsuitable for ASan, whose shadow mapping is much
larger than its resident set.  This wrapper instead monitors the complete child
process tree's resident memory, leaves enough headroom for the fuzz harness,
and records why the child stopped.  The surrounding Slurm cgroup remains a
last-resort backstop; a cgroup OOM delta is recorded separately.
"""

import argparse
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import time


def cgroup_memory_events():
    """Return cgroup-v2 memory events when available."""
    try:
        for line in Path("/proc/self/cgroup").read_text().splitlines():
            hierarchy, controllers, relative = line.split(":", 2)
            if hierarchy == "0" and not controllers:
                path = Path("/sys/fs/cgroup") / relative.lstrip("/") / "memory.events"
                return {
                    key: int(value)
                    for key, value in (
                        item.split() for item in path.read_text().splitlines()
                    )
                }
    except (OSError, ValueError):
        pass
    return {}


def children(pid):
    try:
        path = Path(f"/proc/{pid}/task/{pid}/children")
        return [int(item) for item in path.read_text().split()]
    except (OSError, ValueError):
        return []


def process_tree(root):
    pending, seen = [root], set()
    while pending:
        pid = pending.pop()
        if pid in seen:
            continue
        seen.add(pid)
        pending.extend(children(pid))
    return seen


def resident_kib(pid):
    try:
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        pass
    return 0


def terminate_group(process):
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def atomic_json(path, value):
    destination = Path(path)
    temporary = destination.with_name(destination.name + f".tmp.{os.getpid()}")
    temporary.write_text(json.dumps(value, sort_keys=True) + "\n")
    os.replace(temporary, destination)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--status", required=True)
    parser.add_argument("--memory-mib", type=int, required=True)
    parser.add_argument("--timeout", type=float, required=True)
    parser.add_argument("--cpu-seconds", type=int, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if not args.command or args.memory_mib <= 0 or args.timeout <= 0:
        parser.error("positive limits and a command are required")

    def child_limits():
        resource.setrlimit(resource.RLIMIT_CPU,
                           (args.cpu_seconds, args.cpu_seconds + 2))

    before = cgroup_memory_events()
    start = time.monotonic()
    process = subprocess.Popen(args.command, start_new_session=True,
                               preexec_fn=child_limits)
    limit_kib = args.memory_mib * 1024
    peak_kib = 0
    memory_exceeded = timed_out = False
    try:
        while process.poll() is None:
            current = sum(resident_kib(pid) for pid in process_tree(process.pid))
            peak_kib = max(peak_kib, current)
            if current > limit_kib:
                memory_exceeded = True
                terminate_group(process)
                break
            if time.monotonic() - start >= args.timeout:
                timed_out = True
                terminate_group(process)
                break
            time.sleep(0.01)
        returncode = process.wait()
        # A direct child which exits without reaping its own descendants must
        # not leave them running beyond this invocation's declared boundary.
        terminate_group(process)
    finally:
        if process.poll() is None:
            terminate_group(process)
            process.wait()

    after = cgroup_memory_events()
    cgroup_oom_kills = max(0, after.get("oom_kill", 0) -
                           before.get("oom_kill", 0))
    # Several monitored children can share one Slurm cgroup in the legacy
    # multi-worker mode.  Attribute a cgroup OOM to this child only when the
    # kernel also killed this process group; otherwise merely record the delta.
    memory_exceeded |= cgroup_oom_kills > 0 and returncode == -signal.SIGKILL
    status = {
        "schema": 1,
        "command": args.command,
        "returncode": returncode,
        "signal": -returncode if returncode < 0 else None,
        "timed_out": timed_out,
        "memory_exceeded": memory_exceeded,
        "memory_limit_mib": args.memory_mib,
        "peak_rss_kib": peak_kib,
        "cgroup_oom_kills": cgroup_oom_kills,
        "elapsed_s": time.monotonic() - start,
    }
    atomic_json(args.status, status)
    if timed_out:
        return 124
    if memory_exceeded:
        return 86
    return returncode if returncode >= 0 else 128 - returncode


if __name__ == "__main__":
    sys.exit(main())

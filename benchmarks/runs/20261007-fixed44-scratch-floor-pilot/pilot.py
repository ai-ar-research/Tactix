"""Compute-node mechanics pilot for the fixed44 shared-floor scratch profile.

Runs on one Slurm allocation with real /tmp, O_TMPFILE, flock ledger slots,
statvfs and the runtime's SIGUSR1 floor interruption of a measured() child.
Phase 1: three participants admit together (the cooperative mutex admitted one).
Phase 2: thresholds are set relative to the measured free space so that a few
GB of anonymous writes cross the reserve; exactly the largest participant must
stop, its storage must return, and the others must finish. Writes stay below
5 GB in total; the private pilot ledger is removed at the end.
"""
import json, os, signal, subprocess, sys, time
from pathlib import Path

BENCH = Path(sys.argv[1]).resolve()
OUT = Path(sys.argv[2]).resolve()
sys.path.insert(0, str(BENCH))
import anonymous_scratch as scratch_module
import runtime_task as runtime

GB = 1_000_000_000
LEDGER = Path(f"/tmp/tactix-scratch-pilot-{os.getuid()}-{os.environ['SLURM_JOB_ID']}")
PREFIX = "pilot-slot-"


def free():
    s = os.statvfs("/tmp")
    return s.f_bavail * s.f_frsize


def participant(name, target_bytes, reserve, hard_floor, admission_free, hold):
    signal.signal(signal.SIGUSR1, runtime.scratch_floor_signal)
    scratch, ledger, admission = scratch_module.admit_shared_scratch(
        "/tmp", admission_free_bytes=admission_free, maximum_file_bytes=10 * GB,
        maximum_live_handles=6, ledger_directory=LEDGER, ledger_slot_prefix=PREFIX,
        ledger_slots=8, admission_wait_seconds=30, admission_poll_seconds=5,
        identity=name)
    monitor = scratch_module.ScratchFloorMonitor(
        scratch, ledger, directory="/tmp", reserve_bytes=reserve,
        hard_floor_bytes=hard_floor, poll_seconds=1,
        on_trigger=runtime.request_scratch_floor_stop)
    monitor.start()
    record = dict(name=name, admission=admission, target_bytes=target_bytes)
    try:
        proof = scratch / "proof.out"
        writer = ("import os,sys,time;fd=os.open(sys.argv[1],os.O_WRONLY);"
                  "n=int(sys.argv[2]);b=b'x'*(64<<20);w=0\n"
                  "while w<n:\n k=min(len(b),n-w);os.write(fd,b[:k]);w+=k;time.sleep(0.05)\n"
                  "os.fsync(fd);time.sleep(float(sys.argv[3]))")
        result = runtime.measured([sys.executable, "-c", writer, proof, str(target_bytes), str(hold)],
                                  scratch / "out", scratch / "err", scratch / "time", 600)
        record.update(outcome="complete", measured=result,
                      accounted_bytes=scratch.accounted_bytes())
    except scratch_module.ScratchResourceError as error:
        record.update(outcome="refused", reason=error.scratch_reason, details=error.scratch_details)
    except runtime.ScratchCapacityError as error:
        record.update(outcome="stopped", reason=runtime.scratch_resource_reason(error),
                      details=error.scratch_details, completion=runtime.scratch_incomplete(
                          runtime.scratch_resource_reason(error), error.scratch_details))
    finally:
        monitor.stop()
        runtime.SCRATCH_FLOOR_TRIGGER.clear()
        record["monitor"] = monitor.status()
        scratch.close()
        ledger.close()
        record["free_after_close"] = free()
    print(json.dumps(record), flush=True)


def run_phase(label, specs, reserve, hard_floor, admission_free):
    procs = []
    for name, target, hold in specs:
        procs.append(subprocess.Popen(
            [sys.executable, __file__, str(BENCH), str(OUT), "participant", name, str(target),
             str(reserve), str(hard_floor), str(admission_free), str(hold)],
            stdout=subprocess.PIPE, text=True))
        time.sleep(0.2)
    records = []
    for p in procs:
        out, _ = p.communicate(timeout=900)
        records.append(dict(exit_code=p.returncode, **json.loads(out.strip().splitlines()[-1])))
    return dict(label=label, reserve_bytes=reserve, hard_floor_bytes=hard_floor,
                admission_free_bytes=admission_free, participants=records)


def main():
    assert os.environ.get("SLURM_JOB_ID")
    start_free = free()
    evidence = dict(job=os.environ["SLURM_JOB_ID"], node=os.uname().nodename,
                    start_free_bytes=start_free)
    # Phase 1: no floor pressure; all three admit and complete together.
    evidence["colocated"] = run_phase(
        "colocated", [("a", 256 << 20, 3), ("b", 256 << 20, 3), ("c", 256 << 20, 3)],
        reserve=10 * GB, hard_floor=5 * GB, admission_free=50 * GB)
    # Phase 2: the reserve lies 3.5 GB below current free space; writers add
    # 0.5 + 1.0 + 2.5 GB and hold, so only the largest (c) may be stopped.
    base = free()
    evidence["floor_base_free_bytes"] = base
    evidence["floor"] = run_phase(
        "floor", [("a", 500_000_000, 20), ("b", 1_000_000_000, 20), ("c", 2_500_000_000, 20)],
        reserve=base - 3_500_000_000, hard_floor=base - 6_500_000_000,
        admission_free=50 * GB)
    evidence["end_free_bytes"] = free()
    for slot in LEDGER.iterdir():
        slot.unlink()
    LEDGER.rmdir()
    colocated = evidence["colocated"]["participants"]
    floor = {r["name"]: r for r in evidence["floor"]["participants"]}
    checks = dict(
        colocated_all_admitted=all(r["outcome"] == "complete" for r in colocated),
        colocated_distinct_slots=len({r["admission"]["ledger_slot"] for r in colocated}) == 3,
        largest_stopped=floor["c"]["outcome"] == "stopped" and
            floor["c"]["details"]["trigger"] == "node_floor" and
            floor["c"]["completion"]["reason"] == "scratch_enospc" and
            floor["c"]["completion"].get("retryable") is True,
        others_complete=floor["a"]["outcome"] == "complete" and floor["b"]["outcome"] == "complete",
        storage_returned=evidence["end_free_bytes"] > base - 1_000_000_000,
    )
    evidence["checks"] = checks
    evidence["passed"] = all(checks.values())
    with open(OUT, "x") as f:
        json.dump(evidence, f, indent=2)
    print(("PASS " if evidence["passed"] else "FAIL ") + json.dumps(checks))
    return 0 if evidence["passed"] else 1


if __name__ == "__main__":
    if len(sys.argv) > 3 and sys.argv[3] == "participant":
        name, target, reserve, hard, admission, hold = sys.argv[4:10]
        participant(name, int(target), int(reserve), int(hard), int(admission), float(hold))
    else:
        raise SystemExit(main())

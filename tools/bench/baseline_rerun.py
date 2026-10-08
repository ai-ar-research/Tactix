#!/usr/bin/env python3
"""Rerun selected rows of the frozen SC24--26 owner baselines.

The frozen campaign (benchmarks/baselines/sc24-26-32g-5000s) stays immutable.
This runner prepares a NEW campaign directory from a row selection over a
source campaign's results, then runs each selected (instance, solver) row as a
fresh solve plus checker pipeline under the limits recorded in its manifest.

It reuses the frozen runner's result conventions (exclusive claims, pending
journal, delete-after-classification) and adds what a rerun needs:

* row selection by checker failure class (timeout / memout) or all rows of a
  solver, with the exact rule recorded in the manifest;
* explicit solve, checker, scheduler-memory and solver-memory limits.  A
  solver cap below the task cgroup is enforced by a process-group resident
  memory watchdog, never by RLIMIT_AS (tests/test_slurm_memory_enforcement.py);
* the accepted ``long64-fixed`` witnessed-DSR route (dsr-trim -> dsr-compress
  -d -> Trestle text mode) against the canonical checker CNF;
* pluggable certificate scratch (``directory`` kind; the root may be the
  campaign directory or any other absolute path);
* a frozen copy of the runner and its helper modules inside the campaign.
"""

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(os.environ.get(
    "TACTIX_REPO_ROOT", Path(__file__).resolve().parents[2])).resolve()
os.environ.setdefault("TACTIX_REPO_ROOT", str(ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import frozen_baseline as fb  # noqa: E402
import proof_oracle  # noqa: E402

CACHE = ROOT / "benchmarks/cache/tierD24-26"
SCHEMA = "tactix-baseline-rerun-v1"
RUNNER_FILES = ("baseline_rerun.py", "frozen_baseline.py", "proof_oracle.py")
MODEL_CHECK_SECONDS = 600
CLEANUP_MARGIN_SECONDS = 1800
# CakeML launchers reserve the heap plus a default 4 GiB stack.  Amendment 2 of
# the frozen campaign kept heap + stack 4 GiB below the memory limit; apply the
# same rule to whatever memory the checker receives.
CAKE_STACK_MIB = 4096
CAKE_SLACK_MIB = 4096
MEMOUT_MARKERS = ("memout", "out of memory", "outofmemory", "xrealloc",
                  "memory allocation of", "heap space exhausted", "bad_alloc",
                  "cannot allocate memory")
TIMEOUT_MARKERS = ("s timeout",)
FAILURE_CLASSES = ("timeout", "memout", "crash", "rejected", "other")


# --------------------------------------------------------------------------
# Classification and selection


def classify_check(proof_check):
    """Return verified/timeout/memout/crash/rejected/other, or None if unchecked.

    timeout: the pipeline deadline killed a stage, or drat-trim printed its own
             ``s TIMEOUT``.
    memout:  the runner's memory watchdog or the cgroup OOM killer stopped a
             stage, a stage died by SIGKILL without a deadline, or the last
             stage's output names an allocation failure (MEMOUT_MARKERS).
    crash:   SIGSEGV/SIGBUS (exit 139/135) without an allocation message.
    rejected: an explicit checker rejection (s INVALID / s NOT VERIFIED /
             Error:).
    """
    if not proof_check or not proof_check.get("stages"):
        return None
    if proof_check.get("result") == "verified":
        return "verified"
    stages = proof_check["stages"]
    last = stages[-1]
    text = ((last.get("stdout_tail") or "") + "\n" +
            (last.get("stderr_tail") or "")).lower()
    if (any(stage.get("timed_out") for stage in stages) or
            any(marker in text for marker in TIMEOUT_MARKERS)):
        return "timeout"
    killed = last.get("killed_by_runner")
    if (killed == "memory-cap" or (last.get("oom_kill_delta") or 0) > 0 or
            last.get("exit_code") in (137, -9) or last.get("term_signal") == 9 or
            any(marker in text for marker in MEMOUT_MARKERS)):
        return "memout"
    if last.get("exit_code") in (139, 135, -11, -7) or last.get("term_signal") in (11, 7):
        return "crash"
    if ("s invalid" in text or "s not verified" in text or "error" in text or
            "failed" in text):
        return "rejected"
    return "other"


def selection_rows(results, solvers, classes):
    """Rows of a results list whose UNSAT proof check failed in ``classes``.

    ``classes == ("all",)`` selects every row of the named solvers.
    """
    chosen = []
    for row in sorted(results, key=lambda r: r["task"]):
        if row["solver"] not in solvers:
            continue
        if classes == ("all",):
            chosen.append((row, "all"))
            continue
        if row.get("verdict") != "UNSAT":
            continue
        cls = classify_check(row.get("proof_check"))
        if cls in classes:
            chosen.append((row, cls))
    return chosen


def selection_counts(chosen):
    counts = {}
    for row, cls in chosen:
        counts.setdefault(row["solver"], {}).setdefault(cls, 0)
        counts[row["solver"]][cls] += 1
    return counts


# --------------------------------------------------------------------------
# Measurement


_active_process = None


def cgroup_oom_kills():
    try:
        rel = Path("/proc/self/cgroup").read_text().split("::", 1)[1].strip()
        text = (Path("/sys/fs/cgroup") / rel.lstrip("/") / "memory.events").read_text()
        match = re.search(r"^oom_kill (\d+)$", text, re.M)
        return int(match.group(1)) if match else None
    except (OSError, IndexError):
        return None


def group_rss_kb(pgid, proc=Path("/proc")):
    """Summed resident set of every live process in process group ``pgid``."""
    total = 0
    page_kb = os.sysconf("SC_PAGE_SIZE") // 1024
    for entry in os.scandir(proc):
        if not entry.name.isdigit():
            continue
        try:
            stat = Path(entry.path, "stat").read_text()
        except OSError:
            continue
        fields = stat.rsplit(")", 1)[1].split()
        # fields[0] is state (stat field 3); pgrp is field 5, rss field 24.
        if int(fields[2]) == pgid:
            total += int(fields[21]) * page_kb
    return total


def kill_group(process):
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()


def parse_time_metric(path):
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return {"peak_rss_kb": None, "term_signal": None}
    rss = re.search(r"^max_rss_kb=(\d+)$", text, re.M)
    sig = re.search(r"Command terminated by signal (\d+)", text)
    return {"peak_rss_kb": int(rss.group(1)) if rss else None,
            "term_signal": int(sig.group(1)) if sig else None}


def measured(cmd, stdout, stderr, timeout, metric, env=None, rss_cap_kb=None,
             poll_seconds=2.0, sampler=group_rss_kb):
    """Run one stage in its own session; deadline and optional RSS cap."""
    global _active_process
    wrapped = [str(fb.TIME), "-f", "max_rss_kb=%M", "-o", str(metric), "--",
               *map(str, cmd)]
    oom_before = cgroup_oom_kills()
    start = time.monotonic()
    deadline = start + timeout
    killed = None
    group_peak = 0
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        p = subprocess.Popen(wrapped, stdout=out, stderr=err,
                             start_new_session=True, env=env)
        _active_process = p
        try:
            while True:
                wait = max(0.01, min(poll_seconds, deadline - time.monotonic()))
                try:
                    p.wait(timeout=wait)
                    break
                except subprocess.TimeoutExpired:
                    pass
                try:
                    group = sampler(p.pid)
                except (OSError, ValueError, IndexError):
                    group = 0
                group_peak = max(group_peak, group)
                if rss_cap_kb is not None and group > rss_cap_kb:
                    killed = "memory-cap"
                    kill_group(p)
                    break
                if time.monotonic() >= deadline:
                    killed = "timeout"
                    kill_group(p)
                    break
        finally:
            _active_process = None
    oom_after = cgroup_oom_kills()
    record = {
        "exit_code": None if killed == "timeout" else p.returncode,
        "wall_seconds": round(time.monotonic() - start, 3),
        "timed_out": killed == "timeout",
        "killed_by_runner": killed,
        "group_rss_peak_kb_sampled": group_peak,
        "oom_kill_delta": (oom_after - oom_before
                           if oom_after is not None and oom_before is not None
                           else None),
    }
    record.update(parse_time_metric(metric))
    return record


# --------------------------------------------------------------------------
# Campaign layout helpers


def campaign_path(campaign, rel):
    rel = Path(rel)
    return rel if rel.is_absolute() else ROOT / rel


def scratch_root(campaign, man):
    scratch = man["certificate_policy"]["scratch"]
    if scratch["kind"] != "directory":
        raise SystemExit("unsupported scratch kind: " + scratch["kind"])
    root = scratch["root"]
    if root == "{campaign}/scratch":
        return campaign / "scratch"
    root = Path(root)
    if not root.is_absolute():
        raise SystemExit("scratch root must be absolute or {campaign}/scratch")
    return root / campaign.name


def cake_heap_mib(checker_memory_gib):
    heap = checker_memory_gib * 1024 - CAKE_STACK_MIB - CAKE_SLACK_MIB
    if heap < 4096:
        raise ValueError("checker memory too small for CakeML's default heap")
    return heap


def scheduler_time(solve, check):
    seconds = solve + check + MODEL_CHECK_SECONDS + CLEANUP_MARGIN_SECONDS
    seconds = ((seconds + 899) // 900) * 900
    return "%d:%02d:00" % (seconds // 3600, (seconds % 3600) // 60)


def verify_pins(man):
    for rel, digest in man["pin_sha256"].items():
        path = ROOT / rel
        if not path.is_file() or fb.sha256(path) != digest:
            raise SystemExit("pin mismatch: " + rel)


# --------------------------------------------------------------------------
# Prepare


def load_results(source):
    data = json.loads((source / "results.json").read_text())
    return data["results"] if isinstance(data, dict) else data


def repo_relative(source_man, source, rel):
    """Repository-relative path of a source-campaign pin."""
    if source_man.get("checker_pins_from") == "repository-relative":
        return str(rel)
    return str((source / rel).relative_to(ROOT))


def resolve_source_solver(source_man, source, name):
    solver = next((s for s in source_man["solvers"] if s["name"] == name), None)
    if solver is None:
        raise SystemExit("solver not in source campaign: " + name)
    solver = dict(solver)
    solver["executable"] = repo_relative(source_man, source, solver["executable"])
    solver.pop("sha256", None)
    return solver


def prepare(a):
    campaign = (ROOT / a.campaign).resolve() if not a.campaign.is_absolute() else a.campaign
    if campaign.exists() and any(campaign.iterdir()):
        raise SystemExit("campaign directory exists and is not empty: " + str(campaign))
    frozen_root = (ROOT / "benchmarks/baselines/sc24-26-32g-5000s").resolve()
    if campaign.resolve() == frozen_root or frozen_root in campaign.resolve().parents:
        raise SystemExit("refusing to write inside the frozen campaign")
    source = (ROOT / a.source).resolve() if not a.source.is_absolute() else a.source
    source_man_path = source / "manifest.json"
    source_man = json.loads(source_man_path.read_text())
    solvers = [s.strip() for s in a.solvers.split(",") if s.strip()]
    classes = tuple(c.strip() for c in a.classes.split(",") if c.strip())
    if classes != ("all",) and not set(classes) <= set(FAILURE_CLASSES):
        raise SystemExit("unknown class in --classes")

    # The frozen source has no per-solver executable root beyond its manifest;
    # a rerun source carries absolute-in-repo executables already.
    solver_records = {}
    for name in solvers:
        solver = resolve_source_solver(source_man, source, name)
        solver_records[name] = solver

    if classes == ("all",):
        results = None
        tasks = []
        for index, task in enumerate(source_man["tasks"]):
            solver = (source_man["solvers"][task["solver"]]["name"]
                      if isinstance(task["solver"], int) else task["solver"])
            if solver not in solvers:
                continue
            inst = (source_man["instances"][task["instance"]]
                    if "instance" in task else task)
            tasks.append({"rel": inst["rel"], "expected": inst["expected"],
                          "solver": solver, "source_task": index,
                          "source_class": "all"})
        results_sha = None
    else:
        results = load_results(source)
        chosen = selection_rows(results, set(solvers), classes)
        tasks = [{"rel": row["instance"], "expected": row["expected"],
                  "solver": row["solver"], "source_task": row["task"],
                  "source_class": cls,
                  "source_certificate_bytes": row.get("certificate_bytes")}
                 for row, cls in chosen]
        results_sha = fb.sha256(source / "results.json")
    if a.limit_tasks is not None:
        tasks = [t for t in tasks if t["source_task"] in set(a.limit_tasks)]
    if not tasks:
        raise SystemExit("selection is empty")
    missing = [t["rel"] for t in tasks if not (CACHE / Path(t["rel"]).name).is_file()]
    if missing:
        raise SystemExit(f"cache incomplete: {len(missing)} instances missing")

    campaign.mkdir(parents=True, exist_ok=True)
    pins = campaign / "pins"
    pins.mkdir()
    pin_sha = {}
    checkers = {}
    # Frozen DRAT/PBP/model checker pins are reused in place (read-only, hash
    # bound below).  A rerun source already records repository-relative paths.
    for name in ("drat-trim", "cake-lpr", "veripb", "cake-pb-cnf", "checkmodel",
                 "modelcheck.py"):
        checkers[name] = repo_relative(source_man, source,
                                       source_man["checkers"][name])
    dsr = None
    if any(solver_records[n]["certificate"] == "dsr" for n in solvers):
        if a.dsr_checker_profile != "long64-fixed":
            raise SystemExit("witnessed DSR reruns require --dsr-checker-profile long64-fixed")
        import experimental_tierd as et
        provenance = et.dsr_checker_provenance("long64-fixed")
        sources = {
            "dsr-trim": (et.FIXED_LONG_DSR / "dsr-trim", et.FIXED_LONG_DSR_HASHES["dsr-trim"]),
            "dsr-compress": (et.FIXED_LONG_DSR / "dsr-compress",
                             et.FIXED_LONG_DSR_HASHES["dsr-compress"]),
            "srcheck": (et.FROZEN_DSR / "srcheck", et.FROZEN_DSR_HASHES["srcheck"]),
        }
        for name, (src, digest) in sources.items():
            if fb.sha256(src) != digest:
                raise SystemExit("DSR checker pin mismatch: " + name)
            dst = pins / name
            fb.copy_pin(src, dst)
            dst.chmod(0o555)
            checkers[name] = str(dst.relative_to(ROOT))
        dsr = {"profile": "long64-fixed", "provenance": provenance,
               "pipeline": "dsr-trim(long64-fixed) -> dsr-compress -d -> srcheck text",
               "checker_cnf": "proof_oracle.canonical_checker_cnf for dsr-trim and srcheck",
               "reason": ("frozen amendment-1 used Satsuma's bundled 32-bit dsr-trim, "
                          "whose capacity growth overflows on multi-GB proofs; see "
                          "benchmarks/reports/2026-10-08-dsr-checker-width-and-trust-boundary.md")}
    runner = campaign / "runner"
    runner.mkdir()
    here = Path(__file__).resolve().parent
    for name in RUNNER_FILES:
        fb.copy_pin(here / name, runner / name)
        (runner / name).chmod(0o444)
    for name, rel in checkers.items():
        pin_sha[rel] = fb.sha256(ROOT / rel)
    for solver in solver_records.values():
        solver["sha256"] = fb.sha256(ROOT / solver["executable"])
        pin_sha[solver["executable"]] = solver["sha256"]
    for name in RUNNER_FILES:
        pin_sha[str((runner / name).relative_to(ROOT))] = fb.sha256(runner / name)

    checker_gib = a.checker_memory_gib or a.mem_gib
    solver_gib = a.solver_memory_gib or a.mem_gib
    if solver_gib > a.mem_gib or checker_gib > a.mem_gib:
        raise SystemExit("process limits cannot exceed the Slurm allocation")
    if a.scratch_root != "{campaign}/scratch" and not Path(a.scratch_root).is_absolute():
        raise SystemExit("--scratch-root must be absolute or {campaign}/scratch")
    manifest = {
        "schema": SCHEMA, "campaign": campaign.name,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "once_only": True, "requeue": False,
        "reason": a.reason, "user_instruction": a.instruction,
        "source": {"campaign": str(source.relative_to(ROOT)),
                   "manifest_sha256": fb.sha256(source_man_path),
                   "results_sha256": results_sha},
        "selection": {
            "solvers": solvers, "classes": list(classes),
            "rule": ("every source task of the named solvers" if classes == ("all",)
                     else "source rows with verdict UNSAT whose proof_check failure class "
                          "(baseline_rerun.classify_check) is in classes"),
            "counts": selection_counts([({"solver": t["solver"]}, t["source_class"])
                                        for t in tasks]),
            "limit_tasks": a.limit_tasks,
        },
        "solvers": [solver_records[n] for n in solvers],
        "checkers": checkers, "checker_pins_from": "repository-relative",
        "dsr_route": dsr,
        "pin_sha256": pin_sha,
        "limits": {
            "solve_wall_seconds": a.solve_seconds,
            "checker_pipeline_wall_seconds": a.check_seconds,
            "model_check_wall_seconds": MODEL_CHECK_SECONDS,
            "scheduler_memory_gib": a.mem_gib,
            "memory_enforcement": "slurm_task_cgroup",
            "solver_memory_gib": solver_gib,
            "solver_memory_enforcement": (
                "slurm_task_cgroup" if solver_gib == a.mem_gib else
                "process_group_rss_watchdog (2 s samples of summed /proc RSS; "
                "SIGTERM then SIGKILL above the cap); no RLIMIT_AS"),
            "checker_memory_gib": checker_gib,
            "cake_heap_mib": cake_heap_mib(checker_gib),
        },
        "slurm": {"partition": a.partition, "cpus_per_task": a.cpus,
                  "memory_gib": a.mem_gib,
                  "time": scheduler_time(a.solve_seconds, a.check_seconds),
                  "array_throttle": a.throttle,
                  "array_throttle_reason": a.throttle_reason},
        "certificate_policy": {
            "retention": "none",
            "result_publication": "pending-journal-then-atomic-promotion",
            "stage_cleanup": "delete-input-after-consuming-checker-stage",
            "terminal_cleanup": "bounded-nfs-retry-before-result-publication",
            "scratch": {"kind": "directory", "root": a.scratch_root},
            "child_tmpdir": "task scratch directory",
            "partial_unknown_proofs": "size recorded, not hashed",
        },
        "tasks": tasks,
        "runner": {"path": str(runner.relative_to(ROOT)),
                   "repository_commit": subprocess.run(
                       ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                       capture_output=True, text=True).stdout.strip()},
    }
    fb.atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    print(json.dumps({"campaign": str(campaign), "tasks": len(tasks),
                      "counts": manifest["selection"]["counts"]}, indent=2))


# --------------------------------------------------------------------------
# Run one task


def check_proof(man, solver, cnf, proof, work, env):
    limits = man["limits"]
    check_limit = limits["checker_pipeline_wall_seconds"]
    fmt = solver["certificate"]
    ck = {name: ROOT / rel for name, rel in man["checkers"].items()}
    canonical = None
    if fmt == "drat":
        lrat = work / "proof.lrat"
        specs = [("drat-trim", [ck["drat-trim"], cnf, proof, "-L", lrat], lrat),
                 ("cake-lpr", [ck["cake-lpr"], cnf, lrat], None)]
    elif fmt == "dsr":
        checker_cnf = work / "checker.cnf"
        canonical = proof_oracle.canonical_checker_cnf(cnf, checker_cnf)
        binary_lsr, text_lsr = work / "proof.binary.lsr", work / "proof.lsr"
        stages = proof_oracle.reference_dsr_stages(
            ck["dsr-trim"], ck["dsr-compress"], ck["srcheck"], cnf, proof,
            binary_lsr, text_lsr, checker_cnf=checker_cnf)
        specs = []
        for name, command in stages:
            produced = Path(command[-1]) if name in ("dsr-trim", "dsr-compress") else None
            specs.append((name, command, produced))
    else:
        kernel = work / "proof.kernel.pbp"
        specs = [("veripb", [ck["veripb"], "-u", "--cnf", "--elaborate", kernel,
                             cnf, proof], kernel),
                 ("cake-pb-cnf", [ck["cake-pb-cnf"], cnf, kernel], None)]
    consumed = [Path(proof)] + [spec[2] for spec in specs[:-1]]
    started = time.monotonic()
    stages_out, derived = [], []
    for number, (name, cmd, produced) in enumerate(specs, 1):
        stdout, stderr = work / f"checker-{number}.stdout", work / f"checker-{number}.stderr"
        remaining = max(1, check_limit - int(time.monotonic() - started))
        stage_env = dict(env)
        environment = {}
        if name in ("cake-lpr", "cake-pb-cnf"):
            stage_env["CML_HEAP_SIZE"] = str(limits["cake_heap_mib"])
            environment = {"CML_HEAP_SIZE_MIB": limits["cake_heap_mib"]}
        rec = measured(cmd, stdout, stderr, remaining, work / f"checker-{number}.time",
                       stage_env)
        rec.update({
            "name": name, "environment": environment,
            "command": [str(c.relative_to(work)) if isinstance(c, Path) and work in c.parents
                        else str(c) for c in cmd],
            "pin_sha256": man["pin_sha256"].get(man["checkers"].get(name, ""), None),
            "result": "verified" if fb.checker_ok(name, rec["exit_code"], stdout, stderr)
                      else "not-verified",
            "stdout_tail": fb.tail(stdout), "stderr_tail": fb.tail(stderr),
        })
        stages_out.append(rec)
        if produced is not None and produced.is_file():
            derived.append({"path": produced.name, "bytes": produced.stat().st_size,
                            "deleted": False})
        source = consumed[number - 1]
        rec["input_artifact_deleted"] = True
        if source.exists():
            source.unlink()
        for item in derived:
            if item["path"] == source.name:
                item["deleted"] = True
        if rec["result"] != "verified":
            if produced is not None and produced.exists():
                produced.unlink()
                for item in derived:
                    if item["path"] == produced.name:
                        item["deleted"] = True
            break
    check = {
        "pipeline": ("dsr-trim(long64-fixed) -> dsr-compress -d -> srcheck"
                     if fmt == "dsr" else solver["checker_pipeline"]),
        "result": "verified" if len(stages_out) == len(specs) and all(
            s["result"] == "verified" for s in stages_out) else "not-verified",
        "wall_seconds": round(sum(s["wall_seconds"] for s in stages_out), 3),
        "peak_rss_kb": max((s["peak_rss_kb"] or 0 for s in stages_out), default=0),
        "timed_out": any(s["timed_out"] for s in stages_out),
        "stages": stages_out, "derived_artifacts": derived,
        "checker_cnf": canonical,
    }
    check["failure_class"] = classify_check(check)
    return check, [spec[2] for spec in specs if spec[2] is not None]


class Interrupted(BaseException):
    pass


def _on_sigterm(signum, _frame):
    process = _active_process
    if process is not None:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    raise Interrupted(signum)


def run_one(campaign, index):
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    if man.get("schema") != SCHEMA or man["certificate_policy"]["retention"] != "none":
        raise SystemExit("not a zero-retention rerun campaign")
    verify_pins(man)
    task = man["tasks"][index]
    limits = man["limits"]
    solver = next(s for s in man["solvers"] if s["name"] == task["solver"])
    result_path = campaign / "results" / f"{index:05d}.json"
    pending_path = campaign / "pending" / f"{index:05d}.json"
    fb.atomic_json(campaign / "claims" / f"{index:05d}.json",
                   {"task": index, "host": os.uname().nodename,
                    "slurm_job": os.environ.get("SLURM_JOB_ID"),
                    "started_utc": dt.datetime.now(dt.timezone.utc).isoformat()},
                   exclusive=True)
    if result_path.exists():
        raise SystemExit("result already exists after exclusive claim; refusing rerun")
    work = scratch_root(campaign, man) / f"{index:05d}"
    work.mkdir(parents=True, exist_ok=False)
    previous = signal.signal(signal.SIGTERM, _on_sigterm)
    result = {"task": index, "solver": solver["name"], "instance": task["rel"],
              "expected": task["expected"], "source_task": task["source_task"],
              "source_class": task["source_class"],
              "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
              "slurm_job": os.environ.get("SLURM_JOB_ID"),
              "certificate_format": solver["certificate"]}
    try:
        (work / "tmp").mkdir()
        env = dict(os.environ)
        env["TMPDIR"] = str(work / "tmp")
        cnf = work / "instance.cnf"
        with open(cnf, "wb") as out:
            subprocess.run(["xz", "-dc", CACHE / Path(task["rel"]).name],
                           stdout=out, check=True)
        stdout, stderr = work / "solver.stdout", work / "solver.stderr"
        proof_dir = work / "proof-dir"
        proof_dir.mkdir()
        proof = proof_dir / "proof.out"
        executable = ROOT / solver["executable"]
        cmd = ([executable, cnf, proof] if solver["kind"] == "positional"
               else [executable, cnf, proof_dir])
        cap = (limits["solver_memory_gib"] << 20
               if limits["solver_memory_gib"] < limits["scheduler_memory_gib"] else None)
        solve = measured(cmd, stdout, stderr, limits["solve_wall_seconds"],
                         work / "solve.time", env, rss_cap_kb=cap)
        answer = fb.verdict(solve["exit_code"], stdout)
        if solve["killed_by_runner"] == "memory-cap":
            answer = "UNKNOWN"
        result.update({
            "verdict": answer,
            "expected_match": task["expected"] == "UNKNOWN" or answer == task["expected"],
            "solve": solve, "certificate_bytes": None, "certificate_sha256": None,
            "proof_check": None, "proof_ok": None, "model_check": None,
            "model_ok": None, "vbs_eligible": solver["vbs_eligible"],
        })
        derived = []
        if answer == "UNSAT":
            if proof.is_file():
                result["certificate_bytes"] = proof.stat().st_size
                result["certificate_sha256"] = fb.sha256(proof)
                result["proof_check"], derived = check_proof(
                    man, solver, cnf, proof, work, env)
                result["proof_ok"] = result["proof_check"]["result"] == "verified"
            else:
                result["proof_ok"] = False
        elif answer == "SAT":
            result["certificate_bytes"] = stdout.stat().st_size
            result["certificate_sha256"] = fb.sha256(stdout)
            mc = measured([ROOT / man["checkers"]["checkmodel"], cnf, stdout],
                          work / "model.stdout", work / "model.stderr",
                          limits["model_check_wall_seconds"], work / "model.time", env)
            result["model_protocol_ok"] = fb.sat_output_protocol_ok(stdout)
            result["model_ok"] = mc["exit_code"] == 0 and result["model_protocol_ok"]
            mc["result"] = "verified" if result["model_ok"] else "not-verified"
            mc["stdout_tail"] = fb.tail(work / "model.stdout")
            mc["stderr_tail"] = fb.tail(work / "model.stderr")
            result["model_check"] = mc
        elif proof.is_file():
            result["certificate_bytes"] = proof.stat().st_size
        valid = (answer == "SAT" and result["model_ok"] is True or
                 answer == "UNSAT" and result["proof_ok"] is True)
        result["valid"] = bool(valid and result["expected_match"])
        result["solver_stdout_tail"] = fb.tail(stdout)
        result["solver_stderr_tail"] = fb.tail(stderr)
        result["cleanup_pending"] = True
        fb.atomic_json(pending_path, result, exclusive=True)
        for path in [proof, stdout, *derived]:
            if path.exists():
                path.unlink()
        result["artifact_deleted"] = not proof.exists()
        result["derived_artifacts_deleted"] = all(not p.exists() for p in derived)
        result["cleanup_retries"] = fb.remove_tree_nfs_safe(work)
        result["scratch_deleted"] = not work.exists()
        result["cleanup_pending"] = False
        fb.promote_pending_result(pending_path, result_path, result)
        return 0 if result["valid"] or answer == "UNKNOWN" else 1
    except Interrupted:
        result.update({"outcome": "INTERRUPTED_SIGTERM", "valid": False,
                       "completion": "incomplete"})
        return_code = 3
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        if work.exists():
            try:
                fb.remove_tree_nfs_safe(work)
            except OSError:
                pass
        signal.signal(signal.SIGTERM, previous)
    result["scratch_deleted"] = not work.exists()
    if not result_path.exists():
        fb.atomic_json(result_path, result)
    return return_code


# --------------------------------------------------------------------------
# Aggregate / submit


def outcome_class(row):
    if row.get("outcome"):
        return row["outcome"]
    verdict = row.get("verdict")
    if verdict == "SAT":
        return "SAT-valid" if row.get("valid") else "SAT-invalid"
    if verdict == "UNSAT":
        if row.get("valid"):
            return "UNSAT-verified"
        return "UNSAT-" + str(classify_check(row.get("proof_check")) or "no-proof")
    solve = row.get("solve") or {}
    if solve.get("killed_by_runner") == "memory-cap":
        return "UNKNOWN-solver-memory-cap"
    if solve.get("timed_out"):
        return "UNKNOWN-solve-timeout"
    return "UNKNOWN-other"


def aggregate(campaign):
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    results, missing = [], []
    for i in range(len(man["tasks"])):
        path = campaign / "results" / f"{i:05d}.json"
        if path.exists():
            results.append(json.loads(path.read_text()))
        else:
            missing.append(i)
    claimed = {int(p.stem) for p in (campaign / "claims").glob("*.json")}
    pending = sorted(int(p.stem) for p in (campaign / "pending").glob("*.json"))
    root = scratch_root(campaign, man)
    leftovers = sorted(p.name for p in root.iterdir()) if root.exists() else []
    summary = {}
    transitions = {}
    for row in results:
        cls = outcome_class(row)
        summary.setdefault(row["solver"], {}).setdefault(cls, 0)
        summary[row["solver"]][cls] += 1
        key = f"{row.get('source_class')} -> {cls}"
        transitions.setdefault(row["solver"], {}).setdefault(key, 0)
        transitions[row["solver"]][key] += 1
    out = {"schema": SCHEMA, "manifest_sha256": fb.sha256(campaign / "manifest.json"),
           "complete": not missing, "task_count": len(man["tasks"]),
           "result_count": len(results), "missing_tasks": missing,
           "interrupted_claims": sorted(claimed - {r["task"] for r in results}),
           "pending_cleanup_tasks": pending, "scratch_leftovers": leftovers,
           "summary": summary, "source_class_transitions": transitions,
           "invalid": sum(r.get("valid") is False for r in results),
           "results": results}
    fb.atomic_json(campaign / "results.json", out)
    print(json.dumps({k: v for k, v in out.items() if k != "results"}, indent=2))
    return 0 if out["complete"] else 2


def submit(campaign, dependency=None, governor_jobs=None):
    campaign = campaign.resolve()
    man_path, launch = campaign / "manifest.json", campaign / "launch.json"
    if launch.exists():
        raise SystemExit("campaign already submitted")
    man = json.loads(man_path.read_text())
    verify_pins(man)
    count = len(man["tasks"])
    slurm = man["slurm"]
    runner = ROOT / man["runner"]["path"] / "baseline_rerun.py"
    throttle = f"%{slurm['array_throttle']}" if slurm.get("array_throttle") else ""
    (campaign / "logs").mkdir(exist_ok=True)
    array = campaign / "array.sbatch"
    agg = campaign / "aggregate.sbatch"
    py = sys.executable
    array.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={slurm['partition']}
#SBATCH --job-name={man['campaign'][:40]}
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task={slurm['cpus_per_task']}
#SBATCH --mem={slurm['memory_gib']}G
#SBATCH --time={slurm['time']}
#SBATCH --array=0-{count - 1}{throttle}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
export TACTIX_REPO_ROOT={ROOT}
export PYTHONDONTWRITEBYTECODE=1
exec {py} {runner} --campaign {campaign} --run-one "$SLURM_ARRAY_TASK_ID"
""")
    agg.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={slurm['partition']}
#SBATCH --job-name={man['campaign'][:30]}-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/aggregate.log
export TACTIX_REPO_ROOT={ROOT}
export PYTHONDONTWRITEBYTECODE=1
exec {py} {runner} --campaign {campaign} --aggregate
""")
    cmd = ["sbatch", "--parsable"]
    if dependency:
        cmd.append(f"--dependency={dependency}")
    a = subprocess.run([*cmd, array], capture_output=True, text=True,
                       check=True).stdout.strip().split(";")[0]
    g = subprocess.run(["sbatch", "--parsable", f"--dependency=afterany:{a}", agg],
                       capture_output=True, text=True, check=True).stdout.strip().split(";")[0]
    record = {"array_job_id": a, "aggregate_job_id": g, "tasks": count,
              "array_throttle": slurm.get("array_throttle"),
              "array_throttle_reason": slurm.get("array_throttle_reason"),
              "dependency": dependency,
              "manifest_sha256": fb.sha256(man_path),
              "array_sbatch_sha256": fb.sha256(array),
              "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat()}
    fb.atomic_json(launch, record, exclusive=True)
    if governor_jobs:
        register_with_governor(Path(governor_jobs), [a], man["campaign"])
    print(json.dumps(record, indent=2))


def register_with_governor(jobs_file, job_ids, label):
    data = json.loads(jobs_file.read_text()) if jobs_file.exists() else {
        "jobs": [], "expect_more": True}
    for job in job_ids:
        if job not in [j["id"] for j in data["jobs"]]:
            data["jobs"].append({"id": str(job), "label": label})
    fb.atomic_json(jobs_file, data)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--campaign", type=Path)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--select", action="store_true",
                       help="print the selection and counts; writes nothing")
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--run-one", type=int)
    group.add_argument("--aggregate", action="store_true")
    group.add_argument("--submit", action="store_true")
    ap.add_argument("--source", type=Path,
                    default=Path("benchmarks/baselines/sc24-26-32g-5000s"))
    ap.add_argument("--solvers", default="")
    ap.add_argument("--classes", default="timeout,memout",
                    help="'all' or a comma list of " + ",".join(FAILURE_CLASSES))
    ap.add_argument("--limit-tasks", type=int, nargs="*",
                    help="restrict to these source task ids (smoke campaigns)")
    ap.add_argument("--solve-seconds", type=int, default=5000)
    ap.add_argument("--check-seconds", type=int, default=28800)
    ap.add_argument("--cpus", type=int, default=4)
    ap.add_argument("--mem-gib", type=int, default=32)
    ap.add_argument("--solver-memory-gib", type=int)
    ap.add_argument("--checker-memory-gib", type=int)
    ap.add_argument("--partition", default="cpu-q")
    ap.add_argument("--throttle", type=int)
    ap.add_argument("--throttle-reason")
    ap.add_argument("--dsr-checker-profile", default="long64-fixed",
                    choices=["long64-fixed"])
    ap.add_argument("--scratch-root", default="{campaign}/scratch")
    ap.add_argument("--reason", default="")
    ap.add_argument("--instruction", default="")
    ap.add_argument("--dependency")
    ap.add_argument("--governor-jobs", type=Path)
    a = ap.parse_args(argv)
    if a.throttle and not a.throttle_reason:
        ap.error("--throttle requires --throttle-reason")
    if a.select:
        source = (ROOT / a.source).resolve()
        solvers = {s for s in a.solvers.split(",") if s}
        classes = tuple(a.classes.split(","))
        chosen = selection_rows(load_results(source), solvers, classes)
        for row, cls in chosen:
            stage = (row.get("proof_check") or {}).get("stages", [{}])[-1]
            print(row["task"], row["solver"], cls, stage.get("name"),
                  stage.get("exit_code"), row.get("certificate_bytes"),
                  Path(row["instance"]).name)
        print(json.dumps(selection_counts(chosen), indent=2, sort_keys=True))
        return 0
    if a.campaign is None:
        ap.error("--campaign is required")
    if a.prepare:
        return prepare(a)
    if a.run_one is not None:
        return run_one(a.campaign, a.run_one)
    if a.aggregate:
        return aggregate(a.campaign)
    return submit(a.campaign, a.dependency, a.governor_jobs)


if __name__ == "__main__":
    sys.exit(main() or 0)

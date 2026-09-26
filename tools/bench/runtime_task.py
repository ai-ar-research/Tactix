#!/usr/bin/env python3
"""Checked proof-on comparison: one immutable solver per SLURM task."""

from collections import deque
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

from proof_oracle import (bridge_trivial_input_binary_lsr,
                          bridge_trivial_input_lsr, dsr_compress_status,
                          proof_check_status, reference_dsr_stages,
                          certificate_route, drat_stages, pb_stages,
                          dsr_proof_as_drat, stage_environment,
                          canonical_checker_cnf, bridge_trivial_input_lrat,
                          drat_command_with_timeout, DSRFormatError,
                          restate_duplicate_input_lrat)


CAMPAIGN = None
ROOT = None


def positive_timeout(manifest, name):
    value = manifest.get("resource_contract", {}).get(name)
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise RuntimeError(f"invalid resource timeout: {name}")
    return value


def checker_timeout_for(manifest, instance):
    """The checker window for one instance: the contract's checker_timeout,
    unless resource_contract.checker_timeout_overrides names the instance's
    basename with a shorter positive window. Solve results never depend on
    it; the override only stops a known non-verifying certificate from
    holding a node for the full window."""
    timeout = positive_timeout(manifest, "checker_timeout")
    overrides = manifest.get("resource_contract", {}).get(
        "checker_timeout_overrides") or {}
    value = overrides.get(instance.rsplit("/", 1)[-1])
    if value is None:
        return timeout
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise RuntimeError("invalid checker_timeout_overrides entry")
    return min(timeout, value)


def solver_command(binary, arm, cnf, proof):
    before = arm.get("arguments", [])
    after = arm.get("trailing_arguments", [])
    if (not isinstance(before, list) or not isinstance(after, list) or
            any(not isinstance(arg, str) for arg in before + after)):
        raise RuntimeError("solver arguments must be string lists")
    return [binary, *before, cnf, proof, *after]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def process_group():
    os.setsid()


def sample_vm_peak(root, stop, peak):
    """Sample the process tree; VmPeak is a per-process high-water mark."""
    while not stop.is_set():
        pending, seen = [root], set()
        while pending:
            pid = pending.pop()
            if pid in seen:
                continue
            seen.add(pid)
            try:
                children = Path(f"/proc/{pid}/task/{pid}/children").read_text()
                pending.extend(map(int, children.split()))
                for line in Path(f"/proc/{pid}/status").read_text().splitlines():
                    if line.startswith("VmPeak:"):
                        peak[0] = max(peak[0], int(line.split()[1]))
            except (OSError, ValueError):
                pass
        stop.wait(0.1)


def runtime_context(affinity):
    """Record placement and aggregate occupancy without other jobs' identities."""
    topology = []
    for cpu in affinity:
        base = Path(f"/sys/devices/system/cpu/cpu{cpu}")
        row = {"cpu": cpu}
        for key, relative in (("core", "topology/core_id"),
                              ("socket", "topology/physical_package_id"),
                              ("l3_shared_cpus", "cache/index3/shared_cpu_list")):
            try:
                row[key] = (base / relative).read_text().strip()
            except OSError:
                row[key] = None
        topology.append(row)
    try:
        occupancy = subprocess.run(
            ["squeue", "-h", "-w", os.environ.get("SLURMD_NODENAME", os.uname().nodename),
             "-o", "%P|%T|%C"], capture_output=True, text=True, timeout=15)
        occupancy = dict(exit_code=occupancy.returncode, rows=occupancy.stdout.splitlines(),
                         stderr=occupancy.stderr[-2000:])
    except (OSError, subprocess.TimeoutExpired) as error:
        occupancy = {"error": repr(error)}
    return dict(topology=topology, occupancy=occupancy,
                observed_at=dt.datetime.now(dt.timezone.utc).isoformat())


def measured(command, stdout, stderr, metrics, timeout, environment=None):
    wrapped = ["/usr/bin/time", "-f", "max_rss_kb=%M", "-o", str(metrics),
               "--", *map(str, command)]
    started = time.monotonic()
    timed_out = False
    vm_peak = [0]
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        process = subprocess.Popen(wrapped, stdout=out, stderr=err,
                                   preexec_fn=process_group,
                                   env=environment)
        stop = threading.Event()
        sampler = None
        if os.environ.get("TACTIX_RUNTIME_SAMPLE_VM") == "1":
            sampler = threading.Thread(target=sample_vm_peak,
                                       args=(process.pid, stop, vm_peak), daemon=True)
            sampler.start()
        try:
            exit_code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            exit_code = None
        finally:
            stop.set()
            if sampler:
                sampler.join()
    peak_rss_kb = None
    try:
        match = re.search(r"^max_rss_kb=(\d+)$", metrics.read_text(), re.M)
        peak_rss_kb = int(match.group(1)) if match else None
    except OSError:
        pass
    return {
        "exit_code": exit_code,
        "wall_seconds": round(time.monotonic() - started, 3),
        "peak_rss_kb": peak_rss_kb,
        "sampled_process_vm_peak_kb": vm_peak[0] or None,
        "timed_out": timed_out,
    }


def tail(path, count=40):
    lines = deque(maxlen=count)
    try:
        with open(path, errors="replace") as source:
            for line in source:
                lines.append(line.rstrip("\n")[-4096:])
    except OSError:
        pass
    return list(lines)


def solver_summary(path):
    states = []
    selected = deque(maxlen=260)
    with open(path, errors="replace") as source:
        for raw in source:
            line = raw.rstrip("\n")
            match = re.match(r"^s\s+(SATISFIABLE|UNSATISFIABLE)\s*$", line)
            if match:
                states.append(match.group(1))
            if line.startswith(("c ", "s ")):
                selected.append(line[-4096:])
    verdict = "UNKNOWN"
    if len(states) == 1:
        verdict = "SAT" if states[0] == "SATISFIABLE" else "UNSAT"
    return verdict, list(selected)


def atomic_json(path, value):
    # Focused campaign preparation should normally create every output
    # directory, but a task must not lose its terminal record merely because
    # an empty results/provisional directory was omitted from a frozen bundle.
    # Creating the parent is safe under array concurrency and leaves all
    # immutable inputs untouched.
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with open(temporary, "x") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
    os.replace(temporary, path)


def expected_matches(expected, verdict):
    """Unknown dataset labels do not replace independent verdict checking."""
    return verdict in ("SAT", "UNSAT") and expected in (verdict, "UNKNOWN")


def checked_verdict_valid(expected, verdict, solve, certificate_valid):
    return bool(certificate_valid and expected_matches(expected, verdict) and
                not solve["timed_out"] and
                solve["exit_code"] == (10 if verdict == "SAT" else 20))


def validate_runtime_pin_contract(campaign, manifest):
    """Reject an incomplete checker bundle before spending solver time."""
    required = {"pins/checkmodel", "pins/modelcheck.py"}
    for arm in manifest.get("arms", {}).values():
        required.add(arm["path"])
        certificate = arm.get("certificate", "drat")
        if certificate == "drat":
            required.add("pins/drat-trim")
        elif certificate in ("dsr", "auto"):
            required.update({"pins/dsr-trim", "pins/dsr-compress",
                             "pins/srcheck", "pins/drat-trim",
                             "pins/cake_lpr"})
            if certificate == "auto":
                required.update({"pins/veripb", "pins/cake-pb-cnf"})
        else:
            raise RuntimeError(f"unsupported proof certificate: {certificate}")
    if manifest.get("scratch_policy") == "anonymous-local-files-v1":
        required.add("pins/anonymous_scratch.py")
    hashes = manifest.get("pin_sha256", {})
    missing = sorted(rel for rel in required if rel not in hashes)
    if missing:
        raise RuntimeError(f"runtime pin contract omits {missing}")
    absent = sorted(rel for rel in required if not (campaign / rel).is_file())
    if absent:
        raise RuntimeError(f"runtime pin files missing {absent}")


def proof_stage(scratch, name, command, timeout, produced=None,
                *, input_has_empty_clause=None, trivial_lrat_spelling=None):
    """Run one proof stage and require its exact oracle/output contract."""
    stdout = scratch / f"{name}.stdout"
    stderr = scratch / f"{name}.stderr"
    if name == 'drat-trim':
        command = drat_command_with_timeout(command, timeout)
    result = measured(command, stdout, stderr, scratch / f"{name}.time", timeout,
                      environment=stage_environment(name))
    output = "\n".join(tail(stdout, 100) + tail(stderr, 100))
    if name not in ("drat-trim", "dsr-trim", "dsr-compress", "srcheck",
                    "cake_lpr", "veripb", "cake-pb-cnf"):
        raise RuntimeError(f"unsupported proof stage: {name}")
    if name == "dsr-compress":
        status = dsr_compress_status(result["exit_code"], output)
    else:
        status = proof_check_status(name, result["exit_code"], output, "",
                                    input_has_empty_clause=input_has_empty_clause)
    if (name == 'drat-trim' and status == 'verified' and produced is not None
            and trivial_lrat_spelling is not None):
        result['input_empty_lrat_bridge'] = bridge_trivial_input_lrat(
            trivial_lrat_spelling, produced, result['exit_code'], output)
    if produced is not None and (not produced.is_file() or produced.stat().st_size == 0):
        status = "error"
    result.update(name=name, result=status, output_tail=output[-12000:],
                  command=list(map(str, command)), timeout_seconds=timeout)
    if produced is not None:
        result["produced_bytes"] = (
            produced.stat().st_size if produced.is_file() else None
        )
    return result


def check_unsat_proof(campaign, certificate, cnf, proof, scratch, timeout,
                      require_canonical_dsr=False):
    """Check one certificate, stopping at the first failed pipeline stage."""
    started = time.monotonic()
    stages = []
    checker_cnf = scratch / 'checker.cnf'
    spelling = canonical_checker_cnf(cnf, checker_cnf)

    def remaining():
        return max(1, timeout - int(time.monotonic() - started))

    def append_canonical_dsr():
        # One canonical no-option route for witnessed and witness-free DSR.
        binary_lsr = scratch / 'proof.binary.lsr'
        text_lsr = scratch / 'proof.lsr'
        for name, command in reference_dsr_stages(
                campaign / 'pins/dsr-trim', campaign / 'pins/dsr-compress',
                campaign / 'pins/srcheck', cnf, proof, binary_lsr, text_lsr,
                checker_cnf=checker_cnf):
            made = command[-1] if name != 'srcheck' else None
            stages.append(proof_stage(scratch, name, command, remaining(), made))
            if stages[-1]['result'] != 'verified':
                break
            if name == 'dsr-trim':
                (bridge_trivial_input_binary_lsr if made == binary_lsr
                 else bridge_trivial_input_lsr)(cnf, made)

    try:
        route = certificate_route(proof) if certificate in ("dsr", "auto") else None
    except DSRFormatError as error:
        stage = dict(name='route', result='rejected', exit_code=None,
                     timed_out=False, output_tail=str(error))
        return dict(pipeline='certificate-routing', checker_input=spelling,
                    result='not-verified',
                    wall_seconds=round(time.monotonic() - started, 3),
                    exit_code=None, timed_out=False, output_tail=str(error),
                    stages=[stage])
    if certificate == "dsr" and route == "pbp":
        raise RuntimeError("explicit DSR arm emitted a PBP certificate")
    if certificate == "drat":
        stages.append(proof_stage(
            scratch, "drat-trim",
            [campaign / "pins/drat-trim", checker_cnf, proof], remaining(),
            input_has_empty_clause=spelling['first_empty_clause'] is not None))
        pipeline = "drat-trim"
    elif certificate == "auto" and route == "pbp":
        kernel = scratch / "proof.kernel.pbp"
        if kernel.exists():
            # AnonymousScratch returns an already-open /proc/PID/fd/N path:
            # it can be truncated but has no unlinkable directory entry.
            # Clear stale elaboration in both scratch implementations.
            with kernel.open("wb"):
                pass
        for name, command in pb_stages(
                campaign / "pins/veripb", campaign / "pins/cake-pb-cnf",
                cnf, proof, kernel, checker_cnf=checker_cnf):
            made = kernel if name == "veripb" else None
            stages.append(proof_stage(scratch, name, command, remaining(), made))
            if stages[-1]["result"] != "verified":
                break
        pipeline = "veripb-cake-pb"
    elif certificate in ("dsr", "auto") and route == "drat":
        # Preserve the content-routed DRAT check and its independent LRAT
        # negative controls. Formal DSR gates additionally require the
        # canonical DSR chain below before accepting witness-free proofs.
        drat, lrat = scratch / "proof.drat", scratch / "proof.lrat"
        if not dsr_proof_as_drat(proof, drat):
            raise RuntimeError("routing saw a witnessed line")
        checked_lrat = lrat
        for name, command in drat_stages(
                campaign / "pins/drat-trim", campaign / "pins/cake_lpr",
                cnf, drat, lrat, checker_cnf=checker_cnf):
            if name == 'cake_lpr':
                command = [*command[:-1], checked_lrat]
            made = lrat if name == "drat-trim" else None
            stages.append(proof_stage(scratch, name, command, remaining(),
                                      made, input_has_empty_clause=
                                      spelling['first_empty_clause'] is not None,
                                      trivial_lrat_spelling=spelling))
            if stages[-1]["result"] != "verified":
                break
            if name == 'drat-trim':
                try:
                    checked_lrat, restatements = restate_duplicate_input_lrat(
                        checker_cnf, spelling, lrat, scratch / 'proof.original-input.lrat')
                    stages[-1]['input_restatements'] = restatements
                except ValueError as error:
                    stages.append(dict(name='input-restatement', result='rejected',
                                       exit_code=None, timed_out=False, output_tail=str(error)))
                    break
        pipeline = "witness-free-dsr-as-drat:drat-trim-cake_lpr"
        if (require_canonical_dsr and certificate == 'dsr' and
                all(stage['result'] == 'verified' for stage in stages)):
            append_canonical_dsr()
            pipeline += '+dsr-trim-dsr-compress-trestle'
    elif certificate in ("dsr", "auto"):
        append_canonical_dsr()
        pipeline = "dsr-trim-dsr-compress-trestle"
    else:
        raise RuntimeError(f"unsupported proof certificate: {certificate}")
    return {
        "pipeline": pipeline,
        "checker_input": spelling,
        "result": ("verified" if stages and
                   all(stage["result"] == "verified" for stage in stages)
                   else "not-verified"),
        "wall_seconds": round(time.monotonic() - started, 3),
        "exit_code": stages[-1]["exit_code"] if stages else None,
        "timed_out": any(stage["timed_out"] for stage in stages),
        "output_tail": "\n".join(
            stage["output_tail"] for stage in stages
        )[-12000:],
        "stages": stages,
    }


def provisional_result(result, recorded_at):
    """Publish bounded solver diagnostics now, without inheriting validity.

    Proof checking can take much longer than solving. Keeping stderr only in
    the final record unnecessarily hides stage timers during that interval.
    The bounded tail has already been read from the completed solver output.
    """
    fields = ("task", "rank", "arm", "tier", "instance", "expected", "verdict",
              "expected_match", "solve", "node", "certificate_bytes",
              "certificate_sha256", "solver_statistics", "solver_stderr_tail")
    return dict({key: result[key] for key in fields},
                schema="tactix-matched-slurm-provisional-result-v1",
                validation_pending=True, recorded_at=recorded_at)


def main():
    global CAMPAIGN, ROOT
    if len(sys.argv) != 3 or not os.environ.get("SLURM_JOB_ID"):
        raise SystemExit("usage (SLURM only): runtime_task.py CAMPAIGN TASK")
    CAMPAIGN = Path(sys.argv[1]).resolve()
    ROOT = CAMPAIGN.parents[2]
    task = int(sys.argv[2])
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    if task < 0 or task >= len(rows) or int(rows[task]["task"]) != task:
        raise SystemExit(f"invalid task {task}")
    row = rows[task]
    result_path = CAMPAIGN / "results" / f"{task:03d}.json"
    provisional_path = CAMPAIGN / "provisional" / f"{task:03d}.json"
    claim_path = CAMPAIGN / "claims" / f"{task:03d}.json"
    if result_path.exists() or claim_path.exists():
        raise SystemExit(f"task {task} already claimed or complete")
    atomic_json(claim_path, {
        "task": task,
        "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
        "started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    })

    manifest = json.loads((CAMPAIGN / "manifest.json").read_text())
    solve_timeout = positive_timeout(manifest, "solve_timeout")
    check_timeout = checker_timeout_for(manifest, row["instance"])
    compressed = ROOT / "benchmarks/cache" / row["instance"]
    arm = manifest["arms"][row["arm"]]
    certificate = arm.get("certificate", "drat")
    binary = CAMPAIGN / arm["path"]
    scratch_root = Path(os.environ.get("SLURM_TMPDIR", tempfile.gettempdir()))
    anonymous = manifest.get("scratch_policy") == "anonymous-local-files-v1"
    if anonymous:
        from anonymous_scratch import AnonymousScratch
        # User-authorized node-local scratch. The repository filesystem does
        # not support O_TMPFILE; never fall back to named certificate files.
        scratch = AnonymousScratch("/tmp")
    else:
        scratch = Path(tempfile.mkdtemp(prefix=f"tactix-runtime-{task:03d}-",
                                       dir=scratch_root))
    cnf = scratch / "input.cnf"
    proof = scratch / ("proof.out" if certificate == "auto" else
                       "proof.dsr" if certificate == "dsr" else "proof.drat")
    stdout = scratch / "solver.stdout"
    stderr = scratch / "solver.stderr"
    result = None
    try:
        validate_runtime_pin_contract(CAMPAIGN, manifest)
        if sha256(compressed) != manifest["input_sha256"][row["instance"]]:
            raise RuntimeError("compressed input hash mismatch")
        with open(cnf, "wb") as target:
            subprocess.run(["xz", "-dc", compressed], stdout=target, check=True)
        for rel, expected in manifest["pin_sha256"].items():
            if sha256(CAMPAIGN / rel) != expected:
                raise RuntimeError(f"pin hash mismatch: {rel}")
        for name in list(os.environ):
            if name.startswith(("TACTIX_", "KISSAT_", "SAT_EVENT_", "SAT_INPROCESS_")):
                del os.environ[name]
        affinity = sorted(os.sched_getaffinity(0))
        command = ["taskset", "-c", str(affinity[0]),
                   *solver_command(binary, arm, cnf, proof)]
        context = None
        perf_path = scratch / "solver.perf"
        if manifest.get("collect_runtime_context"):
            context = runtime_context(affinity)
            os.environ["TACTIX_RUNTIME_SAMPLE_VM"] = "1"
            try:
                probe = subprocess.run(["perf", "stat", "-e", "cycles", "--", "true"],
                                       capture_output=True, text=True, timeout=15)
                context["perf_probe"] = dict(exit_code=probe.returncode, stderr=probe.stderr[-2000:])
                if probe.returncode == 0:
                    command = ["perf", "stat", "-x", "\t", "-o", perf_path, "-e",
                               "task-clock,cycles,instructions,context-switches,cpu-migrations,page-faults",
                               "--", *command]
            except (OSError, subprocess.TimeoutExpired) as error:
                context["perf_probe"] = {"error": repr(error)}
        solve = measured(command, stdout, stderr,
                         scratch / "solve.time", solve_timeout)
        if context is not None:
            context["perf_statistics"] = tail(perf_path, 50)
            context["end"] = runtime_context(affinity)
            del os.environ["TACTIX_RUNTIME_SAMPLE_VM"]
        verdict, statistics = solver_summary(stdout)
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
            "rank": int(row["rank"]),
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": verdict,
            "expected_match": expected_matches(row["expected"], verdict),
            "solve": solve,
            "cpu_affinity": affinity,
            "runtime_context": context,
            "command": list(map(str, command)),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "binary_sha256": arm["sha256"],
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "solver_statistics": statistics,
            "solver_stderr_tail": tail(stderr),
            "certificate_bytes": proof.stat().st_size if proof.is_file() else None,
            "certificate_sha256": sha256(proof) if proof.is_file() else None,
            "proof_check": None,
            "model_check": None,
            "valid": False,
            "cleanup_pending": True,
        }
        # Publish solver timing and verdict before potentially long proof
        # checking.  This is deliberately separate from the final validity
        # result and never satisfies the cumulative correctness gate.
        atomic_json(provisional_path, provisional_result(
            result, dt.datetime.now(dt.timezone.utc).isoformat()))
        if verdict == "UNSAT" and proof.is_file():
            checked = check_unsat_proof(
                CAMPAIGN, certificate, cnf, proof, scratch, check_timeout,
                require_canonical_dsr=manifest.get('require_canonical_dsr', False))
            result["proof_check"] = checked
            result["valid"] = checked["result"] == "verified"
        elif verdict == "SAT":
            checker_out = scratch / "model.stdout"
            checker_err = scratch / "model.stderr"
            checked = measured([CAMPAIGN / "pins/checkmodel", cnf, stdout],
                               checker_out, checker_err,
                               scratch / "model.time", 600)
            checked["result"] = (
                "verified" if checked["exit_code"] == 0 else "not-verified")
            checked["stdout_tail"] = tail(checker_out)
            checked["stderr_tail"] = tail(checker_err)
            result["model_check"] = checked
            result["valid"] = checked["result"] == "verified"
        result["valid"] = checked_verdict_valid(
            row["expected"], verdict, solve, result["valid"])
    except Exception as error:
        result = {
            "schema": "tactix-matched-slurm-memory-regression-result-v2",
            "task": task,
            "rank": int(row["rank"]),
            "arm": row["arm"],
            "tier": row["tier"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": "INFRA_ERROR",
            "valid": False,
            "error": repr(error),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "cleanup_pending": True,
        }
    finally:
        if anonymous:
            scratch.close()
        else:
            shutil.rmtree(scratch, ignore_errors=True)
    result["artifact_deleted"] = not scratch.exists()
    result["derived_artifacts_deleted"] = not scratch.exists()
    result["cleanup_pending"] = scratch.exists()
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    atomic_json(result_path, result)
    return 0 if result["valid"] and result["artifact_deleted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

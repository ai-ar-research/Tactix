#!/usr/bin/env python3
"""Checked proof-on comparison: one immutable solver per SLURM task."""

from collections import deque
import csv
import datetime as dt
import errno
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
from contextlib import contextmanager

from proof_oracle import (bridge_trivial_input_binary_lsr,
                          bridge_trivial_input_lsr, dsr_compress_status,
                          proof_check_status, dsr_proof_is_binary,
                          routed_dsr_trim_stage, routed_dsr_compress_stage,
                          routed_dsr_check_stage,
                          certificate_route, dsr_proof_as_drat, drat_stages,
                          pb_stages, stage_environment,
                          canonical_checker_cnf, bridge_trivial_input_lrat,
                          drat_command_with_timeout, DSRFormatError,
                          restate_duplicate_input_lrat,
                          REPORTED_DSR_ROUTES, solver_reported_route)


CAMPAIGN = None
ROOT = None
# Named certificate scratch under a declared campaign directory (home quota).
HOME_SCRATCH_POLICY = "home-directory-files-v1"
# resource_contract.cake_heap_mib: CakeML heap for a non-default task memory.
CAKE_HEAP_MIB = None
CAKE_STAGES = ("cake_lpr", "cake-pb-cnf")
# Manifest contracts of retired campaign lanes; a manifest naming one fails
# before solving. Their runners remain in each campaign's own pins.
RETIRED_CONTRACTS = ("anonymous_scratch_capacity", "causal_preflight",
                     "failed_proof_retention", "matrix_source_summary")


class TaskTerminated(RuntimeError):
    """SIGTERM (scancel or wall limit) reached a home-scratch task."""


def task_terminated_signal(signum, _frame):
    raise TaskTerminated(f"terminated by signal {signum}")


class ProofPipelineDeadline(TimeoutError):
    """The one whole-pipeline wall allowance expired."""


class ScratchCapacityError(RuntimeError):
    """A declared scratch limit stopped a task before checked completion."""

    def __init__(self, reason, **details):
        super().__init__(reason)
        self.scratch_reason = reason
        self.scratch_details = details


def proof_deadline_signal(_signal, _frame):
    raise ProofPipelineDeadline("proof pipeline wall deadline expired")


@contextmanager
def proof_pipeline_deadline(timeout):
    # Runtime array tasks check proofs on their main thread. A real-time alarm
    # also bounds Python conversion/bridging, not just external checker waits.
    if threading.current_thread() is not threading.main_thread():
        raise RuntimeError("proof deadline requires the runtime task main thread")
    if timeout <= 0:
        raise ValueError("proof pipeline timeout must be positive")
    previous_handler = signal.getsignal(signal.SIGALRM)
    previous_timer = signal.getitimer(signal.ITIMER_REAL)
    started = time.monotonic()
    signal.signal(signal.SIGALRM, proof_deadline_signal)
    signal.setitimer(signal.ITIMER_REAL, timeout)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous_handler)
        if previous_timer[0]:
            signal.setitimer(signal.ITIMER_REAL,
                             max(1e-6, previous_timer[0] -
                                 (time.monotonic() - started)), previous_timer[1])


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


def scratch_resource_reason(error):
    reason = getattr(error, "scratch_reason", None)
    if reason is not None:
        return reason
    if isinstance(error, OSError):
        if error.errno == errno.EFBIG:
            return "scratch_file_limit"
        if error.errno == errno.ENOSPC:
            return "scratch_enospc"
    if (isinstance(error, subprocess.CalledProcessError) and
            error.returncode in (-signal.SIGXFSZ, 128 + signal.SIGXFSZ)):
        return "scratch_file_limit"
    return None


def scratch_incomplete(reason, details=None):
    return {"status": "incomplete", "reason": reason, "details": details or {}}


def solver_command(binary, arm, cnf, proof):
    before = arm.get("arguments", [])
    after = arm.get("trailing_arguments", [])
    if (not isinstance(before, list) or not isinstance(after, list) or
            any(not isinstance(arg, str) for arg in before + after)):
        raise RuntimeError("solver arguments must be string lists")
    # certificate "none": the solver gets no proof path and runs proof-free.
    proof_path = [] if arm.get("certificate") == "none" else [proof]
    return [binary, *before, cnf, *proof_path, *after]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def process_group(signal_mask=None):
    if signal_mask is not None:
        signal.pthread_sigmask(signal.SIG_SETMASK, signal_mask)
    # Python ignores SIGXFSZ by default. External solver/checker processes
    # must instead receive the inherited RLIMIT_FSIZE as its normal signal.
    signal.signal(signal.SIGXFSZ, signal.SIG_DFL)
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
        process = None
        stop = threading.Event()
        sampler = None
        try:
            # Keep the process handle available before a pending wall alarm
            # can raise. Children restore the original mask, so checker-native
            # signals retain their normal behavior.
            mask = signal.pthread_sigmask(signal.SIG_BLOCK,
                                            {signal.SIGALRM, signal.SIGUSR1})
            try:
                process = subprocess.Popen(
                    wrapped, stdout=out, stderr=err,
                    preexec_fn=lambda: process_group(mask), env=environment)
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, mask)
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
        except BaseException:
            # In particular, the pipeline alarm must not abandon a checker
            # process group, nor grant another ten seconds after its deadline.
            if process is not None:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
            raise
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
    result = {
        "exit_code": exit_code,
        "wall_seconds": round(time.monotonic() - started, 3),
        "peak_rss_kb": peak_rss_kb,
        "sampled_process_vm_peak_kb": vm_peak[0] or None,
        "timed_out": timed_out,
    }
    try:
        with open(stderr, "rb") as source:
            source.seek(0, os.SEEK_END)
            size = source.tell()
            source.seek(max(0, size - 4096), os.SEEK_SET)
            resource_stderr = source.read(4096).decode(errors="replace")
    except OSError:
        resource_stderr = ""
    file_limit = (exit_code in (-signal.SIGXFSZ, 128 + signal.SIGXFSZ) or
                  "File too large" in resource_stderr or
                  "File size limit exceeded" in resource_stderr or
                  "Command terminated by signal 25" in resource_stderr)
    if file_limit:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        result["resource_limit"] = "file_size"
    return result


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


def verified_reference_identity(manifest, instance, digest):
    """A source campaign's verified certificate for `instance` with the same bytes, or None."""
    spec = manifest.get("verified_certificate_reference")
    if not spec or digest is None:
        return None
    path = CAMPAIGN / spec["path"]
    if sha256(path) != spec["sha256"]:
        raise RuntimeError("verified certificate reference changed")
    with open(path) as source:
        entries = json.load(source).get(instance, [])
    for entry in entries:
        if entry["certificate_sha256"] == digest:
            return entry
    return None


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
        elif certificate == "pbp":
            required.update({"pins/veripb", "pins/cake-pb-cnf"})
        elif certificate in ("dsr", "auto"):
            required.update({"pins/dsr-trim", "pins/dsr-compress",
                             "pins/srcheck", "pins/drat-trim",
                             "pins/cake_lpr"})
            if certificate == "auto":
                required.update({"pins/veripb", "pins/cake-pb-cnf"})
        elif certificate != "none":
            raise RuntimeError(f"unsupported proof certificate: {certificate}")
    if manifest.get("scratch_policy") == "anonymous-local-files-v1":
        required.add("pins/anonymous_scratch.py")
    if manifest.get("scratch_policy") == HOME_SCRATCH_POLICY:
        required.add("pins/directory_scratch.py")
    hashes = manifest.get("pin_sha256", {})
    missing = sorted(rel for rel in required if rel not in hashes)
    if missing:
        raise RuntimeError(f"runtime pin contract omits {missing}")
    for name, arm in manifest.get("arms", {}).items():
        declared = arm.get("sha256")
        if declared is not None and declared != hashes[arm["path"]]:
            raise RuntimeError(f"runtime arm pin mismatch: {name}")
    absent = sorted(rel for rel in required if not (campaign / rel).is_file())
    if absent:
        raise RuntimeError(f"runtime pin files missing {absent}")


def release_scratch(scratch, *names):
    """Release completed anonymous artifacts; named scratch remains unchanged."""
    release = getattr(scratch, "release", None)
    if release is None:
        return
    for name in names:
        if not isinstance(name, str):
            raise TypeError("scratch release requires logical names")
        release(name)


def fresh_scratch_name(scratch, name):
    """Use generation-safe names when an anonymous stage runs more than once."""
    fresh = getattr(scratch, "fresh_name", None)
    return fresh(name) if fresh is not None else name


def proof_stage(scratch, name, command, timeout, produced=None,
                *, input_has_empty_clause=None, trivial_lrat_spelling=None):
    """Run one proof stage and require its exact oracle/output contract."""
    try:
        stdout_name = fresh_scratch_name(scratch, f"{name}.stdout")
        stderr_name = fresh_scratch_name(scratch, f"{name}.stderr")
        metrics_name = fresh_scratch_name(scratch, f"{name}.time")
        stdout = scratch / stdout_name
        stderr = scratch / stderr_name
        metrics = scratch / metrics_name
    except Exception as error:
        if getattr(error, "scratch_reason", None) is not None:
            error.scratch_details.setdefault("stage", name)
        raise
    if name == 'drat-trim':
        command = drat_command_with_timeout(command, timeout)
    environment = stage_environment(name)
    if CAKE_HEAP_MIB is not None and name in CAKE_STAGES:
        environment["CML_HEAP_SIZE"] = str(CAKE_HEAP_MIB)
    result = measured(command, stdout, stderr, metrics, timeout,
                      environment=environment)
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
    release_scratch(scratch, stdout_name, stderr_name, metrics_name)
    if result.get("resource_limit") == "file_size":
        raise ScratchCapacityError(
            "scratch_file_limit", stage=name, measurement=result)
    return result


def check_unsat_proof(campaign, certificate, cnf, proof, scratch, timeout,
                      reported_route=None, *, release_intermediates=False,
                      cnf_scratch_name=None, proof_scratch_name=None):
    if release_intermediates and (not cnf_scratch_name or not proof_scratch_name):
        raise ValueError("anonymous early release requires logical CNF and proof names")
    started = time.monotonic()
    stages = []
    try:
        with proof_pipeline_deadline(timeout):
            return check_unsat_proof_until_deadline(
                campaign, certificate, cnf, proof, scratch, timeout,
                reported_route, started, stages, release_intermediates,
                cnf_scratch_name, proof_scratch_name)
    except ProofPipelineDeadline:
        # Conversion is also inside the allowance. Keep completed stage
        # evidence and never hide an already-recorded rejection behind a TO.
        prior_failure = any(stage.get('result') != 'verified' and
                            not stage.get('timed_out', False) for stage in stages)
        route = REPORTED_DSR_ROUTES.get(reported_route)
        pipeline = ('drat-trim' if certificate == 'drat' else
                    'veripb-cake-pb' if route == 'pbp' else
                    'dsr-as-drat-drat-trim-cake_lpr' if route == 'drat' else
                    'dsr-trim-dsr-compress-trestle' if route == 'dsr' else
                    'certificate-routing')
        stages.append(dict(name='pipeline-deadline', result='timeout',
                           exit_code=None, timed_out=True,
                           output_tail='whole proof-pipeline wall allowance exhausted'))
        return dict(pipeline=pipeline, result='not-verified',
                    solver_reported_route=reported_route,
                    wall_seconds=round(time.monotonic() - started, 3),
                    exit_code=None, timed_out=not prior_failure,
                    deadline_expired=True, timeout_seconds=timeout,
                    output_tail=stages[-1]['output_tail'], stages=stages)
    except Exception as error:
        if getattr(error, 'scratch_reason', None) is None:
            raise
        # Keep every completed checker stage and the interrupted stage's
        # measurement. The outer result remains invalid and explicitly
        # incomplete, but the already-finished solver/checker evidence is not
        # replaced by a generic infrastructure record.
        completed = [dict(stage) for stage in stages]
        interrupted = error.scratch_details.get('measurement')
        if interrupted is not None:
            interrupted = dict(interrupted, result='incomplete')
            completed.append(interrupted)
        elif error.scratch_details.get('stage') is not None:
            completed.append({
                'name': error.scratch_details['stage'],
                'result': 'incomplete',
                'exit_code': None,
                'timed_out': False,
                'output_tail': str(error),
            })
        error.scratch_details['proof_check'] = {
            'pipeline': 'resource-interrupted',
            'solver_reported_route': reported_route,
            'result': 'incomplete',
            'wall_seconds': round(time.monotonic() - started, 3),
            'stages': completed,
        }
        raise
    finally:
        if release_intermediates:
            release_scratch(
                scratch, cnf_scratch_name, 'checker.cnf', 'proof.binary.lsr', 'proof.lsr',
                'proof.drat', 'proof.lrat', 'proof.original-input.lrat',
                'proof.kernel.pbp')
            release_scratch(scratch, proof_scratch_name)


def check_unsat_proof_until_deadline(campaign, certificate, cnf, proof, scratch,
                                    timeout, reported_route, started, stages,
                                    release_intermediates=False,
                                    cnf_scratch_name=None,
                                    proof_scratch_name=None):
    """Check one certificate, stopping at the first failed pipeline stage."""
    checker_cnf = scratch / 'checker.cnf'
    spelling = canonical_checker_cnf(cnf, checker_cnf)

    def remaining():
        amount = timeout - (time.monotonic() - started)
        if amount <= 0:
            raise ProofPipelineDeadline("proof pipeline wall deadline expired")
        return amount

    def append_canonical_dsr():
        # Witnessed SR requires no-option DSR elaboration and Trestle.
        binary = dsr_proof_is_binary(proof)
        trim_output_name = 'proof.binary.lsr' if binary else 'proof.lsr'
        trim_output = scratch / trim_output_name
        name, command = routed_dsr_trim_stage(
            campaign / 'pins/dsr-trim', cnf, proof, trim_output,
            checker_cnf=checker_cnf)
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        stages.append(proof_stage(
            scratch, name, command, remaining(), trim_output))
        if stages[-1]['result'] != 'verified':
            return
        (bridge_trivial_input_binary_lsr if binary
         else bridge_trivial_input_lsr)(checker_cnf, trim_output)
        if release_intermediates:
            release_scratch(scratch, proof_scratch_name)

        text_lsr = trim_output
        if binary:
            # Allocate text LSR only after trim and original-proof release.
            # This keeps the witnessed route within the six-handle contract.
            text_lsr = scratch / 'proof.lsr'
            name, command = routed_dsr_compress_stage(
                campaign / 'pins/dsr-compress', trim_output, text_lsr)
            stages.append(proof_stage(
                scratch, name, command, remaining(), text_lsr))
            if stages[-1]['result'] != 'verified':
                return
            if release_intermediates:
                release_scratch(scratch, trim_output_name)

        name, command = routed_dsr_check_stage(
            campaign / 'pins/srcheck', cnf, text_lsr,
            checker_cnf=checker_cnf)
        stages.append(proof_stage(scratch, name, command, remaining()))
        if stages[-1]['result'] == 'verified' and release_intermediates:
            release_scratch(scratch, 'proof.lsr')

    def append_witness_free_drat():
        # The audited, witness-free DSR stream has a lossless DRAT spelling.
        # A witnessed or malformed stream must never reach drat-trim.
        drat = scratch / 'proof.drat'
        lrat = scratch / 'proof.lrat'
        if not dsr_proof_as_drat(proof, drat):
            stages.append(dict(name='dsr-to-drat', result='rejected',
                               exit_code=None, timed_out=False,
                               output_tail='DSR proof contains an SR witness'))
            return
        if release_intermediates:
            release_scratch(scratch, proof_scratch_name)
        commands = list(drat_stages(
                campaign / 'pins/drat-trim', campaign / 'pins/cake_lpr',
                cnf, drat, lrat, checker_cnf=checker_cnf))
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        for name, command in commands:
            if name == 'cake_lpr':
                checked_lrat_name = 'proof.lrat'
                try:
                    checked_lrat, restatements = restate_duplicate_input_lrat(
                        checker_cnf, spelling, lrat,
                        scratch / 'proof.original-input.lrat')
                except ValueError as error:
                    stages.append(dict(name='input-restatement', result='rejected',
                                       exit_code=None, timed_out=False,
                                       output_tail=str(error)))
                    break
                command = [*command[:-1], checked_lrat]
                if checked_lrat != lrat:
                    checked_lrat_name = 'proof.original-input.lrat'
                stages[-1]['input_restatements'] = restatements
                if release_intermediates and checked_lrat != lrat:
                    release_scratch(scratch, 'proof.lrat')
            stages.append(proof_stage(
                scratch, name, command, remaining(),
                lrat if name == 'drat-trim' else None,
                input_has_empty_clause=spelling['first_empty_clause'] is not None,
                trivial_lrat_spelling=spelling if name == 'drat-trim' else None))
            if release_intermediates and name == 'drat-trim':
                release_scratch(scratch, 'proof.drat')
            elif release_intermediates and name == 'cake_lpr':
                release_scratch(scratch, checked_lrat_name)
            if stages[-1]['result'] != 'verified':
                break

    if certificate in ("pbp", "dsr", "auto") and reported_route not in REPORTED_DSR_ROUTES:
        message = 'missing or invalid solver-reported proof route'
        stage = dict(name='route', result='rejected', exit_code=None,
                     timed_out=False, output_tail=message)
        stages.append(stage)
        return dict(pipeline='certificate-routing', checker_input=spelling,
                    result='not-verified', wall_seconds=round(time.monotonic()-started, 3),
                    exit_code=None, timed_out=False, output_tail=message, stages=stages)
    route = REPORTED_DSR_ROUTES.get(reported_route)
    try:
        audited_route = certificate_route(proof) if certificate in ("pbp", "dsr", "auto") else None
        if route != audited_route:
            raise DSRFormatError('solver-reported route disagrees with certificate content')
    except DSRFormatError as error:
        stage = dict(name='route', result='rejected', exit_code=None,
                     timed_out=False, output_tail=str(error))
        stages.append(stage)
        return dict(pipeline='certificate-routing', checker_input=spelling,
                    result='not-verified',
                    wall_seconds=round(time.monotonic() - started, 3),
                    exit_code=None, timed_out=False, output_tail=str(error),
                    stages=stages)
    if certificate == "dsr" and route == "pbp":
        raise RuntimeError("explicit DSR arm emitted a PBP certificate")
    if certificate == "pbp" and route != "pbp":
        raise RuntimeError("explicit PBP arm emitted a non-PBP certificate")
    if certificate == "drat":
        command = [campaign / "pins/drat-trim", checker_cnf, proof]
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        stages.append(proof_stage(
            scratch, "drat-trim", command, remaining(),
            input_has_empty_clause=spelling['first_empty_clause'] is not None))
        pipeline = "drat-trim"
    elif certificate in ("pbp", "auto") and route == "pbp":
        kernel = scratch / "proof.kernel.pbp"
        if kernel.exists():
            # AnonymousScratch returns an already-open /proc/PID/fd/N path:
            # it can be truncated but has no unlinkable directory entry.
            # Clear stale elaboration in both scratch implementations.
            with kernel.open("wb"):
                pass
        commands = list(pb_stages(
                campaign / "pins/veripb", campaign / "pins/cake-pb-cnf",
                cnf, proof, kernel, checker_cnf=checker_cnf))
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        for name, command in commands:
            made = kernel if name == "veripb" else None
            stages.append(proof_stage(scratch, name, command, remaining(), made))
            if release_intermediates and name == 'veripb':
                release_scratch(scratch, proof_scratch_name)
            elif release_intermediates and name == 'cake-pb-cnf':
                release_scratch(scratch, 'proof.kernel.pbp')
            if stages[-1]["result"] != "verified":
                break
        pipeline = "veripb-cake-pb"
    elif certificate in ("dsr", "auto"):
        if route == 'drat':
            append_witness_free_drat()
            pipeline = 'dsr-as-drat-drat-trim-cake_lpr'
        else:
            append_canonical_dsr()
            pipeline = 'dsr-trim-dsr-compress-trestle'
    else:
        raise RuntimeError(f"unsupported proof certificate: {certificate}")
    return {
        "pipeline": pipeline,
        "solver_reported_route": reported_route,
        "certificate_content_route": audited_route,
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


def home_scratch_root(campaign, manifest):
    """The declared home-filesystem scratch root of a campaign."""
    root = manifest.get("scratch_directory")
    if not isinstance(root, str) or not Path(root).is_absolute():
        raise RuntimeError("home scratch policy requires an absolute scratch_directory")
    root = Path(root)
    if root != campaign / "scratch" and campaign not in root.parents:
        raise RuntimeError("home scratch must live inside the campaign directory")
    return root


def cake_heap_mib(manifest):
    value = manifest.get("resource_contract", {}).get("cake_heap_mib")
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or value < 4096:
        raise RuntimeError("invalid resource_contract.cake_heap_mib")
    return value


def main():
    global CAMPAIGN, ROOT, CAKE_HEAP_MIB
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
    result_schema = "tactix-matched-slurm-memory-regression-result-v2"
    anonymous = manifest.get("scratch_policy") == "anonymous-local-files-v1"
    home = manifest.get("scratch_policy") == HOME_SCRATCH_POLICY
    previous_term_handler = None
    cnf_name = "input.cnf"
    proof_name = ("proof.pbp" if certificate == "pbp" else
                  "proof.out" if certificate == "auto" else
                  "proof.dsr" if certificate == "dsr" else "proof.drat")
    scratch = None
    result = None
    try:
        retired = sorted(key for key in RETIRED_CONTRACTS
                         if manifest.get(key) is not None)
        if retired:
            raise RuntimeError(f"retired manifest contract: {retired}")
        validate_runtime_pin_contract(CAMPAIGN, manifest)
        CAKE_HEAP_MIB = cake_heap_mib(manifest)
        if anonymous:
            from anonymous_scratch import AnonymousScratch
            # User-authorized node-local scratch. The repository filesystem
            # does not support O_TMPFILE; never fall back to named files.
            scratch = AnonymousScratch("/tmp")
        elif home:
            from directory_scratch import DirectoryScratch
            previous_term_handler = signal.signal(
                signal.SIGTERM, task_terminated_signal)
            scratch = DirectoryScratch(
                home_scratch_root(CAMPAIGN, manifest),
                f"{task:04d}-{os.environ['SLURM_JOB_ID']}")
            # Checker temporaries stay on the same accounted filesystem.
            os.environ["TMPDIR"] = str(scratch.tmpdir)
        else:
            scratch = Path(tempfile.mkdtemp(
                prefix=f"tactix-runtime-{task:03d}-", dir=scratch_root))
        cnf = scratch / cnf_name
        proof = scratch / proof_name
        stdout = scratch / "solver.stdout"
        stderr = scratch / "solver.stderr"
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
        if solve.get("resource_limit") == "file_size":
            raise ScratchCapacityError(
                "scratch_file_limit", stage="solver", measurement=solve)
        if context is not None:
            context["perf_statistics"] = tail(perf_path, 50)
            context["end"] = runtime_context(affinity)
            del os.environ["TACTIX_RUNTIME_SAMPLE_VM"]
        verdict, statistics = solver_summary(stdout)
        result = {
            "schema": result_schema,
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
            "certificate_bytes": (proof.stat().st_size if certificate != "none" and
                                  proof.is_file() else None),
            "certificate_sha256": (sha256(proof) if certificate != "none" and
                                   proof.is_file() else None),
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
        release_scratch(scratch, 'solver.stderr', 'solve.time', 'solver.perf')
        identity = (verified_reference_identity(
                        manifest, row["instance"], result["certificate_sha256"])
                    if verdict == "UNSAT" and certificate != "none" else None)
        if identity is not None:
            release_scratch(scratch, cnf_name, proof_name, 'solver.stdout')
            result["proof_check"] = {
                "result": "verified",
                "pipeline": "reference-certificate-identity",
                "reference": identity,
            }
            result["valid"] = True
        elif verdict == "UNSAT" and certificate != "none" and proof.is_file():
            reported_route = solver_reported_route(stdout)
            release_scratch(scratch, 'solver.stdout')
            checked = check_unsat_proof(
                CAMPAIGN, certificate, cnf, proof, scratch, check_timeout,
                reported_route=reported_route, release_intermediates=True,
                cnf_scratch_name=cnf_name, proof_scratch_name=proof_name)
            result["proof_check"] = checked
            result["valid"] = checked["result"] == "verified"
        elif verdict == "SAT":
            release_scratch(scratch, proof_name)
            checker_out = scratch / "model.stdout"
            checker_err = scratch / "model.stderr"
            checked = measured([CAMPAIGN / "pins/checkmodel", cnf, stdout],
                               checker_out, checker_err,
                               scratch / "model.time", 600)
            if checked.get("resource_limit") == "file_size":
                raise ScratchCapacityError(
                    "scratch_file_limit", stage="checkmodel",
                    measurement=checked)
            checked["result"] = (
                "verified" if checked["exit_code"] == 0 else "not-verified")
            checked["stdout_tail"] = tail(checker_out)
            checked["stderr_tail"] = tail(checker_err)
            result["model_check"] = checked
            result["valid"] = checked["result"] == "verified"
            release_scratch(scratch, cnf_name, 'solver.stdout', 'model.stdout',
                            'model.stderr', 'model.time')
        else:
            release_scratch(scratch, cnf_name, proof_name, 'solver.stdout')
        result["valid"] = checked_verdict_valid(
            row["expected"], verdict, solve, result["valid"])
    except Exception as error:
        scratch_reason = scratch_resource_reason(error)
        if result is None or scratch_reason is None:
            result = {
                "schema": result_schema,
                "task": task,
                "rank": int(row["rank"]),
                "arm": row["arm"],
                "tier": row["tier"],
                "instance": row["instance"],
                "expected": row["expected"],
                "verdict": "INFRA_ERROR",
                "node": os.environ.get(
                    "SLURMD_NODENAME", os.uname().nodename),
                "memory_enforcement": "slurm_task_cgroup",
                "address_space_limit": None,
            }
        result["valid"] = False
        result["error"] = repr(error)
        result["cleanup_pending"] = True
        if scratch_reason is not None:
            partial = getattr(error, "scratch_details", {}).get("proof_check")
            if partial is not None:
                result["proof_check"] = partial
            result["completion"] = scratch_incomplete(
                scratch_reason, getattr(error, "scratch_details", {}))
    finally:
        if anonymous and scratch is not None:
            scratch.close()
        if home and scratch is not None:
            # A second SIGTERM must not abandon cleanup half way.
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            try:
                scratch.close()
            except OSError as error:
                if result is not None:
                    result["cleanup_error"] = repr(error)
            if result is not None:
                result["scratch_resources"] = scratch.resource_status()
        if previous_term_handler is not None:
            signal.signal(signal.SIGTERM, previous_term_handler)
        if scratch is not None and not home:
            shutil.rmtree(scratch, ignore_errors=True)
    scratch_exists = scratch is not None and scratch.exists()
    result["artifact_deleted"] = not scratch_exists
    result["derived_artifacts_deleted"] = not scratch_exists
    result["cleanup_pending"] = scratch_exists
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    atomic_json(result_path, result)
    return 0 if result["valid"] and result["artifact_deleted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

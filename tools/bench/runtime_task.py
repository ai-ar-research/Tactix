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
from contextlib import contextmanager

from proof_oracle import (bridge_trivial_input_binary_lsr,
                          bridge_trivial_input_lsr, dsr_compress_status,
                          proof_check_status, routed_dsr_stages,
                          certificate_route, dsr_proof_as_drat, drat_stages,
                          pb_stages, stage_environment,
                          canonical_checker_cnf, bridge_trivial_input_lrat,
                          drat_command_with_timeout, DSRFormatError,
                          restate_duplicate_input_lrat,
                          REPORTED_DSR_ROUTES, solver_reported_route)


CAMPAIGN = None
ROOT = None


class ProofPipelineDeadline(TimeoutError):
    """The one whole-pipeline wall allowance expired."""


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


def solver_command(binary, arm, cnf, proof, instrumentation=()):
    before = arm.get("arguments", [])
    after = arm.get("trailing_arguments", [])
    if (not isinstance(before, list) or not isinstance(after, list) or
            not isinstance(instrumentation, (list, tuple)) or
            any(not isinstance(arg, str) for arg in before + after + list(instrumentation))):
        raise RuntimeError("solver arguments must be string lists")
    return [binary, *before, *instrumentation, cnf, proof, *after]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def process_group(signal_mask=None):
    if signal_mask is not None:
        signal.pthread_sigmask(signal.SIG_SETMASK, signal_mask)
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
            mask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGALRM})
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


def atomic_retain(source, target, maximum_bytes):
    """Retain one bounded raw artifact without exposing a partial sidecar."""
    if (isinstance(maximum_bytes, bool) or not isinstance(maximum_bytes, int) or
            maximum_bytes <= 0):
        raise RuntimeError("invalid telemetry byte cap")
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + f".tmp.{os.getpid()}")
    total = 0
    try:
        with open(source, "rb") as stream, open(temporary, "xb") as output:
            for block in iter(lambda: stream.read(1 << 20), b""):
                total += len(block)
                if total > maximum_bytes:
                    raise RuntimeError(f"telemetry byte cap exceeded: {source.name}")
                output.write(block)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, target)
    finally:
        if temporary.exists():
            temporary.unlink()


def failed_proof_retention_contract(manifest):
    contract = manifest.get("failed_proof_retention")
    if contract is None:
        return None
    required = {"schema", "directory", "maximum_bytes", "purpose"}
    if (type(contract) is not dict or set(contract) != required or
            contract.get("schema") != "tactix-failed-proof-retention-v1"):
        raise RuntimeError("invalid failed-proof retention contract")
    if contract["purpose"] != (
            "checker-only recovery; retain only after an unsuccessful UNSAT proof check"):
        raise RuntimeError("invalid failed-proof retention purpose")
    maximum = contract["maximum_bytes"]
    directory = contract["directory"]
    relative = Path(directory) if isinstance(directory, str) else Path("/")
    if (isinstance(maximum, bool) or not isinstance(maximum, int) or
            maximum < 1 or relative.is_absolute() or ".." in relative.parts or
            not relative.parts):
        raise RuntimeError("invalid failed-proof retention bounds")
    return contract


def gf2_telemetry_contract(manifest):
    contract = manifest.get("gf2_telemetry")
    if contract is None:
        return None
    from gf2_target_score import (LEGACY_SCHEMA, MAX_FEEDBACK_BYTES,
                                  MAX_FEEDBACK_DECODED_BYTES,
                                  MAX_FEEDBACK_RECORDS, MAX_PROVENANCE_BYTES,
                                  SCHEMA)
    modern = contract.get("schema") == SCHEMA
    required = ({"schema", "directory", "feedback_compression",
                 "feedback_wire_version", "feedback_stored_max_bytes",
                 "feedback_decoded_max_bytes", "feedback_max_records",
                 "structural_provenance_max_bytes"} if modern else
                {"schema", "directory", "feedback_max_bytes",
                 "structural_provenance_max_bytes"})
    if type(contract) is not dict or set(contract) != required:
        raise RuntimeError("invalid GF2 telemetry contract fields")
    if contract["schema"] not in (LEGACY_SCHEMA, SCHEMA) or contract["directory"] != "telemetry":
        raise RuntimeError("unsupported GF2 telemetry contract")
    if modern and (contract["feedback_compression"] != "gzip" or
                   contract["feedback_wire_version"] != 2):
        raise RuntimeError("unsupported GF2 feedback format")
    maximums = {("feedback_stored_max_bytes" if modern else "feedback_max_bytes"):
                MAX_FEEDBACK_BYTES,
                "structural_provenance_max_bytes": MAX_PROVENANCE_BYTES}
    if modern:
        maximums.update(feedback_decoded_max_bytes=MAX_FEEDBACK_DECODED_BYTES,
                        feedback_max_records=MAX_FEEDBACK_RECORDS)
    for name, maximum in maximums.items():
        value = contract[name]
        if (isinstance(value, bool) or not isinstance(value, int) or
                not 0 < value <= maximum):
            raise RuntimeError(f"invalid GF2 telemetry cap: {name}")
    return contract


def causal_preflight_contract(manifest):
    """Validate the counter-only discovery lane.

    SAT and UNSAT preflight verdicts still keep their certificate until the
    ordinary checker finishes. Only diagnostic telemetry is suppressed.
    """
    contract = manifest.get("causal_preflight")
    if contract is None:
        return None
    required = {"schema", "maximum_solve_seconds", "output",
                "retain_full_telemetry"}
    if (type(contract) is not dict or set(contract) != required or
            contract.get("schema") != "tactix-causal-preflight-v1" or
            contract.get("maximum_solve_seconds") != 60 or
            contract.get("output") != "terminal-controller-counters" or
            contract.get("retain_full_telemetry") is not False):
        raise RuntimeError("invalid causal-preflight contract")
    if positive_timeout(manifest, "solve_timeout") > 60:
        raise RuntimeError("causal preflight solve timeout exceeds 60 seconds")
    if manifest.get("gf2_telemetry") is not None:
        raise RuntimeError("causal preflight cannot retain full GF2 telemetry")
    forbidden = ("--feedback", "--feedback-replay",
                 "--structural-provenance", "--controller-trace")
    for arm in manifest.get("arms", {}).values():
        arguments = [*arm.get("arguments", []),
                     *arm.get("trailing_arguments", [])]
        if any(argument == option or argument.startswith(option + "=")
               for argument in arguments for option in forbidden):
            raise RuntimeError("causal preflight enables a full telemetry artifact")
    return contract


def _counter_fields(text):
    fields = {}
    for token in text.split():
        if "=" not in token:
            continue
        name, value = token.split("=", 1)
        if name and value.isdigit():
            fields[name] = int(value)
    return fields


def _controller_families(arguments):
    values = []
    for index, argument in enumerate(arguments):
        if argument.startswith("--controller-families="):
            values.append(argument.split("=", 1)[1])
        elif argument == "--controller-families" and index + 1 < len(arguments):
            values.append(arguments[index + 1])
    return {family for value in values for family in value.split(",") if family}


def terminal_counter_summary(stdout, arguments, verdict):
    """Extract the small gate record from a completed causal preflight."""
    configured = _controller_families(arguments)
    rules, producers, loop = {}, {}, None
    rule_lines, producer_lines = {}, {}
    loop_line, census_line, core_lines = None, None, {}
    core_prefixes = ("c decisions ", "c conflicts ", "c propagations ")
    with open(stdout, errors="replace") as source:
        for raw in source:
            line = raw.rstrip("\n")
            rule = re.match(r"^c controller-rule (\S+) (.*)$", line)
            producer = re.match(r"^c controller-producer (\S+) (.*)$", line)
            if rule:
                family, fields = rule.group(1), _counter_fields(rule.group(2))
                if (family in configured if configured else any(fields.values())):
                    rules[family] = fields
                    rule_lines[family] = line[-512:]
            elif producer:
                family = producer.group(1)
                fields = _counter_fields(producer.group(2))
                if (family in configured if configured else any(fields.values())):
                    producers[family] = fields
                    producer_lines[family] = line[-512:]
            elif line.startswith("c controller-loop "):
                loop = _counter_fields(line.removeprefix("c controller-loop "))
                loop_line = line[-512:]
            elif line.startswith("c nary-csp-census "):
                census_line = line[-512:]
            elif (line.startswith(core_prefixes) or
                  re.match(r"^c timing\s+\d+ ms solve$", line) or
                  line.startswith("s ")):
                if line.startswith("c decisions "):
                    key = "decisions"
                elif line.startswith("c conflicts "):
                    key = "conflicts"
                elif line.startswith("c propagations "):
                    key = "propagations"
                elif line.startswith("c timing"):
                    key = "timing"
                else:
                    key = "verdict"
                core_lines[key] = line[-512:]
    lines = [rule_lines[name] for name in sorted(rule_lines)]
    lines.extend(producer_lines[name] for name in sorted(producer_lines))
    if loop_line is not None:
        lines.append(loop_line)
    if census_line is not None:
        lines.append(census_line)
    lines.extend(core_lines[name] for name in
                 ("decisions", "conflicts", "propagations", "timing", "verdict")
                 if name in core_lines)
    return {
        "schema": "tactix-causal-preflight-terminal-v1",
        "verdict": verdict,
        "controller_rules": rules,
        "controller_producers": producers,
        "controller_loop": loop,
        "lines": lines,
        "full_telemetry_retained": False,
    }


def collect_gf2_telemetry(campaign, task, contract, feedback_path,
                          provenance_path, solver_statistics, bindings):
    """Retain, hash, and parse the two telemetry streams.

    Raw sidecars are retained before parsing, so a malformed stream remains
    auditable while its integrity error prevents a causal score.
    """
    from gf2_target_score import (parse_feedback, parse_provenance,
                                  raw_yield, sidecar_descriptor)
    modern = contract["schema"].endswith("-v2")
    directory = campaign / contract["directory"]
    retained = {
        "feedback": directory / f"{task:03d}.feedback.jsonl{'.gz' if modern else ''}",
        "structural_provenance": directory / f"{task:03d}.provenance.jsonl",
    }
    sources = {"feedback": feedback_path,
               "structural_provenance": provenance_path}
    caps = {"feedback": contract["feedback_stored_max_bytes" if modern else
                                 "feedback_max_bytes"],
            "structural_provenance": contract["structural_provenance_max_bytes"]}
    payload = {"schema": contract["schema"], "bindings": bindings, "sidecars": {}}
    errors = []
    for name in ("feedback", "structural_provenance"):
        try:
            atomic_retain(sources[name], retained[name], caps[name])
            if name != "feedback" or not modern:
                payload["sidecars"][name] = sidecar_descriptor(retained[name], campaign)
        except (OSError, RuntimeError) as error:
            errors.append(f"{name}: {error}")
    if not errors:
        try:
            feedback = parse_feedback(
                retained["feedback"], caps["feedback"],
                contract.get("feedback_decoded_max_bytes", caps["feedback"]),
                contract.get("feedback_max_records"))
            if modern:
                payload["sidecars"]["feedback"] = sidecar_descriptor(
                    retained["feedback"], campaign, feedback)
            provenance = parse_provenance(
                retained["structural_provenance"], caps["structural_provenance"])
            payload.update(feedback=feedback, structural_provenance=provenance,
                           raw_yield=raw_yield(feedback, provenance,
                                               solver_statistics))
        except (OSError, KeyError, TypeError, ValueError) as error:
            errors.append(str(error))
    if errors:
        payload["integrity_error"] = "; ".join(errors)
    return payload


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
    if manifest.get("gf2_telemetry") is not None:
        required.update({"pins/gf2_target_score.py",
                         "pins/inspect_policy_feedback.py",
                         "pins/gf2-affine.profile"})
    hashes = manifest.get("pin_sha256", {})
    missing = sorted(rel for rel in required if rel not in hashes)
    if missing:
        raise RuntimeError(f"runtime pin contract omits {missing}")
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
    release_scratch(scratch, f"{name}.stdout", f"{name}.stderr", f"{name}.time")
    return result


def check_unsat_proof(campaign, certificate, cnf, proof, scratch, timeout,
                      reported_route=None, *, release_intermediates=False,
                      retain_original_proof=False, cnf_scratch_name=None,
                      proof_scratch_name=None):
    if release_intermediates and (not cnf_scratch_name or not proof_scratch_name):
        raise ValueError("anonymous early release requires logical CNF and proof names")
    started = time.monotonic()
    stages = []
    try:
        with proof_pipeline_deadline(timeout):
            return check_unsat_proof_until_deadline(
                campaign, certificate, cnf, proof, scratch, timeout,
                reported_route, started, stages, release_intermediates,
                retain_original_proof, cnf_scratch_name, proof_scratch_name)
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
    finally:
        if release_intermediates:
            release_scratch(
                scratch, cnf_scratch_name, 'checker.cnf', 'proof.binary.lsr', 'proof.lsr',
                'proof.drat', 'proof.lrat', 'proof.original-input.lrat',
                'proof.kernel.pbp')
            if not retain_original_proof:
                release_scratch(scratch, proof_scratch_name)


def check_unsat_proof_until_deadline(campaign, certificate, cnf, proof, scratch,
                                    timeout, reported_route, started, stages,
                                    release_intermediates=False,
                                    retain_original_proof=False,
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
        binary_lsr = scratch / 'proof.binary.lsr'
        text_lsr = scratch / 'proof.lsr'
        commands = list(routed_dsr_stages(
                campaign / 'pins/dsr-trim', campaign / 'pins/dsr-compress',
                campaign / 'pins/srcheck', cnf, proof, binary_lsr, text_lsr,
                checker_cnf=checker_cnf, witness_free=False))
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        for name, command in commands:
            made = command[-1] if name in ('dsr-trim', 'dsr-compress') else None
            stages.append(proof_stage(scratch, name, command, remaining(), made))
            if stages[-1]['result'] != 'verified':
                break
            if name == 'dsr-trim':
                (bridge_trivial_input_binary_lsr if made == binary_lsr
                 else bridge_trivial_input_lsr)(checker_cnf, made)
                if release_intermediates and not retain_original_proof:
                    release_scratch(scratch, proof_scratch_name)
            elif name == 'dsr-compress' and release_intermediates:
                release_scratch(scratch, 'proof.binary.lsr')
            elif name == 'srcheck' and release_intermediates:
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
        if release_intermediates and not retain_original_proof:
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

    if certificate in ("dsr", "auto") and reported_route not in REPORTED_DSR_ROUTES:
        message = 'missing or invalid solver-reported proof route'
        stage = dict(name='route', result='rejected', exit_code=None,
                     timed_out=False, output_tail=message)
        stages.append(stage)
        return dict(pipeline='certificate-routing', checker_input=spelling,
                    result='not-verified', wall_seconds=round(time.monotonic()-started, 3),
                    exit_code=None, timed_out=False, output_tail=message, stages=stages)
    route = REPORTED_DSR_ROUTES.get(reported_route)
    try:
        audited_route = certificate_route(proof) if certificate in ("dsr", "auto") else None
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
    if certificate == "drat":
        command = [campaign / "pins/drat-trim", checker_cnf, proof]
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        stages.append(proof_stage(
            scratch, "drat-trim", command, remaining(),
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
        commands = list(pb_stages(
                campaign / "pins/veripb", campaign / "pins/cake-pb-cnf",
                cnf, proof, kernel, checker_cnf=checker_cnf))
        if release_intermediates:
            release_scratch(scratch, cnf_scratch_name)
        for name, command in commands:
            made = kernel if name == "veripb" else None
            stages.append(proof_stage(scratch, name, command, remaining(), made))
            if release_intermediates and name == 'veripb' and not retain_original_proof:
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
    manifest_sha256 = sha256(CAMPAIGN / "manifest.json")
    solve_timeout = positive_timeout(manifest, "solve_timeout")
    check_timeout = checker_timeout_for(manifest, row["instance"])
    compressed = ROOT / "benchmarks/cache" / row["instance"]
    arm = manifest["arms"][row["arm"]]
    certificate = arm.get("certificate", "drat")
    binary = CAMPAIGN / arm["path"]
    scratch_root = Path(os.environ.get("SLURM_TMPDIR", tempfile.gettempdir()))
    telemetry_contract = gf2_telemetry_contract(manifest)
    preflight_contract = causal_preflight_contract(manifest)
    result_schema = ("tactix-experimental-gf2-runtime-result-v1"
                     if telemetry_contract is not None else
                     "tactix-matched-slurm-memory-regression-result-v2")
    telemetry_local = None
    telemetry_payload = None
    telemetry_paths = None
    if telemetry_contract is not None:
        telemetry_local = Path(tempfile.mkdtemp(
            prefix=f"tactix-gf2-telemetry-{task:03d}-", dir=scratch_root))
        telemetry_paths = {
            "feedback": telemetry_local / ("feedback.jsonl.gz" if
                telemetry_contract["schema"].endswith("-v2") else "feedback.jsonl"),
            "structural_provenance": telemetry_local / "provenance.jsonl",
        }
    anonymous = manifest.get("scratch_policy") == "anonymous-local-files-v1"
    if anonymous:
        from anonymous_scratch import AnonymousScratch
        # User-authorized node-local scratch. The repository filesystem does
        # not support O_TMPFILE; never fall back to named certificate files.
        scratch = AnonymousScratch("/tmp")
    else:
        scratch = Path(tempfile.mkdtemp(prefix=f"tactix-runtime-{task:03d}-",
                                       dir=scratch_root))
    cnf_name = "input.cnf"
    proof_name = ("proof.out" if certificate == "auto" else
                  "proof.dsr" if certificate == "dsr" else "proof.drat")
    cnf = scratch / cnf_name
    proof = scratch / proof_name
    stdout = scratch / "solver.stdout"
    stderr = scratch / "solver.stderr"
    result = None
    preflight_payload = None
    retained_proof = None
    try:
        validate_runtime_pin_contract(CAMPAIGN, manifest)
        retention_contract = failed_proof_retention_contract(manifest)
        if retention_contract is not None and certificate != "dsr":
            raise RuntimeError("failed-proof retention requires explicit DSR")
        if sha256(compressed) != manifest["input_sha256"][row["instance"]]:
            raise RuntimeError("compressed input hash mismatch")
        with open(cnf, "wb") as target:
            subprocess.run(["xz", "-dc", compressed], stdout=target, check=True)
        decompressed_input_sha256 = sha256(cnf)
        for rel, expected in manifest["pin_sha256"].items():
            if sha256(CAMPAIGN / rel) != expected:
                raise RuntimeError(f"pin hash mismatch: {rel}")
        for name in list(os.environ):
            if name.startswith(("TACTIX_", "KISSAT_", "SAT_EVENT_", "SAT_INPROCESS_")):
                del os.environ[name]
        affinity = sorted(os.sched_getaffinity(0))
        instrumentation = ()
        if telemetry_paths is not None:
            instrumentation = (
                f"--feedback={telemetry_paths['feedback']}",
                f"--structural-provenance={telemetry_paths['structural_provenance']}",
            )
        command = ["taskset", "-c", str(affinity[0]),
                   *solver_command(binary, arm, cnf, proof, instrumentation)]
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
        if preflight_contract is not None:
            preflight_payload = terminal_counter_summary(
                stdout, [*arm.get("arguments", []),
                         *arm.get("trailing_arguments", [])], verdict)
            statistics = preflight_payload["lines"]
        if telemetry_contract is not None:
            from gf2_target_score import expected_checker_hashes
            checker_hashes = expected_checker_hashes(manifest, arm)
            telemetry_payload = collect_gf2_telemetry(
                CAMPAIGN, task, telemetry_contract,
                telemetry_paths["feedback"],
                telemetry_paths["structural_provenance"], statistics,
                {"manifest_sha256": manifest_sha256,
                 "compressed_input_sha256": manifest["input_sha256"][row["instance"]],
                 "decompressed_input_sha256": decompressed_input_sha256,
                 "binary_sha256": arm["sha256"],
                 "checker_sha256": checker_hashes})
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
            "certificate_bytes": proof.stat().st_size if proof.is_file() else None,
            "certificate_sha256": sha256(proof) if proof.is_file() else None,
            "proof_check": None,
            "model_check": None,
            "valid": False,
            "cleanup_pending": True,
        }
        if telemetry_payload is not None:
            result["gf2_telemetry"] = telemetry_payload
        if preflight_payload is not None:
            result["causal_preflight"] = preflight_payload
        # Publish solver timing and verdict before potentially long proof
        # checking.  This is deliberately separate from the final validity
        # result and never satisfies the cumulative correctness gate.
        atomic_json(provisional_path, provisional_result(
            result, dt.datetime.now(dt.timezone.utc).isoformat()))
        release_scratch(scratch, 'solver.stderr', 'solve.time', 'solver.perf')
        if verdict == "UNSAT" and proof.is_file():
            reported_route = solver_reported_route(stdout)
            release_scratch(scratch, 'solver.stdout')
            checked = check_unsat_proof(
                CAMPAIGN, certificate, cnf, proof, scratch, check_timeout,
                reported_route=reported_route, release_intermediates=True,
                retain_original_proof=retention_contract is not None,
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
        retention = retention_contract
        if (retention is not None and verdict == "UNSAT" and proof.is_file() and
                not result["valid"]):
            maximum = retention["maximum_bytes"]
            directory = retention["directory"]
            relative = Path(directory)
            size = proof.stat().st_size
            if size <= maximum:
                destination = CAMPAIGN / relative / f"{task:03d}.dsr"
                if destination.exists():
                    raise RuntimeError("retained proof path already exists")
                atomic_retain(proof, destination, maximum)
                if (destination.stat().st_size != size or
                        sha256(destination) != result["certificate_sha256"]):
                    destination.unlink(missing_ok=True)
                    raise RuntimeError("retained proof copy mismatch")
                retained_proof = destination
                result["retained_artifact"] = str(destination)
                result["retained_artifact_bytes"] = size
                result["retained_artifact_sha256"] = result["certificate_sha256"]
            else:
                result["retention_skipped"] = {
                    "reason": "certificate_exceeds_maximum_bytes",
                    "certificate_bytes": size,
                    "maximum_bytes": maximum,
                }
    except Exception as error:
        result = {
            "schema": result_schema,
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
        if telemetry_local is not None:
            shutil.rmtree(telemetry_local, ignore_errors=True)
    if telemetry_contract is not None and "gf2_telemetry" not in result:
        result["gf2_telemetry"] = telemetry_payload or {
            "schema": telemetry_contract["schema"],
            "bindings": {"manifest_sha256": manifest_sha256},
            "sidecars": {},
            "integrity_error": "telemetry collection did not complete",
        }
    if preflight_contract is not None and "causal_preflight" not in result:
        result["causal_preflight"] = preflight_payload or {
            "schema": "tactix-causal-preflight-terminal-v1",
            "verdict": result.get("verdict", "INFRA_ERROR"),
            "controller_rules": {},
            "controller_producers": {},
            "controller_loop": None,
            "lines": [],
            "full_telemetry_retained": False,
            "integrity_error": "runtime ended before terminal counters",
        }
    result["artifact_deleted"] = not scratch.exists() and retained_proof is None
    result["derived_artifacts_deleted"] = not scratch.exists()
    result["cleanup_pending"] = (scratch.exists() or retained_proof is not None or
                                 (telemetry_local is not None and
                                  telemetry_local.exists()))
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    atomic_json(result_path, result)
    return 0 if result["valid"] and result["artifact_deleted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

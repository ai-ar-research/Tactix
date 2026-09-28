#!/usr/bin/env python3
"""Freeze, validate, run, and aggregate the 20 winner-VBS matched reruns.

Every large formula, proof, model, solver stream, and checker intermediate is
an O_TMPFILE on node-local storage.  The only persistent task artifacts are
bounded JSON records and Slurm logs.  Preparation is fail-closed against the
final selector evidence and pins the exact Tactix and baseline executables.
"""

import argparse
from collections import Counter, deque
import csv
import datetime as dt
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

from proof_oracle import (DSRFormatError, bridge_trivial_input_lrat,
                          canonical_checker_cnf, certificate_route,
                          drat_command_with_timeout, dsr_proof_as_drat,
                          proof_check_status, restate_duplicate_input_lrat)


SOURCE_ROOT = Path(__file__).resolve().parents[2]
SOLVE_SECONDS = 1200
CHECK_SECONDS = 16000
MEMORY_MIB = 32768
CPUS = 12
PARTITION = "cpu-dense-hwulab-q"
ENVIRONMENT_FIXED = {
    "LANG": "C",
    "LD_LIBRARY_PATH": (
        "/cm/local/apps/gcc/13.1.0/lib64:/cm/local/apps/gcc/13.1.0/lib:"
        "/cm/local/apps/gcc/13.1.0/lib32"
    ),
    "LC_ALL": "C",
    "PATH": (
        "/cm/local/apps/python3/bin:/usr/local/bin:/usr/bin:"
        "/usr/local/sbin:/usr/sbin:/bin"
    ),
    "PYTHONHASHSEED": "0",
    "TMPDIR": "/tmp",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def source_digest(path):
    path = Path(path)
    if path.is_file():
        return sha256(path)
    digest = hashlib.sha256()
    for item in sorted(p for p in path.rglob("*") if p.is_file()):
        digest.update(str(item.relative_to(path)).encode() + b"\0")
        digest.update(bytes.fromhex(sha256(item)))
    return digest.hexdigest()


def atomic_json(path, value, exclusive=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if exclusive:
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        with os.fdopen(os.open(path, flags, 0o444), "w") as target:
            json.dump(value, target, indent=2, sort_keys=True)
            target.write("\n")
            target.flush()
            os.fsync(target.fileno())
        return
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    with open(temporary, "x") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
        target.flush()
        os.fsync(target.fileno())
    os.replace(temporary, path)


def copy_pin(source, target):
    if target.exists():
        raise FileExistsError(f"pin already exists: {target}")
    if source.is_dir():
        # Dereference the historical absolute build symlinks so a new pin is
        # self-contained.  source_digest follows those links too, so the
        # recorded content digest remains identical.
        shutil.copytree(source, target, symlinks=False)
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def readonly(path):
    paths = [path] if path.is_file() else [p for p in path.rglob("*") if p.is_file()]
    for item in paths:
        item.chmod(item.stat().st_mode & ~0o222)


def resolve_evidence(root, text):
    path = Path(text)
    return path if path.is_absolute() else root / path


def evidence_has(selection, root, path):
    wanted = Path(path).resolve()
    matches = []
    for label, expected in selection["evidence_sha256"].items():
        if resolve_evidence(root, label).resolve() == wanted:
            matches.append(expected)
    require(len(matches) == 1, f"selection evidence identity mismatch: {path}")
    require(sha256(wanted) == matches[0], f"selection evidence changed: {path}")


def normalized_selection(selection, root):
    value = json.loads(json.dumps(selection))
    value.pop("observed_at", None)
    # Unselected solved records may be promoted from provisional to checked
    # final records while this tool prepares the campaign.  That is allowed by
    # the selector contract and does not change the timeout set.  Recompute the
    # entire outcome below, while binding the selector, manifests, matrix,
    # baseline aggregate and every selected timeout separately in evidence_has.
    value.pop("evidence_sha256", None)
    for group in ("selected", "candidates"):
        for row in value.get(group, []):
            row["tactix_record"] = str(
                resolve_evidence(root, row["tactix_record"]).resolve()
            )
    return value


def recompute_selection(baseline, tactix, selection, root):
    selector_path = root / "tools/bench/portfolio_targets.py"
    spec = importlib.util.spec_from_file_location(
        "frozen_portfolio_targets", selector_path
    )
    require(spec is not None and spec.loader is not None, "cannot load winner selector")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    fresh = module.snapshot(
        baseline,
        tactix,
        selection["seconds"],
        selection["memory_kib"],
        selection["per_status"],
    )
    require(
        normalized_selection(fresh, root) == normalized_selection(selection, root),
        "winner selection no longer matches a fresh fail-closed snapshot",
    )


def verify_selection(
    selection_path, baseline, tactix, root=SOURCE_ROOT, recompute=True
):
    selection_path = Path(selection_path).resolve()
    baseline, tactix, root = (
        Path(baseline).resolve(),
        Path(tactix).resolve(),
        Path(root).resolve(),
    )
    selection = json.loads(selection_path.read_text())
    require(
        selection.get("schema") == "tactix-winner-coverage-selection-v1",
        "unsupported winner selection schema",
    )
    require(selection.get("selection_final") is True, "winner selection is not final")
    classes = selection.get("tactix_classes") or {}
    require(
        classes.get("solver_pending_or_missing", 0) == 0
        and classes.get("timeout_cleanup_pending", 0) == 0,
        "winner selection still has unresolved Tactix outcomes",
    )
    require(
        selection.get("seconds") == SOLVE_SECONDS, "selection solver budget differs"
    )
    require(
        selection.get("memory_kib") == MEMORY_MIB * 1024,
        "selection memory budget differs",
    )
    per_status = selection.get("per_status")
    selected = selection.get("selected") or []
    require(
        per_status == 10 and len(selected) == 20,
        "selection must contain ten SAT and ten UNSAT",
    )
    require(
        Counter(row.get("verdict") for row in selected) == {"SAT": 10, "UNSAT": 10},
        "selection status quotas differ",
    )
    require(
        len({row.get("instance") for row in selected}) == 20,
        "selection contains duplicate instances",
    )
    baseline_manifest_path = baseline / "manifest.json"
    baseline_results_path = baseline / "results.json"
    tactix_manifest_path = tactix / "manifest.json"
    tactix_matrix_path = tactix / "matrix.tsv"
    for path in (
        baseline_manifest_path,
        baseline_results_path,
        baseline / "validation.json",
        tactix_manifest_path,
        tactix_matrix_path,
        root / "tools/bench/portfolio_targets.py",
    ):
        require(path.is_file(), f"missing selection authority: {path}")
        evidence_has(selection, root, path)
    if recompute:
        recompute_selection(baseline, tactix, selection, root)

    bm = json.loads(baseline_manifest_path.read_text())
    br = json.loads(baseline_results_path.read_text())
    tm = json.loads(tactix_manifest_path.read_text())
    require(
        sha256(baseline / "cache.sha256") == bm["cache_manifest_sha256"],
        "baseline cache manifest differs",
    )
    require(
        br.get("manifest_sha256") == sha256(baseline_manifest_path),
        "baseline aggregate authority differs",
    )
    require(
        br.get("complete") is True and len(br.get("results", [])) == len(bm["tasks"]),
        "baseline aggregate is incomplete",
    )
    require(
        tm["resource_contract"]["solve_timeout"] == SOLVE_SECONDS
        and tm["resource_contract"]["memory_mib"] == MEMORY_MIB,
        "source Tactix campaign resource contract differs",
    )
    require(
        tm["dataset_authority"]["sha256"] == sha256(baseline_manifest_path),
        "Tactix and baseline dataset authorities differ",
    )
    with tactix_matrix_path.open(newline="") as source:
        tactix_rows = list(csv.DictReader(source, delimiter="\t"))
    baseline_by_task = {row["task"]: row for row in br["results"]}
    baseline_solvers = {row["name"]: row for row in bm["solvers"]}
    targets = []
    for target, selected_row in enumerate(selected):
        tactix_task = selected_row["tactix_task"]
        require(
            type(tactix_task) is int and 0 <= tactix_task < len(tactix_rows),
            "invalid selected Tactix task",
        )
        source_row = tactix_rows[tactix_task]
        require(
            source_row["instance"] == selected_row["instance"]
            and source_row["expected"] in (selected_row["verdict"], "UNKNOWN"),
            "selected Tactix identity differs",
        )
        record_path = resolve_evidence(root, selected_row["tactix_record"]).resolve()
        expected_record = (tactix / "results" / f"{tactix_task:03d}.json").resolve()
        require(
            record_path == expected_record and record_path.is_file(),
            "selection does not name the final Tactix record",
        )
        evidence_has(selection, root, record_path)
        tactix_record = json.loads(record_path.read_text())
        require(
            tactix_record.get("verdict") == "UNKNOWN"
            and tactix_record.get("solve", {}).get("timed_out") is True
            and tactix_record.get("artifact_deleted") is True
            and tactix_record.get("cleanup_pending") is False,
            "selected Tactix record is not a cleaned timeout",
        )

        task_ids = selected_row.get("baseline_tasks") or []
        records = [baseline_by_task.get(task) for task in task_ids]
        require(records and all(records), "selected baseline task is absent")
        fastest = min(
            records, key=lambda row: (row["solve"]["wall_seconds"], row["solver"])
        )
        require(
            fastest["solver"] == selected_row["fastest_solver"]
            and fastest["solve"]["wall_seconds"] == selected_row["fastest_seconds"]
            and fastest["instance"] == selected_row["instance"]
            and fastest["verdict"] == selected_row["verdict"]
            and fastest.get("valid") is True
            and fastest.get("cleanup_pending") is False
            and fastest.get("artifact_deleted") is True
            and fastest.get("derived_artifacts_deleted") is True,
            "selected fastest baseline evidence differs",
        )
        require(
            fastest["solver"] in baseline_solvers,
            "selected solver absent from baseline manifest",
        )
        targets.append(
            {
                "target": target,
                "instance": selected_row["instance"],
                "expected": selected_row["verdict"],
                "baseline_solver": fastest["solver"],
                "baseline_task": fastest["task"],
                "baseline_seconds": fastest["solve"]["wall_seconds"],
                "tactix_task": tactix_task,
                "tactix_seconds": tactix_record["solve"]["wall_seconds"],
                "coverage_solvers": selected_row["solvers"],
            }
        )
    return selection, bm, tm, targets


def build_matrix(targets):
    rows = []
    for target in targets:
        for side, arm in (
            ("tactix", "tactix"),
            ("baseline", target["baseline_solver"]),
        ):
            rows.append(
                {
                    "task": len(rows),
                    "target": target["target"],
                    "side": side,
                    "arm": arm,
                    "instance": target["instance"],
                    "expected": target["expected"],
                    "selection_tactix_task": target["tactix_task"],
                    "selection_baseline_task": target["baseline_task"],
                }
            )
    return rows


def batch_text(campaign, tasks, validation=False):
    campaign = Path(campaign).resolve()
    if validation:
        name, wall, body = (
            "winner-rerun-check",
            "01:00:00",
            (
                f'exec python3 "{campaign}/pins/portfolio_rerun.py" validate "{campaign}"'
            ),
        )
        array = ""
    else:
        name, wall, body = (
            "winner-rerun",
            "05:00:00",
            (
                f'exec python3 "{campaign}/pins/portfolio_rerun.py" task "{campaign}" "$SLURM_ARRAY_TASK_ID"'
            ),
        )
        array = f"#SBATCH --array=0-{tasks - 1}\n"
    return f"""#!/usr/bin/env bash
#SBATCH --job-name={name}
#SBATCH --partition={PARTITION}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={CPUS}
#SBATCH --mem={MEMORY_MIB}M
#SBATCH --time={wall}
{array}#SBATCH --no-requeue
#SBATCH --chdir={campaign.parents[2]}
#SBATCH --output={campaign}/logs/%x-%A_%a.log
set -euo pipefail
ulimit -c 0
{body}
"""


def prepare(args):
    root = SOURCE_ROOT.resolve()
    campaign = args.campaign.resolve()
    require(
        campaign.parent == root / "benchmarks/runs",
        "campaign must be directly under benchmarks/runs",
    )
    require(not campaign.exists(), "campaign already exists")
    selection, bm, tm, targets = verify_selection(
        args.selection, args.baseline, args.tactix_campaign, root
    )
    baseline, tactix = args.baseline.resolve(), args.tactix_campaign.resolve()
    rows = build_matrix(targets)
    require(len(rows) == 40, "matched matrix must contain 40 tasks")

    campaign.mkdir()
    for name in ("pins", "claims", "provisional", "results", "logs"):
        (campaign / name).mkdir()
    pin_digests = {}

    def pin(source, name):
        target = campaign / "pins" / name
        copy_pin(Path(source).resolve(), target)
        readonly(target)
        key = str(target.relative_to(campaign))
        pin_digests[key] = source_digest(target)
        return key

    selection_pin = pin(args.selection, "selection.json")
    tactix_binary = tactix / tm["arms"]["tactix"]["path"]
    require(
        sha256(tactix_binary) == tm["arms"]["tactix"]["sha256"],
        "source Tactix binary differs",
    )
    tactix_pin = pin(tactix_binary, "tactix")
    arms = {
        "tactix": {
            "name": "tactix",
            "execution": "positional",
            "certificate": "drat",
            "path": tactix_pin,
            "sha256": sha256(campaign / tactix_pin),
            "source_commit": tm["source_commit"],
            "arguments": [],
        }
    }
    baseline_solvers = {row["name"]: row for row in bm["solvers"]}
    for name in sorted({target["baseline_solver"] for target in targets}):
        entrypoint = baseline / baseline_solvers[name]["executable"]
        source = entrypoint
        output_directory = baseline_solvers[name]["kind"] == "output-directory"
        if output_directory:
            source = baseline / "pins" / name
        require(
            source_digest(entrypoint) == bm["provenance"][name]["sha256"],
            f"baseline entrypoint differs: {name}",
        )
        source_tree_digest = source_digest(source)
        rel = pin(source, name)
        execution = {
            "satsuma-kissat": "satsuma-anonymous",
            "lymphosat": "lymphosat-anonymous",
        }.get(name, "positional")
        arms[name] = {
            **baseline_solvers[name],
            "execution": execution,
            "path": rel,
            "source_digest": pin_digests[rel],
            "source_tree_digest": source_tree_digest,
            "entrypoint_sha256": source_digest(entrypoint),
            "baseline_provenance": bm["provenance"][name],
        }
        require(
            pin_digests[rel] == source_tree_digest,
            f"baseline solver pin differs: {name}",
        )

    if "satsuma-kissat" in arms:
        shim_source = pin(
            Path(__file__).with_name("procfd_realpath.c"), "procfd_realpath.c"
        )
        shim_path = campaign / "pins/procfd_realpath.so"
        compiler = shutil.which("cc")
        require(
            compiler is not None, "C compiler unavailable for proc-fd transport shim"
        )
        shim_command = [
            compiler,
            "-shared",
            "-fPIC",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-o",
            str(shim_path),
            str(campaign / shim_source),
            "-ldl",
        ]
        subprocess.run(shim_command, check=True)
        readonly(shim_path)
        shim_rel = str(shim_path.relative_to(campaign))
        pin_digests[shim_rel] = sha256(shim_path)
        arms["satsuma-kissat"].update(
            transport_shim=shim_rel,
            transport_shim_source=shim_source,
            transport_shim_build=shim_command,
            transport_shim_compiler=subprocess.check_output(
                [compiler, "--version"], text=True
            ).splitlines()[0],
        )

    checker_names = {"drat-trim", "cake-lpr", "checkmodel", "modelcheck.py"}
    if any(arm["certificate"] == "dsr" for arm in arms.values()):
        checker_names.update({"dsr-trim", "dsr-compress", "srcheck"})
    if any(arm["certificate"] == "pbp" for arm in arms.values()):
        checker_names.update({"veripb", "cake-pb-cnf"})
    checkers = {}
    for name in sorted(checker_names):
        rel = pin(baseline / "pins" / name, name)
        checkers[name] = rel
    runner = pin(Path(__file__), "portfolio_rerun.py")
    anonymous = pin(
        Path(__file__).with_name("anonymous_scratch.py"), "anonymous_scratch.py"
    )

    matrix_path = campaign / "matrix.tsv"
    with matrix_path.open("x", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    input_sha256 = {}
    baseline_inputs = {
        fields[1]: fields[0]
        for fields in (
            line.split()
            for line in (baseline / "cache.sha256").read_text().splitlines()
        )
    }
    for target in targets:
        cached = root / "benchmarks/cache" / target["instance"]
        require(
            cached.is_file() and sha256(cached) == baseline_inputs[target["instance"]],
            f"cached input differs: {target['instance']}",
        )
        input_sha256[target["instance"]] = baseline_inputs[target["instance"]]

    array_path = campaign / "array.sbatch"
    validate_path = campaign / "validate.sbatch"
    array_path.write_text(batch_text(campaign, len(rows)))
    validate_path.write_text(batch_text(campaign, len(rows), validation=True))
    for path in (matrix_path, array_path, validate_path):
        readonly(path)
        pin_digests[str(path.relative_to(campaign))] = sha256(path)

    manifest = {
        "schema": "tactix-winner-vbs-matched-rerun-v1",
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "selection": {
            "path": selection_pin,
            "sha256": sha256(campaign / selection_pin),
            "source_path": str(args.selection.resolve()),
            "evidence_sha256": selection["evidence_sha256"],
        },
        "source_campaigns": {
            "tactix": {
                "path": str(tactix),
                "manifest_sha256": sha256(tactix / "manifest.json"),
            },
            "baseline": {
                "path": str(baseline),
                "manifest_sha256": sha256(baseline / "manifest.json"),
                "aggregate_sha256": sha256(baseline / "results.json"),
            },
        },
        "targets": targets,
        "arms": arms,
        "checkers": checkers,
        "input_sha256": input_sha256,
        "pin_digests": pin_digests,
        "resource_contract": {
            "partition": PARTITION,
            "cpus_per_task": CPUS,
            "solver_threads": 1,
            "memory_mib": MEMORY_MIB,
            "solve_timeout": SOLVE_SECONDS,
            "checker_timeout": CHECK_SECONDS,
            "scheduler_wall_seconds": 18000,
            "array": f"0-{len(rows) - 1}",
            "throttle": None,
            "node_constraint": None,
        },
        "solver_environment": clean_environment(),
        "scratch_policy": "anonymous-node-local-otmpfile-v1",
        "cleanup_contract": (
            "Formula, proof, model, solver streams, metrics, profile, transformed CNF "
            "and checker intermediates use O_TMPFILE under /tmp. No directory entry "
            "exists, and the kernel releases storage after normal exit, timeout, "
            "SIGKILL or OOM. Only bounded JSON and Slurm logs persist."
        ),
        "pairing": "instance-major Tactix then selected fastest baseline; full-width array",
        "runner": runner,
        "anonymous_scratch": anonymous,
    }
    atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    readonly(campaign / "manifest.json")
    verify_campaign(campaign, full=True)
    print(campaign)


def load_matrix(campaign):
    with (campaign / "matrix.tsv").open(newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    require(
        all(int(row["task"]) == i for i, row in enumerate(rows)),
        "matrix task order differs",
    )
    return rows


def required_pins(manifest, arm_name=None):
    required = {
        manifest["runner"],
        manifest["anonymous_scratch"],
        "matrix.tsv",
        manifest["selection"]["path"],
    }
    if arm_name is None:
        return set(manifest["pin_digests"])
    arm = manifest["arms"][arm_name]
    required.add(arm["path"])
    if arm.get("transport_shim"):
        required.add(arm["transport_shim"])
    checker_names = {"checkmodel", "modelcheck.py"}
    checker_names.update(
        {
            "drat": {"drat-trim", "cake-lpr"},
            "dsr": {"dsr-trim", "dsr-compress", "srcheck"},
            "pbp": {"veripb", "cake-pb-cnf"},
        }[arm["certificate"]]
    )
    required.update(manifest["checkers"][name] for name in checker_names)
    return required


def verify_campaign(campaign, full=False, arm_name=None):
    campaign = Path(campaign).resolve()
    manifest = json.loads((campaign / "manifest.json").read_text())
    require(
        manifest.get("schema") == "tactix-winner-vbs-matched-rerun-v1",
        "unsupported rerun manifest",
    )
    contract = manifest["resource_contract"]
    require(
        contract
        == {
            "partition": PARTITION,
            "cpus_per_task": CPUS,
            "solver_threads": 1,
            "memory_mib": MEMORY_MIB,
            "solve_timeout": SOLVE_SECONDS,
            "checker_timeout": CHECK_SECONDS,
            "scheduler_wall_seconds": 18000,
            "array": "0-39",
            "throttle": None,
            "node_constraint": None,
        },
        "rerun resource contract differs",
    )
    environment = manifest.get("solver_environment")
    require(
        isinstance(environment, dict)
        and environment.get("PATH")
        and clean_environment() == environment,
        "solver environment contract differs",
    )
    keys = set(manifest["pin_digests"]) if full else required_pins(manifest, arm_name)
    for rel in sorted(keys):
        path = campaign / rel
        require(path.exists(), f"missing pin: {rel}")
        require(
            source_digest(path) == manifest["pin_digests"][rel], f"pin changed: {rel}"
        )
    rows = load_matrix(campaign)
    require(len(rows) == 40, "rerun matrix does not contain 40 tasks")
    return manifest, rows


def process_group():
    os.setsid()


def bound_launch(campaign, validation=False, task=None, wait_seconds=30):
    name = "validation-launch.json" if validation else "launch.json"
    path = Path(campaign) / name
    deadline = time.monotonic() + wait_seconds
    while True:
        try:
            record = json.loads(path.read_text())
            break
        except FileNotFoundError:
            require(
                time.monotonic() < deadline,
                f"{name} was not published after Slurm submission",
            )
            time.sleep(0.05)
    require(
        record.get("manifest_sha256") == sha256(Path(campaign) / "manifest.json"),
        f"{name} manifest authority differs",
    )
    if validation:
        require(
            record.get("job_id") == os.environ.get("SLURM_JOB_ID"),
            "validation job identity differs from its launch record",
        )
    else:
        require(
            record.get("job_id") == os.environ.get("SLURM_ARRAY_JOB_ID"),
            "array job identity differs from its launch record",
        )
        require(
            task is not None and os.environ.get("SLURM_ARRAY_TASK_ID") == str(task),
            "array task identity differs",
        )
    return record


def measured(command, stdout, stderr, metrics, timeout, env=None):
    wrapped = [
        "/usr/bin/time",
        "-f",
        "max_rss_kb=%M",
        "-o",
        str(metrics),
        "--",
        *map(str, command),
    ]
    started = time.monotonic()
    timed_out = False
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        process = subprocess.Popen(
            wrapped, stdout=out, stderr=err, preexec_fn=process_group, env=env
        )
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
    peak = None
    try:
        match = re.search(r"^max_rss_kb=(\d+)$", Path(metrics).read_text(), re.M)
        peak = int(match.group(1)) if match else None
    except OSError:
        pass
    return {
        "exit_code": exit_code,
        "wall_seconds": round(time.monotonic() - started, 3),
        "peak_rss_kb": peak,
        "timed_out": timed_out,
    }


def tail(path, limit=80):
    lines = deque(maxlen=limit)
    try:
        with open(path, errors="replace") as source:
            for line in source:
                lines.append(line.rstrip("\n")[-4096:])
    except OSError:
        pass
    return list(lines)


def solver_summary(path):
    states, selected = [], deque(maxlen=260)
    try:
        with open(path, errors="replace") as source:
            for raw in source:
                line = raw.rstrip("\n")
                match = re.match(r"^s\s+(SATISFIABLE|UNSATISFIABLE)\s*$", line)
                if match:
                    states.append(match.group(1))
                if line.startswith(("c ", "s ")):
                    selected.append(line[-4096:])
    except OSError:
        pass
    verdict = "UNKNOWN"
    if len(states) == 1:
        verdict = "SAT" if states[0] == "SATISFIABLE" else "UNSAT"
    return verdict, list(selected)


def clean_environment():
    return dict(ENVIRONMENT_FIXED)


def truncate(path):
    with open(path, "wb"):
        pass


def adapter_satsuma(campaign, arm_name, cnf, proof, transformed):
    manifest = json.loads((campaign / "manifest.json").read_text())
    arm = manifest["arms"][arm_name]
    root = campaign / arm["path"]
    truncate(proof)
    truncate(transformed)
    print(f"c Running satsuma on {cnf}")
    subprocess.run(
        [
            root / "satsuma",
            "fix",
            cnf,
            "--silent",
            "--full-skip-limit",
            "100000000",
            "--add-reduced-as-unit",
            "--bsr",
            "--proof-file",
            proof,
            "--out-file",
            transformed,
        ]
    )
    # The submitted run_base.sh invokes Kissat even if preprocessing reports
    # an error, and returns Kissat's status.  Preserve that exact behavior.
    environment = os.environ.copy()
    environment["LD_PRELOAD"] = str(campaign / arm["transport_shim"])
    return subprocess.run(
        [root / "solver/kissat/build/kissat", transformed, proof], env=environment
    ).returncode


def replay(source, target):
    with open(source, "rb") as stream:
        shutil.copyfileobj(stream, target)
    target.flush()


def adapter_lymphosat(
    campaign, arm_name, cnf, proof, profile, attempt_out, attempt_err
):
    manifest = json.loads((campaign / "manifest.json").read_text())
    root = campaign / manifest["arms"][arm_name]["path"] / "solver"
    composite = root / "solver"
    profile_run = subprocess.run(
        [composite, cnf, "--profile-only", profile],
        stdout=subprocess.DEVNULL,
        stderr=sys.stderr,
    )
    if profile_run.returncode:
        return profile_run.returncode
    listed = subprocess.run(
        [composite, "--list-matchers"], capture_output=True, text=True
    )
    if listed.returncode:
        sys.stderr.write(listed.stderr)
        return listed.returncode
    names = [line.strip() for line in listed.stdout.splitlines() if line.strip()]

    def attempt(name):
        truncate(proof)
        truncate(attempt_out)
        truncate(attempt_err)
        binary = root / "solvers" / name / "solver" / "solver"
        with open(attempt_out, "wb") as out, open(attempt_err, "wb") as err:
            rc = subprocess.run([binary, cnf, proof], stdout=out, stderr=err).returncode
        if rc in (10, 20):
            replay(attempt_out, sys.stdout.buffer)
            replay(attempt_err, sys.stderr.buffer)
        elif rc == 0:
            print(f"c composite: {name} returned UNKNOWN; continuing", file=sys.stderr)
        else:
            print(f"c composite: {name} exited {rc}; continuing", file=sys.stderr)
        return rc

    for name in names:
        match = subprocess.run(
            [composite, "--check-match", profile, name], capture_output=True, text=True
        )
        if match.returncode:
            print(
                f"c composite: matcher {name} check failed; skipping", file=sys.stderr
            )
            continue
        if match.stdout.strip() != "1":
            continue
        print(f"c composite: matcher {name} accepted formula", file=sys.stderr)
        rc = attempt(name)
        if rc in (10, 20):
            return rc
    rc = attempt("fallback")
    if rc in (0, 10, 20):
        return rc
    print("s UNKNOWN")
    return 0


def solver_command(campaign, manifest, arm_name, scratch, cnf, proof):
    arm = manifest["arms"][arm_name]
    execution = arm["execution"]
    if execution == "positional":
        return [campaign / arm["path"], *arm.get("arguments", []), cnf, proof]
    runner = campaign / manifest["runner"]
    if execution == "satsuma-anonymous":
        return [
            sys.executable,
            runner,
            "adapter-satsuma",
            campaign,
            arm_name,
            cnf,
            proof,
            scratch / "transformed.cnf",
        ]
    if execution == "lymphosat-anonymous":
        return [
            sys.executable,
            runner,
            "adapter-lymphosat",
            campaign,
            arm_name,
            cnf,
            proof,
            scratch / "profile.json",
            scratch / "attempt.stdout",
            scratch / "attempt.stderr",
        ]
    raise ValueError(f"unsupported execution adapter: {execution}")


def checker_ok(name, result, stdout, stderr):
    text = "\n".join(tail(stdout, 120) + tail(stderr, 120))
    if name == "drat-trim":
        return (
            result["exit_code"] == 0
            and "s VERIFIED" in text
            and "s NOT VERIFIED" not in text
        )
    if name == "cake-lpr":
        return (
            result["exit_code"] == 0
            and "s VERIFIED UNSAT" in text
            and "NOT VERIFIED" not in text
            and "INVALID" not in text
        )
    if name == "dsr-trim":
        return result["exit_code"] == 0 and "s VERIFIED UNSAT" in text
    if name == "dsr-compress":
        return (
            result["exit_code"] == 0
            and "ERROR" not in text
            and "not supported" not in text
        )
    if name == "srcheck":
        return (
            result["exit_code"] == 0
            and "s VERIFIED UNSAT" in text
            and "s INVALID" not in text
        )
    if name == "veripb":
        return result["exit_code"] == 0 and "s VERIFIED UNSATISFIABLE" in text
    if name == "cake-pb-cnf":
        return (
            result["exit_code"] == 0
            and ("s VERIFIED UNSAT" in text or "VERIFIED" in text)
            and "NOT VERIFIED" not in text
            and "INVALID" not in text
        )
    return False


def proof_specs(campaign, manifest, certificate, cnf, proof, scratch):
    def checker(name):
        return campaign / manifest["checkers"][name]

    if certificate == "drat":
        lrat = scratch / "proof.lrat"
        return [
            ("drat-trim", [checker("drat-trim"), cnf, proof, "-L", lrat]),
            ("cake-lpr", [checker("cake-lpr"), cnf, lrat]),
        ]
    if certificate == "dsr":
        binary_lsr, text_lsr = scratch / "proof.binary.lsr", scratch / "proof.text.lsr"
        return [
            ("dsr-trim", [checker("dsr-trim"), cnf, proof, binary_lsr]),
            ("dsr-compress", [checker("dsr-compress"), "-d", binary_lsr, text_lsr]),
            ("srcheck", [checker("srcheck"), cnf, text_lsr]),
        ]
    if certificate == "pbp":
        kernel = scratch / "proof.kernel.pbp"
        return [
            (
                "veripb",
                [checker("veripb"), "-u", "--cnf", "--elaborate", kernel, cnf, proof],
            ),
            ("cake-pb-cnf", [checker("cake-pb-cnf"), cnf, kernel]),
        ]
    raise ValueError(f"unsupported certificate format: {certificate}")


def check_proof(campaign, manifest, certificate, cnf, proof, scratch, timeout,
                route_contract=None):
    started, stages = time.monotonic(), []
    content_route = None
    checker_input = None
    spelling = None
    if certificate == 'dsr' and route_contract == 'dsr-hybrid':
        try:
            content_route = certificate_route(proof)
            if content_route not in ('drat', 'dsr'):
                raise DSRFormatError('Satsuma DSR arm emitted a PBP certificate')
            checker_input = scratch / 'checker.cnf'
            spelling = canonical_checker_cnf(cnf, checker_input)
            if content_route == 'drat':
                converted = scratch / 'proof.drat'
                if not dsr_proof_as_drat(proof, converted):
                    raise DSRFormatError('witnessed DSR cannot use DRAT route')
                lrat = scratch / 'proof.lrat'
                specs = proof_specs(campaign, manifest, 'drat', checker_input,
                                    converted, scratch)
            else:
                specs = proof_specs(campaign, manifest, 'dsr', checker_input,
                                    proof, scratch)
        except (DSRFormatError, ValueError) as error:
            return {'pipeline': 'dsr-hybrid', 'certificate_content_route': None,
                    'result': 'not-verified', 'stages': [],
                    'route_error': str(error), 'wall_seconds': round(time.monotonic()-started, 3),
                    'peak_rss_kb': 0, 'timed_out': False}
        pipeline = ('dsr-as-drat-drat-trim-cake-lpr' if content_route == 'drat'
                    else 'dsr-trim-dsr-compress-trestle')
    else:
        specs = proof_specs(campaign, manifest, certificate, cnf, proof, scratch)
        pipeline = certificate
    for number, (name, command) in enumerate(specs, 1):
        if content_route == 'drat' and name == 'cake-lpr':
            try:
                checked_lrat, restatements = restate_duplicate_input_lrat(
                    checker_input, spelling, lrat, scratch / 'proof.original-input.lrat')
            except ValueError as error:
                stages.append({'name': 'input-restatement', 'result': 'not-verified',
                               'error': str(error), 'timed_out': False,
                               'wall_seconds': 0, 'peak_rss_kb': 0})
                break
            command = [*command[:-1], checked_lrat]
            stages[-1]['input_restatements'] = restatements
        remaining = max(1, timeout - int(time.monotonic() - started))
        if name == 'drat-trim':
            command = drat_command_with_timeout(command, remaining)
        stdout, stderr = (
            scratch / f"checker-{number}.stdout",
            scratch / f"checker-{number}.stderr",
        )
        environment = dict(manifest["solver_environment"])
        stage_environment = {}
        if name in ("cake-lpr", "cake-pb-cnf"):
            environment["CML_HEAP_SIZE"] = "24576"
            stage_environment["CML_HEAP_SIZE_MIB"] = 24576
        result = measured(
            command,
            stdout,
            stderr,
            scratch / f"checker-{number}.time",
            remaining,
            environment,
        )
        verified = checker_ok(name, result, stdout, stderr)
        if content_route == 'drat' and name == 'drat-trim':
            output = '\n'.join(tail(stdout, 120) + tail(stderr, 120))
            verified = proof_check_status(
                name, result['exit_code'], output, '',
                input_has_empty_clause=spelling['first_empty_clause'] is not None
            ) == 'verified'
            if verified:
                result['input_empty_lrat_bridge'] = bridge_trivial_input_lrat(
                    spelling, lrat, result['exit_code'], output)
                verified = lrat.is_file() and lrat.stat().st_size > 0
        result.update(
            name=name,
            command=list(map(str, command)),
            timeout_seconds=remaining,
            checker={
                "path": str(command[0]),
                "sha256": manifest["pin_digests"][
                    str(Path(command[0]).relative_to(campaign))
                ],
            },
            environment=stage_environment,
            result="verified" if verified else "not-verified",
            stdout_tail=tail(stdout),
            stderr_tail=tail(stderr),
        )
        stages.append(result)
        if result["result"] != "verified":
            break
    return {
        "pipeline": pipeline,
        "certificate_content_route": content_route,
        "checker_input": spelling,
        "stages": stages,
        "result": "verified"
        if len(stages)
        == len(specs)
        and all(stage["result"] == "verified" for stage in stages)
        else "not-verified",
        "wall_seconds": round(sum(stage["wall_seconds"] for stage in stages), 3),
        "peak_rss_kb": max((stage["peak_rss_kb"] or 0 for stage in stages), default=0),
        "timed_out": any(stage["timed_out"] for stage in stages),
    }


def expected_matches(expected, verdict):
    return verdict in ("SAT", "UNSAT") and expected in (verdict, "UNKNOWN")


def checked_valid(expected, verdict, solve, certificate_ok):
    return bool(
        certificate_ok
        and expected_matches(expected, verdict)
        and not solve["timed_out"]
        and solve["exit_code"] == {"SAT": 10, "UNSAT": 20}.get(verdict)
    )


def classify_result(result):
    cleaned = (
        result.get("artifact_deleted") is True
        and result.get("derived_artifacts_deleted") is True
        and result.get("cleanup_pending") is False
    )
    if result.get("valid") is True and cleaned:
        return "checked_solve"
    solve = result.get("solve") or {}
    peak = solve.get("peak_rss_kb")
    memory_ok = (
        peak is None
        or isinstance(peak, (int, float))
        and not isinstance(peak, bool)
        and math.isfinite(peak)
        and 0 <= peak <= MEMORY_MIB * 1024
    )
    if (
        result.get("verdict") == "UNKNOWN"
        and solve.get("timed_out") is True
        and isinstance(solve.get("wall_seconds"), (int, float))
        and not isinstance(solve.get("wall_seconds"), bool)
        and solve["wall_seconds"] >= SOLVE_SECONDS
        and solve.get("exit_code") not in (10, 20)
        and result.get("memory_enforcement") == "slurm_task_cgroup"
        and memory_ok
        and cleaned
        and not result.get("proof_check")
        and not result.get("model_check")
    ):
        return "timeout"
    return "invalid"


def execute_case(
    campaign,
    manifest,
    arm_name,
    expected,
    cnf,
    scratch,
    solve_timeout=SOLVE_SECONDS,
    check_timeout=CHECK_SECONDS,
    provisional=None,
):
    affinity = sorted(os.sched_getaffinity(0))
    require(len(affinity) >= CPUS, f"allocation exposes only {len(affinity)} CPUs")
    proof, stdout, stderr = (
        scratch / "proof.out",
        scratch / "solver.stdout",
        scratch / "solver.stderr",
    )
    command = [
        "taskset",
        "-c",
        str(affinity[0]),
        *solver_command(campaign, manifest, arm_name, scratch, cnf, proof),
    ]
    solve = measured(
        command,
        stdout,
        stderr,
        scratch / "solver.time",
        solve_timeout,
        dict(manifest["solver_environment"]),
    )
    verdict, statistics = solver_summary(stdout)
    certificate = stdout if verdict == "SAT" else proof
    result = {
        "verdict": verdict,
        "expected": expected,
        "expected_match": expected_matches(expected, verdict),
        "solve": solve,
        "cpu_affinity": affinity,
        "solver_cpu": affinity[0],
        "command": list(map(str, command)),
        "arm": arm_name,
        "certificate_format": manifest["arms"][arm_name]["certificate"],
        "certificate_bytes": certificate.stat().st_size
        if verdict in ("SAT", "UNSAT")
        else None,
        "certificate_sha256": sha256(certificate)
        if verdict in ("SAT", "UNSAT")
        else None,
        "solver_statistics": statistics,
        "solver_stderr_tail": tail(stderr),
        "proof_check": None,
        "model_check": None,
        "valid": False,
        "memory_enforcement": "slurm_task_cgroup",
        "address_space_limit": None,
    }
    if provisional:
        provisional(result)
    certificate_ok = False
    if verdict == "UNSAT" and proof.stat().st_size:
        result["proof_check"] = check_proof(
            campaign,
            manifest,
            manifest["arms"][arm_name]["certificate"],
            cnf,
            proof,
            scratch,
            check_timeout,
            route_contract=manifest["arms"][arm_name].get("route"),
        )
        certificate_ok = result["proof_check"]["result"] == "verified"
    elif verdict == "SAT":
        checked = measured(
            [campaign / manifest["checkers"]["checkmodel"], cnf, stdout],
            scratch / "model.stdout",
            scratch / "model.stderr",
            scratch / "model.time",
            min(600, check_timeout),
            dict(manifest["solver_environment"]),
        )
        checked.update(
            result="verified" if checked["exit_code"] == 0 else "not-verified",
            checker={
                "path": manifest["checkers"]["checkmodel"],
                "sha256": manifest["pin_digests"][manifest["checkers"]["checkmodel"]],
            },
            stdout_tail=tail(scratch / "model.stdout"),
            stderr_tail=tail(scratch / "model.stderr"),
        )
        result["model_check"] = checked
        certificate_ok = checked["result"] == "verified"
    result["valid"] = checked_valid(expected, verdict, solve, certificate_ok)
    return result


def provisional_record(row, result):
    keys = (
        "verdict",
        "expected",
        "expected_match",
        "solve",
        "arm",
        "certificate_format",
        "certificate_bytes",
        "certificate_sha256",
        "solver_statistics",
        "solver_stderr_tail",
    )
    return {
        "schema": "tactix-winner-vbs-rerun-provisional-v1",
        **{key: row[key] for key in ("task", "target", "side", "instance")},
        **{key: result[key] for key in keys},
        "validation_pending": True,
        "recorded_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }


def task_main(campaign, task):
    campaign = Path(campaign).resolve()
    require(
        os.environ.get("SLURM_JOB_ID") and os.environ.get("SLURM_ARRAY_JOB_ID"),
        "portfolio tasks run only in the submitted Slurm array",
    )
    manifest = json.loads((campaign / "manifest.json").read_text())
    bound_launch(campaign, task=task)
    rows = load_matrix(campaign)
    require(0 <= task < len(rows), "invalid task")
    row = rows[task]
    manifest, _ = verify_campaign(campaign, arm_name=row["arm"])
    result_path = campaign / "results" / f"{task:03d}.json"
    claim_path = campaign / "claims" / f"{task:03d}.json"
    provisional_path = campaign / "provisional" / f"{task:03d}.json"
    require(not result_path.exists(), "task already complete")
    atomic_json(
        claim_path,
        {
            "task": task,
            "job_id": os.environ["SLURM_JOB_ID"],
            "array_job_id": os.environ["SLURM_ARRAY_JOB_ID"],
            "array_task_id": int(os.environ["SLURM_ARRAY_TASK_ID"]),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        },
        exclusive=True,
    )
    from anonymous_scratch import AnonymousScratch

    scratch = AnonymousScratch("/tmp")
    result = None
    try:
        compressed = campaign.parents[2] / "benchmarks/cache" / row["instance"]
        require(
            compressed.is_file()
            and sha256(compressed) == manifest["input_sha256"][row["instance"]],
            "compressed input hash mismatch",
        )
        cnf = scratch / "input.cnf"
        with open(cnf, "wb") as target:
            subprocess.run(
                ["xz", "-dc", compressed],
                stdout=target,
                check=True,
                env=dict(manifest["solver_environment"]),
            )
        result = execute_case(
            campaign,
            manifest,
            row["arm"],
            row["expected"],
            cnf,
            scratch,
            provisional=lambda value: atomic_json(
                provisional_path, provisional_record(row, value), exclusive=True
            ),
        )
        result.update(
            {
                key: int(row[key])
                for key in (
                    "task",
                    "target",
                    "selection_tactix_task",
                    "selection_baseline_task",
                )
            }
        )
        result.update(
            side=row["side"],
            instance=row["instance"],
            schema="tactix-winner-vbs-rerun-result-v1",
            node=os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            slurm_job_id=os.environ["SLURM_JOB_ID"],
            slurm_array_job_id=os.environ["SLURM_ARRAY_JOB_ID"],
            slurm_array_task_id=int(os.environ["SLURM_ARRAY_TASK_ID"]),
        )
    except Exception as error:
        result = {
            "schema": "tactix-winner-vbs-rerun-result-v1",
            "task": task,
            "target": int(row["target"]),
            "side": row["side"],
            "arm": row["arm"],
            "instance": row["instance"],
            "expected": row["expected"],
            "verdict": "INFRA_ERROR",
            "valid": False,
            "error": repr(error),
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "slurm_job_id": os.environ["SLURM_JOB_ID"],
            "slurm_array_job_id": os.environ["SLURM_ARRAY_JOB_ID"],
            "slurm_array_task_id": int(os.environ["SLURM_ARRAY_TASK_ID"]),
        }
    finally:
        scratch.close()
    result.update(
        artifact_deleted=not scratch.exists(),
        derived_artifacts_deleted=not scratch.exists(),
        cleanup_pending=scratch.exists(),
        finished_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    )
    result["classification"] = classify_result(result)
    atomic_json(result_path, result, exclusive=True)
    return 0 if result["classification"] != "invalid" else 1


def validate(campaign):
    campaign = Path(campaign).resolve()
    require(os.environ.get("SLURM_JOB_ID"), "validation runs only under Slurm")
    bound_launch(campaign, validation=True)
    require(
        not (campaign / "launch.json").exists(),
        "refusing validation after array launch",
    )
    require(not (campaign / "validation.json").exists(), "validation already exists")
    manifest, _ = verify_campaign(campaign, full=True)
    from anonymous_scratch import AnonymousScratch

    formulas = {
        "SAT": "p cnf 2 2\n1 2 0\n-1 2 0\n",
        "UNSAT": "p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n",
    }
    checks = []
    for arm_name, arm in manifest["arms"].items():
        verdicts = ("SAT",) if arm_name == "lymphosat" else ("SAT", "UNSAT")
        for expected in verdicts:
            scratch = AnonymousScratch("/tmp")
            try:
                cnf = scratch / "input.cnf"
                cnf.write_text(formulas[expected])
                result = execute_case(
                    campaign, manifest, arm_name, expected, cnf, scratch, 60, 600
                )
            finally:
                scratch.close()
            result.update(
                artifact_deleted=not scratch.exists(), cleanup_pending=scratch.exists()
            )
            require(
                result["valid"] and result["artifact_deleted"],
                f"validation failed for {arm_name} {expected}: {result}",
            )
            checks.append(
                {
                    "arm": arm_name,
                    "expected": expected,
                    "solve": result["solve"],
                    "proof_check": result["proof_check"],
                    "model_check": result["model_check"],
                    "cleanup": True,
                }
            )
    for code in ("import os; os._exit(7)", "import time; time.sleep(60)"):
        scratch = AnonymousScratch("/tmp")
        try:
            abnormal = measured(
                [sys.executable, "-c", code],
                scratch / "out",
                scratch / "err",
                scratch / "time",
                0.1,
            )
        finally:
            scratch.close()
        require(
            (abnormal["exit_code"] == 7 or abnormal["timed_out"])
            and not scratch.exists(),
            "abnormal-exit cleanup validation failed",
        )
        checks.append({"abnormal": abnormal, "cleanup": True})
    atomic_json(
        campaign / "validation.json",
        {
            "schema": "tactix-winner-vbs-rerun-validation-v1",
            "passed": True,
            "manifest_sha256": sha256(campaign / "manifest.json"),
            "job_id": os.environ["SLURM_JOB_ID"],
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "checks": checks,
            "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        },
        exclusive=True,
    )
    print(
        "PASS: all selected solver interfaces, proof/model pipelines, and abnormal cleanup"
    )


def submit(campaign, validation=False):
    campaign = Path(campaign).resolve()
    verify_campaign(campaign, full=True)
    record = campaign / ("validation-launch.json" if validation else "launch.json")
    require(not record.exists(), "campaign stage already submitted")
    if not validation:
        validation_record = json.loads((campaign / "validation.json").read_text())
        require(
            validation_record.get("passed") is True
            and validation_record.get("schema")
            == "tactix-winner-vbs-rerun-validation-v1"
            and validation_record.get("manifest_sha256")
            == sha256(campaign / "manifest.json"),
            "compute-node validation has not passed for this manifest",
        )
    script = campaign / ("validate.sbatch" if validation else "array.sbatch")
    job = (
        subprocess.check_output(["sbatch", "--parsable", str(script)], text=True)
        .strip()
        .split(";")[0]
    )
    atomic_json(
        record,
        {
            "job_id": job,
            "manifest_sha256": sha256(campaign / "manifest.json"),
            "submitted_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        },
        exclusive=True,
    )
    print(job)


def aggregate(campaign):
    campaign = Path(campaign).resolve()
    manifest, rows = verify_campaign(campaign, full=True)
    launch = json.loads((campaign / "launch.json").read_text())
    require(
        launch.get("manifest_sha256") == sha256(campaign / "manifest.json"),
        "launch manifest authority differs",
    )
    results, missing = [], []
    for task, row in enumerate(rows):
        path = campaign / "results" / f"{task:03d}.json"
        if not path.exists():
            missing.append(task)
            continue
        result = json.loads(path.read_text())
        require(
            (
                result.get("task"),
                result.get("target"),
                result.get("side"),
                result.get("arm"),
                result.get("instance"),
            )
            == (task, int(row["target"]), row["side"], row["arm"], row["instance"]),
            f"result identity differs for task {task}",
        )
        require(
            result.get("slurm_array_job_id") == launch.get("job_id")
            and result.get("slurm_array_task_id") == task,
            f"result Slurm provenance differs for task {task}",
        )
        results.append(result)
    classes = Counter(classify_result(result) for result in results)
    by_target = {}
    for target in range(20):
        pair = {
            result["side"]: result for result in results if result["target"] == target
        }
        row = {"target": target, "complete": set(pair) == {"tactix", "baseline"}}
        for side in ("tactix", "baseline"):
            if side in pair:
                row[side] = {
                    "classification": classify_result(pair[side]),
                    "verdict": pair[side]["verdict"],
                    "seconds": (pair[side].get("solve") or {}).get("wall_seconds"),
                    "arm": pair[side]["arm"],
                }
        if set(pair) == {"tactix", "baseline"}:
            row["transition"] = (
                f"{classify_result(pair['tactix'])}->{classify_result(pair['baseline'])}"
            )
            if all(classify_result(pair[side]) == "checked_solve" for side in pair):
                row["baseline_over_tactix_runtime"] = round(
                    pair["baseline"]["solve"]["wall_seconds"]
                    / pair["tactix"]["solve"]["wall_seconds"],
                    6,
                )
        by_target[target] = row
    sides = {}
    for side in ("tactix", "baseline"):
        subset = [result for result in results if result["side"] == side]
        solved = [
            result for result in subset if classify_result(result) == "checked_solve"
        ]
        par2 = (
            (
                sum(result["solve"]["wall_seconds"] for result in solved)
                + (len(subset) - len(solved)) * 2 * SOLVE_SECONDS
            )
            / len(subset)
            if subset
            else None
        )
        sides[side] = {
            "results": len(subset),
            "solved": len(solved),
            "sat": sum(result["verdict"] == "SAT" for result in solved),
            "unsat": sum(result["verdict"] == "UNSAT" for result in solved),
            "timeouts": sum(classify_result(result) == "timeout" for result in subset),
            "invalid": sum(classify_result(result) == "invalid" for result in subset),
            "par2_seconds": round(par2, 3) if par2 is not None else None,
            "proof_bytes": sum(
                result.get("certificate_bytes") or 0
                for result in solved
                if result["verdict"] == "UNSAT"
            ),
            "peak_rss_kb": max(
                (result.get("solve", {}).get("peak_rss_kb") or 0 for result in subset),
                default=0,
            ),
        }
    summary = {
        "schema": "tactix-winner-vbs-rerun-aggregate-v1",
        "manifest_sha256": sha256(campaign / "manifest.json"),
        "complete": not missing and classes["invalid"] == 0,
        "task_count": len(rows),
        "result_count": len(results),
        "missing_tasks": missing,
        "classifications": dict(classes),
        "sides": sides,
        "targets": list(by_target.values()),
        "cleanup_pending": sum(
            result.get("cleanup_pending") is not False for result in results
        ),
        "results": results,
        "observed_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    atomic_json(campaign / "aggregate.json", summary)
    print(
        json.dumps(
            {key: value for key, value in summary.items() if key != "results"}, indent=2
        )
    )
    return 0 if summary["complete"] else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--campaign", type=Path, required=True)
    prepare_parser.add_argument("--selection", type=Path, required=True)
    prepare_parser.add_argument("--baseline", type=Path, required=True)
    prepare_parser.add_argument("--tactix-campaign", type=Path, required=True)
    submit_parser = commands.add_parser("submit")
    submit_parser.add_argument("campaign", type=Path)
    submit_parser.add_argument("--validation", action="store_true")
    task_parser = commands.add_parser("task")
    task_parser.add_argument("campaign", type=Path)
    task_parser.add_argument("task", type=int)
    validate_parser = commands.add_parser("validate")
    validate_parser.add_argument("campaign", type=Path)
    aggregate_parser = commands.add_parser("aggregate")
    aggregate_parser.add_argument("campaign", type=Path)
    verify_parser = commands.add_parser("verify")
    verify_parser.add_argument("campaign", type=Path)
    for name, function in (
        ("adapter-satsuma", adapter_satsuma),
        ("adapter-lymphosat", adapter_lymphosat),
    ):
        sub = commands.add_parser(name)
        sub.add_argument("campaign", type=Path)
        sub.add_argument("arm")
        sub.add_argument("paths", nargs="+")
        sub.set_defaults(adapter=function)
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args)
    elif args.command == "submit":
        submit(args.campaign, args.validation)
    elif args.command == "task":
        raise SystemExit(task_main(args.campaign, args.task))
    elif args.command == "validate":
        validate(args.campaign)
    elif args.command == "aggregate":
        raise SystemExit(aggregate(args.campaign))
    elif args.command == "verify":
        verify_campaign(args.campaign, full=True)
        print("verified")
    elif args.command == "adapter-satsuma":
        require(
            len(args.paths) == 3, "satsuma adapter needs CNF, proof, transformed CNF"
        )
        raise SystemExit(args.adapter(args.campaign, args.arm, *args.paths))
    elif args.command == "adapter-lymphosat":
        require(
            len(args.paths) == 5,
            "LymphoSAT adapter needs CNF, proof, profile and streams",
        )
        raise SystemExit(args.adapter(args.campaign, args.arm, *args.paths))


if __name__ == "__main__":
    main()

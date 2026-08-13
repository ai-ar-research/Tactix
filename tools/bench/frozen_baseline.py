#!/usr/bin/env python3
"""Once-only SC24--26 baseline campaign runner.

This runner is deliberately separate from the development benchmark runner:
the five reference submissions use three certificate formats and two output
interfaces.  It records solve/check peak RSS and deletes a certificate only
after an fsync'd classification record exists.
"""

import argparse
import datetime as dt
import hashlib
import json
import math
import os
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CAMPAIGN = ROOT / "benchmarks/baselines/sc24-26-32g-5000s-v1"
CACHE = ROOT / "benchmarks/cache/tierD24-26"
TIME = Path("/usr/bin/time")
SOLVE_LIMIT = 5000
# SAT Competition checker tool chains have historically received 45,000 s;
# this campaign follows that SC2026 verified-pipeline budget independently of
# the 5,000 s solver budget.
CHECK_LIMIT = 45000
MEM_GIB = 32
PARTITION = "cpu-dense-hwulab-q"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def atomic_json(path, value, exclusive=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if exclusive:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
        with os.fdopen(fd, "w") as f:
            json.dump(value, f, indent=2, sort_keys=True)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
    else:
        tmp = path.with_name(f".{path.name}.{os.getpid()}.tmp")
        with open(tmp, "w") as f:
            json.dump(value, f, indent=2, sort_keys=True)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    dfd = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(dfd)
    finally:
        os.close(dfd)


def command_version(cmd):
    for args in (["--version"], ["-h"], ["--help"]):
        try:
            p = subprocess.run([str(cmd), *args], capture_output=True, text=True,
                               errors="replace", timeout=10)
            line = (p.stdout + p.stderr).strip().splitlines()
            if line:
                return line[0][:240]
        except Exception:
            pass
    return "unavailable"


def source_digest(path):
    path = Path(path)
    if path.is_file():
        return sha256(path)
    h = hashlib.sha256()
    for item in sorted(p for p in path.rglob("*") if p.is_file()):
        h.update(str(item.relative_to(path)).encode() + b"\0")
        h.update(bytes.fromhex(sha256(item)))
    return h.hexdigest()


def copy_pin(src, dst):
    if dst.exists():
        raise SystemExit(f"pin already exists: {dst}")
    if src.is_dir():
        shutil.copytree(src, dst, symlinks=True)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def prepare(campaign):
    campaign = campaign.resolve()
    dataset = campaign / "dataset.json"
    if not dataset.is_file():
        raise SystemExit("freeze the dataset first with freeze_sc24_26.py")
    if (campaign / "manifest.json").exists() or (campaign / "launch.json").exists():
        raise SystemExit("campaign already prepared/launched; refusing replacement")
    instances = []
    with open(campaign / "instances.list") as f:
        for line in f:
            if line.strip() and not line.startswith("#"):
                rel, expected, _ = line.split()
                instances.append({"rel": rel, "expected": expected})
    missing = [i["rel"] for i in instances if not (CACHE / Path(i["rel"]).name).is_file()]
    if missing:
        raise SystemExit(f"cache incomplete: {len(missing)} instances missing")
    cache_lines = [f"{sha256(CACHE / Path(i['rel']).name)}  {i['rel']}"
                   for i in instances]
    cache_manifest = campaign / "cache.sha256"
    if cache_manifest.exists():
        raise SystemExit(f"frozen cache manifest already exists: {cache_manifest}")
    cache_manifest.write_text("\n".join(cache_lines) + "\n")
    pins = campaign / "pins"
    pins.mkdir(parents=True, exist_ok=False)

    src = {
        "kissat-4.0.4": ROOT / "tools/solvers/bin/kissat",
        "kissat-sc2024": ROOT / "winners/kissat-sc2024/build/kissat",
        "ae-kissat-mab": ROOT / "winners/AE_kissat2025_MAB/AE_kissat2025_MAB/kissat",
        "satsuma-kissat": ROOT / "winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src",
        "lymphosat": ROOT / "winners/lymphosat-sc2026/green/lymphosat/src",
        "drat-trim": ROOT / "tools/checkers/bin/drat-trim",
        "cake-lpr": ROOT / "cake_lpr/cake_lpr",
        "dsr-compress": ROOT / "winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src/dsr-trim/bin/compress",
        "sr2drat": ROOT / "tools/checkers/sr2drat/sr2drat",
        "veripb": ROOT / "winners/lymphosat-sc2026/green/lymphosat-verified/src/third-party/bin/veripb",
        "cake-pb-cnf": ROOT / "tools/checkers/cakepb/cnf/cake_pb_cnf",
        "checkmodel": ROOT / "tools/fuzz/checkmodel",
        "modelcheck.py": ROOT / "tools/fuzz/modelcheck.py",
    }
    for name, path in src.items():
        if not path.exists():
            raise SystemExit(f"missing required build/checker: {path}")
        copy_pin(path.resolve(), pins / name)

    # The Satsuma and Lympho source packages contain build trees and sources;
    # retain only their self-contained runtime layouts in this immutable pin.
    for path in pins.rglob("*"):
        try:
            path.chmod(path.stat().st_mode & ~0o222)
        except OSError:
            pass

    solvers = [
        {"name": "kissat-4.0.4", "kind": "positional", "certificate": "drat",
         "executable": "pins/kissat-4.0.4", "checker_pipeline": "drat-cake-lpr",
         "vbs_eligible": "SAT+UNSAT"},
        {"name": "kissat-sc2024", "kind": "positional", "certificate": "drat",
         "executable": "pins/kissat-sc2024", "checker_pipeline": "drat-cake-lpr",
         "vbs_eligible": "SAT+UNSAT"},
        {"name": "ae-kissat-mab", "kind": "positional", "certificate": "drat",
         "executable": "pins/ae-kissat-mab", "checker_pipeline": "drat-cake-lpr",
         "vbs_eligible": "SAT+UNSAT"},
        {"name": "satsuma-kissat", "kind": "output-directory", "certificate": "dsr",
         "executable": "pins/satsuma-kissat/run_satsuma_kissat.sh",
         "checker_pipeline": "dsr-sr2drat-cake-lpr", "vbs_eligible": "SAT+UNSAT"},
        {"name": "lymphosat", "kind": "output-directory", "certificate": "pbp",
         "executable": "pins/lymphosat/run.sh", "checker_pipeline": "veripb-cake-pb",
         "vbs_eligible": "SAT-only"},
    ]
    tasks = [{"instance": i, "solver": s}
             for i in range(len(instances)) for s in range(len(solvers))]
    checkers = {
        "drat-trim": "pins/drat-trim",
        "cake-lpr": "pins/cake-lpr",
        "dsr-compress": "pins/dsr-compress",
        "sr2drat": "pins/sr2drat",
        "veripb": "pins/veripb",
        "cake-pb-cnf": "pins/cake-pb-cnf",
        "checkmodel": "pins/checkmodel",
        "modelcheck.py": "pins/modelcheck.py",
    }
    provenance = {}
    for name, rel in {**{s["name"]: s["executable"] for s in solvers},
                      **checkers}.items():
        path = campaign / rel
        provenance[name] = {
            "path": rel, "sha256": source_digest(path),
            "version": command_version(path) if path.is_file() else "runtime-tree",
        }
    explicit_versions = {
        "cake-lpr": "cake_lpr commit " + subprocess.run(
            ["git", "-C", str(ROOT / "cake_lpr"), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True).stdout.strip(),
        "cake-pb-cnf": "CakePB commit " + subprocess.run(
            ["git", "-C", str(ROOT / "tools/checkers/cakepb"), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True).stdout.strip(),
        "sr2drat": "sr2drat commit " + subprocess.run(
            ["git", "-C", str(ROOT / "tools/checkers/sr2drat"), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True).stdout.strip(),
        "checkmodel": "Tactix independent partial-model checker",
        "modelcheck.py": "Tactix independent partial-model checker core",
    }
    for name, version in explicit_versions.items():
        provenance[name]["version"] = version
    manifest = {
        "schema": 1, "campaign": campaign.name,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "once_only": True, "requeue": False,
        "limits": {"solve_wall_seconds": SOLVE_LIMIT,
                   "checker_pipeline_wall_seconds": CHECK_LIMIT,
                   "process_memory_gib": MEM_GIB},
        "slurm": {"partition": PARTITION, "cpus_per_task": 4,
                  "mem_per_cpu_gib": 8, "array_throttle": None},
        "dataset_sha256": sha256(dataset),
        "instances_list_sha256": sha256(campaign / "instances.list"),
        "cache_manifest_sha256": sha256(cache_manifest),
        "instances": instances, "solvers": solvers, "checkers": checkers,
        "provenance": provenance, "tasks": tasks,
        "build_provenance": {
            "compiler": command_version(Path("/cm/local/apps/gcc/13.1.0/bin/gcc")),
            "kissat-4.0.4": "upstream 4.0.4 pinned tools/solvers binary",
            "kissat-sc2024-commit": (ROOT / "winners/kissat-sc2024/SOURCE_COMMIT").read_text().strip(),
            "ae-kissat-mab-archive-sha256":
                (ROOT / "winners/AE_kissat2025_MAB/SOURCE_SHA256").read_text().strip(),
            "satsuma-kissat-archive-sha256":
                (ROOT / "winners/satsuma-iter-kissat-sc2026/SOURCE_SHA256").read_text().strip(),
            "lymphosat-archive-sha256":
                (ROOT / "winners/lymphosat-sc2026/SOURCE_SHA256").read_text().strip(),
            "portable_target": "x86-64-v2, generic tuning",
            "lymphosat-compiler-adapter-sha256":
                sha256(ROOT / "tools/bench/lymphosat-generic-cxx"),
            "lymphosat-build-report-sha256": sha256(campaign / "lymphosat-build.json"),
            "cake-lpr-commit": subprocess.run(
                ["git", "-C", str(ROOT / "cake_lpr"), "rev-parse", "HEAD"],
                capture_output=True, text=True, check=True).stdout.strip(),
            "cake-pb-commit": subprocess.run(
                ["git", "-C", str(ROOT / "tools/checkers/cakepb"), "rev-parse", "HEAD"],
                capture_output=True, text=True, check=True).stdout.strip(),
            "sr2drat-commit": subprocess.run(
                ["git", "-C", str(ROOT / "tools/checkers/sr2drat"), "rev-parse", "HEAD"],
                capture_output=True, text=True, check=True).stdout.strip(),
            "note": ("Official SC26 binaries require glibc 2.38 and Ice Lake; "
                     "the shipped sources were rebuilt without changing algorithms. "
                     "The supplied generic script's nested hard-coded Ice Lake flags "
                     "were filtered by the recorded compiler adapter."),
        },
        "policy": {"lower_time": "exact truncation of solve_wall_seconds",
                   "lower_memory": "limited high-water simulation; not exact",
                   "lymphosat": "exclude UNSAT from VBS and coverage",
                   "sc2026_output": ("run interface is CNF plus output directory; "
                       "UNSAT certificate is proof.out; SAT accepts a partial, "
                       "non-contradictory v-line assignment terminated by 0, with "
                       "at least one assigned satisfying literal per clause"),
                   "unsat": ("count only after the selected SC2026 pipeline reaches "
                       "its formally verified backend: DRAT->LRAT->cake_lpr, "
                       "DSR->text DSR->DRAT->LRAT->cake_lpr, or "
                       "PBP->kernel PBP->cake_pb_cnf")},
    }
    atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    print(f"prepared {len(tasks)} tasks: {len(instances)} instances x {len(solvers)} solvers")


def limits():
    os.setsid()
    b = MEM_GIB << 30
    resource.setrlimit(resource.RLIMIT_AS, (b, b))


def parse_rss(path):
    try:
        text = Path(path).read_text(errors="replace")
        m = re.search(r"^max_rss_kb=(\d+)$", text, re.M)
        return int(m.group(1)) if m else None
    except OSError:
        return None


_active_process = None


def measured(cmd, stdout, stderr, timeout, metric, env=None):
    global _active_process
    wrapped = [str(TIME), "-f", "max_rss_kb=%M", "-o", str(metric), "--", *map(str, cmd)]
    start = time.monotonic()
    timed_out = False
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        p = subprocess.Popen(wrapped, stdout=out, stderr=err, preexec_fn=limits,
                             env=env)
        _active_process = p
        try:
            rc = p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(p.pid, signal.SIGTERM)
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()
            rc = None
        finally:
            _active_process = None
    return {"exit_code": rc, "wall_seconds": round(time.monotonic() - start, 3),
            "peak_rss_kb": parse_rss(metric), "timed_out": timed_out}


def verdict(rc, stdout):
    text = Path(stdout).read_text(errors="replace")
    states = re.findall(r"^s\s+(SATISFIABLE|UNSATISFIABLE)\s*$", text, re.M)
    if len(states) == 1:
        return "SAT" if states[0] == "SATISFIABLE" else "UNSAT"
    return "UNKNOWN"


def sat_output_protocol_ok(stdout):
    lines = Path(stdout).read_text(errors="replace").splitlines()
    value_lines = [line for line in lines if line.startswith("v ")]
    if not value_lines or any(len(line) > 4096 for line in value_lines):
        return False
    try:
        lits = [int(token) for line in value_lines for token in line[2:].split()]
    except ValueError:
        return False
    if not lits or lits[-1] != 0 or 0 in lits[:-1]:
        return False
    assigned = set(lits[:-1])
    return not any(-lit in assigned for lit in assigned)


def retain(path, retained):
    path, retained = Path(path), Path(retained)
    if not path.exists():
        return None
    retained.parent.mkdir(parents=True, exist_ok=True)
    os.replace(path, retained)
    return str(retained)


def checker_ok(name, rc, stdout, stderr):
    if name == "sr2drat":
        return rc == 0 and Path(stdout).is_file() and Path(stdout).stat().st_size > 0
    text = (Path(stdout).read_text(errors="replace") + "\n" +
            Path(stderr).read_text(errors="replace"))
    if name == "drat-trim":
        return rc == 0 and "s VERIFIED" in text and "s NOT VERIFIED" not in text
    if name == "dsr-trim":
        return rc == 0 and "s VERIFIED UNSAT" in text
    if name == "veripb":
        return rc == 0 and "s VERIFIED UNSATISFIABLE" in text
    if name == "cake-lpr":
        return rc == 0 and "s VERIFIED UNSAT" in text
    if name == "cake-pb-cnf":
        return rc == 0 and ("s VERIFIED UNSAT" in text or "VERIFIED" in text)
    if name == "srcheck":
        return rc == 0 and "s VERIFIED UNSAT" in text and "s INVALID" not in text
    if name == "dsr-compress":
        return rc == 0 and "ERROR" not in text and "not supported" not in text
    return False


def check_proof(man, campaign, solver, cnf, proof, work):
    """Run the solver-selected SC2026 frontend and verified backend."""
    fmt = solver["certificate"]
    amendment_path = campaign / "checker-amendment-1.json"
    amendment = json.loads(amendment_path.read_text()) if amendment_path.exists() else None
    heap_amendment_path = campaign / "checker-amendment-2.json"
    heap_amendment = (json.loads(heap_amendment_path.read_text())
                      if heap_amendment_path.exists() else None)
    derived = []
    if fmt == "drat":
        derived = [work / "proof.lrat"]
        specs = [
            ("drat-trim", [campaign / man["checkers"]["drat-trim"],
                           cnf, proof, "-L", derived[0]]),
            ("cake-lpr", [campaign / man["checkers"]["cake-lpr"], cnf, derived[0]]),
        ]
    elif fmt == "dsr":
        if amendment:
            derived = [work / "proof.binary.lsr", work / "proof.lsr"]
            specs = [
                ("dsr-trim", [campaign / amendment["checkers"]["dsr-trim"],
                              cnf, proof, derived[0]]),
                ("dsr-compress", [campaign / man["checkers"]["dsr-compress"],
                                  "-d", derived[0], derived[1]]),
                ("srcheck", [campaign / amendment["checkers"]["srcheck"],
                             cnf, derived[1]]),
            ]
        else:
            derived = [work / "proof.text.dsr", work / "proof.drat", work / "proof.lrat"]
            specs = [
                ("dsr-compress", [campaign / man["checkers"]["dsr-compress"],
                                  "-d", proof, derived[0]]),
                ("sr2drat", [campaign / man["checkers"]["sr2drat"],
                             cnf, derived[0]], derived[1]),
                ("drat-trim", [campaign / man["checkers"]["drat-trim"],
                               cnf, derived[1], "-L", derived[2]]),
                ("cake-lpr", [campaign / man["checkers"]["cake-lpr"], cnf, derived[2]]),
            ]
    else:
        derived = [work / "proof.kernel.pbp"]
        specs = [
            ("veripb", [campaign / man["checkers"]["veripb"], "-u", "--cnf",
                        "--elaborate", derived[0], cnf, proof]),
            ("cake-pb-cnf", [campaign / man["checkers"]["cake-pb-cnf"],
                             cnf, derived[0]]),
        ]
    started = time.monotonic()
    stages = []
    for number, spec in enumerate(specs, 1):
        name, cmd, *redirect = spec
        stdout = work / f"checker-{number}.stdout"
        stderr = work / f"checker-{number}.stderr"
        remaining = max(1, CHECK_LIMIT - int(time.monotonic() - started))
        actual_stdout = redirect[0] if redirect else stdout
        stage_env = None
        if name in ("cake-lpr", "cake-pb-cnf"):
            # CakeML's shipped 4 GiB heap is smaller than the SC2026 32 GiB
            # process contract and can reject a valid large translated proof.
            # Keep 4 GiB for its default stack and further address-space slack.
            stage_env = os.environ.copy()
            stage_env["CML_HEAP_SIZE"] = "24576"  # MiB
        measured_stage = measured(cmd, actual_stdout, stderr, remaining,
                                  work / f"checker-{number}.time", stage_env)
        measured_stage.update({
            "name": name,
            "environment": ({"CML_HEAP_SIZE_MIB": 24576}
                            if stage_env is not None else {}),
            "provenance": (man["provenance"].get(name) or
                           (amendment or {}).get("provenance", {}).get(name)),
            "result": "verified" if checker_ok(
                name, measured_stage["exit_code"], actual_stdout, stderr) else "not-verified",
            "stdout_tail": "proof stream" if redirect else tail(stdout),
            "stderr_tail": tail(stderr),
        })
        stages.append(measured_stage)
        if measured_stage["result"] != "verified":
            break
    return {
        "pipeline": (amendment["pipeline"] if fmt == "dsr" and amendment
                     else solver["checker_pipeline"]),
        "result": "verified" if len(stages) == len(specs) and all(
            stage["result"] == "verified" for stage in stages) else "not-verified",
        "wall_seconds": round(sum(stage["wall_seconds"] for stage in stages), 3),
        "peak_rss_kb": max((stage["peak_rss_kb"] or 0 for stage in stages), default=0),
        "timed_out": any(stage["timed_out"] for stage in stages),
        "configuration_amendment": ({
            "path": str(heap_amendment_path.relative_to(campaign)),
            "sha256": sha256(heap_amendment_path),
        } if heap_amendment and any(
            stage["name"] in ("cake-lpr", "cake-pb-cnf") for stage in stages)
            else None),
        "stages": stages,
        "derived_artifacts": [{"path": str(path.relative_to(campaign)),
                               "bytes": path.stat().st_size,
                               "sha256": sha256(path)}
                              for path in derived if path.is_file()],
    }, derived


def tail(path, lines=8):
    try:
        return "\n".join(Path(path).read_text(errors="replace").splitlines()[-lines:])[-2000:]
    except OSError:
        return ""


def run_one(campaign, index):
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    task = man["tasks"][index]
    inst, solver = man["instances"][task["instance"]], man["solvers"][task["solver"]]
    result_path = campaign / "results" / f"{index:05d}.json"
    claim = campaign / "claims" / f"{index:05d}.json"
    atomic_json(claim, {"task": index, "host": os.uname().nodename,
                        "started_utc": dt.datetime.now(dt.timezone.utc).isoformat()}, exclusive=True)
    if result_path.exists():
        raise SystemExit("result already exists after exclusive claim; refusing rerun")

    source = CACHE / Path(inst["rel"]).name
    if not source.is_file():
        atomic_json(result_path, {"task": index, "solver": solver["name"],
                                  "instance": inst["rel"], "outcome": "MISSING_INPUT",
                                  "cleanup_pending": False})
        return 2
    work = campaign / "scratch" / f"{index:05d}"
    work.mkdir(parents=True, exist_ok=False)
    local = Path(tempfile.mkdtemp(prefix=f"frozen-{index:05d}-"))
    cnf = local / "instance.cnf"
    try:
        with open(cnf, "wb") as out:
            subprocess.run(["xz", "-dc", source], stdout=out, check=True)
        stdout, stderr = work / "solver.stdout", work / "solver.stderr"
        proof_dir = work / "proof-dir"
        proof_dir.mkdir()
        proof = proof_dir / "proof.out"
        executable = campaign / solver["executable"]
        if solver["kind"] == "positional":
            cmd = [executable, cnf, proof]
        else:
            cmd = [executable, cnf, proof_dir]
        solve = measured(cmd, stdout, stderr, SOLVE_LIMIT, work / "solve.time")
        answer = verdict(solve["exit_code"], stdout)
        result = {
            "task": index, "solver": solver["name"], "instance": inst["rel"],
            "expected": inst["expected"], "verdict": answer,
            "expected_match": inst["expected"] == "UNKNOWN" or answer == inst["expected"],
            "solve": solve, "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "certificate_format": solver["certificate"],
            "certificate_path": str(proof.relative_to(campaign)),
            "certificate_bytes": None, "certificate_sha256": None,
            "proof_check": None, "proof_ok": None, "model_check": None,
            "model_ok": None, "cleanup_pending": False, "retained_artifact": None,
            "retained_artifacts": [],
            "vbs_eligible": solver["vbs_eligible"],
        }
        derived = None
        if answer == "UNSAT":
            if proof.is_file():
                result["certificate_bytes"] = proof.stat().st_size
                result["certificate_sha256"] = sha256(proof)
                result["proof_check"], derived = check_proof(
                    man, campaign, solver, cnf, proof, work)
                result["proof_ok"] = result["proof_check"]["result"] == "verified"
            else:
                result["proof_ok"] = False
        elif answer == "SAT":
            model_check = measured([campaign / man["checkers"]["checkmodel"], cnf, stdout],
                                   work / "model-check.stdout", work / "model-check.stderr",
                                   600, work / "model-check.time")
            result["model_check"] = model_check
            result["model_protocol_ok"] = sat_output_protocol_ok(stdout)
            result["model_ok"] = (model_check["exit_code"] == 0 and
                                  result["model_protocol_ok"])
            result["model_check"]["result"] = (
                "verified" if result["model_ok"] else "not-verified")
            result["model_check"]["stdout_tail"] = tail(work / "model-check.stdout")
            result["model_check"]["stderr_tail"] = tail(work / "model-check.stderr")
            result["model_checker"] = man["provenance"]["checkmodel"]

        valid = (answer == "SAT" and result["model_ok"] is True or
                 answer == "UNSAT" and result["proof_ok"] is True)
        accepted = valid and result["expected_match"]
        result["valid"] = accepted
        artifact = proof if answer == "UNSAT" else stdout
        if answer in ("SAT", "UNSAT") and not accepted:
            suffix = solver["certificate"] if answer == "UNSAT" else "model"
            result["retained_artifact"] = retain(artifact,
                campaign / "retained" / f"{index:05d}.{suffix}")
            if result["retained_artifact"]:
                result["retained_artifacts"].append(result["retained_artifact"])
            if answer == "UNSAT" and derived is not None:
                for number, path in enumerate(derived):
                    if path.is_file():
                        kept = retain(path, campaign / "retained" /
                                      f"{index:05d}.derived-{number}{path.suffix}")
                        if kept:
                            result["retained_artifacts"].append(kept)
            result["cleanup_pending"] = bool(result["retained_artifacts"])
        elif answer == "UNKNOWN" and proof.is_file() and proof.stat().st_size:
            result["certificate_bytes"] = proof.stat().st_size
            result["certificate_sha256"] = sha256(proof)
            result["retained_artifact"] = retain(
                proof, campaign / "retained" / f"{index:05d}.partial-{solver['certificate']}")
            if result["retained_artifact"]:
                result["retained_artifacts"].append(result["retained_artifact"])
            result["cleanup_pending"] = True

        # First durable classification: it contains the proof hash/size/checker
        # result while the certificate still exists.
        atomic_json(result_path, result)
        if accepted and artifact.exists():
            artifact.unlink()
            result["artifact_deleted"] = True
            if answer == "UNSAT" and derived is not None:
                for path in derived:
                    if path.exists():
                        path.unlink()
                result["derived_artifacts_deleted"] = True
            if proof != artifact and proof.exists():
                proof.unlink()
            atomic_json(result_path, result)
        shutil.rmtree(work)
        return 0 if result["valid"] or answer == "UNKNOWN" else 1
    finally:
        shutil.rmtree(local, ignore_errors=True)


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
    interrupted = sorted(claimed - {r["task"] for r in results})
    orphan_artifacts = []
    for i in interrupted:
        scratch = campaign / "scratch" / f"{i:05d}"
        if scratch.exists():
            orphan_artifacts.extend(str(p.relative_to(campaign)) for p in scratch.rglob("*")
                                    if p.is_file() and ("proof" in p.name or "certificate" in p.name))
    summary = {"schema": 1, "manifest_sha256": sha256(campaign / "manifest.json"),
               "complete": not missing, "task_count": len(man["tasks"]),
               "result_count": len(results), "missing_tasks": missing,
               "interrupted_claims": interrupted,
               "cleanup_pending": (sum(bool(r.get("cleanup_pending")) for r in results)
                                   + len(orphan_artifacts)),
               "retained_artifacts": (sum(bool(r.get("retained_artifact")) for r in results)
                                      + len(orphan_artifacts)),
               "orphan_artifact_paths": orphan_artifacts,
               "invalid": sum(r.get("valid") is False for r in results),
               "results": results}
    atomic_json(campaign / "results.json", summary)
    subprocess.run([sys.executable, ROOT / "tools/bench/frozen_baseline_report.py",
                    "--campaign", campaign], check=True)
    print(json.dumps({k: v for k, v in summary.items() if k != "results"}, indent=2))
    return 0 if summary["complete"] else 2


def validate(campaign):
    """Exercise every pinned solver and checker without consuming a real task."""
    campaign = campaign.resolve()
    if (campaign / "launch.json").exists():
        raise SystemExit("refusing post-launch validation")
    record_path = campaign / "validation.json"
    if record_path.exists():
        raise SystemExit("validation already recorded; refusing repeat")
    man = json.loads((campaign / "manifest.json").read_text())
    work = campaign / "validation-scratch"
    work.mkdir(exist_ok=False)
    unsat = work / "unsat.cnf"
    sat = work / "sat.cnf"
    unsat.write_text("p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n")
    sat.write_text("p cnf 2 2\n1 2 0\n-1 2 0\n")
    rows = []
    all_artifacts = []
    for solver in man["solvers"]:
        sw = work / solver["name"]
        sw.mkdir()
        executable = campaign / solver["executable"]
        proof_dir = sw / "unsat-proof"
        proof_dir.mkdir()
        proof = proof_dir / "proof.out"
        cmd = ([executable, unsat, proof] if solver["kind"] == "positional"
               else [executable, unsat, proof_dir])
        solve_u = measured(cmd, sw / "unsat.stdout", sw / "unsat.stderr", 120,
                           sw / "unsat.time")
        answer_u = verdict(solve_u["exit_code"], sw / "unsat.stdout")
        check, derived = ({"result": "not-run", "stages": []}, [])
        if answer_u == "UNSAT" and proof.is_file():
            check, derived = check_proof(man, campaign, solver, unsat, proof, sw)
        sat_proof_dir = sw / "sat-proof"
        sat_proof_dir.mkdir()
        sat_proof = sat_proof_dir / "proof.out"
        cmd = ([executable, sat, sat_proof] if solver["kind"] == "positional"
               else [executable, sat, sat_proof_dir])
        solve_s = measured(cmd, sw / "sat.stdout", sw / "sat.stderr", 120,
                           sw / "sat.time")
        answer_s = verdict(solve_s["exit_code"], sw / "sat.stdout")
        model = measured([campaign / man["checkers"]["checkmodel"], sat,
                          sw / "sat.stdout"], sw / "model.stdout",
                         sw / "model.stderr", 120, sw / "model.time")
        row = {
            "solver": solver["name"], "unsat_verdict": answer_u,
            "unsat_solve": solve_u, "certificate_bytes": proof.stat().st_size
                if proof.is_file() else None,
            "certificate_sha256": sha256(proof) if proof.is_file() else None,
            "proof_check": check, "sat_verdict": answer_s, "sat_solve": solve_s,
            "model_exit_code": model["exit_code"],
            "model_protocol_ok": sat_output_protocol_ok(sw / "sat.stdout"),
        }
        row["passed"] = (answer_u == "UNSAT" and check["result"] == "verified" and
                         answer_s == "SAT" and model["exit_code"] == 0 and
                         row["model_protocol_ok"])
        rows.append(row)
        all_artifacts.extend([proof, sat_proof, sw / "sat.stdout", *derived])
    record = {"schema": 1, "manifest_sha256": sha256(campaign / "manifest.json"),
              "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
              "passed": all(row["passed"] for row in rows), "solvers": rows,
              "artifacts_deleted": False}
    atomic_json(record_path, record, exclusive=True)
    if record["passed"]:
        for path in all_artifacts:
            if path.exists():
                path.unlink()
        shutil.rmtree(work)
        record["artifacts_deleted"] = True
        atomic_json(record_path, record)
    print(json.dumps({"passed": record["passed"], "node": record["node"]}, indent=2))
    return 0 if record["passed"] else 1


def recheck(campaign, index):
    """Checker-only repair for a retained UNSAT certificate; never reruns solver."""
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    result_path = campaign / "results" / f"{index:05d}.json"
    result = json.loads(result_path.read_text())
    if result.get("verdict") != "UNSAT" or not result.get("cleanup_pending"):
        raise SystemExit("task is not a cleanup-pending UNSAT result")
    if (result["solver"] == "satsuma-kissat" and
            not (campaign / "checker-amendment-1.json").is_file()):
        raise SystemExit("verified SR checker amendment is not installed")
    proof = Path(result["retained_artifact"])
    if not proof.is_file():
        raise SystemExit("retained source certificate is missing")
    source = CACHE / Path(result["instance"]).name
    work = campaign / "recheck-scratch" / f"{index:05d}"
    work.mkdir(parents=True, exist_ok=False)
    local = Path(tempfile.mkdtemp(prefix=f"frozen-recheck-{index:05d}-"))
    try:
        cnf = local / "instance.cnf"
        with open(cnf, "wb") as out:
            subprocess.run(["xz", "-dc", source], stdout=out, check=True)
        solver = next(s for s in man["solvers"] if s["name"] == result["solver"])
        check, derived = check_proof(man, campaign, solver, cnf, proof, work)
        result.setdefault("proof_check_attempts", []).append(result["proof_check"])
        result["proof_check"] = check
        result["proof_ok"] = check["result"] == "verified"
        result["valid"] = result["proof_ok"] and result["expected_match"]
        result["checker_only_recheck"] = True
        # Durable reclassification while every source/derived artifact exists.
        atomic_json(result_path, result)
        if result["valid"]:
            for path in [Path(p) for p in result.get("retained_artifacts", [])]:
                if path.exists():
                    path.unlink()
            for path in derived:
                if path.exists():
                    path.unlink()
            result["cleanup_pending"] = False
            result["retained_artifact"] = None
            result["retained_artifacts"] = []
            result["artifact_deleted"] = True
            result["derived_artifacts_deleted"] = True
            atomic_json(result_path, result)
            shutil.rmtree(work)
        return 0 if result["valid"] else 1
    finally:
        shutil.rmtree(local, ignore_errors=True)


def submit(campaign):
    campaign = campaign.resolve()
    man_path, launch_path = campaign / "manifest.json", campaign / "launch.json"
    if not man_path.is_file():
        raise SystemExit("prepare campaign first")
    validation = campaign / "validation.json"
    if not validation.is_file() or not json.loads(validation.read_text()).get("passed"):
        raise SystemExit("successful pinned validation is required before submission")
    if launch_path.exists() or (campaign / "SUBMITTING").exists():
        raise SystemExit("campaign was already submitted or submission is in progress")
    submitting = campaign / "SUBMITTING"
    atomic_json(submitting, {"state": "preparing submission"}, exclusive=True)
    man = json.loads(man_path.read_text())
    count = len(man["tasks"])
    me = Path(__file__).resolve()
    array = campaign / "array.sbatch"
    agg = campaign / "aggregate.sbatch"
    array.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={PARTITION}
#SBATCH --job-name=frozen-sc24-26-baselines
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem-per-cpu=8G
#SBATCH --time=14:15:00
#SBATCH --array=0-{count - 1}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
exec {sys.executable} {me} --campaign {campaign} --run-one "$SLURM_ARRAY_TASK_ID"
""")
    agg.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={PARTITION}
#SBATCH --job-name=frozen-sc24-26-aggregate
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/aggregate.log
exec {sys.executable} {me} --campaign {campaign} --aggregate
""")
    (campaign / "logs").mkdir(exist_ok=True)
    a = subprocess.run(["sbatch", "--parsable", array], capture_output=True,
                       text=True, check=True).stdout.strip().split(";")[0]
    atomic_json(submitting, {"state": "array submitted; aggregate pending",
                             "array_job_id": a})
    g = subprocess.run(["sbatch", "--parsable", f"--dependency=afterany:{a}", agg],
                       capture_output=True, text=True, check=True).stdout.strip().split(";")[0]
    launch = {"array_job_id": a, "aggregate_job_id": g, "tasks": count,
              "manifest_sha256": sha256(man_path),
              "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat()}
    atomic_json(launch_path, launch, exclusive=True)
    submitting.unlink()
    print(json.dumps(launch, indent=2))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--campaign", type=Path, default=DEFAULT_CAMPAIGN)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--run-one", type=int)
    group.add_argument("--aggregate", action="store_true")
    group.add_argument("--submit", action="store_true")
    group.add_argument("--validate", action="store_true")
    group.add_argument("--recheck", type=int)
    args = ap.parse_args()
    if args.prepare:
        return prepare(args.campaign)
    if args.run_one is not None:
        return run_one(args.campaign, args.run_one)
    if args.aggregate:
        return aggregate(args.campaign)
    if args.validate:
        return validate(args.campaign)
    if args.recheck is not None:
        return recheck(args.campaign, args.recheck)
    return submit(args.campaign)


if __name__ == "__main__":
    sys.exit(main() or 0)

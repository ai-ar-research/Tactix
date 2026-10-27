#!/usr/bin/env python3
"""Exercise zero-retention paths in the shared Tier-D campaign runner."""

import errno
import importlib.util
import json
import lzma
from pathlib import Path
import stat
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "frozen_baseline", ROOT / "tools/bench/frozen_baseline.py")
FROZEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FROZEN)

# The campaign runner invokes GNU time with -f and -o. macOS /usr/bin/time
# has a different interface; this integration fixture needs the real runner.
time_version = subprocess.run([str(FROZEN.TIME), "--version"],
                              capture_output=True, text=True)
if time_version.returncode or "GNU" not in time_version.stdout:
    print("SKIP: campaign runner requires GNU time at", FROZEN.TIME)
    sys.exit(77)


def executable(path, body):
    path.write_text("#!/usr/bin/env python3\n" + body)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


with tempfile.TemporaryDirectory(prefix="tactix-certificate-cleanup-") as tmp:
    base = Path(tmp)
    campaign = base / "campaign"
    cache = base / "cache"
    cache.mkdir()
    for directory in ("claims", "pending", "pins", "results", "scratch",
                      "retained"):
        (campaign / directory).mkdir(parents=True, exist_ok=True)

    cnf = b"p cnf 1 1\n1 0\n"
    (cache / "case.cnf.xz").write_bytes(lzma.compress(cnf))
    FROZEN.CACHE = cache

    solver_body = r'''
from pathlib import Path
import sys

mode = Path(sys.argv[0]).name
proof = Path(sys.argv[2]) / "proof.out"
if "unknown" in mode:
    proof.write_text("partial proof\n")
    print("c stopped before a verdict")
elif "unsat" in mode:
    proof.write_text("invalid proof\n")
    print("s UNSATISFIABLE")
else:
    print("s SATISFIABLE")
    print("v 1 -1 0")
'''
    modes = ("unknown", "invalid-unsat", "invalid-sat", "retry-unknown",
             "recover-unknown")
    for mode in modes:
        executable(campaign / "pins" / mode, solver_body)

    executable(campaign / "pins/drat-trim", r'''
from pathlib import Path
import sys
Path(sys.argv[-1]).write_text("partial derived proof\n")
print("s NOT VERIFIED")
raise SystemExit(1)
''')
    executable(campaign / "pins/cake-lpr", "raise SystemExit(1)\n")
    executable(campaign / "pins/checkmodel", "raise SystemExit(1)\n")

    solvers = [{
        "name": mode,
        "kind": "output-directory",
        "executable": f"pins/{mode}",
        "certificate": "drat",
        "checker_pipeline": "drat-cake-lpr",
        "vbs_eligible": "SAT+UNSAT",
    } for mode in modes]
    manifest = {
        "certificate_policy": {"retention": "none"},
        "limits": {
            "solve_wall_seconds": 30,
            "checker_pipeline_wall_seconds": 30,
            "scheduler_memory_gib": 1,
            "memory_enforcement": "slurm_task_cgroup",
        },
        "instances": [{"rel": "case.cnf.xz", "expected": "UNKNOWN"}],
        "solvers": solvers,
        "tasks": [{"instance": 0, "solver": i} for i in range(len(solvers))],
        "checkers": {
            "checkmodel": "pins/checkmodel",
            "drat-trim": "pins/drat-trim",
            "cake-lpr": "pins/cake-lpr",
        },
        "provenance": {
            "checkmodel": {"path": "pins/checkmodel"},
            "drat-trim": {"path": "pins/drat-trim"},
            "cake-lpr": {"path": "pins/cake-lpr"},
        },
    }
    (campaign / "manifest.json").write_text(json.dumps(manifest))

    expected_rc = (0, 1, 1)
    for index, rc in enumerate(expected_rc):
        assert FROZEN.run_one(campaign, index) == rc

    real_rmtree = FROZEN.shutil.rmtree
    real_sleep = FROZEN.time.sleep
    retry_work = campaign / "scratch/00003"
    retry_attempts = [0]

    def transient_rmtree(path, *args, **kwargs):
        if Path(path) == retry_work and retry_attempts[0] < 2:
            retry_attempts[0] += 1
            raise OSError(errno.EBUSY, "simulated NFS silly rename")
        return real_rmtree(path, *args, **kwargs)

    FROZEN.shutil.rmtree = transient_rmtree
    FROZEN.time.sleep = lambda _seconds: None
    try:
        assert FROZEN.run_one(campaign, 3) == 0
    finally:
        FROZEN.shutil.rmtree = real_rmtree
        FROZEN.time.sleep = real_sleep
    assert retry_attempts[0] == 2
    retry_result = json.loads((campaign / "results/00003.json").read_text())
    assert retry_result["cleanup_retries"] == 2
    assert not (campaign / "pending/00003.json").exists()

    blocked_work = campaign / "scratch/00004"

    def blocked_rmtree(path, *args, **kwargs):
        if Path(path) == blocked_work:
            raise OSError(errno.ENOTEMPTY, "simulated persistent NFS directory")
        return real_rmtree(path, *args, **kwargs)

    FROZEN.shutil.rmtree = blocked_rmtree
    FROZEN.time.sleep = lambda _seconds: None
    try:
        try:
            FROZEN.run_one(campaign, 4)
            raise AssertionError("persistent cleanup failure was ignored")
        except OSError as error:
            assert error.errno == errno.ENOTEMPTY
    finally:
        FROZEN.shutil.rmtree = real_rmtree
        FROZEN.time.sleep = real_sleep
    assert not (campaign / "results/00004.json").exists()
    pending = json.loads((campaign / "pending/00004.json").read_text())
    assert pending["cleanup_pending"] is True
    # A crash after finalizing the journal but before its atomic promotion is
    # also recoverable: the journal's location, not this flag, is authoritative.
    pending["cleanup_pending"] = False
    FROZEN.atomic_json(campaign / "pending/00004.json", pending)
    assert FROZEN.resume_cleanup(campaign, 4) == 0
    recovered = json.loads((campaign / "results/00004.json").read_text())
    assert recovered["cleanup_pending"] is False
    assert recovered["scratch_deleted"] is True
    assert not (campaign / "pending/00004.json").exists()

    results = [json.loads((campaign / "results" / f"{i:05d}.json").read_text())
               for i in range(5)]
    assert [row["verdict"] for row in results] == [
        "UNKNOWN", "UNSAT", "SAT", "UNKNOWN", "UNKNOWN"]
    assert all(row["cleanup_pending"] is False for row in results)
    assert all(row["retained_artifacts"] == [] for row in results)
    assert all(row["artifact_deleted"] for row in results)
    assert all(row["derived_artifacts_deleted"] for row in results)
    assert all(row["scratch_deleted"] for row in results)
    assert results[0]["certificate_bytes"] > 0
    assert results[1]["proof_check"]["derived_artifacts"][0]["deleted"]
    assert results[2]["certificate_bytes"] > 0
    assert "SATISFIABLE" in results[2]["solver_stdout_tail"]

    residue = [path for path in campaign.rglob("*")
               if path.is_file() and ("proof" in path.name or
                                      path.suffix in {".drat", ".lrat", ".dsr",
                                                      ".lsr", ".pbp"})]
    assert residue == [], residue
    assert not any((campaign / "scratch").iterdir())
    assert not any((campaign / "pending").iterdir())
    assert not any((campaign / "retained").iterdir())

    validation_campaign = base / "validation-campaign"
    (validation_campaign / "pins").mkdir(parents=True)
    executable(validation_campaign / "pins/unknown", solver_body)
    executable(validation_campaign / "pins/checkmodel", "raise SystemExit(1)\n")
    validation_manifest = {
        "certificate_policy": {"retention": "none"},
        "limits": {"scheduler_memory_gib": 1,
                   "memory_enforcement": "slurm_task_cgroup"},
        "solvers": [solvers[0]],
        "checkers": {"checkmodel": "pins/checkmodel"},
        "provenance": {"checkmodel": {"path": "pins/checkmodel"}},
    }
    (validation_campaign / "manifest.json").write_text(
        json.dumps(validation_manifest))
    assert FROZEN.validate(validation_campaign) == 1
    validation = json.loads(
        (validation_campaign / "validation.json").read_text())
    assert validation["passed"] is False
    assert validation["artifacts_deleted"]
    assert validation["scratch_deleted"]
    assert not (validation_campaign / "validation-scratch").exists()

print("campaign certificate cleanup: PASS")

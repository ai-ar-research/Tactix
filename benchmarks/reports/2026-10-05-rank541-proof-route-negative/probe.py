#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path("/home/hwu/SAT-solving")
CAMPAIGN = ROOT / "build/pin-r541-binary-lsr-probe-ee601c089"
INPUT = ROOT / "benchmarks/cache/tierD24-26/6ff70a3aaf261b35b0b072f454a756be-crafted_n10_d6_c4_num9.cnf.xz"
PROOF = ROOT / "benchmarks/runs/20261005-gap44-proof-recovery-small4/retained-proofs/001.dsr"
DSR_TRIM = ROOT / "benchmarks/runs/20261005-gap44-proof-recovery-small4/pins/dsr-trim"
SR_CHECK = ROOT / "benchmarks/runs/20261005-gap44-proof-recovery-small4/pins/srcheck"
TARGET_RESULT = ROOT / "benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq/results/540.json"
EXPECTED = {
    str(INPUT): "5ae5e573a68bfb55169015d17c6a14319844cd587db9929ee8aca480d827d143",
    str(PROOF): "6d8f3c51b3584a2464231ea3b548a96ebfaf3c34967a5b2e4dd803f7082a300b",
    str(DSR_TRIM): "3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee",
    str(SR_CHECK): "19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945",
    str(TARGET_RESULT): "81877aabdb818c1ff58b3b7346197946b0f062d5562964dd1aba4a36ca537441",
}
EXPECTED_CNF = "fa5f798fc77e1949b6813dfccfa11d0e924c0ad4d3fc24c8962081a26e118f30"
DEADLINE_SECONDS = 1800


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        while chunk := source.read(1 << 24):
            digest.update(chunk)
    return digest.hexdigest()


def run_stage(name, command, timeout):
    started = time.monotonic()
    try:
        completed = subprocess.run(command, capture_output=True, text=True,
                                   errors="replace", timeout=timeout)
        timed_out = False
        code = completed.returncode
        output = completed.stdout + completed.stderr
    except subprocess.TimeoutExpired as error:
        timed_out = True
        code = None
        stdout = error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes) else (error.stdout or "")
        stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else (error.stderr or "")
        output = stdout + stderr
    return {
        "name": name,
        "command": [str(part) for part in command],
        "exit_code": code,
        "timed_out": timed_out,
        "wall_seconds": round(time.monotonic() - started, 3),
        "output_tail": output[-8000:],
    }


def write_result(result):
    temporary = CAMPAIGN / "result.json.tmp"
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    os.replace(temporary, CAMPAIGN / "result.json")


def main():
    if not os.environ.get("SLURM_JOB_ID"):
        raise SystemExit("SLURM only")
    bindings = {}
    for path, expected in EXPECTED.items():
        actual = sha256(path)
        if actual != expected:
            raise SystemExit(f"hash mismatch: {path}: {actual}")
        bindings[path] = actual
    reference = json.loads(TARGET_RESULT.read_text())
    checker_input = reference["proof_check"]["checker_input"]
    if (reference.get("rank") != 541 or reference.get("certificate_sha256") != EXPECTED[str(PROOF)] or
            checker_input.get("original_sha256") != EXPECTED_CNF or
            checker_input.get("checker_sha256") != EXPECTED_CNF):
        raise SystemExit("target identity contract mismatch")
    scratch = Path(tempfile.mkdtemp(prefix="r541-binary-lsr-", dir=os.environ.get("SLURM_TMPDIR", "/tmp")))
    result = {
        "schema": "rank541-binary-lsr-causal-probe-v1",
        "job_id": os.environ["SLURM_JOB_ID"],
        "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
        "checker_deadline_seconds": DEADLINE_SECONDS,
        "bindings": bindings,
        "stages": [],
    }
    try:
        cnf = scratch / "rank541.cnf"
        binary_lsr = scratch / "rank541.binary.lsr"
        with open(cnf, "wb") as target:
            subprocess.run(["xz", "-dc", INPUT], stdout=target, check=True)
        result["checker_cnf_sha256"] = sha256(cnf)
        result["checker_cnf_bytes"] = cnf.stat().st_size
        if result["checker_cnf_sha256"] != EXPECTED_CNF:
            raise RuntimeError("checker CNF hash mismatch")

        checker_started = time.monotonic()
        trim = run_stage("dsr-trim", [DSR_TRIM, cnf, PROOF, binary_lsr], DEADLINE_SECONDS)
        result["stages"].append(trim)
        if binary_lsr.exists():
            result["binary_lsr_bytes"] = binary_lsr.stat().st_size
            result["binary_lsr_sha256"] = sha256(binary_lsr)
        if trim["timed_out"] or trim["exit_code"] != 0 or "s VERIFIED UNSAT" not in trim["output_tail"]:
            result["outcome"] = "dsr-trim-failed"
            return 1
        remaining = max(0.001, DEADLINE_SECONDS - (time.monotonic() - checker_started))
        checked = run_stage("srcheck-binary", [SR_CHECK, cnf, binary_lsr, "c"], remaining)
        result["stages"].append(checked)
        verified = (not checked["timed_out"] and checked["exit_code"] == 0 and
                    "s VERIFIED UNSAT" in checked["output_tail"])
        result["outcome"] = "verified" if verified else "not-verified"
        result["checker_wall_seconds"] = round(time.monotonic() - checker_started, 3)
        return 0 if verified else 1
    except Exception as error:
        result["outcome"] = "error"
        result["error"] = repr(error)
        return 1
    finally:
        write_result(result)
        shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

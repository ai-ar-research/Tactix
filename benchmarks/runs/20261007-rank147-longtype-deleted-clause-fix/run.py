#!/usr/bin/env python3
"""Build and exercise the minimal LONGTYPE deleted-clause predicate repair."""

import datetime
import hashlib
import json
import lzma
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parent
MANIFEST = json.loads((ROOT / "manifest.json").read_text())


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def invoke(name, command, timeout=120, cwd=None):
    started = time.monotonic()
    completed = subprocess.run(
        [str(value) for value in command], stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False, cwd=cwd)
    return {
        "name": name,
        "command": [str(value) for value in command],
        "exit_code": completed.returncode,
        "wall_seconds": round(time.monotonic() - started, 3),
        "output_tail": completed.stdout.decode("utf-8", errors="replace")[-12000:],
    }


def require_hash(path, expected):
    observed = sha256(path)
    if observed != expected:
        raise RuntimeError(f"hash mismatch for {path}: {observed} != {expected}")
    return observed


def main():
    require_hash(ROOT / MANIFEST["source"]["archive"], MANIFEST["source"]["sha256"])
    rank = MANIFEST["rank147"]
    require_hash(rank["compressed_input"], rank["compressed_input_sha256"])
    require_hash(rank["proof"], rank["proof_sha256"])
    for pin in MANIFEST["authority_pins"].values():
        require_hash(pin["path"], pin["sha256"])

    stages = []
    with tempfile.TemporaryDirectory(prefix="rank147-longtype-fix-") as temp_name:
        temp = Path(temp_name)
        source = temp / "source"
        source.mkdir()
        stages.append(invoke("extract-source", ["tar", "-xf", ROOT / "source.tar", "-C", source]))
        stages.append(invoke("apply-fix", ["patch", "--batch", "--forward", "-p1",
                                           "-i", ROOT / "fix.patch"], timeout=30,
                             cwd=source))
        if stages[-1]["exit_code"] != 0:
            raise RuntimeError(stages[-1]["output_tail"])

        stages.append(invoke("make-clean", ["make", "clean"], timeout=60, cwd=source))
        stages.append(invoke("make-long", ["make", "long"], timeout=300, cwd=source))
        if stages[-1]["exit_code"] != 0:
            raise RuntimeError(stages[-1]["output_tail"])

        fixed_pins = ROOT / "fixed-pins"
        fixed_pins.mkdir(exist_ok=True)
        shutil.copy2(source / "dsr-trim", fixed_pins / "dsr-trim")
        shutil.copy2(source / "compress", fixed_pins / "dsr-compress")

        stages.append(invoke("compile-mask-reproducer", [
            "gcc", "-O2", "-Wall", "-Wextra", "-o", temp / "mask-reproducer",
            ROOT / "mask_reproducer.c"] ))
        stages.append(invoke("mask-reproducer", [temp / "mask-reproducer"]))

        cnf = temp / "rank147.cnf"
        with lzma.open(rank["compressed_input"], "rb") as source_input, cnf.open("wb") as target:
            shutil.copyfileobj(source_input, target)
        require_hash(cnf, rank["input_sha256"])
        proof = Path(rank["proof"])
        canonical = Path(MANIFEST["authority_pins"]["canonical32_dsr_trim"]["path"])
        unpatched = Path(MANIFEST["authority_pins"]["unpatched_long64_dsr_trim"]["path"])
        srcheck = Path(MANIFEST["authority_pins"]["srcheck"]["path"])
        fixed = fixed_pins / "dsr-trim"
        fixed_compress = fixed_pins / "dsr-compress"

        canonical_lsr = temp / "canonical32.lsr"
        fixed_lsr = temp / "fixed64.lsr"
        unpatched_lsr = temp / "unpatched64.lsr"
        stages.append(invoke("unpatched-long64-backward-negative", [
            unpatched, cnf, proof, unpatched_lsr]))
        stages.append(invoke("canonical32-backward-positive", [
            canonical, cnf, proof, canonical_lsr]))
        stages.append(invoke("fixed-long64-backward-positive", [
            fixed, cnf, proof, fixed_lsr]))

        canonical_sha = sha256(canonical_lsr)
        fixed_sha = sha256(fixed_lsr)
        fixed_text = temp / "fixed64.txt.lsr"
        stages.append(invoke("fixed-long64-decompress", [
            fixed_compress, "-d", fixed_lsr, fixed_text]))
        stages.append(invoke("trestle-fixed-long64", [srcheck, cnf, fixed_text]))

        canonical_forward = temp / "canonical32-forward.lsr"
        unpatched_forward = temp / "unpatched64-forward.lsr"
        stages.append(invoke("canonical32-forward", [
            canonical, "-f", cnf, proof, canonical_forward]))
        stages.append(invoke("unpatched-long64-forward", [
            unpatched, "-f", cnf, proof, unpatched_forward]))

        bad_cnf = temp / "bad.cnf"
        bad_proof = temp / "bad.dsr"
        bad_cnf.write_text("p cnf 2 4\n1 2 0\n1 -2 0\n1 0\n1 0\n")
        bad_proof.write_text("0\n")
        stages.append(invoke("canonical32-malformed-negative", [
            canonical, bad_cnf, bad_proof, temp / "bad32.lsr"]))
        stages.append(invoke("fixed-long64-malformed-negative", [
            fixed, bad_cnf, bad_proof, temp / "bad64.lsr"]))

        expected_error = MANIFEST["expected"]["unpatched_backward_error"]
        checks = {
            "mask_reproducer": stages[5]["exit_code"] == 0
                and "broken=0 fixed=1" in stages[5]["output_tail"],
            "unpatched_backward_rejects_at_known_site": stages[6]["exit_code"] == 1
                and expected_error in stages[6]["output_tail"],
            "canonical_backward_verifies": stages[7]["exit_code"] == 0
                and "s VERIFIED UNSAT" in stages[7]["output_tail"],
            "fixed_backward_verifies": stages[8]["exit_code"] == 0
                and "s VERIFIED UNSAT" in stages[8]["output_tail"],
            "fixed_lsr_matches_canonical": fixed_sha == canonical_sha
                == MANIFEST["expected"]["binary_lsr_sha256"],
            "fixed_text_lsr_matches": sha256(fixed_text)
                == MANIFEST["expected"]["text_lsr_sha256"],
            "trestle_verifies": stages[10]["exit_code"] == 0
                and "s VERIFIED UNSAT" in stages[10]["output_tail"],
            "forward_outputs_identical": sha256(canonical_forward)
                == sha256(unpatched_forward),
            "canonical_rejects_malformed": stages[13]["exit_code"] != 0
                and "s VERIFIED UNSAT" not in stages[13]["output_tail"],
            "fixed_rejects_malformed": stages[14]["exit_code"] != 0
                and "s VERIFIED UNSAT" not in stages[14]["output_tail"],
        }
        result = {
            "schema": "tactix-rank147-longtype-deleted-clause-fix-result-v1",
            "finished_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "job_id": os.environ.get("SLURM_JOB_ID"),
            "node": os.environ.get("SLURMD_NODENAME") or os.uname().nodename,
            "passed": all(checks.values()),
            "checks": checks,
            "fixed_pin_sha256": {
                "dsr-trim": sha256(fixed),
                "dsr-compress": sha256(fixed_compress),
            },
            "rank147": {
                "input_sha256": sha256(cnf),
                "proof_sha256": sha256(proof),
                "canonical_binary_lsr_sha256": canonical_sha,
                "fixed_binary_lsr_sha256": fixed_sha,
                "fixed_text_lsr_sha256": sha256(fixed_text),
                "forward_lsr_sha256": sha256(canonical_forward),
            },
            "stages": stages,
        }
        temporary = ROOT / "result.json.tmp"
        temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        temporary.replace(ROOT / "result.json")
        if not result["passed"]:
            raise SystemExit(1)


if __name__ == "__main__":
    main()

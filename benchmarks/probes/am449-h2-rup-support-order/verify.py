#!/usr/bin/env python3
"""Diagnostic: retime post-unit deletions outside that unit's RUP support."""

from hashlib import sha256
from pathlib import Path
import json
import subprocess
import sys

probe = Path(__file__).resolve().parent
root = probe.parents[2]
sys.path.insert(0, str(root / "tools/bench"))
import proof_oracle

pins = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
expected = {
    probe / "reduced.cnf": "6a1328d127f5aba0ebb01ab3e5f5bbb65bf7b5f27d09b514085f58393b82f73a",
    probe / "reduced-original.dsr": "d20123294529ef0c1c01c70f0a9ac196420860609e1d04e4380c60ec450e8649",
    probe / "original.cnf": "4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa",
    probe / "original-ascii.dsr": "af2f33c74a4f6a02cfa87b16f18a436a1e925a04ae92b2aaf0e7a6dfec7e8cdc",
    pins / "dsr-trim": "3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee",
    pins / "srcheck": "19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945",
}
for path, digest in expected.items():
    assert sha256(path.read_bytes()).hexdigest() == digest, path


def clause_key(line):
    literals = [int(token) for token in line.split()]
    assert literals[-1] == 0
    return tuple(sorted(literals[:-1]))


def run(command, log_path):
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    log = result.stdout + result.stderr
    log_path.write_text(log)
    return result.returncode == 0 and "s VERIFIED UNSAT" in log


def decode(path):
    lines = []
    for kind, tokens in proof_oracle._dsr_lines(path, True):
        literals = [(-1 if token & 1 else 1) * (token >> 1) for token in tokens]
        lines.append(("d " if kind == "d" else "") +
                     " ".join(map(str, literals)) + " 0\n")
    return lines


def check_case(name, cnf, lines):
    baseline = probe / f"{name}-baseline.dsr"
    baseline.write_text("".join(lines))
    assert lines[0] == "17 0\n"
    base_lsr = probe / f"{name}-baseline-forward.lsr"
    assert not run([str(pins / "dsr-trim"), "-f", str(cnf), str(baseline),
                    str(base_lsr)], probe / f"{name}-baseline-forward.log")
    first = [int(token) for token in base_lsr.read_text().splitlines()[0].split()]
    assert len(first) >= 5 and first[1:3] == [17, 0] and first[-1] == 0
    support_ids = first[3:-1]
    assert all(hint > 0 for hint in support_ids)
    clauses = [clause_key(line) for line in cnf.read_text().splitlines()
               if line and not line.startswith(("p", "c"))]
    assert max(support_ids) <= len(clauses)
    support = {clauses[hint - 1] for hint in support_ids}

    block = []
    for index, line in enumerate(lines[1:], 2):
        if not line.startswith("d "):
            break
        block.append(index)
    moved = [index for index in block
             if clause_key(lines[index - 1][2:]) not in support]
    transformed = [lines[index - 1] for index in moved] + [
        line for index, line in enumerate(lines, 1) if index not in moved]
    assert len(transformed) == len(lines)
    selected = probe / f"{name}-support-order.dsr"
    selected.write_text("".join(transformed))
    for direction, flag in (("forward", ["-f"]), ("backward", [])):
        lsr = probe / f"{name}-{direction}.lsr"
        assert run([str(pins / "dsr-trim"), *flag, str(cnf), str(selected),
                    str(lsr)], probe / f"{name}-{direction}.log"), direction
        assert run([str(pins / "srcheck"), str(cnf), str(lsr)],
                   probe / f"{name}-{direction}-trestle.log"), direction
    return {
        "case": name,
        "input_clauses": len(clauses),
        "first_unit_support_ids": support_ids,
        "deletion_block_lines": block,
        "moved_lines": moved,
        "proof_sha256": sha256(selected.read_bytes()).hexdigest(),
        "forward_lsr_sha256": sha256((probe / f"{name}-forward.lsr").read_bytes()).hexdigest(),
        "backward_lsr_sha256": sha256((probe / f"{name}-backward.lsr").read_bytes()).hexdigest(),
    }


results = [
    check_case("reduced", probe / "reduced.cnf", decode(probe / "reduced-original.dsr")),
    check_case("original", probe / "original.cnf",
               (probe / "original-ascii.dsr").read_text().splitlines(keepends=True)),
]
assert results[0]["moved_lines"] == [7]
assert results[1]["moved_lines"] == list(range(2, 24)) + [29, 30]
(probe / "result.json").write_text(json.dumps(results, indent=2) + "\n")
print("Two support-preserving H2 controls pass both no-option directions and Trestle")

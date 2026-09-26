#!/usr/bin/env python3
"""Checker-only controls for a resolution bridge and an early root-reason deletion."""
from pathlib import Path
import subprocess
import tarfile

root = Path(__file__).resolve().parents[4]
here = Path(__file__).resolve().parent
pin = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
d1 = root / "benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection"
h2 = root / "benchmarks/probes/am436-h2-shrink/minimal.cnf"


def resolvent(left, right, pivot):
    assert pivot in left and -pivot in right
    result = sorted(set(left + right) - {pivot, -pivot})
    assert all(-lit not in result for lit in result)
    return " ".join(map(str, result)) + " 0\n"


def move_before_first(proof, deletion, bridge=""):
    assert proof.count(deletion) == 1
    return bridge + deletion + proof.replace(deletion, "", 1)


d1_source = (d1 / "min-rel.dsr.txt").read_text()
assert d1_source.startswith("-14 0\n")
d1_bridge = resolvent([10, 21], [9, -21], 21)
assert d1_bridge == "9 10 0\n"
d1_proof = here / "d1-bridge.dsr"
d1_proof.write_text(move_before_first(d1_source, "d 10 21 0\n", d1_bridge))

with tarfile.open(root / "benchmarks/probes/am437-h2-unit-publication/artifacts.tar.gz") as archive:
    h2_source = archive.extractfile("./baseline.dsr").read().decode()
assert h2_source.startswith("17 0\n")
h2_proof = here / "h2-early-reason.dsr"
h2_proof.write_text(move_before_first(h2_source, "d -18 19 0\n"))

for label, cnf, proof in (("D1", d1 / "min.cnf", d1_proof), ("H2", h2, h2_proof)):
    for direction in ("forward", "backward"):
        lsr = here / f"{label.lower()}-{direction}.lsr"
        args = [str(pin / "dsr-trim")]
        if direction == "forward":
            args.append("-f")
        args += [str(cnf), str(proof), str(lsr)]
        result = subprocess.run(args, capture_output=True, text=True)
        assert result.returncode == 0 and "s VERIFIED UNSAT" in result.stdout, (label, direction, result.stdout, result.stderr)
        checked = subprocess.run([str(pin / "srcheck"), str(cnf), str(lsr)], capture_output=True, text=True)
        assert checked.returncode == 0 and "s VERIFIED UNSAT" in checked.stdout, (label, direction, checked.stdout, checked.stderr)
        print(f"{label} {direction}: dsr-trim and Trestle VERIFIED UNSAT")

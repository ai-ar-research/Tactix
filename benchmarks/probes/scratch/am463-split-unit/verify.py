#!/usr/bin/env python3
"""No-option DSR controls for a reusable two-clause RUP-unit split."""
from pathlib import Path
import hashlib
import subprocess
import tarfile

root = Path(__file__).resolve().parents[4]
here = Path(__file__).resolve().parent
pin = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
d1 = root / "benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection"
with tarfile.open(root / "benchmarks/probes/am437-h2-unit-publication/artifacts.tar.gz") as archive:
    h2_minimal = archive.extractfile("./baseline.dsr").read().decode()


def split_first_unit(source, unit, auxiliary):
    lines = source.splitlines(keepends=True)
    assert lines[0] == f"{unit} 0\n"
    end = next((i for i in range(1, len(lines)) if not lines[i].startswith("d ")), len(lines))
    helpers = f"{unit} {auxiliary} 0\n{unit} {-auxiliary} 0\n"
    deletions = "".join(lines[1:end])
    retire = f"d {unit} {auxiliary} 0\nd {unit} {-auxiliary} 0\n"
    return helpers + deletions + lines[0] + retire + "".join(lines[end:])


cases = [
    ("d1", d1 / "min.cnf", (d1 / "min-rel.dsr.txt").read_text(), -14, 30),
    ("h2-minimal", root / "benchmarks/probes/am436-h2-shrink/minimal.cnf", h2_minimal, 17, 22),
    ("h2-original", root / "benchmarks/probes/am448-h2-subset-order/checker.cnf",
     (root / "benchmarks/probes/am448-h2-subset-order/baseline-ascii.dsr").read_text(), 17, 22),
]

for label, cnf, source, unit, auxiliary in cases:
    proof = here / f"{label}-split.dsr"
    proof.write_text(split_first_unit(source, unit, auxiliary))
    print(label, hashlib.sha256(proof.read_bytes()).hexdigest())
    for direction in ("forward", "backward"):
        lsr = here / f"{label}-{direction}.lsr"
        command = [str(pin / "dsr-trim")]
        if direction == "forward":
            command.append("-f")
        command += [str(cnf), str(proof), str(lsr)]
        check = subprocess.run(command, capture_output=True, text=True)
        assert check.returncode == 0 and "s VERIFIED UNSAT" in check.stdout, (label, direction, check.stdout, check.stderr)
        trestle = subprocess.run([str(pin / "srcheck"), str(cnf), str(lsr)], capture_output=True, text=True)
        assert trestle.returncode == 0 and "s VERIFIED UNSAT" in trestle.stdout, (label, direction, trestle.stdout, trestle.stderr)
        print(label, direction, "dsr-trim and Trestle VERIFIED UNSAT")

# Reuse the same proof-only identity after its two helper deletions in D1.
proof = here / "d1-split-reuse.dsr"
source = (here / "d1-split.dsr").read_text()
marker = "\n-16 0\n"
assert source.count(marker) == 1
proof.write_text(source.replace(marker,
    "\n-16 30 0\n-16 -30 0\n-16 0\nd -16 30 0\nd -16 -30 0\n", 1))
for direction in ("forward", "backward"):
    lsr = here / f"d1-reuse-{direction}.lsr"
    command = [str(pin / "dsr-trim")]
    if direction == "forward":
        command.append("-f")
    command += [str(d1 / "min.cnf"), str(proof), str(lsr)]
    check = subprocess.run(command, capture_output=True, text=True)
    assert check.returncode == 0 and "s VERIFIED UNSAT" in check.stdout, (direction, check.stdout, check.stderr)
    trestle = subprocess.run([str(pin / "srcheck"), str(d1 / "min.cnf"), str(lsr)], capture_output=True, text=True)
    assert trestle.returncode == 0 and "s VERIFIED UNSAT" in trestle.stdout, (direction, trestle.stdout, trestle.stderr)
    print("d1-reuse", direction, "dsr-trim and Trestle VERIFIED UNSAT")

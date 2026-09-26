#!/usr/bin/env python3
"""Exhaustive checker-only ordering control on AM423's original H2 proof."""

from hashlib import sha256
from itertools import product
from pathlib import Path
import csv
import subprocess
import tempfile

probe = Path(__file__).resolve().parent
root = probe.parents[2]
pins = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
cnf = probe / "checker.cnf"
baseline = probe / "baseline-ascii.dsr"
expected = {
    cnf: "4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa",
    baseline: "af2f33c74a4f6a02cfa87b16f18a436a1e925a04ae92b2aaf0e7a6dfec7e8cdc",
    pins / "dsr-trim": "3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee",
    pins / "srcheck": "19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945",
}
for path, digest in expected.items():
    assert sha256(path.read_bytes()).hexdigest() == digest, path

lines = baseline.read_text().splitlines(keepends=True)
assert len(lines) == 134 and lines[0] == "17 0\n"
positions = (19, 21, 23, 28, 29, 30)
assert all(lines[i - 1].startswith("d ") for i in positions)
selected = (21, 23, 29)
rows = []
both = []
with tempfile.TemporaryDirectory(prefix="am448-", dir="/tmp") as temp:
    temp = Path(temp)
    for bits in product((0, 1), repeat=len(positions)):
        moved = tuple(i for i, bit in zip(positions, bits) if bit)
        mask = sum(bit << i for i, bit in enumerate(bits))
        proof = temp / "proof.dsr"
        proof.write_text("".join(lines[i - 1] for i in moved) +
                         "".join(line for i, line in enumerate(lines, 1) if i not in moved))
        results = []
        for mode, switch in (("forward", ("-f",)), ("backward", ())):
            lsr = temp / f"{mode}.lsr"
            result = subprocess.run(
                [str(pins / "dsr-trim"), *switch, str(cnf), str(proof), str(lsr)],
                capture_output=True, text=True, check=False,
            )
            log = result.stdout + result.stderr
            good = result.returncode == 0 and "s VERIFIED UNSAT" in log
            error = next((line for line in log.splitlines() if line.startswith("Error:")), "")
            results.append((good, error))
            if moved == selected:
                (probe / f"selected-{mode}.log").write_text(log)
                (probe / f"selected-{mode}.lsr").write_bytes(lsr.read_bytes())
        rows.append((mask, moved, *results))
        if all(result[0] for result in results):
            both.append(moved)
        if moved == selected:
            (probe / "selected.dsr").write_bytes(proof.read_bytes())

with (probe / "matrix.tsv").open("w", newline="") as stream:
    writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
    writer.writerow(("mask", "early_physical_lines", "forward", "forward_error",
                     "backward", "backward_error"))
    for mask, moved, forward, backward in sorted(rows):
        writer.writerow((mask, ",".join(map(str, moved)) or "-",
                         "PASS" if forward[0] else "FAIL", forward[1] or "-",
                         "PASS" if backward[0] else "FAIL", backward[1] or "-"))

assert len(rows) == 64
assert sum(row[2][0] for row in rows) == 4
assert sum(row[3][0] for row in rows) == 16
assert set(both) == {
    (21, 23, 29), (19, 21, 23, 29),
    (21, 23, 29, 30), (19, 21, 23, 29, 30),
}
for mode in ("forward", "backward"):
    result = subprocess.run(
        [str(pins / "srcheck"), str(cnf), str(probe / f"selected-{mode}.lsr")],
        capture_output=True, text=True, check=False,
    )
    log = result.stdout + result.stderr
    (probe / f"selected-{mode}-trestle.log").write_text(log)
    assert result.returncode == 0 and "s VERIFIED UNSAT" in log, mode
print("64/64 subsets checked; four pass in both directions; selected LSRs pass Trestle")

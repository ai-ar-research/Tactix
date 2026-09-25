#!/usr/bin/env python3
"""Bounded checker-only diagnostic for the AM369 H2 six-deletion set."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CNF = ROOT / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch/case.cnf"
PROOF = ROOT / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch/nox5.ascii.dsr"
CHECKER = ROOT / "benchmarks/runs/20260923-am341-unwatch/pins/dsr-trim"
SRCHECK = ROOT / "benchmarks/runs/20260923-am341-unwatch/pins/srcheck"
TARGETS = (
    "d 7 -20 0",
    "d -8 -9 0",
    "d -9 15 0",
    "d -15 16 0",
    "d -19 20 0",
    "d -20 21 0",
)

lines = PROOF.read_text().splitlines()
assert lines[0] == "17 0"
assert all(lines.count(target) == 1 for target in TARGETS)

with tempfile.TemporaryDirectory(prefix="am387-h2-subsets-") as directory:
    directory = Path(directory)
    accepted = []
    forward_accepted = []
    for mask in range(64):
        chosen = {target for bit, target in enumerate(TARGETS) if mask & (1 << bit)}
        moved = [line for line in lines[1:] if line in chosen]
        retained = [line for line in lines[1:] if line not in chosen]
        candidate = directory / f"{mask:02d}.dsr"
        lsr = directory / f"{mask:02d}.lsr"
        candidate.write_text("\n".join(moved + [lines[0]] + retained) + "\n")
        check = subprocess.run(
            [str(CHECKER), "-f", str(CNF), str(candidate), str(lsr)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False,
        )
        if check.returncode != 0:
            first = check.stdout.splitlines()[0] if check.stdout else "NO OUTPUT"
            print(f"mask={mask:02d} FORWARD FAIL {first}")
            continue
        forward_accepted.append(mask)
        forward_srcheck = subprocess.run(
            [str(SRCHECK), str(CNF), str(lsr)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False,
        )
        backward = subprocess.run(
            [str(CHECKER), str(CNF), str(candidate), str(lsr)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False,
        )
        backward_srcheck = None
        if backward.returncode == 0:
            backward_srcheck = subprocess.run(
                [str(SRCHECK), str(CNF), str(lsr)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
        tre_f_ok = forward_srcheck.returncode == 0 and "s VERIFIED UNSAT" in forward_srcheck.stdout
        tre_b_ok = (backward_srcheck is not None and backward_srcheck.returncode == 0
                    and "s VERIFIED UNSAT" in backward_srcheck.stdout)
        print(
            f"mask={mask:02d} FORWARD PASS "
            f"TRE_F={'PASS' if tre_f_ok else 'FAIL'} "
            f"BACKWARD={'PASS' if backward.returncode == 0 else 'FAIL'} "
            f"TRE_B={('PASS' if tre_b_ok else 'FAIL') if backward_srcheck else 'N/A'} "
            f"BACKWARD_MSG={backward.stdout.splitlines()[0] if backward.stdout else '-'}"
        )
        if backward.returncode == 0 and tre_f_ok and tre_b_ok:
            accepted.append(mask)
    assert forward_accepted == [22, 23, 54, 55], forward_accepted
    assert accepted == [22, 23, 54, 55], accepted

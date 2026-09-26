#!/usr/bin/env python3
"""Try a generic one-resolvent-per-deletion unit-support transaction."""
from pathlib import Path
import subprocess
import sys

from analyze import read_cnf, resolvents, root, rup_support

here = Path(__file__).resolve().parent
pin = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
cnf = root / "benchmarks/probes/am448-h2-subset-order/checker.cnf"
baseline = root / "benchmarks/probes/am448-h2-subset-order/baseline-ascii.dsr"
clauses = read_cnf(cnf)
lines = baseline.read_text().splitlines(keepends=True)
assert lines[0] == "17 0\n"
protected_lines = (19, 21, 23, 28, 29, 30)  # AM445 frozen checker diagnosis
if len(sys.argv) == 2 and sys.argv[1] == "--all":
    protected_lines = tuple(i for i in range(2, 31) if lines[i - 1].startswith("d "))
early = []
removed = set()
for physical in protected_lines:
    deletion = lines[physical - 1]
    assert deletion.startswith("d ")
    target = tuple(map(int, deletion.split()[1:-1]))
    idx = next(i for i, clause in enumerate(clauses)
               if i not in removed and set(clause) == set(target))
    remaining = [clause for i, clause in enumerate(clauses) if i not in removed | {idx}]
    if rup_support(remaining, 17) is None:
        choices = []
        for partner_idx, partner in enumerate(clauses):
            if partner_idx in removed | {idx}:
                continue
            for bridge in resolvents(target, partner):
                if rup_support(remaining + [bridge], 17) is not None:
                    choices.append((len(bridge), bridge, partner_idx))
        assert choices, (physical, target)
        _, bridge, partner_idx = min(choices)
        print("bridge", physical, target, "with", partner_idx + 1, "=>", bridge)
        early.append(" ".join(map(str, bridge)) + " 0\n")
        clauses.append(bridge)
    early.append(deletion)
    removed.add(idx)
    assert rup_support([clause for i, clause in enumerate(clauses) if i not in removed], 17) is not None

proof = here / ("h2-original-greedy-all.dsr" if len(sys.argv) == 2 else "h2-original-greedy.dsr")
proof.write_text("".join(early) + "".join(line for i, line in enumerate(lines, 1)
                                          if i not in protected_lines))
for direction in ("forward", "backward"):
    lsr = here / f"h2-original-greedy-{direction}.lsr"
    command = [str(pin / "dsr-trim")]
    if direction == "forward":
        command.append("-f")
    command += [str(cnf), str(proof), str(lsr)]
    result = subprocess.run(command, capture_output=True, text=True)
    print(direction, "dsr-trim", result.returncode,
          next((line for line in (result.stdout + result.stderr).splitlines()
                if line.startswith(("Error:", "s VERIFIED"))), ""))
    if result.returncode == 0:
        checked = subprocess.run([str(pin / "srcheck"), str(cnf), str(lsr)],
                                 capture_output=True, text=True)
        print(direction, "Trestle", checked.returncode,
              next((line for line in checked.stdout.splitlines()
                    if line.startswith("s VERIFIED")), ""))

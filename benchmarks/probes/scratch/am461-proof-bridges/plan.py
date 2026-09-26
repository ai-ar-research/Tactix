#!/usr/bin/env python3
"""Checker-independent root-reason transaction control on three frozen proofs.

This is a bounded diagnostic, not the production proof writer: it handles a
first explicit unit and its following deletion block over original clauses.
"""
from pathlib import Path
import hashlib
import subprocess
import tarfile

from analyze import read_cnf, resolvents, root, root_reasons, rup_support

here = Path(__file__).resolve().parent
pin = root / "benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
d1 = root / "benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection"
with tarfile.open(root / "benchmarks/probes/am437-h2-unit-publication/artifacts.tar.gz") as archive:
    minimal_h2 = archive.extractfile("./baseline.dsr").read().decode()


def plan(label, cnf, source, unit):
    clauses = read_cnf(cnf)
    lines = source.splitlines(keepends=True)
    assert lines[0] == f"{unit} 0\n"
    end = next((i for i in range(1, len(lines)) if not lines[i].startswith("d ")), len(lines))
    values, reason_by_literal, conflict = root_reasons(clauses, unit)
    assert not conflict
    reason_ids = set(reason for reason in reason_by_literal.values() if reason is not None)
    early = []
    moved = []
    live = list(enumerate(clauses))
    bridges = []
    for physical in range(1, end):
        line = lines[physical]
        target = tuple(map(int, line.split()[1:-1]))
        matches = [(idx, clause) for idx, clause in live if set(clause) == set(target)]
        assert len(matches) == 1, (label, physical, target, matches)
        target_id, target_clause = matches[0]
        if target_id not in reason_ids:
            continue
        remaining = [(idx, clause) for idx, clause in live if idx != target_id]
        if rup_support([clause for _, clause in remaining], unit) is None:
            choices = []
            for partner_id, partner in live:
                if partner_id == target_id:
                    continue
                for bridge in resolvents(target_clause, partner):
                    if rup_support([clause for _, clause in remaining] + [bridge], unit) is not None:
                        choices.append((len(bridge), bridge, partner_id))
            assert choices, (label, physical, target)
            _, bridge, partner_id = min(choices)
            early.append(" ".join(map(str, bridge)) + " 0\n")
            bridges.append((bridge, target_id + 1, partner_id + 1))
            live.append((len(clauses), bridge))
            clauses.append(bridge)
            remaining.append((len(clauses) - 1, bridge))
        early.append(line)
        moved.append(physical)
        live = remaining
        assert rup_support([clause for _, clause in live], unit) is not None
    result = here / f"{label}-planned.dsr"
    result.write_text("".join(early) + "".join(line for i, line in enumerate(lines)
                                           if i not in moved))
    print(label, "root_literals", len(values), "moved_lines", [i + 1 for i in moved],
          "bridges", bridges, "sha256", hashlib.sha256(result.read_bytes()).hexdigest())
    for direction in ("forward", "backward"):
        lsr = here / f"{label}-planned-{direction}.lsr"
        command = [str(pin / "dsr-trim")]
        if direction == "forward":
            command.append("-f")
        command += [str(cnf), str(result), str(lsr)]
        checked = subprocess.run(command, capture_output=True, text=True)
        assert checked.returncode == 0 and "s VERIFIED UNSAT" in checked.stdout, (label, direction, checked.stdout, checked.stderr)
        trestle = subprocess.run([str(pin / "srcheck"), str(cnf), str(lsr)], capture_output=True, text=True)
        assert trestle.returncode == 0 and "s VERIFIED UNSAT" in trestle.stdout, (label, direction, trestle.stdout, trestle.stderr)
        print(label, direction, "dsr-trim and Trestle VERIFIED UNSAT")


plan("d1", d1 / "min.cnf", (d1 / "min-rel.dsr.txt").read_text(), -14)
plan("h2-minimal", root / "benchmarks/probes/am436-h2-shrink/minimal.cnf", minimal_h2, 17)
plan("h2-original", root / "benchmarks/probes/am448-h2-subset-order/checker.cnf",
     (root / "benchmarks/probes/am448-h2-subset-order/baseline-ascii.dsr").read_text(), 17)

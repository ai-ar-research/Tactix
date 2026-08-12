#!/usr/bin/env python3
"""Kissat overlay record/replay and exact-field mismatch smoke test."""

import os
import pathlib
import subprocess
import sys
import tempfile


binary = pathlib.Path(sys.argv[1]).resolve()


def run(cnf, env):
    result = subprocess.run([str(binary), str(cnf)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode in (10, 20), result.stderr
    return result


with tempfile.TemporaryDirectory(prefix="kissat-rule-journal-") as tmp:
    root = pathlib.Path(tmp)
    cnf = root / "php-4-3.cnf"
    clauses = []
    variable = lambda pigeon, hole: pigeon * 3 + hole + 1
    for pigeon in range(4):
        clauses.append([variable(pigeon, hole) for hole in range(3)])
    for hole in range(3):
        for first in range(4):
            for second in range(first + 1, 4):
                clauses.append([-variable(first, hole),
                                -variable(second, hole)])
    cnf.write_text(
        f"p cnf 12 {len(clauses)}\n" +
        "".join(" ".join(map(str, clause)) + " 0\n" for clause in clauses))

    journal = root / "record.tsv"
    env = os.environ.copy()
    env["KISSAT_RULE_JOURNAL"] = str(journal)
    run(cnf, env)
    text = journal.read_text()
    records = [line for line in text.splitlines() if not line.startswith("#")]
    assert records
    assert "\ttransaction\t" in text
    assert "scheduled=1" in text

    env = os.environ.copy()
    env["KISSAT_RULE_JOURNAL_REPLAY"] = str(journal)
    run(cnf, env)

    altered = root / "altered.tsv"
    altered.write_text(text.replace("scheduled=1", "scheduled=0", 1))
    env["KISSAT_RULE_JOURNAL_REPLAY"] = str(altered)
    failed = subprocess.run([str(binary), str(cnf)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert failed.returncode not in (10, 20)
    assert "field 'guards'" in failed.stderr
    assert "expected: scheduled=0" in failed.stderr
    assert "actual:   scheduled=1" in failed.stderr

print("test_kissat_rule_journal OK")

#!/usr/bin/env python3
"""Exercise journal coverage through DefaultPolicy's static CDCL dispatch."""

import os
import pathlib
import subprocess
import sys
import tempfile


binary = pathlib.Path(sys.argv[1]).resolve()
disabled = ("BCE,Backbone,BVE,CCE,Congruence,Factor,HyperBinaryRes,"
            "HyperTernaryRes,Instantiate,Probe,Strengthen,Substitute,"
            "SubsumeForward,Sweep,TransitiveReduce,Vivify")


def run(cnf, env):
    result = subprocess.run([str(binary), str(cnf)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode in (10, 20), result.stderr
    return result


with tempfile.TemporaryDirectory(prefix="tactix-default-journal-") as tmp:
    root = pathlib.Path(tmp)
    cnf = root / "php-6-5.cnf"
    clauses = []
    variable = lambda pigeon, hole: pigeon * 5 + hole + 1
    for pigeon in range(6):
        clauses.append([variable(pigeon, hole) for hole in range(5)])
    for hole in range(5):
        for first in range(6):
            for second in range(first + 1, 6):
                clauses.append([-variable(first, hole),
                                -variable(second, hole)])
    cnf.write_text(
        f"p cnf 30 {len(clauses)}\n" +
        "".join(" ".join(map(str, clause)) + " 0\n" for clause in clauses))

    journal = root / "record.tsv"
    env = os.environ.copy()
    env["TACTIX_SLOT_DISABLE"] = disabled
    env["TACTIX_RULE_JOURNAL"] = str(journal)
    env["TACTIX_RULE_JOURNAL_LIMIT"] = "20000"
    run(cnf, env)
    text = journal.read_text()
    records = [line for line in text.splitlines() if not line.startswith("#")]
    for rule in ("Propagate", "Lucky", "Decide", "ConflictAnalysis"):
        if f"\t{rule}\t" not in text:
            seen = sorted({line.split("\t")[3] for line in text.splitlines()
                           if line and not line.startswith("#")})
            raise AssertionError(
                f"{rule}: recorded rules={seen}; records={records[:5]}")
    assert "literal=" in text
    assert "conflict=" in text

    env = os.environ.copy()
    env["TACTIX_SLOT_DISABLE"] = disabled
    env["TACTIX_RULE_JOURNAL_REPLAY"] = str(journal)
    env["TACTIX_RULE_JOURNAL_LIMIT"] = "20000"
    run(cnf, env)

print("test_tactix_default_journal OK")

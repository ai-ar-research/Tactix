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

    # Every inprocessing family is measured in its source-defined local
    # currency at the scheduler boundary.  Exercise one batch family here;
    # candidate-oriented families share the same wrapper and have focused
    # rule tests for their portable candidate identities.
    inprocess = root / "inprocess.tsv"
    env = os.environ.copy()
    env["TACTIX_SLOT_DISABLE"] = disabled.replace("Congruence,", "")
    env["TACTIX_RULE_JOURNAL"] = str(inprocess)
    env["TACTIX_RULE_JOURNAL_RULE"] = "Congruence"
    env["TACTIX_RULE_JOURNAL_LIMIT"] = "20000"
    run(cnf, env)
    inprocess_text = inprocess.read_text()
    invocation = next(
        line for line in inprocess_text.splitlines()
        if "\tinprocess-invocation\t" in line)
    assert "currency=congruence-ticks" in invocation
    for field in ("reference=", "allowance=", "entry=", "exit=",
                  "delta=", "overshoot=", "terminal="):
        assert field in invocation, invocation
    # Congruence has no EFFORT_SPECS row: the journal meter is diagnostic
    # only, so the record is inclusive (exit - entry in the rule's own
    # counter) and never bounded, overshot, or exhausted.
    context = next(f for f in invocation.split("\t") if "currency=" in f)
    kv = dict(item.split("=", 1) for item in context.split(";"))
    assert kv["bounded"] == "0", context
    assert kv["allowance"] == "0", context
    assert kv["terminal"] == "unbounded", context
    assert kv["overshoot"] == "0", context
    assert int(kv["delta"]) == int(kv["exit"]) - int(kv["entry"]), context
    assert int(kv["delta"]) > 0, context
    # The rule's own attempt record is priced in the same diagnostic currency
    # (column 18 is effort): both sides of the slot boundary read the
    # inclusive congruence counter, so the efforts agree exactly.
    attempts = [line.split("\t") for line in inprocess_text.splitlines()
                if "\tCongruence\trule-attempt\t" in line]
    assert len(attempts) == 1, inprocess_text[:2000]
    assert int(attempts[0][18]) == int(kv["delta"]), (attempts[0][18], kv)

print("test_tactix_default_journal OK")

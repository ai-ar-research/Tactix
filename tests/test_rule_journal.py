#!/usr/bin/env python3
"""Record, replay, and exact-field mismatch checks for the rule journal."""

import os
import pathlib
import subprocess
import sys
import tempfile


binary = pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix="tactix-rule-journal-") as tmp:
    journal = pathlib.Path(tmp) / "record.tsv"
    env = os.environ.copy()
    env["TACTIX_RULE_JOURNAL"] = str(journal)
    subprocess.run([str(binary)], env=env, check=True)

    text = journal.read_text()
    records = [line for line in text.splitlines() if not line.startswith("#")]
    assert len(records) == 5
    for outcome in ("applied", "declined", "no-op", "refuted"):
        assert f"\t{outcome}\t" in text
    assert "\tbudget-exhausted\t" in text
    assert "add-binary(id=17;lits=1,-2)" in text
    assert "root-unit(lit=1;reason=" in text
    assert "retire-var(var=2;status=substituted)" in text
    assert "reconstruct(witness=1;rest=-2)" in text
    assert "rup(id=17;chain=3,5,8)" in text

    filtered = pathlib.Path(tmp) / "filtered.tsv"
    env = os.environ.copy()
    env["TACTIX_RULE_JOURNAL"] = str(filtered)
    env["TACTIX_RULE_JOURNAL_CANDIDATE_CONTAINS"] = "cline"
    subprocess.run([str(binary)], env=env, check=True)
    filtered_records = [line for line in filtered.read_text().splitlines()
                        if not line.startswith("#")]
    assert len(filtered_records) == 1
    assert "\tdeclined\t" in filtered_records[0]

    env = os.environ.copy()
    env["TACTIX_RULE_JOURNAL_REPLAY"] = str(journal)
    subprocess.run([str(binary)], env=env, check=True)

    altered = pathlib.Path(tmp) / "altered.tsv"
    altered.write_text(text.replace("eligible=1", "eligible=0", 1))
    env["TACTIX_RULE_JOURNAL_REPLAY"] = str(altered)
    failed = subprocess.run([str(binary)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert failed.returncode != 0
    assert "field 'guards'" in failed.stderr
    assert "expected: eligible=0" in failed.stderr
    assert "actual:   eligible=1" in failed.stderr

print("test_rule_journal OK")

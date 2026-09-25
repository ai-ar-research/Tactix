#!/usr/bin/env python3
"""A public pre-solve X5 record survives later API solve sessions."""

import os
import pathlib
import subprocess
import sys
import tempfile


binary = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="tactix-api-journal-") as tmp:
    journal = pathlib.Path(tmp) / "record.tsv"
    env = os.environ.copy()
    env["TACTIX_RULE_JOURNAL"] = str(journal)
    env["TACTIX_RULE_JOURNAL_RULE"] = "Condition"
    subprocess.run([str(binary)], env=env, check=True)
    records = [line.split("\t") for line in journal.read_text().splitlines()
               if line and not line.startswith("#")]
    applied = [row for row in records if row[3] == "Condition" and
               row[14] == "applied"]
    assert len(applied) == 2, records
    assert [int(row[1]) for row in applied] == [0, 1], applied

    replay = os.environ.copy()
    replay["TACTIX_RULE_JOURNAL_REPLAY"] = str(journal)
    replay["TACTIX_RULE_JOURNAL_RULE"] = "Condition"
    subprocess.run([str(binary)], env=replay, check=True)

    extra = pathlib.Path(tmp) / "extra.tsv"
    extra.write_text(journal.read_text() + "\t".join(applied[-1]) + "\n")
    replay["TACTIX_RULE_JOURNAL_REPLAY"] = str(extra)
    failed = subprocess.run([str(binary)], env=replay, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert failed.returncode != 0
    assert "expected additional record" in failed.stderr, failed.stderr

print("test_api_journal OK")

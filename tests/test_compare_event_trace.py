#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys
import tempfile


HEADER = (
    "# tactix-event-trace\t1\n"
    "# solver\tseq\tevent\tconflicts\tdecisions\tpropagations\tlevel\tmode\t"
    "active\tunassigned\ttrail\tqueue\theap\tretired_queue\tretired_heap\t"
    "irred\tred\tbinary\tdigest\tsearch_external\ttop32\tpayload\n"
)


def row(solver, seq, event, decisions=0, payload="-"):
    values = [solver, seq, event, 0, decisions, 0, 0, "focused", 3, 3, 0,
              3, 0, 0, 0, 2, 0, 1, "abcd", 1, "1,2,3", payload]
    return "\t".join(map(str, values)) + "\n"


def run(*args):
    return subprocess.run([sys.executable, str(TOOL), *map(str, args)],
                          text=True, capture_output=True)


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "bench" / "compare_event_trace.py"

with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    a = tmp / "a.tsv"
    b = tmp / "b.tsv"
    report = tmp / "report.json"
    common = row("tactix", 0, "startup_begin")
    a.write_text(HEADER + common + row("tactix", 1, "decision", 1, "1"))
    b.write_text(HEADER + common.replace("tactix", "kissat") +
                 row("kissat", 1, "decision", 1, "-2"))

    unexplained = run(a, b, "--mode", "semantic", "--json", report,
                      "--require-explained")
    assert unexplained.returncode == 2, unexplained
    data = json.loads(report.read_text())
    assert data["first_divergence"]["kind"] == "trajectory"
    assert not data["explained"]
    assert data["first_divergence"]["delta"]["payload"] == ["1", "-2"]

    explained = run(a, b, "--mode", "semantic", "--json", report,
                    "--require-explained", "--disposition", "actionable",
                    "--cause", "different initial ranking",
                    "--evidence", "kissat/src/decide.c:1")
    assert explained.returncode == 1, explained
    assert json.loads(report.read_text())["explained"]

    identical = run(a, a, "--mode", "semantic", "--require-explained")
    assert identical.returncode == 0, identical

print("test_compare_event_trace OK")

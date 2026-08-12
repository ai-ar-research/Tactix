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


def row(solver, seq, event, decisions=0, payload="-", irred=2,
        top32="1,2,3"):
    values = [solver, seq, event, 0, decisions, 0, 0, "focused", 3, 3, 0,
              3, 0, 0, 0, irred, 0, 1, "abcd", 1, top32, payload]
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
                      "--require-closed")
    assert unexplained.returncode == 2, unexplained
    data = json.loads(report.read_text())
    assert data["first_divergence"]["kind"] == "trajectory"
    assert not data["explained"]
    assert data["first_divergence"]["delta"]["payload"] == ["1", "-2"]

    localized = run(a, b, "--mode", "semantic", "--json", report,
                    "--require-closed", "--status", "actionable",
                    "--disposition", "repair-required", "--strength", "weaker",
                    "--cause", "different initial ranking",
                    "--evidence", "kissat/src/decide.c:1")
    assert localized.returncode == 2, localized
    data = json.loads(report.read_text())
    assert data["schema"] == "tactix-kissat-divergence-v2"
    assert data["localized"] and not data["closed"]

    closed = run(a, b, "--mode", "semantic", "--json", report,
                 "--require-closed", "--status", "closed",
                 "--disposition", "repaired", "--strength", "weaker",
                 "--cause", "different initial ranking",
                 "--evidence", "kissat/src/decide.c:1",
                 "--reproducer", "fixture-a", "--owner", "DefaultPolicy",
                 "--reopening-condition", "ranking diverges again")
    assert closed.returncode == 1, closed
    assert json.loads(report.read_text())["closed"]

    terminal = row("tactix", 2, "terminate", payload="20")
    a.write_text(HEADER + common + terminal)
    identical = run(a, a, "--mode", "semantic", "--require-closed")
    assert identical.returncode == 0, identical

    # A capped or externally interrupted identical prefix is not a match.
    a.write_text(HEADER + common + "# truncated at 1 records\n")
    truncated = run(a, a, "--mode", "semantic", "--json", report,
                    "--require-closed")
    assert truncated.returncode == 2, truncated
    data = json.loads(report.read_text())
    assert data["trace_status"]["a"]["truncated"]
    assert not data["complete_match"]

    a.write_text(HEADER + common)
    incomplete = run(a, a, "--mode", "semantic")
    assert incomplete.returncode == 1, incomplete

    # Snapshotless detailed records must not become false state divergences.
    # The Kissat overlay retains cheap global clause counters on these rows;
    # Tactix deliberately writes zero because it did not take a snapshot.
    a.write_text(HEADER + common + row("tactix", 1, "watch", irred=0,
                                       payload="scan=1") +
                 row("tactix", 2, "terminate", payload="20"))
    b.write_text(HEADER + common.replace("tactix", "kissat") +
                 row("kissat", 1, "watch", irred=2, payload="scan=1") +
                 row("kissat", 2, "terminate", payload="20"))
    snapshotless = run(a, b, "--mode", "semantic", "--require-closed")
    assert snapshotless.returncode == 0, snapshotless

    # Cross-solver auxiliary diagnostics have different observation
    # boundaries and payload schemas.  They remain visible in exact mode but
    # cannot mask the first portable semantic edge.
    a.write_text(HEADER + common +
                 row("tactix", 1, "retier", payload="mode=focused") +
                 row("tactix", 2, "mode_clock", payload="ticks=12") +
                 row("tactix", 3, "analyzed", payload="vars=1") +
                 row("tactix", 4, "randec_draw", payload="raw=1") +
                 row("tactix", 5, "terminate", payload="20"))
    b.write_text(HEADER + common.replace("tactix", "kissat") +
                 row("kissat", 1, "reduce_tiers", payload="tier1=7") +
                 row("kissat", 2, "retier", payload="tier1=7") +
                 row("kissat", 3, "mode_clock", payload="ticks=9") +
                 row("kissat", 4, "randec_draw", payload="raw=2") +
                 row("kissat", 5, "terminate", payload="20"))
    auxiliary = run(a, b, "--mode", "semantic", "--require-closed")
    assert auxiliary.returncode == 0, auxiliary
    exact_auxiliary = run(a, b, "--mode", "exact")
    assert exact_auxiliary.returncode == 1, exact_auxiliary

    # Analysis-end is snapshot-bearing on both sides and must retain the
    # ranking/state localization contract.
    a.write_text(HEADER + row("tactix", 0, "analysis_end", irred=1,
                              top32="1,2,3"))
    b.write_text(HEADER + row("kissat", 0, "analysis_end", irred=2,
                              top32="2,1,3"))
    analyzed = run(a, b, "--mode", "semantic")
    assert analyzed.returncode == 1, analyzed
    # Formula storage is intentionally representation-specific at this edge;
    # the portable contract is the search-visible ranking snapshot.
    assert "top32: 1,2,3 -> 2,1,3" in analyzed.stdout

print("test_compare_event_trace OK")

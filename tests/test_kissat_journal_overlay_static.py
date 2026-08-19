#!/usr/bin/env python3
"""Keep static diagnostic-overlay coverage invariants."""

import pathlib
import re


root = pathlib.Path(__file__).resolve().parents[1]
prepare = (root / "tools/solvers/variants/kissat-event-trace/prepare.py").read_text()
journal = (root / "tools/solvers/variants/kissat-event-trace/overlay/rulejournal.c").read_text()
eventtrace = (root / "tools/solvers/variants/kissat-event-trace/overlay/eventtrace.c").read_text()

# These are Kissat's scheduled semantic families in search, elimination and the
# probe pipeline.  Search-guidance-only switch/rephase/reorder operations are
# control events, not proof rules.  Early/late variants normalize to one family.
required = {
    "lucky", "propagate", "conflict-analysis", "decide", "restart",
    "reduce", "eliminate", "congruence", "substitute", "backbone",
    "vivify", "sweep", "transitive", "factor",
}
recorded = set(re.findall(
    r'kissat_rule_journal_rule_attempt \(\\?\n?'
    r'(?:[^\n]*\n){0,2}?[^\n]*solver, [f]?"([a-z_-]+)', prepare))
recorded |= set(re.findall(r'TRACE_RULE \([^\n]+, "([a-z_-]+)"\)', prepare))
recorded |= set(re.findall(r'\("kissat_[^\"]+", "([a-z_-]+)"\)', prepare))
recorded |= {name.split("_")[0] for name in recorded}

missing = sorted(required - recorded)
assert not missing, f"Kissat proof-rule journal coverage missing: {missing}; got {sorted(recorded)}"
assert "kissat_rule_journal_sweep_core" in prepare
assert "scheduled=1;consistent-before=1;call-returned=1" in journal
assert "field '%s'" in journal, "replay must name the exact differing field"
assert eventtrace.count('strcmp (event, "randec_state")') >= 2, (
    "randec_state must survive both SEARCH_ONLY and CONTROL_ONLY filters")
assert 'solver->statistics.conflicts < stop' in eventtrace, (
    "stop-mode must suppress expensive snapshots before its endpoint")
assert re.search(
    r'if \(snapshot && stop != UINT64_MAX &&\s*'
    r'solver->statistics.conflicts >= stop\)\s*emit_heap_scores \(solver\);',
    eventtrace), "stop-mode must emit one endpoint heap digest"
assert re.search(
    r'!solver->stable \|\|\s*stop_after_conflict \(\) != UINT64_MAX\)',
    eventtrace), "ordinary heap snapshots must be suppressed in stop-mode"

print("test_kissat_journal_overlay_static OK")

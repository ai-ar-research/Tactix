#!/usr/bin/env python3
"""Keep static diagnostic-overlay coverage invariants."""

import pathlib
import re


root = pathlib.Path(__file__).resolve().parents[1]
prepare = (root / "tools/solvers/variants/kissat-event-trace/prepare.py").read_text()
journal = (root / "tools/solvers/variants/kissat-event-trace/overlay/rulejournal.c").read_text()
eventtrace = (root / "tools/solvers/variants/kissat-event-trace/overlay/eventtrace.c").read_text()
tactix_trace = (root / "src/policies/event_trace.cpp").read_text()
tactix_analyze = (root / "src/rules/analyze/analyze.cpp").read_text()

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
    r'if \(stop_here && solver->stable\)\s*emit_heap_scores \(solver\);',
    eventtrace), "stable analysis-end must emit one endpoint heap digest"
assert 'if (stop != UINT64_MAX) {' in eventtrace, (
    "stop-mode must gate intermediate heap snapshots")
assert 'if (stop_here) {\n    fflush (trace_file);\n    raise (SIGTERM);' in eventtrace, (
    "focused analysis-end must stop directly")
assert 'trace_stop_pending' not in eventtrace
assert 'TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT' in tactix_trace
assert 'if (stop_here && k.h && k.h->stable)\n    emit_heap_scores(k);' in tactix_trace
assert 'if (stop_after_conflict_ != UINT64_MAX) {' in tactix_trace, (
    "Tactix stop-mode must gate intermediate full-heap hashes")
assert tactix_trace.count('std::raise(SIGTERM);') == 1, (
    "Tactix stop-mode must terminate directly from analysis-end")
assert 'stop_pending_' not in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_EACH_BUMP' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_EACH_BUMP' in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_FROM_DECISION' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_FROM_DECISION' in tactix_trace
assert 'solver->statistics.decisions < trace_heap_from_decision' in eventtrace
assert 'k.stats.decisions >= heap_from_decision_' in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_POP_DECISION' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_POP_DECISION' in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_POP_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_POP_ONLY' in tactix_trace
assert 'kissat_event_trace_heap_pop (solver, "move"' in prepare
assert 'event_trace_heap_pop(k, "move"' in tactix_analyze or (
    'event_trace_heap_pop(k, "move"' in
    (root / "src/policies/heuristics.cpp").read_text())
assert 'KISSAT_EVENT_TRACE_BINARY_ASSIGNMENT' in eventtrace
assert 'TACTIX_EVENT_TRACE_BINARY_ASSIGNMENT' in tactix_trace
assert 'SAT_EVENT_TRACE_WATCH_TARGETS' in eventtrace
assert 'SAT_EVENT_TRACE_WATCH_TARGETS' in tactix_trace
assert 'SAT_EVENT_TRACE_BINARY_ADD' in eventtrace
assert 'SAT_EVENT_TRACE_BINARY_ADD' in tactix_trace
assert 'SAT_EVENT_TRACE_SWEEP_REWRITE' in eventtrace
assert 'SAT_EVENT_TRACE_SWEEP_REWRITE' in tactix_trace
assert 'strcmp (event, "sweep_equivalence")' in eventtrace
assert 'std::strcmp(event, "sweep_equivalence")' in tactix_trace
assert 'strcmp (event, "sweep_seed")' in eventtrace
assert 'std::strcmp(event, "sweep_seed")' in tactix_trace
assert 'strcmp (event, "sweep_environment")' in eventtrace
assert 'std::strcmp(event, "sweep_environment")' in tactix_trace
assert 'strcmp (event, "sweep_environment_clause")' in eventtrace
assert 'std::strcmp(event, "sweep_environment_clause")' in tactix_trace
assert 'kissat_event_trace_binary_add (solver, first, second);' in prepare
assert 'event_trace_binary_add(k, a, b);' in (root / "src/kernel/kernel.cpp").read_text()
assert 'kissat_event_trace_sweep_rewrite_binary (\\n' in prepare
assert 'event_trace_sweep_rewrite_binary(\n' in (
    root / "src/rules/substitute/substitute.cpp").read_text()
assert 'kissat_event_trace (solver, "sweep_equivalence", payload, false);' in prepare
assert 'kissat_event_trace (solver, "sweep_seed", payload, false);' in prepare
assert 'kissat_event_trace (solver, "sweep_environment", payload, false);' in prepare
assert 'kissat_event_trace_sweep_environment_clause (\\n' in prepare
assert 'kissat_event_trace (solver, "sweep_environment_clause", payload, false);' in eventtrace
assert 'binary_count=%zu|binary_hash=%016' in eventtrace
assert 'binary_count=%zu|binary_hash=%016llx' in tactix_trace
assert 'kissat_event_trace_binary_assignment (solver, lit, immediate_reason,' in prepare
assert 'event_trace_binary_assignment(k, lit, other_false' in (
    root / "src/kernel/kernel.hpp").read_text()
assert 'kissat_event_trace_heap_before_bumps (solver);' in prepare
assert 'void kissat_event_trace_heap_before_bumps (kissat *solver)' in eventtrace
assert 'event_trace_heap_scores(k);\n    for (const Var v : k.bump_order)' in tactix_analyze

print("test_kissat_journal_overlay_static OK")

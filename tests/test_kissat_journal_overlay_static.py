#!/usr/bin/env python3
"""Keep static diagnostic-overlay coverage invariants."""

import pathlib
import re


root = pathlib.Path(__file__).resolve().parents[1]
prepare = (root / "tools/solvers/variants/kissat-event-trace/prepare.py").read_text()
journal = (root / "tools/solvers/variants/kissat-event-trace/overlay/rulejournal.c").read_text()
eventtrace = (root / "tools/solvers/variants/kissat-event-trace/overlay/eventtrace.c").read_text()
tactix_trace = (root / "src/diagnostics/event_trace.cpp").read_text()
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
assert re.search(
    r'if \(!stop_here && trace_detail_window &&\s*'
    r'\(solver->statistics.conflicts < trace_detail_from \|\|\s*'
    r'solver->statistics.conflicts > trace_detail_to\)\)\s*return;',
    eventtrace), "endpoint stop must survive a skipped detail-window conflict"
assert 'if (stop != UINT64_MAX) {' in eventtrace, (
    "stop-mode must gate intermediate heap snapshots")
assert 'if (stop_here) {\n    fflush (trace_file);\n    raise (SIGTERM);' in eventtrace, (
    "focused analysis-end must stop directly")
assert 'trace_stop_pending' not in eventtrace
assert 'TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT' in tactix_trace
assert 'TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS' in tactix_trace
assert 'KISSAT_EVENT_TRACE_KEEP_SNAPSHOTS' in eventtrace
assert 'TACTIX_EVENT_TRACE_FORMULA_ONLY' in tactix_trace
assert 'KISSAT_EVENT_TRACE_FORMULA_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS' in tactix_trace
assert 'KISSAT_EVENT_TRACE_FORMULA_THRESHOLDS' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_DUMP' in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_DUMP' in eventtrace
assert 'if (formula_only_ || conflict_only_)' in tactix_trace
assert 'if (trace_formula_only)\n    return false;' in eventtrace
assert 'std::strcmp(event, "analysis_end") &&\n      std::strcmp(event, "formula_checkpoint")' in tactix_trace
assert 'strcmp (event, "analysis_end") &&\n      strcmp (event, "formula_checkpoint")' in eventtrace
for event in ('trace_begin', 'formula_checkpoint', 'terminate'):
    assert f'std::strcmp(event, "{event}")' in tactix_trace
    assert f'strcmp (event, "{event}")' in eventtrace
assert 'const bool snapshot = !trace_trajectory_only && analysis_end;' in eventtrace, (
    "trajectory-only analysis_end must not scan and rank the full solver state")
assert 'const bool stop_here = analysis_end && stop != UINT64_MAX' in eventtrace, (
    "trajectory-only analysis_end must retain the stop contract")
assert '!strcmp (event, "randec_state") || !strcmp (event, "phase_pick")' in eventtrace
assert 'strcmp (event, "phase_pick")' in eventtrace
assert 'strcmp (event, "phase_save")' in eventtrace
assert 'kissat_event_trace_phase_pick (solver, idx, value);' in prepare
assert 'kissat_event_trace_phase_save (solver, lit);' in prepare
assert 'SAT_EVENT_TRACE_PHASE_VAR' in eventtrace
assert 'SAT_EVENT_TRACE_PHASE_VAR' in tactix_trace
assert 'append_sorted_clause (solver, candidate, &p, &left);' in eventtrace
assert 'written = snprintf (p, left, "|pose=");' in eventtrace
assert 'for (all_literals_in_clause (lit, candidate))' in eventtrace
assert 'annotate_clause_set (solver, payload, sizeof payload);' in eventtrace
assert 'used=%u,priority=%u,pose=' in eventtrace
assert 'c->used, c->vivify' in eventtrace
assert 'SAT_EVENT_TRACE_VIVIFY_LITERALS' in eventtrace
assert 'SAT_EVENT_TRACE_VIVIFY_CLAUSES' in eventtrace
assert '"vivify_occurrence_clause"' in eventtrace
assert 'kissat_event_trace_vivify_counts (solver, vivifier->counts);' in prepare
assert 'strncmp (event, "vivify_", 7)' in eventtrace
assert 'KISSAT_CONGRUENCE_BINARY_TRACE' in eventtrace
assert 'kissat_congruence_binary_trace_begin' in prepare
assert 'kissat_congruence_sparse_binary_trace (solver, "sparse-entry")' in prepare
assert 'kissat_congruence_binary_trace_end (solver, "exit")' in prepare
assert 'KISSAT_CONGRUENCE_BINARY_TRACE_STOP' in eventtrace
assert 'solver, "post-binary-extraction", trace_binary_ordinal++' in prepare
assert 'solver, "exit", trace_binary_ordinal++' in prepare
assert 'TACTIX_CONGRUENCE_BINARY_TRACE' in (
    root / "src/rules/congruence/congruence.cpp").read_text()
assert 'KISSAT_CONGRUENCE_CANDIDATE_TRACE' in eventtrace
assert 'kissat_congruence_candidate_trace_begin (solver);' in prepare
assert 'candidate_duplicate ? "entry-duplicate" : "extract"' in prepare
assert 'kissat_congruence_candidate_trace_end (solver, true, extracted);' in prepare
assert 'TACTIX_CONGRUENCE_CANDIDATE_TRACE' in (
    root / "src/rules/congruence/congruence.cpp").read_text()
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
assert 'kissat_event_trace_heap_insert (solver, heap, idx' in prepare
assert 'event_trace_heap_insert(k, v, from' in (
    root / "src/policies/heuristics.cpp").read_text()
assert 'KISSAT_EVENT_TRACE_HEAP_REMOVE_DECISION' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION' in tactix_trace
assert 'kissat_event_trace_heap_remove (solver, heap, idx, last' in prepare
assert 'event_trace_heap_remove(k, v, last' in (
    root / "src/policies/heuristics.cpp").read_text()
assert 'KISSAT_EVENT_TRACE_BINARY_ASSIGNMENT' in eventtrace
assert 'TACTIX_EVENT_TRACE_BINARY_ASSIGNMENT' in tactix_trace
assert 'SAT_EVENT_TRACE_SUBSTITUTE_ALL' in eventtrace
assert 'SAT_EVENT_TRACE_SUBSTITUTE_ALL' in tactix_trace
assert 'KISSAT_EVENT_TRACE_COMPACT_PROBE' in eventtrace
assert 'KISSAT_BINARY_LIFECYCLE_TRACE' in eventtrace
assert 'TACTIX_BINARY_LIFECYCLE_TRACE' in tactix_trace
assert 'kissat_event_trace_binary_delete (solver, a, b);' in prepare
assert 'KISSAT_TRANSITIVE_TRACE' in eventtrace
assert 'TACTIX_TRANSITIVE_TRACE' in (
    root / "src/rules/transitive_reduce/transitive_reduce.cpp").read_text()
assert 'kissat_transitive_trace_source_begin' in prepare
assert 'kissat_transitive_trace_candidate' in prepare
assert 'SAT_EVENT_TRACE_WATCH_TARGETS' in eventtrace
assert 'SAT_EVENT_TRACE_WATCH_TARGETS' in tactix_trace
assert 'SAT_EVENT_TRACE_BINARY_ADD' in eventtrace
assert 'SAT_EVENT_TRACE_BINARY_ADD' in tactix_trace
assert 'SAT_EVENT_TRACE_SWEEP_REWRITE' in eventtrace
assert 'SAT_EVENT_TRACE_SWEEP_REWRITE' in tactix_trace
assert 'strcmp (event, "sweep_equivalence")' in eventtrace
assert 'std::strcmp(event, "sweep_equivalence")' in tactix_trace
assert 'KISSAT_EVENT_TRACE_SWEEP_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_SWEEP_ONLY' in tactix_trace
assert 'kissat_event_trace (solver, "sweep_effort", payload, false);' in prepare
assert 'trace_ticks_after - trace_ticks_before' in prepare
assert '"|before=" + std::to_string(ticks_before)' in tactix_trace
assert 'strcmp (event, "sweep_seed")' in eventtrace
assert 'std::strcmp(event, "sweep_seed")' in tactix_trace
assert 'strcmp (event, "sweep_environment")' in eventtrace
assert 'std::strcmp(event, "sweep_environment")' in tactix_trace
assert 'strcmp (event, "sweep_environment_clause")' in eventtrace
assert 'std::strcmp(event, "sweep_environment_clause")' in tactix_trace
assert 'strcmp (event, "sweep_query")' in eventtrace
assert 'std::strcmp(event, "sweep_query")' in tactix_trace
assert 'strcmp (event, "sweep_query_model")' in eventtrace
assert 'std::strcmp(event, "sweep_query_model")' in tactix_trace
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
assert 'kissat_event_trace_sweep_environment_var (\\n' in prepare
assert 'kissat_event_trace (solver, "sweep_environment_var", payload, false);' in eventtrace
assert 'kissat_event_trace_sweep_binary_attempt (\\n' in prepare
assert 'kissat_event_trace (solver, "sweep_binary_attempt", payload, false);' in eventtrace
assert 'kissat_event_trace_sweep_query (\\n' in prepare
assert 'kissat_event_trace (solver, "sweep_query", payload, false);' in eventtrace
assert 'kissat_event_trace_sweep_query_model (\\n' in prepare
assert 'kissat_event_trace (solver, "sweep_query_model", payload, false);' in eventtrace
assert 'binary_count=%zu|binary_hash=%016' in eventtrace
assert 'binary_count=%zu|binary_hash=%016llx' in tactix_trace
assert 'SAT_EVENT_TRACE_BINARY_SET_PATH' in eventtrace
assert 'SAT_EVENT_TRACE_BINARY_SET_MATCH' in eventtrace
assert 'SAT_EVENT_TRACE_BINARY_SET_CONFLICT' in eventtrace
assert '# kissat-binary-set-snapshot\\t1' in eventtrace
assert 'emit_binary_set_snapshot (solver, event, payload);' in eventtrace
assert 'kissat_event_trace_binary_assignment (solver, lit, immediate_reason,' in prepare
assert 'event_trace_binary_assignment(k, lit, other_false' in (
    root / "src/kernel/kernel.hpp").read_text()
assert 'kissat_event_trace_heap_before_bumps (solver);' in prepare
assert 'void kissat_event_trace_heap_before_bumps (kissat *solver)' in eventtrace
assert 'event_trace_heap_scores(k);\n    for (const Var v : k.bump_order)' in tactix_analyze

print("test_kissat_journal_overlay_static OK")

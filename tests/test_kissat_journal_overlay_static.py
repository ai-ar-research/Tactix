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
tactix_factor = (root / "src/rules/factor/factor.cpp").read_text()

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
    r'if \(stop_here && solver->stable && !trace_heap_vars_only\)\s*'
    r'emit_heap_scores \(solver\);',
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
assert 'KISSAT_MACRO_ACTIVE_SET' in eventtrace
assert 'TACTIX_MACRO_ACTIVE_SET' in tactix_trace
assert 'SAT_MACRO_ACTIVE_SET_EVENT' in eventtrace
assert 'SAT_MACRO_ACTIVE_SET_EVENT' in tactix_trace
assert 'KISSAT_EVENT_TRACE_WATCH_ONLY_STRICT' in eventtrace
assert ('trace_watch_only_strict && strcmp (event, "watch")' in eventtrace)
assert 'TACTIX_EVENT_TRACE_WATCH_ONLY_STRICT' in tactix_trace
assert ('watch_only_strict_ && std::strcmp(event, "watch")' in tactix_trace)
assert 'KISSAT_EVENT_TRACE_LEARNED_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_LEARNED_ONLY' in tactix_trace
assert 'KISSAT_EVENT_TRACE_BVE_HEAP_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_BVE_HEAP_ONLY' in tactix_trace
assert 'KISSAT_EVENT_TRACE_BVE_POP_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_BVE_POP_ONLY' in tactix_trace
assert 'kissat_event_trace_factor_fresh (' in eventtrace
assert 'kissat_event_trace_factor_fresh (\\n' in prepare
assert 'event_trace_factor_fresh(k, seed, added_fresh' in (
    root / "src/rules/factor/factor.cpp").read_text()
assert 'SAT_EVENT_TRACE_FACTOR_FRESH_ONLY' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_FRESH_CONFLICT' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_FRESH_CONFLICT' in tactix_trace
assert ('trace_heap_vars_only ||\n      trace_factor_fresh_only' in eventtrace)
assert ('trace_factor_fresh_only && strcmp (event, "trace_begin")' in
        eventtrace)
assert ('strcmp (event, "factor_fresh") && !exit_after_match' in eventtrace)
assert ('factor_fresh_only_ && std::strcmp(event, "trace_begin")' in
        tactix_trace)
assert ('std::strcmp(event, "factor_fresh") && !exit_after_match' in
        tactix_trace)
assert 'if (!want_detail(k) && !exit_after_match)' in tactix_trace
assert 'factor_fresh_only_' in tactix_trace
assert 'SAT_EVENT_TRACE_FACTOR_UPDATE_ONLY' in eventtrace
assert 'kissat_event_trace_factor_update (' in eventtrace
assert 'factoring->trace_update_ordinal++' in prepare
assert 'trace_factor_update_only && strcmp (event, "trace_begin")' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL' in eventtrace
assert tactix_trace.count('SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL') >= 2
assert 'SAT_EVENT_TRACE_FACTOR_HEAP_SET_PATH' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_HEAP_SET_PATH' in (
    root / "src/rules/factor/factor.cpp").read_text()
assert 'first, "popped"' in prepare
assert '"updated"' in prepare
assert 'factor_update_only_' in tactix_trace
assert 'SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY' in eventtrace
assert ('trace_factor_literal_only && strcmp (event, "trace_begin")' in
        eventtrace)
assert ('factor_literal_only_ && std::strcmp(event, "trace_begin")' in
        tactix_trace)
assert 'strcmp (event, "binary_add")' in eventtrace
assert 'std::strcmp(event, "binary_add")' in tactix_trace
assert ('trace_bve_heap_only && strcmp (event, "trace_begin")' in eventtrace)
assert ('bve_heap_only_ && std::strcmp(event, "trace_begin")' in tactix_trace)
assert 'strcmp (event, "bve_schedule")' in eventtrace
assert 'std::strcmp(event, "bve_schedule")' in tactix_trace
assert 'kissat_event_trace_bve_schedule (solver, &solver->schedule);' in prepare
assert 'event_trace_bve_schedule(k, stack_.data()' in (
    root / "src/strategy/bve_schedule.hpp").read_text()
assert 'insertion_hash=' in eventtrace and 'topology_hash=' in eventtrace
assert 'score_xor=' in eventtrace and 'score_sum=' in eventtrace
assert 'SAT_EVENT_TRACE_BVE_POP_FROM' in eventtrace
assert 'SAT_BVE_SCHEDULE_CONFLICT' in eventtrace
assert 'SAT_BVE_SCHEDULE_CONFLICT' in tactix_trace
assert 'SAT_BVE_SCHEDULE_ORDINAL' in eventtrace
assert 'SAT_BVE_SCHEDULE_ORDINAL' in tactix_trace
assert 'KISSAT_BVE_CANDIDATE_TRACE' in eventtrace
assert 'TACTIX_BVE_CANDIDATE_TRACE' in tactix_trace
assert 'SAT_BVE_CANDIDATE_EXIT_AFTER_END' in eventtrace
assert 'SAT_BVE_CANDIDATE_EXIT_AFTER_END' in tactix_trace
assert 'SAT_BVE_CANDIDATE_EXIT_STAGE' in eventtrace
assert 'SAT_BVE_CANDIDATE_EXIT_STAGE' in tactix_trace
assert 'solver, idx, "postflush"' in prepare
assert 'TACTIX_BVE_RETRY_TRACE' in tactix_trace
assert 'SAT_BVE_RETRY_EXIT_AFTER_MARK' in tactix_trace
assert 'SAT_BVE_SCHEDULE_SELECTED' in eventtrace
assert 'SAT_BVE_SCHEDULE_SELECTED' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_VALUES' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_VALUES' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_ORDINAL' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_ORDINAL' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_HASH' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_HASH' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_POSE' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_POSE' in tactix_trace
assert 'SAT_EVENT_TRACE_FORMULA_SET_META' in eventtrace
assert 'SAT_EVENT_TRACE_FORMULA_SET_META' in tactix_trace
assert 'KISSAT_VIVIFY_COUNTS_TRACE' in eventtrace
assert 'TACTIX_VIVIFY_COUNTS_TRACE' in tactix_trace
assert 'SAT_VIVIFY_COUNTS_TRACE_LITERALS' in eventtrace
assert 'SAT_VIVIFY_COUNTS_TRACE_LITERALS' in tactix_trace
assert 'SAT_VIVIFY_COUNTS_TRACE_CLAUSE_HASH' in eventtrace
assert 'SAT_VIVIFY_COUNTS_TRACE_CLAUSE_HASH' in tactix_trace
assert 'SAT_EVENT_TRACE_HEAP_SET_STOP' in eventtrace
assert 'KISSAT_INPROCESS_SCORE_TRACE' in eventtrace
assert 'SAT_INPROCESS_SCORE_CONFLICT_FROM' in eventtrace
assert 'SAT_INPROCESS_SCORE_CONFLICT_FROM' in tactix_trace
assert 'KISSAT_MACRO_WATCH_TRACE' in eventtrace
assert 'TACTIX_MACRO_WATCH_TRACE' in tactix_trace
assert 'SAT_MACRO_WATCH_EVENT' in eventtrace
assert 'SAT_MACRO_WATCH_EVENT' in tactix_trace
assert 'SAT_MACRO_WATCH_EXTERNAL' in eventtrace
assert 'SAT_MACRO_WATCH_EXTERNAL' in tactix_trace
assert 'SAT_MACRO_WATCH_ORDINAL' in eventtrace
assert 'SAT_MACRO_WATCH_ORDINAL' in tactix_trace
assert 'KISSAT_MACRO_WATCH_SERIES_TRACE' in eventtrace
assert 'TACTIX_MACRO_WATCH_SERIES_TRACE' in tactix_trace
assert 'KISSAT_FORWARD_CLAUSE_TRACE' in eventtrace
assert 'kissat_event_trace_forward_partner' in eventtrace
assert 'kissat_event_trace_forward_partner' in prepare
assert 'TACTIX_FORWARD_CLAUSE_TRACE' in (
    root / "src/rules/strengthen/strengthen.cpp").read_text()
assert 'SAT_EVENT_TRACE_BINARY_SET_EXTERNALS' in eventtrace
assert 'SAT_EVENT_TRACE_BINARY_SET_EXTERNALS' in tactix_trace
assert 'solver, "name=Strengthen|stage=begin")' in prepare
assert 'solver, "name=Strengthen|stage=end")' in prepare
assert 'SAT_EVENT_TRACE_SET_STOP' in eventtrace
assert 'SAT_EVENT_TRACE_SET_STOP' in tactix_trace
assert 'KISSAT_EVENT_TRACE_BOUNDARY_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_BOUNDARY_ONLY' in tactix_trace
assert 'KISSAT_INPROCESS_BOUNDARY_TRACE' in eventtrace
assert 'TACTIX_INPROCESS_BOUNDARY_TRACE' in tactix_trace
assert 'SAT_SEARCH_CHECKPOINT_INTERVAL' in eventtrace
assert 'SAT_SEARCH_CHECKPOINT_INTERVAL' in tactix_trace
assert 'SAT_SEARCH_CHECKPOINT_CONFLICT_FROM' in eventtrace
assert 'SAT_SEARCH_CHECKPOINT_CONFLICT_FROM' in tactix_trace
assert 'solver, "search_checkpoint_begin", true' in eventtrace
assert 'emit_standalone_macro_boundary(k, "search_checkpoint")' in tactix_trace
assert 'kissat_event_trace_eliminate_round_boundary' in eventtrace
assert 'solver, "strengthen", "begin")' in prepare
assert 'solver, "bve", "end")' in prepare
assert 'SAT_EVENT_TRACE_BVE_POP_FROM' in tactix_trace
assert 'bve_pop_checkpoint' in eventtrace and 'bve_pop_checkpoint' in tactix_trace
assert 'strcmp (event, "bve_candidate")' in eventtrace
assert 'std::strcmp(event, "bve_candidate")' in tactix_trace
assert 'TACTIX_EVENT_TRACE_FORMULA_ONLY' in tactix_trace
assert 'KISSAT_EVENT_TRACE_FORMULA_ONLY' in eventtrace
assert 'TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS' in tactix_trace
assert 'KISSAT_EVENT_TRACE_FORMULA_THRESHOLDS' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_DUMP' in tactix_trace
assert 'KISSAT_EVENT_TRACE_HEAP_DUMP' in eventtrace
assert 'if (formula_only_ || conflict_only_ || checkpoint_only_)' in tactix_trace, (
    "all summary-only trace modes must reject expensive ordinary detail payloads")
assert ('if (trace_formula_only || trace_learned_only || trace_heap_vars_only ||\n'
        '      trace_factor_fresh_only || trace_factor_update_only ||\n'
        '      trace_factor_literal_only || trace_clause_lifecycle_only)\n'
        '    return false;') in eventtrace
assert 'trace_clause_lifecycle_only && !snapshot' in eventtrace
assert 'clause_lifecycle_only_ && !snapshot' in tactix_trace
assert 'trace_clause_lifecycle_only && snapshot && trace_detail_window' in eventtrace
assert 'clause_lifecycle_only_ && snapshot && detail_window_' in tactix_trace
assert 'std::strcmp(event, "analysis_end") &&\n      std::strcmp(event, "formula_checkpoint")' in tactix_trace
assert 'strcmp (event, "analysis_end") &&\n      strcmp (event, "formula_checkpoint")' in eventtrace
for event in ('trace_begin', 'formula_checkpoint', 'terminate'):
    assert f'std::strcmp(event, "{event}")' in tactix_trace
    assert f'strcmp (event, "{event}")' in eventtrace
assert ('!trace_trajectory_only && !trace_clause_lifecycle_only && analysis_end' in
        eventtrace), (
    "trajectory/lifecycle-only analysis_end must not scan and rank full state")
assert 'trace_formula_only || trace_learned_only || trace_heap_vars_only ||' in eventtrace, (
    "learned-only must reject non-learned payloads at the helper gate")
assert 'trace_learned_only && strcmp (event, "learned")' in eventtrace
assert 'learned_only_ && std::strcmp(event, "learned")' in tactix_trace
assert ('if (learned_only_ || heap_vars_only_ || factor_fresh_only_ ||\n'
        '      factor_update_only_ || factor_literal_only_ || clause_lifecycle_only_)\n'
        '    return false;') in tactix_trace, (
    "Tactix learned-only must reject non-learned payloads at the helper gate")
assert 'trace_learned_only && !stop_here' in eventtrace, (
    "learned-only must retain analysis-end solely for the stop contract")
assert 'analysis_end && !learned_only_' in tactix_trace
assert 'analysis_end && !trace_learned_only' in eventtrace
assert 'const bool stop_here = analysis_end && stop != UINT64_MAX' in eventtrace, (
    "trajectory-only analysis_end must retain the stop contract")
assert '!strcmp (event, "randec_state") || !strcmp (event, "phase_pick")' in eventtrace
assert 'strcmp (event, "phase_pick")' in eventtrace
assert 'strcmp (event, "phase_save")' in eventtrace
assert 'kissat_event_trace_phase_pick (solver, idx, value);' in prepare
assert 'kissat_event_trace_phase_save (solver, lit);' in prepare
assert '!trace_phase_only || !detail_enabled (solver)' in eventtrace
assert '!phase_only_ || !want_detail(k)' in tactix_trace
assert 'SAT_INPROCESS_SCORE_PHASES' in eventtrace
assert 'SAT_INPROCESS_SCORE_PHASES' in tactix_trace
assert 'target_assigned\\tbest_assigned' in eventtrace
assert 'target_assigned\\tbest_assigned' in tactix_trace
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
assert ('if (stop_here && k.h && k.h->stable && !heap_vars_only_)\n'
        '    emit_heap_scores(k);') in tactix_trace
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
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_CONFLICT' in eventtrace
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_FROM_DECISION' in eventtrace
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_TO_DECISION' in eventtrace
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_CONFLICT' in tactix_trace
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_FROM_DECISION' in tactix_trace
assert 'SAT_EVENT_TRACE_ASSIGNMENT_HASH_TO_DECISION' in tactix_trace
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
assert 'SAT_EVENT_TRACE_SUBSTITUTE_ONLY' in eventtrace
assert 'SAT_EVENT_TRACE_SUBSTITUTE_ONLY' in tactix_trace
assert ('trace_substitute_only && strcmp (event, "trace_begin")' in
        eventtrace)
assert ('substitute_only_ && std::strcmp(event, "trace_begin")' in
        tactix_trace)
assert 'kissat_event_trace_late (solver, NAME "_end", "-", true);' in prepare
assert 'KISSAT_EVENT_TRACE_COMPACT_PROBE' in eventtrace
assert 'KISSAT_BINARY_LIFECYCLE_TRACE' in eventtrace
assert 'TACTIX_BINARY_LIFECYCLE_TRACE' in tactix_trace
assert 'kissat_event_trace_forward_binary (' in eventtrace
assert 'kissat_event_trace_forward_binary (solver, first, second, c);' in prepare
assert '"strengthened-binary"' in prepare
assert '"strengthened-large"' in prepare
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
assert 'kissat_event_trace_sweep_equivalence (solver, lit, other);' in prepare
assert 'SAT_EVENT_TRACE_SWEEP_COMMIT' in eventtrace
assert 'kissat_event_trace_substitute_binary_collapse (' in prepare
assert 'substitute-large-collapse-source' in eventtrace
assert 'kissat_event_trace_binary_add_site' in prepare
assert '"factor-divider"' in prepare
assert '"congruence"' in prepare
assert '"sweep"' in prepare
assert '"substitute"' in prepare
assert '"vivify"' in prepare
assert '"strengthen"' in prepare
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
assert 'SAT_EVENT_TRACE_HEAP_SET_PATH' in eventtrace
assert 'SAT_EVENT_TRACE_HEAP_SET_CONFLICT' in eventtrace
assert 'SAT_EVENT_TRACE_HEAP_SET_DECISION' in eventtrace
assert '# kissat-heap-set\\t1' in eventtrace
assert 'emit_heap_set_snapshot (solver);' in eventtrace
assert 'KISSAT_EVENT_TRACE_HEAP_VARS_ONLY' in eventtrace
assert 'SAT_EVENT_TRACE_HEAP_VARS' in eventtrace
assert 'selected_heap_variable (kissat *solver, unsigned idx)' in eventtrace
assert 'trace_heap_vars_only ? !selected' in eventtrace
assert 'TACTIX_EVENT_TRACE_HEAP_VARS_ONLY' in tactix_trace
assert 'SAT_EVENT_TRACE_HEAP_VARS' in tactix_trace
assert 'heap_vars_only_ ? !selected' in tactix_trace
assert eventtrace.count(
    'trace_heap_vars_only && trace_detail_window &&') >= 3
assert eventtrace.count(
    'solver->statistics.conflicts < trace_detail_from ||') >= 4
assert tactix_trace.count(
    'heap_vars_only_ && detail_window_ &&') >= 4
assert 'kissat_event_trace_root_assign (solver, lit, binary, reason);' in prepare
assert 'void kissat_event_trace_root_assign (kissat *solver, unsigned lit,' in eventtrace
assert 'strcmp (event, "root_assign")' in eventtrace
assert 'std::strcmp(event, "root_assign")' in tactix_trace
assert 'active_trace->root_assign(k, lit);' in tactix_trace
assert 'strcmp (event, "substitute_map")' in eventtrace
assert 'std::strcmp(event, "substitute_map")' in tactix_trace
assert 'selected_heap_variable (solver, IDX (lit))' in eventtrace
assert 'heap_var_selected(k, var_of(lit))' in tactix_trace
assert 'emit_binary_set_snapshot(k, "slot_boundary", payload.c_str());' in tactix_trace
assert 'emit_formula_set_snapshot(k, "slot_boundary", payload.c_str());' in tactix_trace
assert 'kissat_event_trace (solver, "factor_admitted", "-", true);' in prepare
assert 'kissat_event_trace (solver, "factor_applied", "-", true);' in prepare
assert 'trace_emitted >= trace_limit && !exit_after_match' in eventtrace
assert 'emitted_ >= limit_ && !exit_after_match' in tactix_trace
assert 'SAT_EVENT_TRACE_TARGET_ONLY' in eventtrace
assert 'SAT_EVENT_TRACE_TARGET_ONLY' in tactix_trace
assert 'SAT_EVENT_TRACE_EXIT_CONFLICT' in eventtrace
assert 'SAT_EVENT_TRACE_EXIT_CONFLICT' in tactix_trace
assert 'KISSAT_EVENT_TRACE_LATE_PATH' in eventtrace
assert 'TACTIX_EVENT_TRACE_LATE_PATH' in tactix_trace
assert 'kissat_event_trace_late (solver, "factor_begin", "-", true);' in prepare
assert "'  kissat_event_trace_factor_heap (solver, 0, \"initial\",\\n'" in prepare
assert "'                                  &factoring.schedule);\\n'" in prepare
assert "'              reduction, *ticks - trace_tick_start, trace_pops,\\n'" in prepare
assert "'              trace_first_ticks, trace_next_ticks,\\n'" in prepare
assert "'              trace_match_ticks);\\n'" in prepare
assert 'SAT_EVENT_TRACE_FACTOR_METER' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_METER' in tactix_trace
assert 'SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS' in tactix_trace
assert 'KISSAT_FACTOR_MATCH_TRACE' in eventtrace
assert 'TACTIX_FACTOR_MATCH_TRACE' in tactix_factor
assert 'SAT_FACTOR_MATCH_PAIR' in eventtrace
assert 'SAT_FACTOR_MATCH_PAIR' in tactix_factor
assert 'kissat_event_trace_factor_match_selected (' in prepare
assert 'kissat_event_trace_factor_match_large (' in prepare
assert 'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE' in eventtrace
assert 'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE' in tactix_factor
assert 'SAT_EVENT_TRACE_FACTOR_LITERAL_CONFLICT' in eventtrace
assert tactix_trace.count('SAT_EVENT_TRACE_FACTOR_LITERAL_CONFLICT') == 2
assert "'                                    \"factor_pop\");\\n'" in prepare
assert re.search(
    r'if \(selected_ordinal &&\s*'
    r'std::strtoull\(selected_ordinal, nullptr, 10\) != k\.stats\.factorings\)\s*'
    r'return;', tactix_factor), (
    "selected Factor heap tracing must reject other ordinals before scanning")
assert 'kissat_event_trace_late (solver, NAME "_begin", "-", true);' in prepare
assert 'f\'        kissat_event_trace_late (solver, "{event}_begin", "-", true);\\n\'' in prepare
assert 'f\'        kissat_event_trace_late (solver, "{event}_end", "-", true);\\n\'' in prepare
assert 'SAT_RULE_JOURNAL_SKIP_DIGEST' in journal
assert 'SAT_RULE_JOURNAL_SKIP_DIGEST' in (
    root / "src/diagnostics/rule_journal.cpp").read_text()
assert '? &solver->statistics.factor_ticks' in prepare
assert 'journal_effort, *journal_meter, 0' in prepare
assert 'kissat_event_trace_binary_assignment (solver, lit, immediate_reason,' in prepare
assert 'event_trace_binary_assignment(k, lit, other_false' in (
    root / "src/kernel/kernel.hpp").read_text()
assert 'kissat_event_trace_heap_before_bumps (solver);' in prepare
assert 'void kissat_event_trace_heap_before_bumps (kissat *solver)' in eventtrace
assert 'event_trace_heap_scores(k);\n    for (const Var v : k.bump_order)' in tactix_analyze
assert 'TACTIX_DECISION_PREFIX_TRACE' in tactix_trace
assert 'note_decision_prefix(k, static_cast<int>(payload));' in tactix_trace
assert 'flush_decision_prefix(k);' in tactix_trace
assert 'KISSAT_DECISION_PREFIX_TRACE' in eventtrace
assert 'note_decision_prefix (solver, lit);' in eventtrace
assert 'flush_decision_prefix (solver);' in eventtrace
assert 'TACTIX_RANDEC_STATE_TRACE' in tactix_trace
assert 'KISSAT_RANDEC_STATE_TRACE' in eventtrace
assert 'randec_sequences, randec_rng' in (
    root / "src/policies/default_policy.cpp").read_text()

print("test_kissat_journal_overlay_static OK")

#!/usr/bin/env python3
"""Materialize the pinned Kissat tree with the semantic-trace overlay."""
import argparse
import pathlib
import shutil
import subprocess


PIN = "8af8e56f174b778aef3aa45af9f739b2a5f492c2"


def replace_once(path, old, new):
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected one overlay anchor, found "
                         f"{text.count(old)}")
    path.write_text(text.replace(old, new))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--dest", required=True)
    args = ap.parse_args()
    source = pathlib.Path(args.source).resolve()
    dest = pathlib.Path(args.dest).resolve()
    if dest.exists():
        raise SystemExit(f"refusing to overwrite existing variant: {dest}")
    commit = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != PIN:
        raise SystemExit(f"Kissat source is {commit}, expected {PIN}")
    shutil.copytree(source, dest, ignore=shutil.ignore_patterns("build", ".git"))
    overlay = pathlib.Path(__file__).resolve().parent / "overlay"
    shutil.copy2(overlay / "eventtrace.c", dest / "src" / "eventtrace.c")
    shutil.copy2(overlay / "eventtrace.h", dest / "src" / "eventtrace.h")
    shutil.copy2(overlay / "rulejournal.c", dest / "src" / "rulejournal.c")
    shutil.copy2(overlay / "rulejournal.h", dest / "src" / "rulejournal.h")

    # Attribute persistent per-polarity Factor-bit transitions to the exact
    # semantic producer.  The singular helper is inline, so a macro carries
    # the caller's function name; the plural helper forwards its own caller
    # rather than collapsing every source to kissat_mark_added_literals.
    inline_h = dest / "src" / "inline.h"
    replace_once(inline_h, '#include "logging.h"\n',
                 '#include "logging.h"\n#include "eventtrace.h"\n')
    replace_once(
        inline_h,
        'static inline void kissat_mark_added_literal (kissat *solver,\n'
        '                                              unsigned lit) {\n',
        'static inline void kissat_mark_added_literal_with_source (\n'
        '    kissat *solver, unsigned lit, const char *source) {\n')
    replace_once(
        inline_h,
        '    INC (literals_factor);\n'
        '  }\n'
        '}\n\n'
        'static inline void\n'
        'kissat_push_large_watch',
        '    INC (literals_factor);\n'
        '    kissat_event_trace_factor_mark (solver, lit, true, source);\n'
        '  }\n'
        '}\n\n'
        '#define kissat_mark_added_literal(SOLVER, LIT) \\\n'
        '  kissat_mark_added_literal_with_source ((SOLVER), (LIT), __func__)\n\n'
        'static inline void\n'
        'kissat_push_large_watch')
    flags_h = dest / "src" / "flags.h"
    replace_once(
        flags_h,
        'void kissat_mark_added_literals (struct kissat *, unsigned, unsigned *);\n',
        'void kissat_mark_added_literals_with_source (\n'
        '    struct kissat *, unsigned, unsigned *, const char *);\n'
        '#define kissat_mark_added_literals(SOLVER, SIZE, LITS) \\\n'
        '  kissat_mark_added_literals_with_source ( \\\n'
        '      (SOLVER), (SIZE), (LITS), __func__)\n')
    flags_c = dest / "src" / "flags.c"
    replace_once(
        flags_c,
        'void kissat_mark_added_literals (kissat *solver, unsigned size,\n'
        '                                 unsigned *lits) {\n'
        '  for (unsigned i = 0; i < size; i++)\n'
        '    kissat_mark_added_literal (solver, lits[i]);\n'
        '}\n',
        'void kissat_mark_added_literals_with_source (\n'
        '    kissat *solver, unsigned size, unsigned *lits,\n'
        '    const char *source) {\n'
        '  for (unsigned i = 0; i < size; i++)\n'
        '    kissat_mark_added_literal_with_source (solver, lits[i], source);\n'
        '}\n')

    # Definition-core parity needs Kitten's otherwise opaque deterministic
    # state.  These accessors exist only in the diagnostic materialization.
    kitten_h = dest / "src" / "kitten.h"
    replace_once(
        kitten_h,
        'void kitten_set_ticks_limit (kitten *, uint64_t);\n',
        'void kitten_set_ticks_limit (kitten *, uint64_t);\n'
        'uint64_t kitten_trace_generator (kitten *);\n'
        'uint64_t kitten_trace_initialized (kitten *);\n'
        'unsigned kitten_trace_variables (kitten *);\n')
    kitten_c = dest / "src" / "kitten.c"
    kitten_c.write_text(
        kitten_c.read_text() +
        '\nuint64_t kitten_trace_generator (kitten *kitten) {\n'
        '  return kitten->generator;\n'
        '}\n'
        'uint64_t kitten_trace_initialized (kitten *kitten) {\n'
        '  return kitten->initialized;\n'
        '}\n'
        'unsigned kitten_trace_variables (kitten *kitten) {\n'
        '  return kitten->lits / 2;\n'
        '}\n')

    definition = dest / "src" / "definition.c"
    replace_once(definition, '#include "definition.h"\n',
                 '#include "definition.h"\n#include "eventtrace.h"\n')
    replace_once(
        definition,
        'struct definition_extractor {\n'
        '  unsigned lit;\n'
        '  kissat *solver;\n'
        '  watches *watches[2];\n'
        '};\n',
        'struct definition_extractor {\n'
        '  unsigned lit;\n'
        '  kissat *solver;\n'
        '  watches *watches[2];\n'
        '};\n\n'
        'typedef struct definition_trace_core definition_trace_core;\n'
        'struct definition_trace_core {\n'
        '  uint64_t hash;\n'
        '  unsigned side0, size, core0, core1;\n'
        '};\n\n'
        'static uint64_t definition_trace_attempt;\n\n'
        'static void traverse_definition_trace_core (void *state, unsigned id) {\n'
        '  definition_trace_core *trace = state;\n'
        '  const unsigned normalized = id + 1;\n'
        '  trace->hash ^= normalized;\n'
        '  trace->hash *= UINT64_C (1099511628211);\n'
        '  trace->size++;\n'
        '  if (id < trace->side0) trace->core0++; else trace->core1++;\n'
        '}\n\n'
        'static void trace_definition_core (\n'
        '    kissat *solver, kitten *kitten, unsigned lit, uint64_t attempt,\n'
        '    unsigned core, const char *outcome, uint64_t ticks_entry,\n'
        '    uint64_t budget, unsigned side0, unsigned side1, bool traverse) {\n'
        '  definition_trace_core trace;\n'
        '  trace.hash = UINT64_C (1469598103934665603);\n'
        '  trace.side0 = side0, trace.size = trace.core0 = trace.core1 = 0;\n'
        '  if (traverse)\n'
        '    kitten_traverse_core_ids (kitten, &trace,\n'
        '                              traverse_definition_trace_core);\n'
        '  const uint64_t ticks = solver->statistics.kitten_ticks - ticks_entry;\n'
        '  int external = kissat_export_literal (solver, lit);\n'
        '  if (external < 0) external = -external;\n'
        '  const char *selected = getenv ("KISSAT_EVENT_TRACE_BVE_EXTERNAL");\n'
        '  if (selected && atoi (selected) != external) return;\n'
        '  char payload[512];\n'
        '  snprintf (payload, sizeof payload,\n'
        '            "external=%d|attempt=%llu|core=%u|outcome=%s|ticks=%llu|"\n'
        '            "budget=%llu|generator=%016llx|variables=%u|side0=%u|"\n'
        '            "side1=%u|core_size=%u|core0=%u|core1=%u|core_hash=%016llx",\n'
        '            external, (unsigned long long) attempt, core, outcome,\n'
        '            (unsigned long long) ticks, (unsigned long long) budget,\n'
        '            (unsigned long long) kitten_trace_generator (kitten),\n'
        '            kitten_trace_variables (kitten), side0, side1, trace.size,\n'
        '            trace.core0, trace.core1, (unsigned long long) trace.hash);\n'
        '  kissat_event_trace (solver, "bve_definition", payload, false);\n'
        '}\n')
    replace_once(
        definition,
        '  START (definition);\n'
        '  struct kitten *kitten = solver->kitten;\n',
        '  START (definition);\n'
        '  const uint64_t trace_ticks_entry = solver->statistics.kitten_ticks;\n'
        '  struct kitten *kitten = solver->kitten;\n')
    replace_once(
        definition,
        '  unsigned exported = 0;\n',
        '  unsigned exported = 0;\n'
        '  unsigned trace_occs[2] = {0, 0};\n')
    replace_once(
        definition,
        '#if !defined(QUIET) || !defined(NDEBUG)\n'
        '      occs[sign]++;\n'
        '#endif\n'
        '      exported++;\n',
        '#if !defined(QUIET) || !defined(NDEBUG)\n'
        '      occs[sign]++;\n'
        '#endif\n'
        '      trace_occs[sign]++;\n'
        '      exported++;\n')
    replace_once(
        definition,
        '  INC (definitions_checked);\n'
        '  const size_t limit = GET_OPTION (definitionticks);\n',
        '  const uint64_t trace_attempt = ++definition_trace_attempt;\n'
        '  trace_definition_core (solver, kitten, lit, trace_attempt, 0,\n'
        '                         "imported", trace_ticks_entry,\n'
        '                         GET_OPTION (definitionticks), trace_occs[0],\n'
        '                         trace_occs[1], false);\n'
        '  INC (definitions_checked);\n'
        '  const size_t limit = GET_OPTION (definitionticks);\n')
    replace_once(
        definition,
        '    LOG ("1st sub-solver core of size %u original clauses out of %u",\n'
        '         reduced, exported);\n',
        '    LOG ("1st sub-solver core of size %u original clauses out of %u",\n'
        '         reduced, exported);\n'
        '    trace_definition_core (solver, kitten, lit, trace_attempt, 1,\n'
        '                           "unsat", trace_ticks_entry, limit,\n'
        '                           trace_occs[0], trace_occs[1], true);\n')
    replace_once(
        definition,
        '      if (!tmp) {\n'
        '        LOG ("aborting core extraction");\n'
        '        goto ABORT;\n'
        '      }\n',
        '      if (!tmp) {\n'
        '        trace_definition_core (solver, kitten, lit, trace_attempt, i,\n'
        '                               "budget", trace_ticks_entry, 10 * limit,\n'
        '                               trace_occs[0], trace_occs[1], false);\n'
        '        LOG ("aborting core extraction");\n'
        '        goto ABORT;\n'
        '      }\n')
    replace_once(
        definition,
        '      LOG ("%s sub-solver core of size %u original clauses out of %u",\n'
        '           FORMAT_ORDINAL (i), reduced, exported);\n',
        '      LOG ("%s sub-solver core of size %u original clauses out of %u",\n'
        '           FORMAT_ORDINAL (i), reduced, exported);\n'
        '      trace_definition_core (solver, kitten, lit, trace_attempt, i,\n'
        '                             "unsat", trace_ticks_entry, 10 * limit,\n'
        '                             trace_occs[0], trace_occs[1], true);\n')
    replace_once(
        definition,
        '  } else {\n'
        '  ABORT:\n',
        '  } else {\n'
        '    trace_definition_core (solver, kitten, lit, trace_attempt, 1,\n'
        '                           status == 10 ? "sat" : "budget",\n'
        '                           trace_ticks_entry, limit,\n'
        '                           trace_occs[0], trace_occs[1], false);\n'
        '  ABORT:\n')

    dense = dest / "src" / "dense.c"
    replace_once(dense, '#include "dense.h"\n',
                 '#include "dense.h"\n#include "eventtrace.h"\n')
    replace_once(
        dense,
        'resume_watching_large_clauses_after_elimination (kissat *solver) {\n',
        'resume_watching_large_clauses_after_elimination (kissat *solver) {\n'
        '  kissat_event_trace_eliminate_cleanup (solver, "before");\n')
    replace_once(
        dense,
        '  LOG ("resumed watching %zu irredundant and %zu redundant large clauses",\n'
        '       resumed_watching_irredundant, resumed_watching_redundant);\n'
        '}\n',
        '  LOG ("resumed watching %zu irredundant and %zu redundant large clauses",\n'
        '       resumed_watching_irredundant, resumed_watching_redundant);\n'
        '  kissat_event_trace_eliminate_cleanup (solver, "after");\n'
        '}\n')
    replace_once(
        dense,
        '            if (lit < other) {\n'
        '              kissat_delete_binary (solver, lit, other);\n',
        '            if (lit < other) {\n'
        '              kissat_event_trace_binary_delete_site (\n'
        '                  solver, "dense-duplicate", lit, other);\n'
        '              kissat_delete_binary (solver, lit, other);\n')
    replace_once(
        dense,
        '          if (lit < other) {\n'
        '            kissat_delete_binary (solver, lit, other);\n'
        '            collected++;\n',
        '          if (lit < other) {\n'
        '            kissat_event_trace_binary_delete_site (\n'
        '                solver, "dense-satisfied", lit, other);\n'
        '            kissat_delete_binary (solver, lit, other);\n'
        '            collected++;\n')

    # Expose the two Congruence dense-to-sparse resumes as ordinary snapshot
    # events.  The shared SAT_EVENT_TRACE_CLAUSE_SET observer can then compare
    # a clause pose at each lifecycle boundary without changing solver state.
    congruence = dest / "src" / "congruence.c"
    replace_once(congruence, '#include "dense.h"\n',
                 '#include "dense.h"\n#include "eventtrace.h"\n')
    replace_once(
        congruence,
        '  kissat_enter_dense_mode (solver, &closure->binaries);\n',
        '  kissat_event_trace (solver, "congruence_dense_entry", "-", true);\n'
        '  kissat_congruence_sparse_binary_trace (solver, "sparse-entry");\n'
        '  kissat_enter_dense_mode (solver, &closure->binaries);\n'
        '  size_t trace_binary_ordinal = 0;\n'
        '  kissat_congruence_binary_trace_begin (\n'
        '      solver, "entry", SIZE_STACK (closure->binaries));\n'
        '  for (all_stack (litpair, pair, closure->binaries))\n'
        '    kissat_congruence_binary_trace_pair (\n'
        '        solver, "entry", trace_binary_ordinal++,\n'
        '        pair.lits[0], pair.lits[1]);\n')
    replace_once(
        congruence,
        '  extract_binaries (closure);\n'
        '  assert (!solver->inconsistent);\n',
        '  extract_binaries (closure);\n'
        '  size_t trace_binary_ordinal = 0;\n'
        '  kissat_congruence_binary_trace_begin (\n'
        '      solver, "post-binary-extraction",\n'
        '      SIZE_STACK (closure->binaries));\n'
        '  for (all_stack (litpair, pair, closure->binaries))\n'
        '    kissat_congruence_binary_trace_pair (\n'
        '        solver, "post-binary-extraction", trace_binary_ordinal++,\n'
        '        pair.lits[0], pair.lits[1]);\n'
        '  assert (!solver->inconsistent);\n')
    replace_once(
        congruence,
        '  const value *const values = solver->values;\n'
        '  for (all_clauses (d)) {\n',
        '  const value *const values = solver->values;\n'
        '  kissat_congruence_candidate_trace_begin (solver);\n'
        '  for (all_clauses (d)) {\n')
    replace_once(
        congruence,
        '    else\n'
        '      continue;\n'
        '    LOGCLS (d, "strengthening");\n'
        '    if (!find_binary (solver, binaries, offsetsize, l, k)) {\n',
        '    else {\n'
        '      kissat_congruence_candidate_trace (\n'
        '          solver, lits, INVALID_LIT, INVALID_LIT, "no-antecedent");\n'
        '      continue;\n'
        '    }\n'
        '    LOGCLS (d, "strengthening");\n'
        '    const bool candidate_duplicate =\n'
        '        find_binary (solver, binaries, offsetsize, l, k);\n'
        '    kissat_congruence_candidate_trace (\n'
        '        solver, lits, l, k,\n'
        '        candidate_duplicate ? "entry-duplicate" : "extract");\n'
        '    if (!candidate_duplicate) {\n')
    replace_once(
        congruence,
        '  kissat_verbose (solver, "extracted %zu binaries (plus %zu duplicated)",\n'
        '                  extracted, duplicated);\n'
        '  STOP (extractbinaries);\n',
        '  kissat_verbose (solver, "extracted %zu binaries (plus %zu duplicated)",\n'
        '                  extracted, duplicated);\n'
        '  kissat_congruence_candidate_trace_end (solver, true, extracted);\n'
        '  STOP (extractbinaries);\n')
    replace_once(
        congruence,
        '        LOGBINARY (lit, other, "removing duplicated");\n'
        '        kissat_delete_binary (solver, lit, other);\n',
        '        LOGBINARY (lit, other, "removing duplicated");\n'
        '        kissat_event_trace_binary_delete_site (\n'
        '            solver, "congruence-extracted-duplicate", lit, other);\n'
        '        kissat_delete_binary (solver, lit, other);\n')
    replace_once(
        congruence,
        '  kissat_resume_sparse_mode (solver, false, &closure->binaries);\n'
        '  RELEASE_STACK (closure->binaries);\n',
        '  size_t trace_binary_ordinal = 0;\n'
        '  kissat_congruence_binary_trace_begin (\n'
        '      solver, "exit", SIZE_STACK (closure->binaries));\n'
        '  for (all_stack (litpair, pair, closure->binaries))\n'
        '    kissat_congruence_binary_trace_pair (\n'
        '        solver, "exit", trace_binary_ordinal++,\n'
        '        pair.lits[0], pair.lits[1]);\n'
        '  kissat_resume_sparse_mode (solver, false, &closure->binaries);\n'
        '  kissat_event_trace (solver, "congruence_dense_first", "-", true);\n'
        '  kissat_congruence_binary_trace_end (solver, "exit");\n'
        '  RELEASE_STACK (closure->binaries);\n')
    replace_once(
        congruence,
        '  kissat_enter_dense_mode (solver, &binaries);\n',
        '  kissat_event_trace (solver, "congruence_dense_second_entry",\n'
        '                      "-", true);\n'
        '  kissat_enter_dense_mode (solver, &binaries);\n')
    replace_once(
        congruence,
        '  kissat_resume_sparse_mode (solver, false, &binaries);\n'
        '  RELEASE_STACK (candidates);\n',
        '  kissat_resume_sparse_mode (solver, false, &binaries);\n'
        '  kissat_event_trace (solver, "congruence_dense_second", "-", true);\n'
        '  RELEASE_STACK (candidates);\n')

    vivify = dest / "src" / "vivify.c"
    replace_once(vivify, '#include "terminate.h"\n',
                 '#include "terminate.h"\n#include "eventtrace.h"\n')
    replace_once(
        vivify,
        '  kissat_flush_large_watches (solver);\n\n'
        '  schedule_vivification_candidates (vivifier);\n',
        '  kissat_event_trace (solver, "vivify_dense_entry", "-", true);\n'
        '  kissat_flush_large_watches (solver);\n\n'
        '  schedule_vivification_candidates (vivifier);\n'
        '  kissat_event_trace_vivify_counts (solver, vivifier->counts);\n')
    replace_once(
        vivify,
        '  kissat_watch_large_clauses (solver);\n#ifndef QUIET\n',
        '  kissat_watch_large_clauses (solver);\n'
        '  kissat_event_trace (solver, "vivify_dense_resume", "-", true);\n'
        '#ifndef QUIET\n')
    replace_once(
        vivify,
        '  assert (!vivifier->vivified);\n'
        '  assert (!vivifier->tried);\n'
        '  while (!EMPTY_STACK (vivifier->schedule)) {\n',
        '  assert (!vivifier->vivified);\n'
        '  assert (!vivifier->tried);\n'
        '  unsigned trace_candidate_ordinal = 0;\n'
        '  while (!EMPTY_STACK (vivifier->schedule)) {\n')
    replace_once(
        vivify,
        '      break;\n'
        '    }\n'
        '    if (TERMINATED (vivify_terminated_1))\n',
        '      const reference trace_ref = TOP_STACK (vivifier->schedule);\n'
        '      clause *trace_candidate =\n'
        '          kissat_dereference_clause (solver, trace_ref);\n'
        '      kissat_event_trace_vivify_candidate (\n'
        '          solver, "vivify_budget_stop", trace_candidate_ordinal,\n'
        '          trace_candidate, probing_ticks, limit);\n'
        '      break;\n'
        '    }\n'
        '    if (TERMINATED (vivify_terminated_1))\n')
    replace_once(
        vivify,
        '    clause *candidate = kissat_dereference_clause (solver, ref);\n'
        '    assert (!candidate->garbage);\n'
        '    vivifier->tried++;\n',
        '    clause *candidate = kissat_dereference_clause (solver, ref);\n'
        '    assert (!candidate->garbage);\n'
        '    kissat_event_trace_vivify_candidate (\n'
        '        solver, "vivify_candidate", trace_candidate_ordinal++,\n'
        '        candidate, probing_ticks, limit);\n'
        '    vivifier->tried++;\n')
    replace_once(
        vivify,
        '    if (vivify_clause (vivifier, candidate))\n'
        '      vivifier->vivified++;\n',
        '    if (vivify_clause (vivifier, candidate))\n'
        '      vivifier->vivified++;\n'
        '    kissat_event_trace_vivify_candidate (\n'
        '        solver, "vivify_candidate_end", trace_candidate_ordinal - 1,\n'
        '        candidate, solver->statistics.probing_ticks, limit);\n')
    replace_once(
        vivify,
        '  unsigned *lits = c->lits;\n'
        '  memcpy (lits, BEGIN_STACK (*learned), new_size * sizeof *lits);\n',
        '  unsigned *lits = c->lits;\n'
        '  kissat_event_trace_vivify_factor_image (\n'
        '      solver, c, new_size, BEGIN_STACK (*learned));\n'
        '  memcpy (lits, BEGIN_STACK (*learned), new_size * sizeof *lits);\n')

    eliminate = dest / "src" / "eliminate.c"
    replace_once(eliminate, '#include "dense.h"\n',
                 '#include "dense.h"\n#include "eventtrace.h"\n')
    replace_once(
        eliminate,
        '  kissat_backtrack_propagate_and_flush_trail (solver);\n'
        '  assert (!solver->inconsistent);\n',
        '  kissat_backtrack_propagate_and_flush_trail (solver);\n'
        '  assert (!solver->inconsistent);\n'
        '  kissat_event_trace_eliminate_cleanup (solver, "entry");\n')
    replace_once(
        eliminate,
        '      if (eliminate_variable (solver, idx))\n'
        '        last_round_eliminated++;\n',
        '      const uint64_t trace_entry = s->eliminate_resolutions;\n'
        '      kissat_event_trace_bve_candidate (\n'
        '          solver, idx, "begin", trace_entry, resolution_limit, false);\n'
        '      const bool trace_applied = eliminate_variable (solver, idx);\n'
        '      kissat_event_trace_bve_candidate (\n'
        '          solver, idx, "end", s->eliminate_resolutions,\n'
        '          resolution_limit, trace_applied);\n'
        '      if (trace_applied)\n'
        '        last_round_eliminated++;\n')
    replace_once(
        eliminate,
        '  kissat_update_heap (solver, schedule, idx, -new_score);\n',
        '  kissat_update_heap (solver, schedule, idx, -new_score);\n'
        '  kissat_event_trace_bve_heap (solver, "update", schedule, idx);\n')
    replace_once(
        eliminate,
        '  return scheduled;\n}\n\nvoid kissat_flush_units_while_connected',
        '  kissat_event_trace_bve_heap (solver, "initial", &solver->schedule,\n'
        '                               UINT_MAX);\n'
        '  kissat_event_trace_bve_schedule (solver, &solver->schedule);\n'
        '  return scheduled;\n}\n\nvoid kissat_flush_units_while_connected')
    replace_once(
        eliminate,
        '      unsigned idx = kissat_pop_max_heap (solver, &solver->schedule);\n'
        '      if (!can_eliminate_variable (solver, idx))\n',
        '      unsigned idx = kissat_pop_max_heap (solver, &solver->schedule);\n'
        '      kissat_event_trace_bve_heap (solver, "pop", &solver->schedule,\n'
        '                                   idx);\n'
        '      if (!can_eliminate_variable (solver, idx))\n')

    resolve = dest / "src" / "resolve.c"
    replace_once(resolve, '#include "eliminate.h"\n',
                 '#include "eliminate.h"\n#include "eventtrace.h"\n')
    replace_once(
        resolve,
        '  uint64_t limit;\n\n  {\n',
        '  uint64_t limit;\n'
        '  unsigned trace_side0 = 0, trace_side1 = 0;\n\n'
        '  {\n')
    replace_once(
        resolve,
        '    const unsigned occlim = GET_OPTION (eliminateocclim);\n'
        '    limit = pos_count + (uint64_t) neg_count;\n',
        '    trace_side0 = pos_count;\n'
        '    trace_side1 = neg_count;\n'
        '    const unsigned occlim = GET_OPTION (eliminateocclim);\n'
        '    limit = pos_count + (uint64_t) neg_count;\n')
    replace_once(
        resolve,
        '  statches *const antecedents1 = &solver->antecedents[1];\n\n'
        '  if (gates) {\n',
        '  statches *const antecedents1 = &solver->antecedents[1];\n\n'
        '  const size_t trace_gate0 = SIZE_STACK (*gates0);\n'
        '  const size_t trace_gate1 = SIZE_STACK (*gates1);\n'
        '  const uint64_t trace_charged =\n'
        '      gates ? trace_gate0 * SIZE_STACK (*antecedents1) +\n'
        '                  trace_gate1 * SIZE_STACK (*antecedents0) +\n'
        '                  (solver->resolve_gate ? trace_gate0 * trace_gate1 : 0)\n'
        '            : (uint64_t) SIZE_STACK (*antecedents0) *\n'
        '                  SIZE_STACK (*antecedents1);\n'
        '  kissat_event_trace_bve_plan (\n'
        '      solver, idx, trace_side0, trace_side1, gates, trace_gate0,\n'
        '      trace_gate1, trace_charged, solver->resolve_gate);\n\n'
        '  if (gates) {\n')
    replace_once(
        resolve,
        '      INC (eliminate_resolutions);\n\n'
        '      for (all_literals_in_clause (other, d)) {\n',
        '      INC (eliminate_resolutions);\n'
        '      const uint32_t trace_pair_ordinal =\n'
        '          (uint32_t) solver->statistics.eliminate_resolutions;\n'
        '      kissat_event_trace_bve_pair (solver, IDX (lit),\n'
        '                                    trace_pair_ordinal, c, d, "begin");\n\n'
        '      for (all_literals_in_clause (other, d)) {\n')
    replace_once(
        resolve,
        '      if (size_resolvent == 1) {\n'
        '        const unsigned unit = PEEK_STACK (solver->resolvents, saved);\n'
        '        INC (eliminate_units);\n',
        '      if (size_resolvent == 1) {\n'
        '        const unsigned unit = PEEK_STACK (solver->resolvents, saved);\n'
        '        kissat_event_trace_bve_pair (\n'
        '            solver, IDX (lit), trace_pair_ordinal, c, d,\n'
        '            "unit-immediate");\n'
        '        INC (eliminate_units);\n')

    search = dest / "src" / "search.c"
    replace_once(search, '#include "eliminate.h"\n',
                 '#include "eliminate.h"\n#include "eventtrace.h"\n'
                 '#include "rulejournal.h"\n')
    replace_once(search,
        '  REPORT (0, \'*\');\n  int res = 0;\n',
        '  REPORT (0, \'*\');\n'
        '  kissat_event_trace (solver, "trace_begin", "-", true);\n'
        '  kissat_event_trace (solver, "startup_begin", "-", true);\n'
        '  int res = 0;\n')
    replace_once(search,
        '  if (!res && GET_OPTION (luckyearly))\n'
        '    res = kissat_lucky (solver);\n',
        '  if (!res && GET_OPTION (luckyearly)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.search_ticks;\n'
        '    kissat_event_trace (solver, "lucky_early_begin", "-", true);\n'
        '    res = kissat_lucky (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "lucky", "early", "startup", journal_pre,\n'
        '        journal_effort, solver->statistics.search_ticks, 0);\n'
        '    kissat_event_trace (solver, "lucky_early_end",\n'
        '                        res == 10 ? "sat" : "unknown", true);\n'
        '  }\n')
    replace_once(search,
        '  if (!res && kissat_preprocessing (solver))\n'
        '    res = kissat_preprocess (solver);\n',
        '  if (!res && kissat_preprocessing (solver)) {\n'
        '    kissat_event_trace (solver, "preprocess_begin", "-", true);\n'
        '    res = kissat_preprocess (solver);\n'
        '    kissat_event_trace (solver, "preprocess_end", "-", true);\n'
        '  }\n')
    replace_once(search,
        '  if (!res && GET_OPTION (luckylate))\n'
        '    res = kissat_lucky (solver);\n',
        '  if (!res && GET_OPTION (luckylate)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.search_ticks;\n'
        '    kissat_event_trace (solver, "lucky_late_begin", "-", true);\n'
        '    res = kissat_lucky (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "lucky", "late", "startup", journal_pre,\n'
        '        journal_effort, solver->statistics.search_ticks, 0);\n'
        '    kissat_event_trace (solver, "lucky_late_end",\n'
        '                        res == 10 ? "sat" : "unknown", true);\n'
        '  }\n')
    replace_once(search,
        '    while (!res) {\n'
        '      clause *conflict = kissat_search_propagate (solver);\n'
        '      if (conflict)\n'
        '        res = kissat_analyze (solver, conflict);\n',
        '    while (!res) {\n'
        '      kissat_event_trace_checkpoint (solver);\n'
        '      kissat_event_trace_propagate_cursor (solver);\n'
        '      const size_t trace_trail_begin = SIZE_ARRAY (solver->trail);\n'
        '      const uint64_t propagate_pre = kissat_rule_journal_digest (solver);\n'
        '      const uint64_t propagate_effort = solver->statistics.search_ticks;\n'
        '      clause *conflict = kissat_search_propagate (solver);\n'
        '      kissat_rule_journal_rule_attempt (\n'
        '          solver, "propagate", "search", "search-loop", propagate_pre,\n'
        '          propagate_effort, solver->statistics.search_ticks, 0);\n'
        '      kissat_event_trace_implications (solver, trace_trail_begin);\n'
        '      if (conflict) {\n'
        '        const uint64_t analyze_pre = kissat_rule_journal_digest (solver);\n'
        '        const uint64_t analyze_effort = solver->statistics.search_ticks;\n'
        '        kissat_event_trace_conflict (solver, conflict);\n'
        '        kissat_event_trace_detail (solver, "analysis_begin",\n'
        '                                   (int) solver->level);\n'
        '        kissat_event_trace_mode_clock (solver, "pre_analyze");\n'
        '        res = kissat_analyze (solver, conflict);\n'
        '        kissat_rule_journal_rule_attempt (\n'
        '            solver, "conflict-analysis", "current-conflict",\n'
        '            "search-loop", analyze_pre, analyze_effort,\n'
        '            solver->statistics.search_ticks, 0);\n'
        '        kissat_event_trace_heap_scores (solver);\n'
        '        kissat_event_trace_detail (solver, "analysis_end",\n'
        '                                   res == 20 ? -1 : (int) solver->level);\n'
        '        kissat_event_trace_mode_clock (solver, "post_analyze");\n'
        '      }\n')
    replace_once(
        search,
        '      else if (kissat_restarting (solver))\n',
        '      else if ((kissat_event_trace_restart_clock (solver),\n'
        '                kissat_restarting (solver)))\n')
    wrappers = [
        ("kissat_reduce (solver)", "reduce"),
        ("kissat_switch_search_mode (solver)", "switch"),
        ("kissat_restart (solver)", "restart"),
        ("kissat_reorder (solver)", "reorder"),
        ("kissat_rephase (solver)", "rephase"),
        ("kissat_probe (solver)", "probe"),
        ("kissat_eliminate (solver)", "eliminate"),
    ]
    text = search.read_text()
    for call, event in wrappers:
        if call not in text:
            raise SystemExit(f"{search}: missing call anchor {call}")
        if call.startswith("kissat_switch") or call.startswith("kissat_restart") or call.startswith("kissat_reorder") or call.startswith("kissat_rephase"):
            old = f"        {call};"
            after = ("\n        kissat_event_trace_heap_scores (solver);"
                     if call.startswith("kissat_switch") else "")
            journal_before = (
                '        const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
                '        const uint64_t journal_effort = solver->statistics.search_ticks;\n'
                if event == "restart" else "")
            journal_after = (
                '        kissat_rule_journal_rule_attempt (\n'
                '            solver, "restart", "canonical", "search-loop",\n'
                '            journal_pre, journal_effort,\n'
                '            solver->statistics.search_ticks, 0);\n'
                if event == "restart" else "")
            new = (journal_before +
                   f'        kissat_event_trace (solver, "{event}_begin", "-", true);\n'
                   f"        {call};\n"
                   f"{journal_after}"
                   f'        kissat_event_trace (solver, "{event}_end", "-", true);'
                   f"{after}")
        else:
            old = f"        res = {call};"
            proof = event in ("reduce", "eliminate")
            journal_before = (
                '        const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
                '        const uint64_t journal_effort = solver->statistics.search_ticks;\n'
                if proof else "")
            journal_after = (
                f'        kissat_rule_journal_rule_attempt (\n'
                f'            solver, "{event}", "canonical", "search-loop",\n'
                f'            journal_pre, journal_effort,\n'
                f'            solver->statistics.search_ticks, 0);\n'
                if proof else "")
            new = (journal_before +
                   f'        kissat_event_trace (solver, "{event}_begin", "-", true);\n'
                   f"        res = {call};\n"
                   f"{journal_after}"
                   f'        kissat_event_trace (solver, "{event}_end", "-", true);')
        if text.count(old) != 1:
            raise SystemExit(f"{search}: expected one wrapper anchor {old!r}")
        text = text.replace(old, "{\n" + new + "\n      }")
    search.write_text(text)
    replace_once(
        search,
        '      else\n        kissat_decide (solver);\n',
        '      else {\n'
        '        const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '        const uint64_t journal_effort = solver->statistics.search_ticks;\n'
        '        kissat_decide (solver);\n'
        '        kissat_rule_journal_rule_attempt (\n'
        '            solver, "decide", "canonical", "search-loop", journal_pre,\n'
        '            journal_effort, solver->statistics.search_ticks, 0);\n'
        '      }\n')
    replace_once(search,
        '  report_search_result (solver, res);\n  return res;\n',
        '  {\n'
        '    char payload[32];\n'
        '    snprintf (payload, sizeof payload, "%d", res);\n'
        '    kissat_event_trace (solver, "terminate", payload, true);\n'
        '  }\n'
        '  kissat_event_trace_close ();\n'
        '  kissat_rule_journal_close ();\n'
        '  report_search_result (solver, res);\n'
        '  return res;\n')

    # Expose the ordered preprocessing/probing control surface, not only the
    # outer `probe` call in search.c.  The calls remain the real guards and
    # actions; the diagnostic overlay only brackets them.
    probe = dest / "src" / "probe.c"
    replace_once(probe, '#include "probe.h"\n',
                 '#include "probe.h"\n#include "eventtrace.h"\n'
                 '#include "rulejournal.h"\n'
                 '#include "krite.h"\n')
    replace_once(probe, '#include <inttypes.h>\n',
                 '#include <inttypes.h>\n#include <stdio.h>\n'
                 '#include <stdlib.h>\n#include <string.h>\n')
    replace_once(
        probe,
        'static void probe (kissat *solver) {\n'
        '  kissat_backtrack_propagate_and_flush_trail (solver);\n'
        '  assert (!solver->inconsistent);\n'
        '  STOP_SEARCH_AND_START_SIMPLIFIER (probe);\n'
        '  kissat_phase (solver, "probe", GET (probings),\n'
        '                "probing limit hit after %" PRIu64 " conflicts",\n'
        '                solver->limits.probe.conflicts);\n'
        '  kissat_congruence (solver);\n'
        '  kissat_substitute (solver, false);\n'
        '  kissat_binary_clauses_backbone (solver);\n'
        '  kissat_vivify (solver);\n'
        '  kissat_sweep (solver);\n'
        '  kissat_substitute (solver, false);\n'
        '  kissat_transitive_reduction (solver);\n'
        '  kissat_binary_clauses_backbone (solver);\n'
        '  kissat_factor (solver);\n'
        '  STOP_SIMPLIFIER_AND_RESUME_SEARCH (probe);\n'
        '}\n',
        'static void probe (kissat *solver) {\n'
        '  kissat_backtrack_propagate_and_flush_trail (solver);\n'
        '  assert (!solver->inconsistent);\n'
        '  STOP_SEARCH_AND_START_SIMPLIFIER (probe);\n'
        '  kissat_phase (solver, "probe", GET (probings),\n'
        '                "probing limit hit after %" PRIu64 " conflicts",\n'
        '                solver->limits.probe.conflicts);\n'
        '#define TRACE_RULE(CALL, NAME) do { \\\n'
        '  const uint64_t journal_pre = kissat_rule_journal_digest (solver); \\\n'
        '  uint64_t *const journal_meter = !strcmp (NAME, "factor") \\\n'
        '      ? &solver->statistics.factor_ticks \\\n'
        '      : &solver->statistics.probing_ticks; \\\n'
        '  const uint64_t journal_effort = *journal_meter; \\\n'
        '  kissat_event_trace_late (solver, NAME "_begin", "-", true); \\\n'
        '  kissat_event_trace (solver, NAME "_begin", "-", true); \\\n'
        '  CALL; \\\n'
        '  kissat_rule_journal_rule_attempt ( \\\n'
        '      solver, NAME, "canonical", "probe-pipeline", journal_pre, \\\n'
        '      journal_effort, *journal_meter, 0); \\\n'
        '  kissat_event_trace (solver, NAME "_end", "-", true); \\\n'
        '} while (0)\n'
        '  TRACE_RULE (kissat_congruence (solver), "congruence");\n'
        '  TRACE_RULE (kissat_substitute (solver, false), "substitute_early");\n'
        '  TRACE_RULE (kissat_binary_clauses_backbone (solver), "backbone_early");\n'
        '  TRACE_RULE (kissat_vivify (solver), "vivify");\n'
        '  TRACE_RULE (kissat_sweep (solver), "sweep");\n'
        '  TRACE_RULE (kissat_substitute (solver, false), "substitute_late");\n'
        '  TRACE_RULE (kissat_transitive_reduction (solver), "transitive");\n'
        '  TRACE_RULE (kissat_binary_clauses_backbone (solver), "backbone_late");\n'
        '  TRACE_RULE (kissat_factor (solver), "factor");\n'
        '#undef TRACE_RULE\n'
        '  STOP_SIMPLIFIER_AND_RESUME_SEARCH (probe);\n'
        '}\n')
    replace_once(
        probe,
        'static void probe_initially (kissat *solver) {\n'
        '  assert (!solver->level);\n'
        '  assert (!solver->inconsistent);\n'
        '  kissat_phase (solver, "probe", GET (probings), "initial probing");\n'
        '  bool substitute_at_the_end = true;\n'
        '  if (GET_OPTION (preprocesscongruence)) {\n'
        '    if (kissat_congruence (solver)) {\n'
        '      kissat_substitute (solver, true);\n'
        '      substitute_at_the_end = false;\n'
        '    }\n'
        '  }\n'
        '  if (GET_OPTION (preprocessbackbone))\n'
        '    kissat_binary_clauses_backbone (solver);\n'
        '  if (GET_OPTION (preprocessweep)) {\n'
        '    if (kissat_sweep (solver)) {\n'
        '      kissat_substitute (solver, true);\n'
        '      substitute_at_the_end = false;\n'
        '    }\n'
        '  }\n'
        '  if (substitute_at_the_end)\n'
        '    kissat_substitute (solver, false);\n'
        '  if (GET_OPTION (preprocessfactor))\n'
        '    kissat_factor (solver);\n'
        '}\n',
        'static void probe_initially (kissat *solver) {\n'
        '  assert (!solver->level);\n'
        '  assert (!solver->inconsistent);\n'
        '  kissat_phase (solver, "probe", GET (probings), "initial probing");\n'
        '  bool substitute_at_the_end = true;\n'
        '  if (GET_OPTION (preprocesscongruence)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.probing_ticks;\n'
        '    kissat_event_trace (solver, "congruence_begin", "-", true);\n'
        '    const bool congruent = kissat_congruence (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "congruence", "canonical", "preprocess", journal_pre,\n'
        '        journal_effort, solver->statistics.probing_ticks, 0);\n'
        '    kissat_event_trace (solver, "congruence_end", "-", true);\n'
        '    if (congruent) {\n'
        '      const uint64_t sub_pre = kissat_rule_journal_digest (solver);\n'
        '      const uint64_t sub_effort = solver->statistics.probing_ticks;\n'
        '      kissat_event_trace (solver, "substitute_complete_begin", "-", true);\n'
        '      kissat_substitute (solver, true);\n'
        '      kissat_rule_journal_rule_attempt (\n'
        '          solver, "substitute", "complete", "after-congruence",\n'
        '          sub_pre, sub_effort, solver->statistics.probing_ticks, 0);\n'
        '      kissat_event_trace (solver, "substitute_complete_end", "-", true);\n'
        '      substitute_at_the_end = false;\n'
        '    }\n'
        '  }\n'
        '  if (GET_OPTION (preprocessbackbone)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.probing_ticks;\n'
        '    kissat_event_trace (solver, "backbone_begin", "-", true);\n'
        '    kissat_binary_clauses_backbone (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "backbone", "canonical", "preprocess", journal_pre,\n'
        '        journal_effort, solver->statistics.probing_ticks, 0);\n'
        '    kissat_event_trace (solver, "backbone_end", "-", true);\n'
        '  }\n'
        '  if (GET_OPTION (preprocessweep)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.probing_ticks;\n'
        '    kissat_event_trace (solver, "sweep_begin", "-", true);\n'
        '    const bool swept = kissat_sweep (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "sweep", "canonical", "preprocess", journal_pre,\n'
        '        journal_effort, solver->statistics.probing_ticks, 0);\n'
        '    kissat_event_trace (solver, "sweep_end", "-", true);\n'
        '    if (swept) {\n'
        '      const uint64_t sub_pre = kissat_rule_journal_digest (solver);\n'
        '      const uint64_t sub_effort = solver->statistics.probing_ticks;\n'
        '      kissat_event_trace (solver, "substitute_complete_begin", "-", true);\n'
        '      kissat_substitute (solver, true);\n'
        '      kissat_rule_journal_rule_attempt (\n'
        '          solver, "substitute", "complete", "after-sweep", sub_pre,\n'
        '          sub_effort, solver->statistics.probing_ticks, 0);\n'
        '      kissat_event_trace (solver, "substitute_complete_end", "-", true);\n'
        '      substitute_at_the_end = false;\n'
        '    }\n'
        '  }\n'
        '  if (substitute_at_the_end) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.probing_ticks;\n'
        '    kissat_event_trace (solver, "substitute_begin", "-", true);\n'
        '    kissat_substitute (solver, false);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "substitute", "canonical", "preprocess-final",\n'
        '        journal_pre, journal_effort,\n'
        '        solver->statistics.probing_ticks, 0);\n'
        '    kissat_event_trace (solver, "substitute_end", "-", true);\n'
        '  }\n'
        '  if (GET_OPTION (preprocessfactor)) {\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '    const uint64_t journal_effort = solver->statistics.factor_ticks;\n'
        '    kissat_event_trace (solver, "factor_begin", "-", true);\n'
        '    const char *predump_path = getenv ("KISSAT_DUMP_PREFACTOR");\n'
        '    if (predump_path && *predump_path) {\n'
        '      FILE *predump_file = fopen (predump_path, "w");\n'
        '      if (!predump_file) { perror (predump_path); exit (1); }\n'
        '      kissat_write_dimacs (solver, predump_file);\n'
        '      fclose (predump_file);\n'
        '      exit (0);\n'
        '    }\n'
        '    kissat_factor (solver);\n'
        '    kissat_rule_journal_rule_attempt (\n'
        '        solver, "factor", "canonical", "preprocess", journal_pre,\n'
        '        journal_effort, solver->statistics.factor_ticks, 0);\n'
        '    kissat_event_trace (solver, "factor_end", "-", true);\n'
        '    const char *dump_path = getenv ("KISSAT_DUMP_FACTORED");\n'
        '    if (dump_path && *dump_path) {\n'
        '      FILE *dump_file = fopen (dump_path, "w");\n'
        '      if (!dump_file) {\n'
        '        perror (dump_path);\n'
        '        exit (1);\n'
        '      }\n'
        '      kissat_write_dimacs (solver, dump_file);\n'
        '      fclose (dump_file);\n'
        '      exit (0);\n'
        '    }\n'
        '  }\n'
        '}\n')

    factor = dest / "src" / "factor.c"
    replace_once(factor, '#include "factor.h"\n',
                 '#include "factor.h"\n#include "eventtrace.h"\n')

    substitute = dest / "src" / "substitute.c"
    replace_once(substitute, '#include "substitute.h"\n',
                 '#include "substitute.h"\n#include "eventtrace.h"\n')
    replace_once(
        substitute,
        '  determine_representatives (solver, repr);\n',
        '  determine_representatives (solver, repr);\n'
        '  for (all_literals (lit))\n'
        '    kissat_event_trace_substitute_map (solver, lit, repr[lit]);\n')
    replace_once(
        substitute,
        '    const unsigned other = watch.binary.lit;\n'
        '    kissat_delete_binary (solver, lit, other);\n',
        '    const unsigned other = watch.binary.lit;\n'
        '    kissat_event_trace_binary_delete_site (\n'
        '        solver, "substitute", lit, other);\n'
        '    kissat_delete_binary (solver, lit, other);\n')

    preprocess = dest / "src" / "preprocess.c"
    replace_once(preprocess, '#include "preprocess.h"\n',
                 '#include "preprocess.h"\n#include "eventtrace.h"\n')
    replace_once(
        preprocess,
        '  kissat_initial_sparse_collect (solver);\n',
        '  for (all_literals (lit))\n'
        '    kissat_event_trace_substitute_map (solver, lit, lit);\n'
        '  kissat_initial_sparse_collect (solver);\n')

    search = dest / "src" / "search.c"
    replace_once(
        search,
        '  if (!res)\n    kissat_classify (solver);\n',
        '  if (!res)\n'
        '    for (all_literals (lit))\n'
        '      kissat_event_trace_factor_literal_state (\n'
        '          solver, lit, ACTIVE (IDX (lit)), false, solver->values[lit],\n'
        '          SIZE_WATCHES (WATCHES (lit)));\n'
        '  if (!res)\n    kissat_classify (solver);\n')
    replace_once(
        factor,
        'void kissat_factor (kissat *solver) {\n'
        '  assert (!solver->level);\n',
        'void kissat_factor (kissat *solver) {\n'
        '  assert (!solver->level);\n'
        '  kissat_event_trace_late (solver, "factor_begin", "-", true);\n')
    replace_once(
        factor,
        '  for (all_variables (idx)) {\n'
        '    if (ACTIVE (idx)) {\n'
        '      struct flags *f = flags + idx;\n'
        '      const unsigned lit = LIT (idx);\n'
        '      const unsigned not_lit = NOT (lit);\n'
        '      if (f->factor & 1)\n'
        '        update_candidate (factoring, lit);\n'
        '      if (f->factor & 2)\n'
        '        update_candidate (factoring, not_lit);\n'
        '    }\n'
        '  }\n',
        '  size_t trace_schedule_ordinal = 0;\n'
        '  for (all_variables (idx)) {\n'
        '    struct flags *f = flags + idx;\n'
        '    const unsigned lit = LIT (idx);\n'
        '    const unsigned not_lit = NOT (lit);\n'
        '    kissat_event_trace_factor_literal_state (\n'
        '        solver, lit, ACTIVE (idx), f->factor & 1, solver->values[lit],\n'
        '        SIZE_WATCHES (solver->watches[lit]));\n'
        '    kissat_event_trace_factor_literal_state (\n'
        '        solver, not_lit, ACTIVE (idx), f->factor & 2,\n'
        '        solver->values[not_lit],\n'
        '        SIZE_WATCHES (solver->watches[not_lit]));\n'
        '    if (ACTIVE (idx)) {\n'
        '      if (f->factor & 1) {\n'
        '        kissat_event_trace_factor_schedule (\n'
        '            solver, trace_schedule_ordinal++, lit,\n'
        '            SIZE_WATCHES (solver->watches[lit]));\n'
        '        update_candidate (factoring, lit);\n'
        '      }\n'
        '      if (f->factor & 2) {\n'
        '        kissat_event_trace_factor_schedule (\n'
        '            solver, trace_schedule_ordinal++, not_lit,\n'
        '            SIZE_WATCHES (solver->watches[not_lit]));\n'
        '        update_candidate (factoring, not_lit);\n'
        '      }\n'
        '    }\n'
        '  }\n')
    replace_once(
        factor,
        '  uint64_t *ticks = &solver->statistics.factor_ticks;\n',
        '  uint64_t *ticks = &solver->statistics.factor_ticks;\n'
        '  const uint64_t trace_tick_start = *ticks;\n')
    replace_once(
        factor,
        '  START (factor);\n'
        '  INC (factorizations);\n',
        '  kissat_event_trace (solver, "factor_admitted", "-", true);\n'
        '  START (factor);\n'
        '  INC (factorizations);\n')
    replace_once(
        factor,
        '  STOP (factor);\n'
        '}\n',
        '  STOP (factor);\n'
        '  kissat_event_trace (solver, "factor_applied", "-", true);\n'
        '}\n')
    replace_once(
        factor,
        '  bool completed = kissat_empty_heap (&factoring.schedule);\n'
        '  adjust_scores_and_phases_of_fresh_variables (&factoring);\n',
        '  bool completed = kissat_empty_heap (&factoring.schedule);\n'
        '  kissat_event_trace_factor_summary (solver, *ticks - trace_tick_start);\n'
        '  adjust_scores_and_phases_of_fresh_variables (&factoring);\n')
    replace_once(
        factor,
        '    const unsigned first =\n'
        '        kissat_pop_max_heap (solver, &factoring.schedule);\n',
        '    const unsigned first =\n'
        '        kissat_pop_max_heap (solver, &factoring.schedule);\n'
        '    kissat_event_trace_factor_heap (solver, first,\n'
        '                                    &factoring.schedule);\n')
    replace_once(
        factor,
        '    const size_t first_count = first_factor (&factoring, first);\n'
        '    if (first_count > 1) {\n',
        '    const size_t first_count = first_factor (&factoring, first);\n'
        '    if (first_count > 1) {\n'
        '      kissat_event_trace_factor_seed (solver, first, first_count);\n')
    replace_once(
        factor,
        '      if (q && reduction > factoring.bound) {\n'
        '        if (apply_factoring (&factoring, q)) {\n',
        '      if (q && reduction > factoring.bound) {\n'
        '        kissat_event_trace_factor_grid (solver, q->id + 1,\n'
        '                                        SIZE_STACK (q->clauses),\n'
        '                                        reduction);\n'
        '        for (quotient *p = q; p; p = p->prev)\n'
        '          kissat_event_trace_factor_grid_member (solver, p->id,\n'
        '                                                 p->factor);\n'
        '        if (apply_factoring (&factoring, q)) {\n'
        '          kissat_event_trace_factor_heap (solver, q->factor,\n'
        '                                          &factoring.schedule);\n')
    replace_once(
        factor,
        '      double next_score = -1;\n'
        '      for (all_stack (unsigned, lit, *counted)) {\n'
        '        const unsigned lit_count = count[lit];\n'
        '        if (lit_count != next_count)\n'
        '          continue;\n'
        '        double lit_score = tied_next_factor_score (factoring, lit);\n',
        '      double next_score = -1;\n'
        '      size_t candidate_ordinal = 0;\n'
        '      for (all_stack (unsigned, lit, *counted)) {\n'
        '        const size_t ordinal = candidate_ordinal++;\n'
        '        const unsigned lit_count = count[lit];\n'
        '        if (lit_count != next_count)\n'
        '          continue;\n'
        '        double lit_score = tied_next_factor_score (factoring, lit);\n'
        '        const quotient *first_q = factoring->quotients.first;\n'
        '        const quotient *last_q = factoring->quotients.last;\n'
        '        kissat_event_trace_factor_candidate (\n'
        '            solver, first_q->factor, last_q->id + 1, ordinal, lit,\n'
        '            lit_count, (unsigned) lit_score);\n'
        '        kissat_event_trace_factor_score_entries (\n'
        '            solver, first_q->factor, last_q->id + 1, lit);\n')
    replace_once(
        factor,
        '  uint64_t ticks = 0;\n'
        '  for (all_binary_large_watches (watch, *factor_watches)) {\n'
        '    PUSH_STACK (*clauses, watch);\n',
        '  uint64_t ticks = 0;\n'
        '  size_t trace_ordinal = 0;\n'
        '  for (all_binary_large_watches (watch, *factor_watches)) {\n'
        '    if (watch.type.binary)\n'
        '      kissat_event_trace_factor_remainder_binary (\n'
        '          solver, factor, trace_ordinal, watch.binary.lit);\n'
        '    else {\n'
        '      clause *trace_clause =\n'
        '          kissat_dereference_clause (solver, watch.large.ref);\n'
        '      kissat_event_trace_factor_remainder_clause (\n'
        '          solver, factor, trace_ordinal, trace_clause);\n'
        '    }\n'
        '    trace_ordinal++;\n'
        '    PUSH_STACK (*clauses, watch);\n')
    replace_once(
        factor,
        '          const unsigned next_idx = IDX (next);\n'
        '          if (!ACTIVE (next_idx))\n'
        '            continue;\n'
        '          assert (!(marks[next] & (FACTOR | NOUNTED)));\n',
        '          const unsigned next_idx = IDX (next);\n'
        '          if (!ACTIVE (next_idx))\n'
        '            continue;\n'
        '          const quotient *trace_first_q =\n'
        '              factoring->quotients.first;\n'
        '          const quotient *trace_last_q = factoring->quotients.last;\n'
        '          kissat_event_trace_factor_candidate_source (\n'
        '              solver, trace_first_q->factor, trace_last_q->id + 1,\n'
        '              min_lit, next, c, d);\n'
        '          assert (!(marks[next] & (FACTOR | NOUNTED)));\n')
    replace_once(
        factor,
        '        if (marks[next] & FACTOR)\n'
        '          continue;\n'
        '        const unsigned next_idx = IDX (next);\n'
        '        if (!ACTIVE (next_idx))\n'
        '          continue;\n'
        '        if (!count[next])\n'
        '          PUSH_STACK (*counted, next);\n'
        '        count[next]++;\n',
        '        if (marks[next] & FACTOR)\n'
        '          continue;\n'
        '        const unsigned next_idx = IDX (next);\n'
        '        if (!ACTIVE (next_idx))\n'
        '          continue;\n'
        '        const quotient *trace_first_q = factoring->quotients.first;\n'
        '        const quotient *trace_last_q = factoring->quotients.last;\n'
        '        kissat_event_trace_factor_candidate_binary_source (\n'
        '            solver, trace_first_q->factor, trace_last_q->id + 1, q,\n'
        '            next);\n'
        '        if (!count[next])\n'
        '          PUSH_STACK (*counted, next);\n'
        '        count[next]++;\n')

    decide = dest / "src" / "decide.c"
    replace_once(decide, '#include "decide.h"\n',
                 '#include "decide.h"\n#include "eventtrace.h"\n')
    replace_once(
        decide,
        'static unsigned next_random_decision (kissat *solver) {\n'
        '  if (!VARS)\n',
        'static unsigned next_random_decision (kissat *solver) {\n'
        '  kissat_event_trace_randec_state (solver, "pre");\n'
        '  if (!VARS)\n')
    replace_once(
        decide,
        '    kissat_start_random_sequence (solver);\n'
        '  }\n\n'
        '  for (;;) {\n',
        '    kissat_start_random_sequence (solver);\n'
        '    kissat_event_trace_randec_state (solver, "post-start");\n'
        '  }\n\n'
        '  for (;;) {\n')
    replace_once(
        decide,
        '  for (;;) {\n'
        '    unsigned idx = kissat_next_random32 (&solver->random) % VARS;\n'
        '    if (!ACTIVE (idx))\n'
        '      continue;\n'
        '    unsigned lit = LIT (idx);\n'
        '    if (solver->values[lit])\n'
        '      continue;\n'
        '    return idx;\n'
        '  }\n',
        '  for (;;) {\n'
        '    const unsigned raw = kissat_next_random32 (&solver->random);\n'
        '    const unsigned idx = raw % VARS;\n'
        '    const unsigned lit = LIT (idx);\n'
        '    const bool accepted = ACTIVE (idx) && !solver->values[lit];\n'
        '    kissat_event_trace_randec_draw (solver, raw, idx, accepted);\n'
        '    if (accepted)\n'
        '      return idx;\n'
        '  }\n')
    replace_once(decide,
        '  kissat_assign_decision (solver, lit);\n  STOP (decide);\n',
        '  kissat_assign_decision (solver, lit);\n'
        '  kissat_event_trace_literal (solver, "decision", lit);\n'
        '  kissat_event_trace_heap_scores (solver);\n'
        '  STOP (decide);\n')
    replace_once(
        decide,
        '  const value value = kissat_decide_phase (solver, idx);\n',
        '  const value value = kissat_decide_phase (solver, idx);\n'
        '  kissat_event_trace_phase_pick (solver, idx, value);\n')

    inlineassign = dest / "src" / "inlineassign.h"
    replace_once(inlineassign, '#define _inlineassign_h_INLCUDED\n',
                 '#define _inlineassign_h_INLCUDED\n\n#include "eventtrace.h"\n')
    replace_once(
        inlineassign,
        '  if (!level) {\n'
        '    kissat_mark_fixed_literal (solver, lit);\n',
        '  if (!level) {\n'
        '    kissat_event_trace_root_assign (solver, lit, binary, reason);\n'
        '    kissat_mark_fixed_literal (solver, lit);\n')
    replace_once(
        inlineassign,
        '    value *saved = &SAVED (idx);\n'
        '    *saved = new_value;\n',
        '    value *saved = &SAVED (idx);\n'
        '    *saved = new_value;\n'
        '    kissat_event_trace_phase_save (solver, lit);\n')

    inlineheap = dest / "src" / "inlineheap.h"
    replace_once(inlineheap, '#include "internal.h"\n',
                 '#include "internal.h"\n#include "eventtrace.h"\n')
    replace_once(
        inlineheap,
        '  heap->pos[idx] = SIZE_STACK (heap->stack);\n'
        '  PUSH_STACK (heap->stack, idx);\n'
        '  kissat_bubble_up (solver, heap, idx);\n',
        '  const unsigned trace_from = SIZE_STACK (heap->stack);\n'
        '  heap->pos[idx] = trace_from;\n'
        '  PUSH_STACK (heap->stack, idx);\n'
        '  kissat_bubble_up (solver, heap, idx);\n'
        '  kissat_event_trace_heap_insert (solver, heap, idx, trace_from,\n'
        '                                  heap->pos[idx]);\n')
    replace_once(
        inlineheap,
        '  const unsigned last = POP_STACK (*stack);\n'
        '  unsigned *const pos = heap->pos;\n',
        '  const unsigned last_pos = SIZE_STACK (*stack) - 1;\n'
        '  const unsigned last = POP_STACK (*stack);\n'
        '  kissat_event_trace_heap_pop (solver, "begin", heap, last, idx,\n'
        '                               last_pos, 0);\n'
        '  unsigned *const pos = heap->pos;\n')
    replace_once(
        inlineheap,
        '    stack[idx_pos] = child;\n'
        '    pos[child] = idx_pos;\n',
        '    kissat_event_trace_heap_pop (solver, "move", heap, idx, child,\n'
        '                                 child_pos, idx_pos);\n'
        '    stack[idx_pos] = child;\n'
        '    pos[child] = idx_pos;\n')
    replace_once(
        inlineheap,
        '  kissat_bubble_down (solver, heap, last);\n'
        '#ifdef CHECK_HEAP\n'
        '  kissat_check_heap (heap);\n'
        '#endif\n'
        '  return idx;\n',
        '  kissat_bubble_down (solver, heap, last);\n'
        '  kissat_event_trace_heap_pop (solver, "end", heap, last, last,\n'
        '                               heap->pos[last], heap->pos[last]);\n'
        '#ifdef CHECK_HEAP\n'
        '  kissat_check_heap (heap);\n'
        '#endif\n'
        '  return idx;\n')
    replace_once(
        inlineheap,
        '  const unsigned last = POP_STACK (heap->stack);\n'
        '  heap->pos[last] = DISCONTAIN;\n'
        '  if (last == idx)\n'
        '    return;\n'
        '  const unsigned idx_pos = heap->pos[idx];\n'
        '  heap->pos[idx] = DISCONTAIN;\n'
        '  POKE_STACK (heap->stack, idx_pos, last);\n'
        '  heap->pos[last] = idx_pos;\n'
        '  kissat_bubble_up (solver, heap, last);\n'
        '  kissat_bubble_down (solver, heap, last);\n',
        '  const unsigned last_pos = SIZE_STACK (heap->stack) - 1;\n'
        '  const unsigned last = POP_STACK (heap->stack);\n'
        '  heap->pos[last] = DISCONTAIN;\n'
        '  const unsigned idx_pos = heap->pos[idx];\n'
        '  if (last == idx) {\n'
        '    kissat_event_trace_heap_remove (solver, heap, idx, last, last_pos,\n'
        '                                    last_pos, UINT_MAX);\n'
        '    return;\n'
        '  }\n'
        '  heap->pos[idx] = DISCONTAIN;\n'
        '  POKE_STACK (heap->stack, idx_pos, last);\n'
        '  heap->pos[last] = idx_pos;\n'
        '  kissat_bubble_up (solver, heap, last);\n'
        '  kissat_bubble_down (solver, heap, last);\n'
        '  kissat_event_trace_heap_remove (solver, heap, idx, last, idx_pos,\n'
        '                                  last_pos, heap->pos[last]);\n')
    replace_once(
        decide,
        '    kissat_pop_max_heap (solver, scores);\n'
        '    res = kissat_max_heap (scores);\n',
        '    kissat_pop_max_heap (solver, scores);\n'
        '    kissat_event_trace_heap_scores (solver);\n'
        '    res = kissat_max_heap (scores);\n')

    fastassign = dest / "src" / "fastassign.h"
    replace_once(fastassign, '#include "inlineassign.h"\n',
                 '#include "inlineassign.h"\n#include "eventtrace.h"\n')
    replace_once(
        fastassign,
        '  if (GET_OPTION (jumpreasons) && level && solver->classification.bigbig) {\n',
        '  const unsigned immediate_reason = other;\n'
        '  bool jumped = false;\n'
        '  if (GET_OPTION (jumpreasons) && level && solver->classification.bigbig) {\n')
    replace_once(
        fastassign,
        '      INC (jumped_reasons);\n'
        '      other = a->reason;\n',
        '      INC (jumped_reasons);\n'
        '      other = a->reason;\n'
        '      jumped = true;\n')
    replace_once(
        fastassign,
        '  kissat_fast_assign (solver, probing, level, values, assigned, true, lit,\n'
        '                      other);\n',
        '  kissat_event_trace_binary_assignment (solver, lit, immediate_reason,\n'
        '                                        other, jumped);\n'
        '  kissat_fast_assign (solver, probing, level, values, assigned, true, lit,\n'
        '                      other);\n')

    clause = dest / "src" / "clause.c"
    replace_once(clause, '#include "collect.h"\n',
                 '#include "collect.h"\n#include "eventtrace.h"\n')
    replace_once(
        clause,
        '  if (watch)\n'
        '    kissat_watch_binary (solver, first, second);\n',
        '  kissat_event_trace_binary_add (solver, first, second);\n'
        '  if (watch)\n'
        '    kissat_watch_binary (solver, first, second);\n')
    replace_once(
        clause,
        'void kissat_delete_binary (kissat *solver, unsigned a, unsigned b) {\n'
        '  LOGBINARY (a, b, "delete");\n',
        'void kissat_delete_binary (kissat *solver, unsigned a, unsigned b) {\n'
        '  kissat_event_trace_binary_delete (solver, a, b);\n'
        '  LOGBINARY (a, b, "delete");\n')

    transitive = dest / "src" / "transitive.c"
    replace_once(transitive, '#include "terminate.h"\n',
                 '#include "terminate.h"\n#include "eventtrace.h"\n')
    replace_once(
        transitive,
        '  const size_t size_src_watches = SIZE_WATCHES (*src_watches);\n'
        '  const unsigned src_ticks =\n',
        '  const size_t size_src_watches = SIZE_WATCHES (*src_watches);\n'
        '  const uint64_t event_trace_source_ticks_before =\n'
        '      solver->statistics.transitive_ticks;\n'
        '  bool event_trace_source_selected =\n'
        '      kissat_transitive_trace_all_sources ();\n'
        '  size_t event_trace_candidate_count = 0;\n'
        '  unsigned event_trace_out_degree = 0;\n'
        '  for (const watch *q = begin_src; q != end_src; q++) {\n'
        '    const watch source_watch = *q;\n'
        '    if (!source_watch.type.binary)\n'
        '      break;\n'
        '    const unsigned destination = source_watch.binary.lit;\n'
        '    event_trace_out_degree++;\n'
        '    unsigned event_trace_copies = 0;\n'
        '    for (const watch *r = begin_src; r != end_src; r++) {\n'
        '      const watch copy = *r;\n'
        '      if (!copy.type.binary)\n'
        '        break;\n'
        '      event_trace_copies += copy.binary.lit == destination;\n'
        '    }\n'
        '    const char *event_trace_skip =\n'
        '        destination < src ? "ownership"\n'
        '        : VALUE (destination) ? "destination-fixed"\n'
        '        : event_trace_copies > 1 ? "duplicate" : "admitted";\n'
        '    if (kissat_transitive_trace_pair_selected (solver, src,\n'
        '                                                destination)) {\n'
        '      event_trace_source_selected = true;\n'
        '      kissat_transitive_trace_admission (\n'
        '          solver, src, destination, event_trace_copies,\n'
        '          event_trace_skip);\n'
        '    }\n'
        '    if (destination < src || VALUE (destination))\n'
        '      continue;\n'
        '    event_trace_candidate_count++;\n'
        '  }\n'
        '  const uint64_t event_trace_source_sequence =\n'
        '      kissat_transitive_trace_source_begin (\n'
        '          solver, event_trace_source_selected, src,\n'
        '          event_trace_source_ticks_before, event_trace_candidate_count,\n'
        '          event_trace_out_degree);\n'
        '  size_t event_trace_candidate_ordinal = 0;\n'
        '  const unsigned src_ticks =\n')
    replace_once(
        transitive,
        '  START (transitive);\n'
        '  INC (transitive_reductions);\n',
        '  START (transitive);\n'
        '  INC (transitive_reductions);\n'
        '  kissat_transitive_trace_endpoints (solver);\n')
    replace_once(
        transitive,
        '    assert (kissat_propagated (solver));\n'
        '    unsigned *saved = solver->propagate;\n',
        '    const size_t event_trace_ordinal = event_trace_candidate_ordinal++;\n'
        '    const uint64_t event_trace_candidate_ticks_before =\n'
        '        solver->statistics.transitive_ticks;\n'
        '    assert (kissat_propagated (solver));\n'
        '    unsigned *saved = solver->propagate;\n')
    replace_once(
        transitive,
        '      const size_t size_lit_watches = SIZE_WATCHES (*lit_watches);\n'
        '      inner_ticks +=\n'
        '          1 + kissat_cache_lines (size_lit_watches, sizeof (watch));\n'
        '      for (const watch *q = begin_lit; q != end_lit; q++) {\n',
        '      const size_t size_lit_watches = SIZE_WATCHES (*lit_watches);\n'
        '      const uint64_t event_trace_node_ticks_before =\n'
        '          solver->statistics.transitive_ticks + inner_ticks;\n'
        '      inner_ticks +=\n'
        '          1 + kissat_cache_lines (size_lit_watches, sizeof (watch));\n'
        '      kissat_transitive_trace_walk_node (\n'
        '          solver, src, dst, lit, propagate - solver->propagate - 1,\n'
        '          event_trace_node_ticks_before,\n'
        '          solver->statistics.transitive_ticks + inner_ticks,\n'
        '          size_lit_watches);\n'
        '      kissat_transitive_trace_walk_entries (\n'
        '          solver, src, dst, lit, not_lit);\n'
        '      for (const watch *q = begin_lit; q != end_lit; q++) {\n')
    replace_once(
        transitive,
        '    transitive_backtrack (solver, saved);\n\n'
        '    if (transitive) {\n',
        '    transitive_backtrack (solver, saved);\n'
        '    kissat_transitive_trace_candidate (\n'
        '        solver, event_trace_source_sequence, src, event_trace_ordinal,\n'
        '        dst, event_trace_candidate_ticks_before,\n'
        '        solver->statistics.transitive_ticks, transitive, failed,\n'
        '        event_trace_candidate_count, event_trace_out_degree);\n\n'
        '    if (transitive) {\n')
    replace_once(
        transitive,
        '  return res;\n}\n\nstatic inline bool less_stable_transitive',
        '  kissat_transitive_trace_source_end (\n'
        '      solver, event_trace_source_sequence, src,\n'
        '      event_trace_candidate_ordinal, event_trace_source_ticks_before,\n'
        '      solver->statistics.transitive_ticks, res,\n'
        '      event_trace_candidate_count, event_trace_out_degree);\n'
        '  return res;\n}\n\nstatic inline bool less_stable_transitive')

    bump = dest / "src" / "bump.c"
    replace_once(bump, '#include "analyze.h"\n',
                 '#include "analyze.h"\n#include "eventtrace.h"\n')
    replace_once(
        bump,
        '  for (all_stack (unsigned, idx, solver->analyzed))\n'
        '    if (flags[idx].active)\n'
        '      bump_analyzed_variable_score (solver, idx);\n',
        '  kissat_event_trace_heap_before_bumps (solver);\n'
        '  for (all_stack (unsigned, idx, solver->analyzed))\n'
        '    if (flags[idx].active) {\n'
        '      bump_analyzed_variable_score (solver, idx);\n'
        '      kissat_event_trace_literal (solver, "score_bump", LIT (idx));\n'
        '      kissat_event_trace_heap_scores (solver);\n'
        '    }\n')

    backtrack = dest / "src" / "backtrack.c"
    replace_once(backtrack, '#include "analyze.h"\n',
                 '#include "analyze.h"\n#include "eventtrace.h"\n')
    replace_once(
        backtrack,
        '  if (!kissat_heap_contains (scores, idx))\n'
        '    kissat_push_heap (solver, scores, idx);\n',
        '  if (!kissat_heap_contains (scores, idx)) {\n'
        '    kissat_event_trace_literal (solver, "heap_reinsert", LIT (idx));\n'
        '    kissat_push_heap (solver, scores, idx);\n'
        '  }\n')
    replace_once(
        backtrack,
        '  if (solver->target_assigned < assigned) {\n'
        '    kissat_extremely_verbose (solver,\n',
        '  if (solver->target_assigned < assigned) {\n'
        '    kissat_event_trace_target_state (solver, "target-save-pre",\n'
        '                                     assigned);\n'
        '    kissat_extremely_verbose (solver,\n')
    replace_once(
        backtrack,
        '    kissat_save_target_phases (solver);\n'
        '    INC (target_saved);\n'
        '  }\n',
        '    kissat_save_target_phases (solver);\n'
        '    INC (target_saved);\n'
        '    kissat_event_trace_target_state (solver, "target-save-post",\n'
        '                                     assigned);\n'
        '  }\n')

    compact = dest / "src" / "compact.c"
    replace_once(compact, '#include "compact.h"\n',
                 '#include "compact.h"\n#include "eventtrace.h"\n')
    replace_once(
        compact,
        '  compact_best_and_target_values (solver, vars);\n\n'
        '  solver->vars = vars;\n',
        '  kissat_event_trace_target_state (solver, "compact-pre",\n'
        '                                   kissat_assigned (solver));\n'
        '  compact_best_and_target_values (solver, vars);\n\n'
        '  solver->vars = vars;\n'
        '  kissat_event_trace_target_state (solver, "compact-post",\n'
        '                                   kissat_assigned (solver));\n')

    deduce = dest / "src" / "deduce.c"
    replace_once(deduce, '#include "deduce.h"\n',
                 '#include "deduce.h"\n#include "eventtrace.h"\n')
    replace_once(
        deduce,
        '  unsigned glue = MIN (c->glue, MAX_GLUE_USED);\n'
        '  solver->statistics.used[solver->stable].glue[glue]++;\n',
        '  unsigned glue = MIN (c->glue, MAX_GLUE_USED);\n'
        '  kissat_event_trace_clause_used (solver, c);\n'
        '  solver->statistics.used[solver->stable].glue[glue]++;\n')
    replace_once(
        deduce,
        '  kissat_push_analyzed (solver, all_assigned, idx);\n'
        '  assert (level <= solver->level);\n',
        '  kissat_push_analyzed (solver, all_assigned, idx);\n'
        '  kissat_event_trace_literal (solver, "analysis_mark", lit);\n'
        '  assert (level <= solver->level);\n')
    replace_once(
        deduce,
        '    resolved++;\n'
        '    if (a->binary) {\n'
        '      const unsigned other = a->reason;\n',
        '    resolved++;\n'
        '    kissat_event_trace_literal (solver, "analysis_resolve", uip);\n'
        '    if (a->binary) {\n'
        '      const unsigned other = a->reason;\n'
        '      kissat_event_trace_literal (solver, "analysis_binary_reason",\n'
        '                                  other);\n')
    replace_once(
        deduce,
        '      clause *reason = kissat_dereference_clause (solver, ref);\n'
        '      for (all_literals_in_clause (lit, reason))\n'
        '        if (lit != uip &&\n'
        '            analyze_literal (solver, all_assigned, frames, lit))\n'
        '          unresolved_on_current_level++;\n',
        '      clause *reason = kissat_dereference_clause (solver, ref);\n'
        '      kissat_event_trace_detail (solver, "analysis_reason_size",\n'
        '                                 reason->size);\n'
        '      for (all_literals_in_clause (lit, reason))\n'
        '        if (lit != uip) {\n'
        '          kissat_event_trace_literal (solver, "analysis_reason_lit",\n'
        '                                      lit);\n'
        '          if (analyze_literal (solver, all_assigned, frames, lit))\n'
        '            unresolved_on_current_level++;\n'
        '        }\n')

    analyze = dest / "src" / "analyze.c"
    replace_once(analyze, '#include "deduce.h"\n',
                 '#include "deduce.h"\n#include "eventtrace.h"\n')
    replace_once(analyze,
        '  kissat_compute_and_set_tier_limits (solver);\n'
        '  if (solver->limits.glue.interval < (1u << 16))\n',
        '  kissat_compute_and_set_tier_limits (solver);\n'
        '  kissat_event_trace_retier (solver);\n'
        '  if (solver->limits.glue.interval < (1u << 16))\n')
    replace_once(analyze,
        '    INC (search_ticks);\n',
        '    kissat_event_trace_detail (solver, "analysis_tick_reason_bump", 1);\n'
        '    INC (search_ticks);\n')
    replace_once(analyze,
        'static void analyze_reason_side_literals (kissat *solver) {\n'
        '  if (!GET_OPTION (bump))\n',
        'static void analyze_reason_side_literals (kissat *solver) {\n'
        '  kissat_event_trace_detail (solver, "reason_bump_delay_before",\n'
        '                             solver->delays.bumpreasons.count);\n'
        '  kissat_event_trace_detail (solver, "reason_bump_rate_milli",\n'
        '                             (int) (AVERAGE (decision_rate) * 1000));\n'
        '  if (!GET_OPTION (bump))\n')
    replace_once(analyze,
        '    } else {\n'
        '      if (GET_OPTION (minimize)) {\n'
        '        sort_deduced_clause (solver);\n'
        '        kissat_minimize_clause (solver);\n'
        '        if (GET_OPTION (shrink))\n'
        '          kissat_shrink_clause (solver);\n'
        '      }\n'
        '      analyze_reason_side_literals (solver);\n'
        '      kissat_learn_clause (solver);\n',
        '    } else {\n'
        '      kissat_event_trace_clause (solver, "learned_raw");\n'
        '      if (GET_OPTION (minimize)) {\n'
        '        sort_deduced_clause (solver);\n'
        '        kissat_event_trace_clause (solver, "learned_sorted");\n'
        '        kissat_minimize_clause (solver);\n'
        '        if (GET_OPTION (shrink))\n'
        '          kissat_shrink_clause (solver);\n'
        '      } else {\n'
        '        kissat_event_trace_clause (solver, "learned_sorted");\n'
        '      }\n'
        '      kissat_event_trace_clause (solver, "learned_shrunk");\n'
        '      analyze_reason_side_literals (solver);\n'
        '      kissat_event_trace_learned (solver);\n'
        '      kissat_learn_clause (solver);\n')

    minimize = dest / "src" / "minimize.c"
    replace_once(minimize, '#include "minimize.h"\n',
                 '#include "minimize.h"\n#include "eventtrace.h"\n')
    replace_once(minimize,
        '  if (GET_OPTION (minimizeticks))\n'
        '    INC (search_ticks);\n',
        '  if (GET_OPTION (minimizeticks)) {\n'
        '    kissat_event_trace_detail (solver, "analysis_tick_minimize", 1);\n'
        '    INC (search_ticks);\n'
        '  }\n')

    promote = dest / "src" / "promote.c"
    replace_once(promote, '#include "promote.h"\n',
                 '#include "promote.h"\n#include "eventtrace.h"\n')
    replace_once(promote,
        '  c->glue = new_glue;\n',
        '  kissat_event_trace_clause_glue (solver, c, old_glue, new_glue);\n'
        '  c->glue = new_glue;\n')

    shrink = dest / "src" / "shrink.c"
    replace_once(shrink, '#include "shrink.h"\n',
                 '#include "shrink.h"\n#include "eventtrace.h"\n')
    replace_once(shrink,
        '  if (GET_OPTION (minimizeticks))\n'
        '    INC (search_ticks);\n',
        '  if (GET_OPTION (minimizeticks)) {\n'
        '    kissat_event_trace_detail (solver, "analysis_tick_shrink", 1);\n'
        '    INC (search_ticks);\n'
        '  }\n')

    proplit = dest / "src" / "proplit.h"
    replace_once(proplit,
                 'static inline void kissat_watch_large_delayed',
                 '#include "eventtrace.h"\n\n'
                 'static inline void kissat_watch_large_delayed')
    replace_once(proplit,
        '  const unsigned not_lit = NOT (lit);\n\n'
        '  assert (not_lit < LITS);\n',
        '  const unsigned not_lit = NOT (lit);\n\n'
        '  kissat_event_trace_watch_scan (solver, lit);\n'
        '  assert (not_lit < LITS);\n')
    replace_once(proplit,
        '  const size_t size_watches = SIZE_WATCHES (*watches);\n'
        '  uint64_t ticks = 1 + kissat_cache_lines (size_watches, sizeof (watch));\n',
        '  const size_t size_watches = SIZE_WATCHES (*watches);\n'
        '  const uint64_t base_ticks =\n'
        '      1 + kissat_cache_lines (size_watches, sizeof (watch));\n'
        '  uint64_t binary_assignments = 0, large_dereferences = 0;\n'
        '  uint64_t relocations = 0, large_assignments = 0;\n'
        '  uint64_t ticks = base_ticks;\n')
    replace_once(proplit,
        '        kissat_fast_binary_assign (solver, probing, level, values, assigned,\n'
        '                                   blocking, not_lit);\n'
        '        ticks++;\n',
        '        kissat_fast_binary_assign (solver, probing, level, values, assigned,\n'
        '                                   blocking, not_lit);\n'
        '        binary_assignments++;\n'
        '        ticks++;\n')
    replace_once(proplit,
        '      clause *const c = (clause *) (arena + ref);\n'
        '      ticks++;\n',
        '      clause *const c = (clause *) (arena + ref);\n'
        '      large_dereferences++;\n'
        '      ticks++;\n')
    replace_once(proplit,
        '        kissat_delay_watching_large (solver, delayed, replacement, other,\n'
        '                                     ref);\n'
        '        ticks++;\n',
        '        kissat_delay_watching_large (solver, delayed, replacement, other,\n'
        '                                     ref);\n'
        '        relocations++;\n'
        '        ticks++;\n')
    replace_once(proplit,
        '        kissat_fast_assign_reference (solver, values, assigned, other, ref,\n'
        '                                      c);\n'
        '        ticks++;\n',
        '        kissat_fast_assign_reference (solver, values, assigned, other, ref,\n'
        '                                      c);\n'
        '        large_assignments++;\n'
        '        ticks++;\n')
    replace_once(proplit,
        '  kissat_watch_large_delayed (solver, all_watches, delayed);\n\n'
        '  return res;\n',
        '  kissat_watch_large_delayed (solver, all_watches, delayed);\n'
        '  kissat_event_trace_watch_scan_post (solver, lit);\n'
        '  kissat_event_trace_watch_cost (\n'
        '      solver, lit, size_watches, SIZE_WATCHES (*watches), base_ticks,\n'
        '      binary_assignments, large_dereferences, relocations,\n'
        '      large_assignments, ticks);\n\n'
        '  return res;\n')

    reduce = dest / "src" / "reduce.c"
    replace_once(reduce, '#include "collect.h"\n',
                 '#include "collect.h"\n#include "eventtrace.h"\n')
    replace_once(reduce,
        '  INC (reductions);\n'
        '  kissat_phase (solver, "reduce", GET (reductions),\n',
        '  INC (reductions);\n'
        '  kissat_event_trace_tiers (solver, "reduce_tiers");\n'
        '  kissat_phase (solver, "reduce", GET (reductions),\n')
    replace_once(reduce,
        '    const unsigned used = c->used;\n'
        '    if (used)\n'
        '      c->used = used - 1;\n'
        '    if (c->reason)\n'
        '      continue;\n'
        '    const unsigned glue = c->glue;\n',
        '    const unsigned used = c->used;\n'
        '    const unsigned trace_glue = c->glue;\n'
        '    const char *trace_eligibility =\n'
        '        c->reason ? "reason"\n'
        '        : trace_glue <= tier1 && used ? "tier1-used"\n'
        '        : trace_glue <= tier2 && used >= MAX_USED - 1\n'
        '              ? "tier2-recent"\n'
        '              : "candidate";\n'
        '    kissat_reduce_trace_clause (solver, c, (ward *) c - arena, used,\n'
        '                                tier1, tier2, trace_eligibility);\n'
        '    if (used)\n'
        '      c->used = used - 1;\n'
        '    if (c->reason)\n'
        '      continue;\n'
        '    const unsigned glue = c->glue;\n')
    replace_once(reduce,
        '  size_t target = size * fraction;\n#ifndef QUIET\n',
        '  size_t target = size * fraction;\n'
        '  kissat_reduce_trace_summary (solver, TIER1, TIER2, size, target);\n'
        '  {\n'
        '    size_t rank = 0;\n'
        '    for (all_stack (reducible, red, *reds)) {\n'
        '      kissat_reduce_trace_rank (solver, red.ref, rank, rank < target);\n'
        '      rank++;\n'
        '    }\n'
        '  }\n'
        '#ifndef QUIET\n')

    sweep = dest / "src" / "sweep.c"
    replace_once(sweep, '#include "terminate.h"\n',
                 '#include "terminate.h"\n#include "eventtrace.h"\n'
                 '#include "rulejournal.h"\n')
    replace_once(sweep,
        '  unsigned encoded;\n'
        '  unsigned save;\n',
        '  unsigned encoded;\n'
        '  unsigned trace_seed, trace_query_ordinal, trace_binary_ordinal;\n'
        '  unsigned save;\n')
    replace_once(sweep,
        '    sweep_candidate cand;\n'
        '    cand.rank = occ;\n'
        '    cand.idx = idx;\n'
        '    PUSH_STACK (fresh, cand);\n',
        '    sweep_candidate cand;\n'
        '    cand.rank = occ;\n'
        '    cand.idx = idx;\n'
        '    {\n'
        '      char payload[192];\n'
        '      const unsigned lit = LIT (idx);\n'
        '      const size_t pos = SIZE_WATCHES (WATCHES (lit));\n'
        '      const size_t neg = SIZE_WATCHES (WATCHES (NOT (lit)));\n'
        '      snprintf (payload, sizeof payload,\n'
        '                "external=%d|internal=%u|import=%u|pos=%zu|neg=%zu"\n'
        '                "|rank=%u",\n'
        '                kissat_export_literal (solver, lit), idx, idx, pos,\n'
        '                neg, cand.rank);\n'
        '      kissat_event_trace (solver, "sweep_schedule", payload, false);\n'
        '    }\n'
        '    PUSH_STACK (fresh, cand);\n')
    replace_once(sweep,
        '  return res;\n'
        '}\n\n'
        'static void set_kitten_ticks_limit (sweeper *sweeper) {\n',
        '  return res;\n'
        '}\n\n'
        'static int sweep_solve_traced (sweeper *sweeper, const char *kind,\n'
        '                               unsigned a, unsigned b) {\n'
        '  kissat *solver = sweeper->solver;\n'
        '  const uint64_t ticks_before = solver->statistics.kitten_ticks;\n'
        '  const int res = sweep_solve (sweeper);\n'
        '  const unsigned ordinal = sweeper->trace_query_ordinal++;\n'
        '  const int normalized = res == 10 ? 0 : res == 20 ? 1 : 2;\n'
        '  kissat_event_trace_sweep_query (\n'
        '      solver, sweeper->trace_seed, ordinal, kind, a, b, normalized,\n'
        '      solver->statistics.kitten_ticks - ticks_before);\n'
        '  if (res == 10)\n'
        '    kissat_event_trace_sweep_query_model (\n'
        '        solver, sweeper->trace_seed, ordinal,\n'
        '        SIZE_STACK (sweeper->vars), BEGIN_STACK (sweeper->vars));\n'
        '  return res;\n'
        '}\n\n'
        'static void set_kitten_ticks_limit (sweeper *sweeper) {\n')
    replace_once(sweep,
        '  kitten_clause (solver->kitten, SIZE_STACK (sweeper->clause),\n'
        '                 BEGIN_STACK (sweeper->clause));\n',
        '  kissat_event_trace_sweep_environment_clause (\n'
        '      solver, sweeper->trace_seed, sweeper->encoded,\n'
        '      SIZE_STACK (sweeper->clause), BEGIN_STACK (sweeper->clause));\n'
        '  kitten_clause (solver->kitten, SIZE_STACK (sweeper->clause),\n'
        '                 BEGIN_STACK (sweeper->clause));\n')
    replace_once(sweep,
        '          const unsigned second = POP_STACK (solver->clause);\n'
        '          const unsigned first = POP_STACK (solver->clause);\n'
        '          LOGCLS (c, "reduces to binary clause %s %s", LOGLIT (first),\n',
        '          const unsigned second = POP_STACK (solver->clause);\n'
        '          const unsigned first = POP_STACK (solver->clause);\n'
        '          kissat_event_trace_binary_collapse (solver, first, second, c);\n'
        '          LOGCLS (c, "reduces to binary clause %s %s", LOGLIT (first),\n')
    replace_once(sweep,
        '        CHECK_AND_ADD_BINARY (repr, other);\n'
        '        ADD_BINARY_TO_PROOF (repr, other);\n',
        '        kissat_event_trace_binary_rewrite (solver, lit, other, repr,\n'
        '                                           other);\n'
        '        CHECK_AND_ADD_BINARY (repr, other);\n'
        '        ADD_BINARY_TO_PROOF (repr, other);\n')
    replace_once(sweep,
        '  const unsigned start = LIT (idx);\n'
        '  if (sweeper->reprs[start] != start)\n',
        '  const unsigned start = LIT (idx);\n'
        '  sweeper->trace_seed = start;\n'
        '  sweeper->trace_query_ordinal = 0;\n'
        '  sweeper->trace_binary_ordinal = 0;\n'
        '  if (sweeper->reprs[start] != start)\n')
    replace_once(sweep,
        'static void sweep_binary (sweeper *sweeper, unsigned depth, unsigned lit,\n'
        '                          unsigned other) {\n'
        '  if (sweep_repr (sweeper, lit) != lit)\n'
        '    return;\n'
        '  if (sweep_repr (sweeper, other) != other)\n'
        '    return;\n',
        'static void sweep_binary (sweeper *sweeper, unsigned depth, unsigned lit,\n'
        '                          unsigned other) {\n'
        '  if (sweep_repr (sweeper, lit) != lit) {\n'
        '    kissat_event_trace_sweep_binary_attempt (\n'
        '        sweeper->solver, sweeper->trace_seed,\n'
        '        sweeper->trace_binary_ordinal++, lit, other, (int) depth, -1,\n'
        '        "skip-lit-repr");\n'
        '    return;\n'
        '  }\n'
        '  if (sweep_repr (sweeper, other) != other) {\n'
        '    kissat_event_trace_sweep_binary_attempt (\n'
        '        sweeper->solver, sweeper->trace_seed,\n'
        '        sweeper->trace_binary_ordinal++, lit, other, (int) depth, -1,\n'
        '        "skip-other-repr");\n'
        '    return;\n'
        '  }\n')
    replace_once(sweep,
        '  if (other_value > 0) {\n'
        '    LOGBINARY (lit, other, "skipping satisfied");\n'
        '    return;\n'
        '  }\n',
        '  if (other_value > 0) {\n'
        '    LOGBINARY (lit, other, "skipping satisfied");\n'
        '    kissat_event_trace_sweep_binary_attempt (\n'
        '        solver, sweeper->trace_seed, sweeper->trace_binary_ordinal++,\n'
        '        lit, other, (int) depth, -1, "skip-satisfied");\n'
        '    return;\n'
        '  }\n')
    replace_once(sweep,
        '  if (other_depth && other_depth < lit_depth) {\n'
        '    LOGBINARY (lit, other, "skipping depth %u copied", other_depth);\n'
        '    return;\n'
        '  }\n'
        '  assert (!other_value);\n',
        '  if (other_depth && other_depth < lit_depth) {\n'
        '    LOGBINARY (lit, other, "skipping depth %u copied", other_depth);\n'
        '    kissat_event_trace_sweep_binary_attempt (\n'
        '        solver, sweeper->trace_seed, sweeper->trace_binary_ordinal++,\n'
        '        lit, other, (int) lit_depth - 1, (int) other_depth - 1,\n'
        '        "skip-shallower");\n'
        '    return;\n'
        '  }\n'
        '  kissat_event_trace_sweep_binary_attempt (\n'
        '      solver, sweeper->trace_seed, sweeper->trace_binary_ordinal++,\n'
        '      lit, other, (int) lit_depth - 1,\n'
        '      other_depth ? (int) other_depth - 1 : -1, "add");\n'
        '  assert (!other_value);\n')
    replace_once(sweep,
        '  int res = sweep_solve (sweeper);\n'
        '  LOG ("sub-solver returns \'%d\'", res);\n',
        '  int res = sweep_solve_traced (sweeper, "initial", INVALID_LIT,\n'
        '                                INVALID_LIT);\n'
        '  LOG ("sub-solver returns \'%d\'", res);\n')
    replace_once(sweep,
        '  int res = sweep_solve (sweeper);\n'
        '  if (res == 10) {\n'
        '    LOG ("sweeping backbone candidate %s failed", LOGLIT (lit));\n',
        '  int res = sweep_solve_traced (sweeper, "backbone", lit,\n'
        '                                INVALID_LIT);\n'
        '  if (res == 10) {\n'
        '    LOG ("sweeping backbone candidate %s failed", LOGLIT (lit));\n')
    replace_once(sweep,
        '  int res = sweep_solve (sweeper);\n'
        '  if (res == 10) {\n'
        '    INC (sweep_sat_equivalences);\n',
        '  int res = sweep_solve_traced (sweeper, "equiv_fwd", lit, other);\n'
        '  if (res == 10) {\n'
        '    INC (sweep_sat_equivalences);\n')
    replace_once(sweep,
        '  res = sweep_solve (sweeper);\n'
        '  INC (sweep_solved_equivalences);\n',
        '  res = sweep_solve_traced (sweeper, "equiv_rev", lit, other);\n'
        '  INC (sweep_solved_equivalences);\n')
    replace_once(sweep,
        '  ADD (sweep_environment, SIZE_STACK (sweeper->vars));\n'
        '  kissat_extremely_verbose (solver,\n',
        '  ADD (sweep_environment, SIZE_STACK (sweeper->vars));\n'
        '  {\n'
        '    unsigned trace_ordinal = 0;\n'
        '    for (all_stack (unsigned, trace_idx, sweeper->vars))\n'
        '      kissat_event_trace_sweep_environment_var (\n'
        '          solver, sweeper->trace_seed, trace_ordinal++, trace_idx,\n'
        '          sweeper->depths[trace_idx] - 1);\n'
        '  }\n'
        '  {\n'
        '    char payload[128];\n'
        '    snprintf (payload, sizeof payload,\n'
        '              "external=%d|vars=%zu|clauses=%u",\n'
        '              kissat_export_literal (solver, LIT (idx)),\n'
        '              SIZE_STACK (sweeper->vars), sweeper->encoded);\n'
        '    kissat_event_trace (solver, "sweep_environment", payload, false);\n'
        '  }\n'
        '  kissat_extremely_verbose (solver,\n')
    replace_once(sweep,
        '  INC (sweep_variables);\n\n'
        '  LOG ("sweeping %s", LOGVAR (idx));\n',
        '  INC (sweep_variables);\n'
        '  {\n'
        '    char payload[64];\n'
        '    snprintf (payload, sizeof payload, "external=%d",\n'
        '              kissat_export_literal (solver, LIT (idx)));\n'
        '    kissat_event_trace (solver, "sweep_seed", payload, false);\n'
        '  }\n\n'
        '  LOG ("sweeping %s", LOGVAR (idx));\n')
    replace_once(sweep,
        '#ifndef QUIET\n'
        '    const char *res =\n'
        '#endif\n'
        '        sweep_variable (&sweeper, idx);\n'
        '    kissat_extremely_verbose (\n',
        '    const uint64_t trace_ticks_before = statistics->kitten_ticks;\n'
        '    const uint64_t trace_solves_before = statistics->sweep_solved;\n'
        '    const int trace_external =\n'
        '        kissat_export_literal (solver, LIT (idx));\n'
        '#ifndef QUIET\n'
        '    const char *res =\n'
        '#endif\n'
        '        sweep_variable (&sweeper, idx);\n'
        '    const uint64_t trace_ticks_after = statistics->kitten_ticks;\n'
        '    if (trace_ticks_after != trace_ticks_before) {\n'
        '      char payload[256];\n'
        '      snprintf (payload, sizeof payload,\n'
        '                "external=%d|before=%" PRIu64 "|after=%" PRIu64\n'
        '                "|delta=%" PRIu64 "|solves=%" PRIu64,\n'
        '                trace_external, trace_ticks_before, trace_ticks_after,\n'
        '                trace_ticks_after - trace_ticks_before,\n'
        '                statistics->sweep_solved - trace_solves_before);\n'
        '      kissat_event_trace (solver, "sweep_effort", payload, false);\n'
        '    }\n'
        '    kissat_extremely_verbose (\n')
    replace_once(sweep,
        '  LOG ("sweep equivalence %s = %s", LOGLIT (lit), LOGLIT (other));\n'
        '  INC (sweep_equivalences);\n',
        '  LOG ("sweep equivalence %s = %s", LOGLIT (lit), LOGLIT (other));\n'
        '  INC (sweep_equivalences);\n'
        '  {\n'
        '    char payload[128];\n'
        '    snprintf (payload, sizeof payload, "a=%d|b=%d",\n'
        '              kissat_export_literal (solver, lit),\n'
        '              kissat_export_literal (solver, other));\n'
        '    kissat_event_trace (solver, "sweep_equivalence", payload, false);\n'
        '  }\n')
    replace_once(sweep,
        '    watch *q = begin_watches;\n'
        '    const watch *p = q;\n\n'
        '    while (p != end_watches) {\n'
        '      const watch head = *q++ = *p++;\n',
        '    watch *q = begin_watches;\n'
        '    const watch *p = q;\n'
        '    size_t trace_position = 0;\n\n'
        '    while (p != end_watches) {\n'
        '      const size_t current_trace_position = trace_position++;\n'
        '      const watch head = *q++ = *p++;\n')
    replace_once(sweep,
        '        PUSH_STACK (*delayed, head.raw);\n'
        '        watch src = {.raw = head.raw};\n',
        '        kissat_event_trace_sweep_rewrite_binary (\n'
        '            solver, lit, repr, current_trace_position, other);\n'
        '        kissat_event_trace_sweep_rewrite_occurrence (\n'
        '            solver, lit, repr, current_trace_position, true, other, 0);\n'
        '        PUSH_STACK (*delayed, head.raw);\n'
        '        watch src = {.raw = head.raw};\n')
    replace_once(sweep,
        '          dst.binary.lit = other;\n'
        '          PUSH_STACK (*delayed, dst.raw);\n',
        '          dst.binary.lit = other;\n'
        '          kissat_event_trace_sweep_rewrite_occurrence (\n'
        '              solver, lit, repr, current_trace_position, true, other, 0);\n'
        '          PUSH_STACK (*delayed, dst.raw);\n')
    replace_once(sweep,
        '        if (!repr_already_watched)\n'
        '          PUSH_STACK (*delayed, head.raw);\n',
        '        if (!repr_already_watched) {\n'
        '          kissat_event_trace_sweep_rewrite_occurrence (\n'
        '              solver, lit, repr, current_trace_position, false,\n'
        '              INVALID_LIT, c);\n'
        '          PUSH_STACK (*delayed, head.raw);\n'
        '        }\n')
    replace_once(sweep,
        '  unsigned *q = BEGIN_STACK (*core);\n'
        '  const unsigned *const end_core = END_STACK (*core), *p = q;\n\n'
        '  while (p != end_core) {\n',
        '  unsigned *q = BEGIN_STACK (*core);\n'
        '  const unsigned *const end_core = END_STACK (*core), *p = q;\n'
        '  unsigned journal_ordinal = 0;\n\n'
        '  while (p != end_core) {\n')
    replace_once(sweep,
        '    while (*p != INVALID_LIT)\n'
        '      p++;\n#ifdef LOGGING\n',
        '    while (*p != INVALID_LIT)\n'
        '      p++;\n'
        '    const size_t journal_old_size = p - c;\n'
        '    char journal_literals[2048];\n'
        '    kissat_rule_journal_literals (solver, c, journal_old_size,\n'
        '                                  journal_literals,\n'
        '                                  sizeof journal_literals);\n'
        '    const uint64_t journal_pre = kissat_rule_journal_digest (solver);\n'
        '#ifdef LOGGING\n')
    replace_once(sweep,
        '    if (satisfied) {\n'
        '      q = d;\n'
        '      LOG ("not adding satisfied clause");\n'
        '      continue;\n'
        '    }\n',
        '    if (satisfied) {\n'
        '      q = d;\n'
        '      LOG ("not adding satisfied clause");\n'
        '      kissat_rule_journal_sweep_core (\n'
        '          solver, core_idx, journal_ordinal++, journal_literals,\n'
        '          journal_old_size, true, new_size, INVALID_LIT, "no-op",\n'
        '          "satisfied", journal_pre);\n'
        '      continue;\n'
        '    }\n')
    replace_once(sweep,
        '      solver->inconsistent = true;\n'
        '      CLEAR_STACK (*core);\n'
        '      return;\n',
        '      solver->inconsistent = true;\n'
        '      kissat_rule_journal_sweep_core (\n'
        '          solver, core_idx, journal_ordinal++, journal_literals,\n'
        '          journal_old_size, false, new_size, INVALID_LIT, "refuted",\n'
        '          "none", journal_pre);\n'
        '      CLEAR_STACK (*core);\n'
        '      return;\n')
    replace_once(sweep,
        '      kissat_assign_unit (solver, unit, "sweeping backbone reason");\n'
        '      INC (sweep_units);\n'
        '      continue;\n',
        '      kissat_assign_unit (solver, unit, "sweeping backbone reason");\n'
        '      INC (sweep_units);\n'
        '      kissat_event_trace_sweep_unit (solver, unit, "core");\n'
        '      kissat_rule_journal_sweep_core (\n'
        '          solver, core_idx, journal_ordinal++, journal_literals,\n'
        '          journal_old_size, false, new_size, unit, "applied", "none",\n'
        '          journal_pre);\n'
        '      continue;\n')
    replace_once(sweep,
        '          kissat_assign_unit (solver, lit, "substituted binary clause");\n'
        '          INC (sweep_units);\n',
        '          kissat_assign_unit (solver, lit, "substituted binary clause");\n'
        '          INC (sweep_units);\n'
        '          kissat_event_trace_sweep_unit (solver, lit, "binary");\n')
    replace_once(sweep,
        '          kissat_assign_unit (solver, unit, "substituted large clause");\n'
        '          INC (sweep_units);\n',
        '          kissat_assign_unit (solver, unit, "substituted large clause");\n'
        '          INC (sweep_units);\n'
        '          kissat_event_trace_sweep_unit (solver, unit, "large");\n')
    replace_once(sweep,
        '    CHECK_AND_ADD_LITS (new_size, d);\n'
        '    ADD_LITS_TO_PROOF (new_size, d);\n'
        '  }\n',
        '    CHECK_AND_ADD_LITS (new_size, d);\n'
        '    ADD_LITS_TO_PROOF (new_size, d);\n'
        '    kissat_rule_journal_sweep_core (\n'
        '        solver, core_idx, journal_ordinal++, journal_literals,\n'
        '        journal_old_size, false, new_size, INVALID_LIT, "applied",\n'
        '        "none", journal_pre);\n'
        '  }\n')


if __name__ == "__main__":
    main()

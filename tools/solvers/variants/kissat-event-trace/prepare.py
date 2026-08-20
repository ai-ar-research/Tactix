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
        '        res = kissat_analyze (solver, conflict);\n'
        '        kissat_rule_journal_rule_attempt (\n'
        '            solver, "conflict-analysis", "current-conflict",\n'
        '            "search-loop", analyze_pre, analyze_effort,\n'
        '            solver->statistics.search_ticks, 0);\n'
        '        kissat_event_trace_heap_scores (solver);\n'
        '        kissat_event_trace_detail (solver, "analysis_end",\n'
        '                                   res == 20 ? -1 : (int) solver->level);\n'
        '      }\n')
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
                 '#include <stdlib.h>\n')
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
        '  const uint64_t journal_effort = solver->statistics.probing_ticks; \\\n'
        '  kissat_event_trace (solver, NAME "_begin", "-", true); \\\n'
        '  CALL; \\\n'
        '  kissat_rule_journal_rule_attempt ( \\\n'
        '      solver, NAME, "canonical", "probe-pipeline", journal_pre, \\\n'
        '      journal_effort, solver->statistics.probing_ticks, 0); \\\n'
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
        '      if (f->factor & 1)\n'
        '        update_candidate (factoring, lit);\n'
        '      if (f->factor & 2)\n'
        '        update_candidate (factoring, not_lit);\n'
        '    }\n'
        '  }\n')
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

    deduce = dest / "src" / "deduce.c"
    replace_once(deduce, '#include "deduce.h"\n',
                 '#include "deduce.h"\n#include "eventtrace.h"\n')
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
        '  unsigned trace_seed, trace_query_ordinal;\n'
        '  unsigned save;\n')
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
        '  const unsigned start = LIT (idx);\n'
        '  if (sweeper->reprs[start] != start)\n',
        '  const unsigned start = LIT (idx);\n'
        '  sweeper->trace_seed = start;\n'
        '  sweeper->trace_query_ordinal = 0;\n'
        '  if (sweeper->reprs[start] != start)\n')
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
        '        PUSH_STACK (*delayed, head.raw);\n'
        '        watch src = {.raw = head.raw};\n')
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

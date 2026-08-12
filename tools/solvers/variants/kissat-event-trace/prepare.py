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
        '    kissat_event_trace (solver, "lucky_early_begin", "-", true);\n'
        '    res = kissat_lucky (solver);\n'
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
        '    kissat_event_trace (solver, "lucky_late_begin", "-", true);\n'
        '    res = kissat_lucky (solver);\n'
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
        '      const size_t trace_trail_begin = SIZE_ARRAY (solver->trail);\n'
        '      clause *conflict = kissat_search_propagate (solver);\n'
        '      kissat_event_trace_implications (solver, trace_trail_begin);\n'
        '      if (conflict) {\n'
        '        kissat_event_trace_conflict (solver, conflict);\n'
        '        kissat_event_trace_detail (solver, "analysis_begin",\n'
        '                                   (int) solver->level);\n'
        '        res = kissat_analyze (solver, conflict);\n'
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
            new = (f'        kissat_event_trace (solver, "{event}_begin", "-", true);\n'
                   f"        {call};\n"
                   f'        kissat_event_trace (solver, "{event}_end", "-", true);')
        else:
            old = f"        res = {call};"
            new = (f'        kissat_event_trace (solver, "{event}_begin", "-", true);\n'
                   f"        res = {call};\n"
                   f'        kissat_event_trace (solver, "{event}_end", "-", true);')
        if text.count(old) != 1:
            raise SystemExit(f"{search}: expected one wrapper anchor {old!r}")
        text = text.replace(old, "{\n" + new + "\n      }")
    search.write_text(text)
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

    decide = dest / "src" / "decide.c"
    replace_once(decide, '#include "decide.h"\n',
                 '#include "decide.h"\n#include "eventtrace.h"\n')
    replace_once(decide,
        '  kissat_assign_decision (solver, lit);\n  STOP (decide);\n',
        '  kissat_assign_decision (solver, lit);\n'
        '  kissat_event_trace_literal (solver, "decision", lit);\n'
        '  STOP (decide);\n')

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
                 '#include "terminate.h"\n#include "rulejournal.h"\n')
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
        '      kissat_rule_journal_sweep_core (\n'
        '          solver, core_idx, journal_ordinal++, journal_literals,\n'
        '          journal_old_size, false, new_size, unit, "applied", "none",\n'
        '          journal_pre);\n'
        '      continue;\n')
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

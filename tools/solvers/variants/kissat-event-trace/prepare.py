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

    search = dest / "src" / "search.c"
    replace_once(search, '#include "eliminate.h"\n',
                 '#include "eliminate.h"\n#include "eventtrace.h"\n')
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
        '      analyze_reason_side_literals (solver);\n'
        '      kissat_learn_clause (solver);\n',
        '      analyze_reason_side_literals (solver);\n'
        '      kissat_event_trace_learned (solver);\n'
        '      kissat_learn_clause (solver);\n')


if __name__ == "__main__":
    main()

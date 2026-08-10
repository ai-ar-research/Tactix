#!/usr/bin/env python3
"""Locate the first semantic divergence between solver event traces.

``exact`` is the strict Tactix-arm lockstep check.  ``semantic`` is the
cross-solver diagnostic: it compares the common event vocabulary, reports the
first lifecycle/state/trajectory mismatch with bounded context, and can write
a machine-readable divergence record.  Identical prefixes count as a match
only when both traces reached a terminal record without truncation.  A
divergence is closed only when the causal-frontier record carries its cause,
strength judgment, evidence, reproducer, owner, and reopening condition.
"""
import argparse
import csv
import json
import sys


FIELDS = [
    "solver", "seq", "event", "conflicts", "decisions", "propagations",
    "level", "mode", "active", "unassigned", "trail", "queue", "heap",
    "retired_queue", "retired_heap", "irred", "red", "binary", "digest",
    "search_external", "top32", "payload",
]
DETAIL_EVENTS = {
    "decision", "watch", "imply", "conflict", "analysis_begin",
    "learned_raw", "learned_sorted", "learned_shrunk", "learned",
    "analysis_end",
}
AUXILIARY_EVENTS = {
    "checkpoint", "retier", "reduce_tiers", "mode_clock", "analyzed",
    "randec_draw", "phase_pick", "target_save", "phase_save",
    "slot_boundary", "sweep_seed", "sweep_environment",
    "sweep_equivalence", "sweep_unit",
}
# Detailed records normally carry zeroes in the expensive snapshot fields.
# Analysis-end is the deliberate exception: both overlays snapshot ranking
# state there so a latent bump-order mismatch is localized to its conflict.
SNAPSHOT_DETAIL_EVENTS = {"analysis_end"}
NON_MACRO_EVENTS = DETAIL_EVENTS | AUXILIARY_EVENTS
STATE_FIELDS = (
    "conflicts", "decisions", "level", "mode", "active", "unassigned",
    "trail", "queue", "heap", "irred", "red", "binary", "top32",
)


def load(path):
    rows = []
    truncated = False
    with open(path, encoding="utf-8", errors="replace") as src:
        for raw in src:
            if raw.startswith("# truncated"):
                truncated = True
            if not raw.strip() or raw.startswith("#"):
                continue
            vals = next(csv.reader([raw], delimiter="\t"))
            if len(vals) != len(FIELDS):
                print(f"warning: {path}: skipped partial record with "
                      f"{len(vals)}/{len(FIELDS)} fields", file=sys.stderr)
                continue
            rows.append(dict(zip(FIELDS, vals)))
    terminal = bool(rows and rows[-1]["event"] == "terminate")
    return {
        "rows": rows,
        "truncated": truncated,
        "terminal": terminal,
        "terminal_payload": rows[-1]["payload"] if terminal else "",
    }


def brief(row):
    keys = ("seq", "event", "conflicts", "decisions", "level", "mode",
            "active", "queue", "heap", "retired_queue", "retired_heap",
            "irred", "red", "binary", "digest", "search_external", "top32",
            "payload")
    return " ".join(f"{k}={row[k]}" for k in keys)


def context(rows, point, radius):
    if point is None:
        return []
    lo = max(0, point - radius)
    hi = min(len(rows), point + radius + 1)
    return [{"index": i, **rows[i]} for i in range(lo, hi)]


def print_context(label, rows, point, radius):
    if point is None:
        return
    print(f"{label} context")
    for i in range(max(0, point - radius),
                   min(len(rows), point + radius + 1)):
        marker = ">" if i == point else " "
        print(f" {marker} [{i}] {brief(rows[i])}")


def row_delta(left, right, fields):
    return {key: [left[key], right[key]] for key in fields
            if left[key] != right[key]}


def exact(trace_a, trace_b, radius):
    a, b = trace_a["rows"], trace_b["rows"]
    ignored = {"solver", "seq"}
    for i, (left, right) in enumerate(zip(a, b)):
        delta = row_delta(left, right,
                          [k for k in FIELDS if k not in ignored])
        if delta:
            print(f"first divergence at comparable record {i}")
            print_context("A", a, i, radius)
            print_context("B", b, i, radius)
            for key, values in delta.items():
                print(f"  {key}: {values[0]} -> {values[1]}")
            return 1
    if len(a) != len(b):
        point = min(len(a), len(b))
        print(f"common prefix {point}; lengths {len(a)} vs {len(b)}")
        return 1
    complete = (trace_a["terminal"] and trace_b["terminal"] and
                not trace_a["truncated"] and not trace_b["truncated"])
    if not complete:
        print(f"identical prefix ({len(a)} records), but at least one trace "
              "is non-terminal or truncated")
        return 1
    print(f"identical complete semantic trace ({len(a)} records)")
    return 0


def first_stream_difference(a, b, fields):
    for i, (left, right) in enumerate(zip(a, b)):
        delta = row_delta(left, right, fields)
        if delta:
            return i, i, delta
    if len(a) != len(b):
        return (len(a) if len(a) < len(b) else None,
                len(b) if len(b) < len(a) else None,
                {"length": [str(len(a)), str(len(b))]})
    return None


def primary_semantic_difference(a, b):
    """Compare records in execution order using only portable semantics."""
    for i, (left, right) in enumerate(zip(a, b)):
        if left["event"] != right["event"]:
            return "event", i, i, {"event": [left["event"], right["event"]]}
        event = left["event"]
        if event in DETAIL_EVENTS:
            fields = ("payload", "conflicts", "decisions", "level")
            if event in SNAPSHOT_DETAIL_EVENTS:
                fields += STATE_FIELDS
            delta = row_delta(left, right, fields)
            if delta:
                return "trajectory", i, i, delta
        else:
            delta = row_delta(left, right, STATE_FIELDS + ("payload",))
            if delta:
                return "state", i, i, delta
    if len(a) != len(b):
        ia = len(a) if len(a) < len(b) else None
        ib = len(b) if len(b) < len(a) else None
        return "length", ia, ib, {"length": [str(len(a)), str(len(b))]}
    return None


def semantic(trace_a, trace_b, radius, metadata):
    a, b = trace_a["rows"], trace_b["rows"]
    # These records expose implementation-specific observation boundaries or
    # raw diagnostic currencies.  They remain available to exact comparison
    # and dedicated payload checks, but must not hide the first portable
    # lifecycle/trajectory divergence in cross-solver semantic mode.
    portable_a = [row for row in a if row["event"] not in AUXILIARY_EVENTS]
    portable_b = [row for row in b if row["event"] not in AUXILIARY_EVENTS]
    macro_a = [row for row in a if row["event"] not in NON_MACRO_EVENTS]
    macro_b = [row for row in b if row["event"] not in NON_MACRO_EVENTS]
    detail_a = [row for row in a if row["event"] in DETAIL_EVENTS]
    detail_b = [row for row in b if row["event"] in DETAIL_EVENTS]

    primary = primary_semantic_difference(portable_a, portable_b)
    macro_order = first_stream_difference(macro_a, macro_b, ("event",))
    macro_state = first_stream_difference(macro_a, macro_b,
                                          ("event",) + STATE_FIELDS)
    trajectory = None
    for i, (left, right) in enumerate(zip(detail_a, detail_b)):
        fields = ("event", "payload", "conflicts", "decisions", "level")
        if left["event"] == right["event"] and \
                left["event"] in SNAPSHOT_DETAIL_EVENTS:
            fields += STATE_FIELDS
        delta = row_delta(left, right, fields)
        if delta:
            trajectory = i, i, delta
            break
    if trajectory is None and len(detail_a) != len(detail_b):
        trajectory = (len(detail_a) if len(detail_a) < len(detail_b) else None,
                      len(detail_b) if len(detail_b) < len(detail_a) else None,
                      {"length": [str(len(detail_a)), str(len(detail_b))]})

    complete_match = bool(
        not primary and trace_a["terminal"] and trace_b["terminal"] and
        not trace_a["truncated"] and not trace_b["truncated"])
    closure_dispositions = {
        "strictly-better", "deliberate-choice", "repaired", "downstream"
    }
    required_closure = (
        metadata["status"] == "closed" and
        metadata["disposition"] in closure_dispositions and
        metadata["strength"] != "unknown" and
        metadata["cause"] and metadata["evidence"] and
        metadata["reproducer"] and metadata["owner"] and
        metadata["reopening_condition"])
    localized = bool(primary and metadata["status"] in
                     ("localized", "actionable", "closed") and
                     metadata["cause"] and metadata["evidence"])
    closed = bool(complete_match or (primary and required_closure))

    effective_status = "closed" if complete_match else metadata["status"]
    report = {
        "schema": "tactix-kissat-divergence-v2",
        "trace_a": metadata["trace_a"],
        "trace_b": metadata["trace_b"],
        "fixture": metadata["fixture"],
        "tactix_commit": metadata["tactix_commit"],
        "kissat_commit": metadata["kissat_commit"],
        "status": effective_status,
        "disposition": metadata["disposition"],
        "strength": metadata["strength"],
        "cause": metadata["cause"],
        "evidence": metadata["evidence"],
        "reproducer": metadata["reproducer"],
        "owner": metadata["owner"],
        "reopening_condition": metadata["reopening_condition"],
        "common_anchor": metadata["common_anchor"],
        "trace_status": {
            "a": {key: trace_a[key] for key in
                  ("terminal", "truncated", "terminal_payload")},
            "b": {key: trace_b[key] for key in
                  ("terminal", "truncated", "terminal_payload")},
        },
        "complete_match": complete_match,
        "localized": localized,
        "closed": closed,
        # Compatibility for existing report readers.  In v2 an explanation is
        # not complete until the causal frontier is actually closed.
        "explained": closed,
    }

    def finding(name, result, rows_a, rows_b):
        if not result:
            report[name] = None
            print(f"{name.replace('_', ' ')}: no divergence")
            return
        if len(result) == 4:
            kind, ia, ib, delta = result
        else:
            kind, (ia, ib, delta) = name, result
        item = {
            "kind": kind,
            "a_index": ia,
            "b_index": ib,
            "delta": delta,
            "a_context": context(rows_a, ia, radius),
            "b_context": context(rows_b, ib, radius),
        }
        report[name] = item
        print(f"{name.replace('_', ' ')}: {kind} divergence "
              f"at A={ia} B={ib}")
        for key, values in delta.items():
            print(f"  {key}: {values[0]} -> {values[1]}")
        print_context("A", rows_a, ia, radius)
        print_context("B", rows_b, ib, radius)

    finding("first_divergence", primary, portable_a, portable_b)
    finding("macro_order", macro_order, macro_a, macro_b)
    finding("macro_state", macro_state, macro_a, macro_b)
    finding("trajectory", trajectory, detail_a, detail_b)

    if primary:
        print(f"causal frontier: {metadata['status']} "
              f"({'closed' if closed else 'OPEN'})")
    else:
        if complete_match:
            print("portable semantic streams are identical and complete")
        else:
            print("portable semantic prefixes are identical, but the match "
                  "is incomplete or truncated")
    return report


def anchor(rows, radius):
    points = [i for i, row in enumerate(rows) if row["event"] == "reorder_begin"]
    if not points:
        print("no reorder_begin anchor")
        return
    for number, point in enumerate(points, 1):
        print(f"reorder anchor {number}")
        print_context("trace", rows, point, radius)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace_a")
    ap.add_argument("trace_b", nargs="?")
    ap.add_argument("--mode", choices=("exact", "semantic", "anchor"),
                    default="exact")
    ap.add_argument("--radius", type=int, default=2)
    ap.add_argument("--json", help="write a divergence-v2 JSON record")
    ap.add_argument("--fixture", default="")
    ap.add_argument("--tactix-commit", default="")
    ap.add_argument("--kissat-commit", default="")
    ap.add_argument("--status",
                    choices=("unknown", "localized", "actionable", "closed"),
                    default="unknown")
    ap.add_argument("--disposition",
                    choices=("unknown", "strictly-better", "deliberate-choice",
                             "repair-required", "repaired", "downstream"),
                    default="unknown")
    ap.add_argument("--strength",
                    choices=("unknown", "stronger", "equivalent", "weaker",
                             "incomparable", "downstream"),
                    default="unknown")
    ap.add_argument("--cause", default="")
    ap.add_argument("--evidence", action="append", default=[])
    ap.add_argument("--reproducer", default="")
    ap.add_argument("--owner", default="")
    ap.add_argument("--reopening-condition", default="")
    ap.add_argument("--common-anchor", default="")
    ap.add_argument("--require-closed", action="store_true")
    ap.add_argument("--require-explained", action="store_true",
                    help="deprecated alias for --require-closed")
    args = ap.parse_args()
    a = load(args.trace_a)
    if args.mode == "anchor":
        print(f"== {args.trace_a} ==")
        anchor(a["rows"], args.radius)
        if args.trace_b:
            print(f"== {args.trace_b} ==")
            anchor(load(args.trace_b)["rows"], args.radius)
        return
    if not args.trace_b:
        ap.error(f"{args.mode} mode requires two traces")
    b = load(args.trace_b)
    if args.mode == "exact":
        raise SystemExit(exact(a, b, args.radius))

    metadata = vars(args)
    report = semantic(a, b, args.radius, metadata)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as out:
            json.dump(report, out, indent=2, sort_keys=True)
            out.write("\n")
    require_closed = args.require_closed or args.require_explained
    if require_closed and not report["closed"]:
        print("error: trace pair is not closed (needs a complete terminal "
              "match or a fully dispositioned causal frontier)", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(0 if report["complete_match"] else 1)


if __name__ == "__main__":
    main()

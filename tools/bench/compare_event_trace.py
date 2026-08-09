#!/usr/bin/env python3
"""Locate the first semantic divergence between solver event traces.

``exact`` is the strict Tactix-arm lockstep check.  ``semantic`` is the
cross-solver diagnostic: it compares the common event vocabulary, reports the
first lifecycle/state/trajectory mismatch with bounded context, and can write
a machine-readable divergence record.  A semantic divergence is considered
explained only when its disposition, cause, and source/evidence handles are
all supplied; ``--require-explained`` turns that contract into a gate.
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
DETAIL_EVENTS = {"decision", "conflict", "analysis_begin", "analysis_end"}
NON_MACRO_EVENTS = DETAIL_EVENTS | {"checkpoint"}
STATE_FIELDS = (
    "conflicts", "decisions", "level", "mode", "active", "unassigned",
    "trail", "queue", "heap", "irred", "red", "binary", "top32",
)


def load(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as src:
        for raw in src:
            if not raw.strip() or raw.startswith("#"):
                continue
            vals = next(csv.reader([raw], delimiter="\t"))
            if len(vals) != len(FIELDS):
                print(f"warning: {path}: skipped partial record with "
                      f"{len(vals)}/{len(FIELDS)} fields", file=sys.stderr)
                continue
            rows.append(dict(zip(FIELDS, vals)))
    return rows


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


def exact(a, b, radius):
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
    print(f"identical semantic trace ({len(a)} records)")
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
            delta = row_delta(left, right,
                              ("payload", "conflicts", "decisions", "level"))
            if delta:
                return "trajectory", i, i, delta
        elif event != "checkpoint":
            delta = row_delta(left, right, STATE_FIELDS)
            if delta:
                return "state", i, i, delta
    if len(a) != len(b):
        ia = len(a) if len(a) < len(b) else None
        ib = len(b) if len(b) < len(a) else None
        return "length", ia, ib, {"length": [str(len(a)), str(len(b))]}
    return None


def semantic(a, b, radius, metadata):
    macro_a = [row for row in a if row["event"] not in NON_MACRO_EVENTS]
    macro_b = [row for row in b if row["event"] not in NON_MACRO_EVENTS]
    detail_a = [row for row in a if row["event"] in DETAIL_EVENTS]
    detail_b = [row for row in b if row["event"] in DETAIL_EVENTS]

    primary = primary_semantic_difference(a, b)
    macro_order = first_stream_difference(macro_a, macro_b, ("event",))
    macro_state = first_stream_difference(macro_a, macro_b,
                                          ("event",) + STATE_FIELDS)
    trajectory = first_stream_difference(
        detail_a, detail_b,
        ("event", "payload", "conflicts", "decisions", "level"))

    report = {
        "schema": "tactix-kissat-divergence-v1",
        "trace_a": metadata["trace_a"],
        "trace_b": metadata["trace_b"],
        "fixture": metadata["fixture"],
        "tactix_commit": metadata["tactix_commit"],
        "kissat_commit": metadata["kissat_commit"],
        "disposition": metadata["disposition"],
        "cause": metadata["cause"],
        "evidence": metadata["evidence"],
        "reproducer": metadata["reproducer"],
        "explained": bool(primary and metadata["cause"] and
                           metadata["evidence"] and
                           metadata["disposition"] != "unknown"),
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

    finding("first_divergence", primary, a, b)
    finding("macro_order", macro_order, macro_a, macro_b)
    finding("macro_state", macro_state, macro_a, macro_b)
    finding("trajectory", trajectory, detail_a, detail_b)

    if primary:
        state = "explained" if report["explained"] else "UNEXPLAINED"
        print(f"disposition: {state}")
    else:
        report["explained"] = True
        print("portable semantic streams are identical")
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
    ap.add_argument("--json", help="write a divergence-v1 JSON record")
    ap.add_argument("--fixture", default="")
    ap.add_argument("--tactix-commit", default="")
    ap.add_argument("--kissat-commit", default="")
    ap.add_argument("--disposition",
                    choices=("unknown", "intentional", "accepted", "actionable"),
                    default="unknown")
    ap.add_argument("--cause", default="")
    ap.add_argument("--evidence", action="append", default=[])
    ap.add_argument("--reproducer", default="")
    ap.add_argument("--require-explained", action="store_true")
    args = ap.parse_args()
    a = load(args.trace_a)
    if args.mode == "anchor":
        print(f"== {args.trace_a} ==")
        anchor(a, args.radius)
        if args.trace_b:
            print(f"== {args.trace_b} ==")
            anchor(load(args.trace_b), args.radius)
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
    if report["first_divergence"] and args.require_explained and not report["explained"]:
        print("error: first divergence lacks disposition, cause, or evidence",
              file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(1 if report["first_divergence"] else 0)


if __name__ == "__main__":
    main()

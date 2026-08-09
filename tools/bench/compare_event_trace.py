#!/usr/bin/env python3
"""Compare version-1 semantic event traces.

Exact mode is intended for two Tactix arms and reports the first record that
differs after ignoring the solver and sequence columns.  Anchor mode prints the
Reorder windows from traces produced by different solvers; their preprocessing
streams are deliberately not treated as lock-step comparable.
"""
import argparse
import csv


FIELDS = [
    "solver", "seq", "event", "conflicts", "decisions", "propagations",
    "level", "mode", "active", "unassigned", "trail", "queue", "heap",
    "retired_queue", "retired_heap", "irred", "red", "binary", "digest",
    "search_external", "top32", "payload",
]


def load(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as src:
        for raw in src:
            if not raw.strip() or raw.startswith("#"):
                continue
            vals = next(csv.reader([raw], delimiter="\t"))
            if len(vals) != len(FIELDS):
                raise SystemExit(f"{path}: expected {len(FIELDS)} fields, got {len(vals)}")
            rows.append(dict(zip(FIELDS, vals)))
    return rows


def brief(row):
    keys = ("seq", "event", "conflicts", "decisions", "level", "mode",
            "active", "queue", "heap", "retired_queue", "retired_heap",
            "digest", "search_external", "top32", "payload")
    return " ".join(f"{k}={row[k]}" for k in keys)


def exact(a, b):
    ignored = {"solver", "seq"}
    for i, (left, right) in enumerate(zip(a, b)):
        delta = {k: (left[k], right[k]) for k in FIELDS
                 if k not in ignored and left[k] != right[k]}
        if delta:
            print(f"first divergence at comparable record {i}")
            print("A", brief(left))
            print("B", brief(right))
            for key, values in delta.items():
                print(f"  {key}: {values[0]} -> {values[1]}")
            return 1
    if len(a) != len(b):
        print(f"common prefix {min(len(a), len(b))}; lengths {len(a)} vs {len(b)}")
        return 1
    print(f"identical semantic trace ({len(a)} records)")
    return 0


def anchor(rows, radius):
    points = [i for i, row in enumerate(rows) if row["event"] == "reorder_begin"]
    if not points:
        print("no reorder_begin anchor")
        return
    for number, point in enumerate(points, 1):
        print(f"reorder anchor {number}")
        for row in rows[max(0, point-radius):point+radius+1]:
            print(" ", brief(row))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace_a")
    ap.add_argument("trace_b", nargs="?")
    ap.add_argument("--mode", choices=("exact", "anchor"), default="exact")
    ap.add_argument("--radius", type=int, default=4)
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
        ap.error("exact mode requires two traces")
    raise SystemExit(exact(a, load(args.trace_b)))


if __name__ == "__main__":
    main()

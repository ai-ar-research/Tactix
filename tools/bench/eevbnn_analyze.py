#!/usr/bin/env python3
"""Measure the EEVBNN CNF shape used by the specialized public policy."""

import argparse
import collections
import gzip
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORPUS = os.path.join(ROOT, "corpus", "eevbnn")


def open_text(path):
    return gzip.open(path, "rt", encoding="ascii") if path.endswith(".gz") \
        else open(path, encoding="ascii")


def analyze(path):
    variables = clauses = input_variables = input_extent = 0
    widths = collections.Counter()
    regions = collections.Counter()
    positive = negative = literals = 0
    monotone_input_binaries = 0
    with open_text(path) as source:
        for line in source:
            if line.startswith("p "):
                _, kind, v, c = line.split()
                if kind != "cnf":
                    raise ValueError(f"not CNF: {path}")
                variables, clauses = int(v), int(c)
            elif line.startswith("c vname "):
                fields = line.split(maxsplit=3)
                if len(fields) == 4 and fields[3].startswith("inp_"):
                    input_variables += 1
                    input_extent = max(input_extent, int(fields[2]))
            elif line and line[0] not in "cp\n":
                values = [int(value) for value in line.split()]
                if not values or values[-1] != 0:
                    raise ValueError(f"unterminated clause: {path}")
                clause = values[:-1]
                widths[str(len(clause))] += 1
                literals += len(clause)
                positive += sum(value > 0 for value in clause)
                negative += sum(value < 0 for value in clause)
                inside = sum(abs(value) <= input_extent for value in clause)
                regions["input-only" if inside == len(clause)
                        else "aux-only" if inside == 0 else "mixed"] += 1
                if len(clause) == 2 and inside == 2 and clause[0] < 0 < clause[1]:
                    monotone_input_binaries += 1
    if sum(widths.values()) != clauses:
        raise ValueError(f"header clause mismatch: {path}")
    return {
        "file": os.path.relpath(path, ROOT),
        "variables": variables,
        "clauses": clauses,
        "clause_variable_ratio": round(clauses / variables, 6),
        "input_named_variables": input_variables,
        "input_extent": input_extent,
        "input_fraction": round(input_variables / variables, 8),
        "clause_widths": dict(sorted(widths.items(), key=lambda item: int(item[0]))),
        "clause_regions": dict(regions),
        "literals": literals,
        "positive_literals": positive,
        "negative_literals": negative,
        "monotone_input_binaries": monotone_input_binaries,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=os.path.join(CORPUS, "structure.json"))
    args = parser.parse_args()
    with open(os.path.join(CORPUS, "manifest.json"), encoding="utf-8") as source:
        rows = json.load(source)["instances"]
    selected = [row for row in rows if row["family"] == "d"]
    results = []
    for index, row in enumerate(selected, 1):
        basename = os.path.basename(row["file"])[:-3]
        plain = os.path.join(CORPUS, "plain", basename)
        path = plain if os.path.isfile(plain) else os.path.join(CORPUS, row["file"])
        print(f"[{index}/{len(selected)}] {basename}", flush=True)
        result = analyze(path)
        result["rank"] = row["rank"]
        result["split"] = "validation" if row["rank"] % 5 in (2, 3) else "train"
        results.append(result)
    payload = {
        "purpose": "structural admission evidence for examples/eevbnn_policy.c",
        "instances": results,
    }
    with open(args.output, "w", encoding="utf-8") as target:
        json.dump(payload, target, indent=2, sort_keys=True)
        target.write("\n")
    print(args.output)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Reproduce the Rank-263 root affine-closure and source-cap diagnosis."""

import argparse
import hashlib
import json
import lzma
from collections import defaultdict


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def popcount(value):
    # The cluster's system Python predates int.bit_count().
    return bin(value).count("1")


def read_cnf(path):
    compressed = open(path, "rb").read()
    plain = lzma.decompress(compressed)
    clauses = []
    variables = None
    declared_clauses = None
    for raw in plain.decode("ascii").splitlines():
        if not raw or raw.startswith("c"):
            continue
        if raw.startswith("p"):
            _, kind, variables, declared_clauses = raw.split()
            assert kind == "cnf"
            variables = int(variables)
            declared_clauses = int(declared_clauses)
            continue
        clause = [int(item) for item in raw.split()]
        assert clause[-1] == 0
        clauses.append(clause[:-1])
    assert variables is not None and declared_clauses == len(clauses)
    return clauses, variables, compressed, plain


def affine_partition(clauses):
    groups = defaultdict(list)
    unit_count = 0
    for clause in clauses:
        if len(clause) == 1:
            unit_count += 1
        elif len(clause) == 3 and len(set(map(abs, clause))) == 3:
            groups[tuple(sorted(map(abs, clause)))].append(clause)
        else:
            raise AssertionError("non-affine-shaped clause: %r" % (clause,))

    equations = []
    for variables, group in sorted(groups.items()):
        assert len(group) == 4
        forbidden_parities = set()
        falsifying_assignments = set()
        for clause in group:
            assignment = tuple(int(literal < 0) for literal in clause)
            by_variable = dict((abs(literal), int(literal < 0)) for literal in clause)
            canonical = tuple(by_variable[variable] for variable in variables)
            falsifying_assignments.add(canonical)
            forbidden_parities.add(sum(assignment) & 1)
        assert len(falsifying_assignments) == 4
        assert len(forbidden_parities) == 1
        equations.append((variables, 1 ^ next(iter(forbidden_parities))))
    assert 4 * len(equations) + unit_count == len(clauses)
    return equations, unit_count


def unit_propagate(clauses):
    assignments = {}
    passes = 0
    while True:
        passes += 1
        discovered = {}
        for clause in clauses:
            if any(abs(lit) in assignments and assignments[abs(lit)] == (lit > 0)
                   for lit in clause):
                continue
            unset = [lit for lit in clause if abs(lit) not in assignments]
            assert unset, "Boolean unit propagation found a root conflict"
            if len(unset) == 1:
                variable = abs(unset[0])
                value = unset[0] > 0
                assert variable not in discovered or discovered[variable] == value
                discovered[variable] = value
        delta = dict((var, value) for var, value in discovered.items()
                     if var not in assignments)
        if not delta:
            return assignments, passes
        assignments.update(delta)


def read_session(path):
    with open(path) as stream:
        header = json.loads(next(stream))
        session = json.loads(next(stream))
    assert header["kind"] == "session"
    assert session["kind"] == "affine-matrix-session"
    assert session["admission"] == "complete"
    return header, session


def condition_rows(rows, assignments):
    conditioned = []
    for row in rows:
        variables = 0
        rhs = int(row["rhs"])
        for variable in row["variables"]:
            if variable in assignments:
                rhs ^= int(assignments[variable])
            else:
                variables ^= 1 << variable
        sources = 0
        for source in row["sources"]:
            sources ^= 1 << source
        conditioned.append((variables, rhs, sources))
    return conditioned


def eliminate(rows, maximum_width=None, maximum_sources=None):
    basis = {}
    dropped = []
    maximum_seen_width = 0
    maximum_seen_sources = 0
    reductions = 0
    contradiction = None
    for ordinal, original in enumerate(rows):
        variables, rhs, sources = original
        discard = False
        while variables:
            pivot = (variables & -variables).bit_length() - 1
            if pivot not in basis:
                break
            other_variables, other_rhs, other_sources = basis[pivot]
            variables ^= other_variables
            rhs ^= other_rhs
            sources ^= other_sources
            reductions += 1
            width = popcount(variables)
            source_count = popcount(sources)
            maximum_seen_width = max(maximum_seen_width, width)
            maximum_seen_sources = max(maximum_seen_sources, source_count)
            if ((maximum_width is not None and width > maximum_width) or
                    (maximum_sources is not None and source_count > maximum_sources)):
                dropped.append({
                    "input_row": ordinal,
                    "intermediate_width": width,
                    "intermediate_sources": source_count,
                })
                discard = True
                break
        if discard:
            continue
        width = popcount(variables)
        source_count = popcount(sources)
        maximum_seen_width = max(maximum_seen_width, width)
        maximum_seen_sources = max(maximum_seen_sources, source_count)
        if not variables:
            if rhs:
                contradiction = {
                    "input_row": ordinal,
                    "source_count": source_count,
                }
                break
            continue
        pivot = (variables & -variables).bit_length() - 1
        basis[pivot] = (variables, rhs, sources)
    return {
        "basis_rows": len(basis),
        "row_reductions": reductions,
        "maximum_intermediate_width": maximum_seen_width,
        "maximum_intermediate_sources": maximum_seen_sources,
        "dropped_rows": len(dropped),
        "first_dropped_rows": dropped[:10],
        "contradiction": contradiction,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cnf_xz")
    parser.add_argument("provenance")
    parser.add_argument("--output")
    args = parser.parse_args()

    clauses, variable_count, compressed, plain = read_cnf(args.cnf_xz)
    equations, unit_count = affine_partition(clauses)
    assignments, propagation_passes = unit_propagate(clauses)
    header, session = read_session(args.provenance)
    assert header["input_file_sha256"] == sha256_bytes(plain)
    rows = session["rows"]
    conditioned = condition_rows(rows, assignments)

    result = {
        "input": {
            "compressed_sha256": sha256_bytes(compressed),
            "decompressed_sha256": sha256_bytes(plain),
            "variables": variable_count,
            "clauses": len(clauses),
            "ternary_xor_equations": len(equations),
            "ternary_xor_clauses": 4 * len(equations),
            "unit_equations": unit_count,
            "all_clauses_accounted_for_as_affine": True,
        },
        "boolean_root_propagation": {
            "assignments": len(assignments),
            "passes_including_final_fixed_point_scan": propagation_passes,
            "conflict": False,
        },
        "provenance": {
            "sha256": hashlib.sha256(open(args.provenance, "rb").read()).hexdigest(),
            "admission": session["admission"],
            "admitted_sources": len(session["admitted_sources"]),
            "basis_rows_before_root_conditioning": len(rows),
            "basis_nonzeros_before_root_conditioning": sum(
                len(row["variables"]) for row in rows),
            "admission_work": session["work"],
        },
        "unbounded_root_elimination": eliminate(conditioned),
        "current_cap_root_elimination": eliminate(
            conditioned, maximum_width=68, maximum_sources=160),
        "interpretation": {
            "source_count_is_minimal": False,
            "note": "Counts follow this deterministic admitted-basis order; 670 is not a minimum-support claim.",
        },
    }
    encoded = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        with open(args.output, "w") as output:
            output.write(encoded)
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()

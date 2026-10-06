#!/usr/bin/env python3
"""The competition CLI owns the persistent matrix session for the whole solve."""

import itertools
import pathlib
import re
import subprocess
import sys
import tempfile


def xor_clauses(variables, rhs):
    clauses = []
    for values in itertools.product((False, True), repeat=len(variables)):
        if (sum(values) & 1) == rhs:
            continue
        clauses.append([(-variable if value else variable)
                        for variable, value in zip(variables, values)])
    return clauses


def main():
    solver = pathlib.Path(sys.argv[1])
    clauses = xor_clauses((1, 3, 4), 0)
    clauses += xor_clauses((2, 3, 4, 5), 0)
    profile = (
        "sequence-repeat=once:1 "
        "step=1:AffineMatrixInitialize:3:once:1:0:1:post(0):"
        "rulework(2,10000000):material(0,0):"
        "affine-consequence2(12000,30000,1000000,2500,10000,10000000,"
        "8,68,160,4,1000000,1000000,10000000,10000000,68,160,10000,"
        "10000,3,100000,10000,1000000):instances[]:grids[]:sr[]\n"
    )
    with tempfile.TemporaryDirectory(prefix="tactix-affine-cli-") as raw:
        root = pathlib.Path(raw)
        cnf = root / "input.cnf"
        actions = root / "actions.txt"
        proof = root / "proof.dsr"
        provenance = root / "provenance.jsonl"
        cnf.write_text(
            f"p cnf 5 {len(clauses)}\n" +
            "".join(" ".join(map(str, clause)) + " 0\n" for clause in clauses))
        actions.write_text(profile)
        result = subprocess.run(
            [str(solver), "--dsr", "--controller-policy=2",
             f"--controller-actions={actions}",
             "--controller-search-configurations=vmtf/adaptive-glue/focused-rotation",
             f"--structural-provenance={provenance}", str(cnf), str(proof)],
            text=True, capture_output=True, timeout=20)
        assert result.returncode == 10, (result.returncode, result.stdout,
                                        result.stderr)
        match = re.search(
            r"^c controller-rule AffineMatrixInitialize requests=(\d+) "
            r"applications=(\d+) effective=(\d+)", result.stdout, re.M)
        assert match and tuple(map(int, match.groups())) == (1, 1, 1), result.stdout
        assert provenance.read_text().startswith('{"kind":"session"')


if __name__ == "__main__":
    main()

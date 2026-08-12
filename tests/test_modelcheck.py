#!/usr/bin/env python3
"""File-scale SAT model oracle: dense assignment and streaming CNF cases."""

import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "fuzz"))

from modelcheck import check_files  # noqa: E402


def check(cnf, model):
    with tempfile.TemporaryDirectory(prefix="tactix-modelcheck-") as tmp:
        directory = pathlib.Path(tmp)
        cnf_path = directory / "case.cnf"
        out_path = directory / "case.out"
        cnf_path.write_text(cnf)
        out_path.write_text(model)
        return check_files(cnf_path, out_path)


def main():
    # Clauses may share a line or span lines. Repeated model assignments retain
    # the final value, and variables beyond the declared universe are ignored.
    ok, message = check(
        "p cnf 3 3\n1 -2 0 2\n3 0\n-3 0\n",
        "s SATISFIABLE\nv -1 1 2 -3 99 0\n",
    )
    assert ok, message
    assert message == "3 clauses satisfied, 3 vars assigned"

    ok, message = check("p cnf 2 1\n1 2 0\n", "v -1 0\n")
    assert not ok and message.startswith("clause 1 ")

    ok, message = check("p cnf 1 1\n1 0\n", "s SATISFIABLE\n")
    assert not ok and message == "no v lines in solver output"

    ok, message = check("1 0\n", "v 1 0\n")
    assert not ok and message == "missing DIMACS header"


if __name__ == "__main__":
    main()

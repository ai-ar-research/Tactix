#!/usr/bin/env python3
"""File-scale SAT model oracle: dense assignment and streaming CNF cases."""

import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "fuzz"))

from modelcheck import check_files  # noqa: E402
from generators import parse_dimacs  # noqa: E402


def check(cnf, model):
    with tempfile.TemporaryDirectory(prefix="tactix-modelcheck-") as tmp:
        directory = pathlib.Path(tmp)
        cnf_path = directory / "case.cnf"
        out_path = directory / "case.out"
        cnf_path.write_text(cnf)
        out_path.write_text(model)
        return check_files(cnf_path, out_path)


def main():
    # A comment line may interrupt a clause, but an inline 'c' token may not.
    # Both readers must preserve the pending clause across the legal spelling.
    for cnf in (
        'p cnf 2 2\n1\nc inside clause\n2 0\n-2 0\n',
        'p cnf 2 2\r\n01\r\n c\tinside clause\r\n2 0 -02 0\r\n',
    ):
        ok, message = check(cnf, 'v 1 -2 0\n')
        assert ok, message
        ok, message = check(cnf, 'v -1 -2 0\n')
        assert not ok and message.startswith('clause 1 '), message
        with tempfile.TemporaryDirectory(prefix='tactix-dimacs-comment-') as tmp:
            path = pathlib.Path(tmp)/'input.cnf'
            path.write_text(cnf)
            assert parse_dimacs(path) == (2, [[1, 2], [-2]])

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

    for cnf, model in [
        ('p cnf 1 1\n1 c not a comment\n0\n', 'v 1 0\n'),
        ('p cnf invalid 1\n1 0\n', 'v 1 0\n'),
        ('p cnf 1 1\n1 0\n', 'v invalid 0\n'),
    ]:
        ok, message = check(cnf, model)
        assert not ok and message.startswith('invalid DIMACS/model input:'), message


if __name__ == "__main__":
    main()

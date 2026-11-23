"""Check the same conflict ER transaction with proof logging on and off."""
import sys
from pathlib import Path

from proof_test_support import Fixture

fixture, root = map(Path, sys.argv[1:])
f = Fixture(root, "conflict-er")
for sat in (False, True):
    for mode in ("none", "dsr", "pbp"):
        for path in ("public", "native"):
            stem = f.fresh(f"{sat}-{mode}-{path}")
            f.run(sat, fixture, stem, mode, "sat" if sat else "unsat", path)
            q = f.query(stem, mode, sat, (mode, sat), fmt=mode, audit="dsr")
            if not sat and mode == "dsr" and path == "native":
                f.reject_without(q.cnf, q.proof, q.kind, q.route)
    stem = f.fresh(f"{sat}-fx-er")
    f.run((sat, "fx-er"), fixture, stem, "dsr", "sat" if sat else "unsat",
          "native", "fx")
    f.query(stem, "dsr", sat, (sat, "fx-er"), fmt="dsr", audit="dsr")
f.report("Conflict-clause ER", "native and public transactions checked")

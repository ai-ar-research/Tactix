"""Check the same conflict ER transaction with proof logging on and off."""
import sys
from pathlib import Path

from proof_test_support import Fixture, check_model, formula

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
    for mode in ("none", "dsr", "pbp"):
        for path, extra in (("automatic", ()), ("explicit", ()), ("reuse", ()), ("automatic", ("fx",))):
            case = "automatic-fx" if extra else path
            stem = f.fresh(f"{sat}-factor-{mode}-{case}")
            f.run(("factor", sat, case), fixture, "factor-learned", stem, mode,
                  "sat" if sat else "unsat", path, *extra)
            cnf, proof = Path(f"{stem}.cnf"), Path(f"{stem}.proof")
            # The storage test requires a wide tail. The model and proof
            # checkers certify this fixture without enumerating 2**28 models.
            if sat:
                check_model(Path(f"{stem}.model"), *formula(cnf))
                f.count["SAT models"] += 1
            elif mode != "none":
                f.refute(cnf, proof, fmt=mode, audit="dsr")
f.report("Conflict-clause ER", "native and public transactions checked")

"""Bind query refutations to their exact input and assumptions."""
from proof_test_support import proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root, checkers=('VERIPB', 'CAKE_PB_CNF', 'DRAT_TRIM', 'CAKE_LPR'))
    with tempfile.TemporaryDirectory(prefix="tactix-query-proof-") as raw:
        directory = Path(raw)
        traces = {}
        for mode, route in (("none", None), ("pbp", "pbp"), ("dsr", "dsr-cake_lpr")):
            for case in ("search", "root", "late-root", "opposite"):
                cnf, proof = directory / f"{case}.cnf", directory / f"{case}.{mode}"
                generated = subprocess.run(
                    [fixture, cnf, proof, mode, case], text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
                assert generated.returncode == 0, generated.stdout
                if mode == "none":
                    traces[case] = generated.stdout
                    assert not proof.exists()
                    continue
                assert generated.stdout == traces[case], (mode, case, generated.stdout, traces[case])
                result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                assert result[:3] == (True, "VERIFIED", runner.CHK_VERIFIED), result
                # Remove precisely the query assumption. The permanent formula
                # is SAT in both fixtures; accepting this proof would leak a
                # failed assumption into a false base-UNSAT theorem.
                original = cnf.read_text().splitlines()
                header = original[0].split()
                header[3] = str(int(header[3]) - 1)
                base = directory / "base.cnf"
                base.write_text(" ".join(header) + "\n" + "\n".join(original[1:-1]) + "\n")
                result = runner.check_proof(mode, str(base), str(proof), 30, route=route)
                assert not result[0] and result[2] == runner.CHK_REJECTED, result
    print("query certificates: PBP and DSR independently checked; missing assumptions rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main())

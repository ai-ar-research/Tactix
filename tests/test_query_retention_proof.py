"""Check dependent retained lemmas across query and input changes."""
from proof_test_support import proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root / "tools" / "bench"))
    runner = proof_runner(root, checkers=('VERIPB', 'CAKE_PB_CNF', 'DRAT_TRIM', 'CAKE_LPR'))
    with tempfile.TemporaryDirectory(prefix="tactix-retained-proof-") as raw:
        directory = Path(raw)
        for scenario, queries in (("dependent", (1, 2, 3)), ("shapes", (2, 3, 4))):
            for schedule in ("direct", "registry"):
                trace = None
                for mode, route in (("none", None), ("pbp", "pbp"), ("dsr", "dsr-cake_lpr")):
                    prefix = directory / f"{scenario}-{schedule}-{mode}"
                    result = subprocess.run([fixture, prefix, mode, schedule, scenario], text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                            timeout=30)
                    assert result.returncode == 0, result.stdout
                    if trace is None:
                        trace = result.stdout
                    assert trace == result.stdout, (schedule, mode, trace, result.stdout)
                    for query in queries:
                        cnf, proof = Path(f"{prefix}-{query}.cnf"), Path(f"{prefix}-{query}.proof")
                        if mode == "none":
                            assert not proof.exists()
                            continue
                        verified = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                        assert verified[:3] == (True, "VERIFIED", runner.CHK_VERIFIED), verified
                        # The final assumption or permanent clause is decisive.
                        # Removing it makes this exact query satisfiable.
                        lines = cnf.read_text().splitlines()
                        header = lines[0].split()
                        header[3] = str(int(header[3]) - 1)
                        wrong = directory / "wrong.cnf"
                        wrong.write_text(" ".join(header) + "\n" + "\n".join(lines[1:-1]) + "\n")
                        rejected = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
    print("retained query proofs: PBP/DSR verify; altered queries reject; proof modes preserve search/retention")
    return 0


if __name__ == "__main__":
    sys.exit(main())

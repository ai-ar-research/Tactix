#!/usr/bin/env python3
"""Normal CLI supplies the provenance sink required by Ramsey startup."""

from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/bench"))
from proof_oracle import check_dsr


def ramsey_44(vertices: int) -> str:
    edge = [[0] * vertices for _ in range(vertices)]
    variable = 0
    for a in range(vertices):
        for b in range(a + 1, vertices):
            variable += 1
            edge[a][b] = variable
    clauses = []
    for a in range(vertices):
        for b in range(a + 1, vertices):
            for c in range(b + 1, vertices):
                for d in range(c + 1, vertices):
                    clique = (edge[a][b], edge[a][c], edge[a][d],
                              edge[b][c], edge[b][d], edge[c][d])
                    clauses.append(" ".join(map(str, clique)) + " 0")
                    clauses.append(" ".join(str(-lit) for lit in clique) + " 0")
    return f"p cnf {variable} {len(clauses)}\n" + "\n".join(clauses) + "\n"


with tempfile.TemporaryDirectory(prefix="ramsey-cli-") as directory:
    work = Path(directory)
    cnf, proof = work / "ramsey-18.cnf", work / "ramsey-18.proof"
    cnf.write_text(ramsey_44(18))
    result = subprocess.run([sys.argv[1], "--auto-proof", str(cnf), str(proof)],
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 20, result
    assert "s UNSATISFIABLE" in result.stdout
    line = next(row for row in result.stdout.splitlines()
                if row.startswith("c ramsey"))
    assert "1 attempts, 1 recognized" in line and "1 UNSAT" in line, line
    assert proof.stat().st_size > 0
    pins = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/pins"
    check = work / "check"
    check.mkdir()
    verified = check_dsr(pins / "dsr-trim", pins / "dsr-compress",
                         pins / "srcheck", cnf, proof, check, timeout=60)
    assert verified[0], verified

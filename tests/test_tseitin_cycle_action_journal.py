#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import tempfile
import itertools


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_tseitin_cycle_action_journal.py EXECUTABLE")
    with tempfile.TemporaryDirectory(prefix="tseitin-cycle-journal-") as tmp:
        path = os.path.join(tmp, "journal.jsonl")
        repeated_path = os.path.join(tmp, "repeated.jsonl")
        ascii_proof = os.path.join(tmp, "action.dsr")
        binary_proof = os.path.join(tmp, "action.dsrbin")
        repeated_proof = os.path.join(tmp, "repeated.dsr")
        cnf = os.path.join(tmp, "triangle.cnf")
        repeated_cnf = os.path.join(tmp, "repeated.cnf")
        env = dict(os.environ)
        env["TACTIX_TEST_TSEITIN_JOURNAL"] = path
        env["TACTIX_TEST_TSEITIN_REPEAT_JOURNAL"] = repeated_path
        env["TACTIX_TEST_TSEITIN_DSR_ASCII"] = ascii_proof
        env["TACTIX_TEST_TSEITIN_DSR_BINARY"] = binary_proof
        env["TACTIX_TEST_TSEITIN_REPEAT_DSR"] = repeated_proof
        subprocess.run([sys.argv[1]], env=env, check=True)
        with open(cnf, "w", encoding="ascii") as target:
            target.write(
                "p cnf 3 6\n"
                "1 3 0\n-1 -3 0\n-1 2 0\n1 -2 0\n-2 3 0\n2 -3 0\n"
            )
        equations = [
            (True, (1, 2, 3)),
            (False, (3, 4, 5)),
            (False, (1, 5, 6)),
            (False, (2, 4, 6)),
        ]
        clauses = []
        for offset in (0, 6):
            for rhs, variables in equations:
                shifted = tuple(v + offset for v in variables)
                for bits in itertools.product((False, True), repeat=len(shifted)):
                    if (sum(bits) % 2 == 1) == rhs:
                        continue
                    clauses.append(
                        [(-v if bit else v) for v, bit in zip(shifted, bits)]
                    )
        with open(repeated_cnf, "w", encoding="ascii") as target:
            target.write(f"p cnf 12 {len(clauses)}\n")
            for clause in clauses:
                target.write(" ".join(map(str, clause)) + " 0\n")
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        oracle = os.path.join(root, "tools", "bench", "proof_oracle.py")
        checker = os.path.join(root, "tools", "checkers", "bin")
        for proof in (ascii_proof, binary_proof):
            workdir = os.path.join(tmp, "check-" + os.path.basename(proof))
            os.mkdir(workdir)
            subprocess.run(
                [
                    sys.executable,
                    oracle,
                    "check-dsr",
                    cnf,
                    proof,
                    "--dsr-trim",
                    os.path.join(checker, "dsr-trim"),
                    "--dsr-compress",
                    os.path.join(checker, "dsr-compress"),
                    "--srcheck",
                    os.path.join(checker, "srcheck"),
                    "--workdir",
                    workdir,
                    "--every-lemma",
                ],
                check=True,
            )
        repeated_lsr = os.path.join(tmp, "repeated.lsr")
        subprocess.run(
            [
                os.path.join(checker, "dsr-trim"),
                "-f",
                repeated_cnf,
                repeated_proof,
                repeated_lsr,
            ],
            check=True,
        )
        with open(path, encoding="utf-8") as source:
            rows = [json.loads(line) for line in source if line.strip()]
        with open(repeated_path, encoding="utf-8") as source:
            repeated = [json.loads(line) for line in source if line.strip()]
    assert len(rows) == 1
    record = rows[0]
    assert record["action"] == "TseitinCycleBreaking"
    assert record["proof_format"] == "none"
    assert record["semantic_key"].startswith("tcd1:")
    transaction = record["transaction"]
    assert transaction["proof_events"] == []
    assert transaction["helper_additions"] == []
    assert transaction["helper_deletions"] == []
    assert transaction["deletion_state"] == "none"
    assert transaction["emitted_accounting"] == [0, 0, 0, 0, 1, 0]
    assert record["installed_unit"] == transaction["clause"][0]
    assert len(repeated) == 2
    assert repeated[0]["mapping"]["external_root_units"] == []
    assert repeated[1]["mapping"]["external_root_units"]
    assert repeated[0]["installed_unit"] in repeated[1]["mapping"][
        "external_root_units"
    ]
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Pinned VeriPB+CakePB check for whole-run RUP/RAT/SR PBP output."""
import pathlib
import subprocess
import sys
import tempfile


def run(argv):
    return subprocess.run([str(x) for x in argv], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main():
    generator = pathlib.Path(sys.argv[1])
    root = pathlib.Path(__file__).resolve().parents[1]
    veripb = root / "tools/checkers/bin/veripb"
    cakepb = root / "tools/checkers/bin/cake-pb-cnf"
    with tempfile.TemporaryDirectory(prefix="tactix-streaming-pbp-") as raw:
        work = pathlib.Path(raw)
        for negative in (0, 1):
            for compact in (0, 1):
                stem = work / f"sr-{negative}-{compact}"
                cnf = stem.with_suffix(".cnf")
                proof = stem.with_suffix(".pbp")
                kernel = stem.with_suffix(".kernel.pbp")
                generated = run([generator, cnf, proof, "pbp", negative,
                                 compact])
                assert generated.returncode == 0, generated.stdout
                checked = run([veripb, "-u", "--cnf", "--elaborate", kernel,
                               cnf, proof])
                assert checked.returncode == 0 and "VERIFIED" in checked.stdout, \
                    checked.stdout
                independent = run([cakepb, cnf, kernel])
                assert independent.returncode == 0 and \
                    "VERIFIED UNSAT" in independent.stdout, independent.stdout

                corrupted = stem.with_suffix(".corrupt.pbp")
                text = proof.read_text()
                assert "rup >= 1 ;\n" in text
                corrupted.write_text(text.replace("rup >= 1 ;\n", "", 1))
                rejected = run([veripb, "-u", "--cnf", cnf, corrupted])
                assert rejected.returncode != 0 or \
                    "VERIFIED UNSATISFIABLE" not in rejected.stdout, \
                    rejected.stdout
    print("streaming PBP: 4 RUP/RAT/SR proofs passed VeriPB+CakePB; "
          "4 corrupted controls rejected")


if __name__ == "__main__":
    main()

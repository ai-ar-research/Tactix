#!/usr/bin/env python3
"""Check a late-attached public-C-API PBP through the benchmark route."""

import importlib.util
import pathlib
import subprocess
import sys
import tempfile


def main():
    fixture = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    sys.path.insert(0, str(root / "tools" / "bench"))
    spec = importlib.util.spec_from_file_location(
        "tactix_bench_run", root / "tools" / "bench" / "run.py")
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)

    with tempfile.TemporaryDirectory(prefix="tactix-api-pbp-") as raw:
        directory = pathlib.Path(raw)
        cnf = directory / "input.cnf"
        proof = directory / "proof.pbp"
        generated = subprocess.run([fixture, cnf, proof], text=True,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert generated.returncode == 0, generated.stdout

        checked = runner.check_proof("pbp", str(cnf), str(proof), 30,
                                     route="pbp")
        assert checked[:3] == (True, "VERIFIED", runner.CHK_VERIFIED), checked
        wrong_route = runner.check_proof("pbp", str(cnf), str(proof), 30,
                                          route="dsr-trestle")
        assert not wrong_route[0] and wrong_route[2] == runner.CHK_REJECTED

        corrupted = directory / "corrupt.pbp"
        text = proof.read_text()
        assert "rup >= 1 ;\n" in text
        corrupted.write_text(text.replace("rup >= 1 ;\n", "", 1))
        rejected = runner.check_proof("pbp", str(cnf), str(corrupted), 30,
                                      route="pbp")
        assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected

    print("C API PBP: VeriPB+CakePB pass; route and corruption controls reject")


if __name__ == "__main__":
    main()

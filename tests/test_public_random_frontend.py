#!/usr/bin/env python3
"""The only seeded random frontend is an external public-C-API client."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


def run(binary, *arguments):
    return subprocess.run([str(binary), *map(str, arguments)],
                          capture_output=True, text=True, timeout=20)


def main():
    public_random = Path(sys.argv[1]).resolve()
    production = Path(sys.argv[2]).resolve()
    root = Path(sys.argv[3]).resolve()
    source = (root / "tools/fuzz/public_random_main.cpp").read_text()

    includes = re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.M)
    assert "tactix.h" in includes
    assert not any(name.startswith("src/") or name.startswith("policies/") or
                   name.startswith("strategy/") or name.startswith("kernel/") or
                   name.startswith("rules/") for name in includes), includes
    assert "tactix_solve(" not in source

    with tempfile.TemporaryDirectory(prefix="tactix-public-random-") as tmp:
        work = Path(tmp)
        unsat = work / "unsat.cnf"
        unsat.write_text("p cnf 2 4\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 0\n")
        sat = work / "sat.cnf"
        sat.write_text("p cnf 3 2\n1 2 0\n-2 3 0\n")

        combined = ""
        for seed in range(6):
            result = run(public_random, f"--random-policy={seed}", unsat)
            assert result.returncode == 20, (seed, result.stdout, result.stderr)
            assert "s UNSATISFIABLE" in result.stdout
            assert f"seed {seed}" in result.stdout
            combined += result.stdout
        for profile in ("root-heavy", "search-heavy", "alternating",
                        "restart-heavy", "instance-heavy", "algebraic-heavy"):
            assert f"profile {profile}" in combined
        for form in ("canonical", "instance", "semantic", "online"):
            assert re.search(rf"\b{form}=[1-9]", combined), form
        assert re.search(r"root-to-search=[1-9]", combined)
        assert re.search(r"algebraic-actions=[1-9]", combined)

        result = run(public_random, "--random-policy=5", sat)
        assert result.returncode == 10, (result.stdout, result.stderr)
        assert "s SATISFIABLE" in result.stdout and re.search(r"^v .* 0$", result.stdout, re.M)

        lrat = work / "unsat.lrat"
        result = run(public_random, "--random-policy=1", "--lrat", unsat, lrat)
        assert result.returncode == 20, (result.stdout, result.stderr)
        assert lrat.is_file() and lrat.stat().st_size
        assert "c proof-route lrat" in result.stdout

        retired = run(production, "--random-policy=5", sat)
        assert retired.returncode == 1
        assert "unknown option '--random-policy=5'" in retired.stderr

        retired = run(production, "--auto-proof", sat)
        assert retired.returncode == 1
        assert "unknown option '--auto-proof'" in retired.stderr
        retired = run(public_random, "--auto-proof", sat)
        assert retired.returncode == 1
        assert "unsupported option '--auto-proof'" in retired.stderr

    print("public random frontend profiles/forms and production-policy purge PASS")


if __name__ == "__main__":
    main()

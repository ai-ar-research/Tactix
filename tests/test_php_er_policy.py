"""Fixed generated PHP integration tests; no randomized/fuzz campaign."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

binary, root = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
spec = importlib.util.spec_from_file_location("examples_generator", root / "examples/cnf/generate.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)

def dimacs(pigeons, holes):
    variables, clauses = generator.pigeonhole(pigeons, holes)
    return (f"p cnf {variables} {len(clauses)}\n" +
            "".join(" ".join(map(str, clause + [0])) + "\n" for clause in clauses))

with tempfile.TemporaryDirectory(prefix="php-er-fixed-", dir=Path.cwd()) as temporary:
    directory = Path(temporary)
    for holes in (2, 3, 4):
        cnf = directory / f"php-{holes}.cnf"
        cnf.write_text(dimacs(holes + 1, holes))
        run = subprocess.run([binary, cnf], capture_output=True, text=True, timeout=30)
        assert run.returncode == 20, (run.returncode, run.stdout, run.stderr)
        assert "s UNSATISFIABLE" in run.stdout and "c php-er decisions=0" in run.stdout
        assert "c php-er bve-eliminated=0 substituted=0" in run.stdout
        last = [line for line in run.stdout.splitlines() if "level=" in line][-1]
        assert f"definitions={2*holes*(holes+1)*(holes-1)//3}" in last

    good = dimacs(4, 3)
    lines = good.splitlines(keepends=True)
    invalid = [dimacs(3, 3), good.replace("1 2 3 0", "1 2 -3 0", 1),
               "".join(lines[:-1]), good + "0\n", good + "garbage\n",
               good.replace("p cnf", "p dnf", 1)]
    for index, text in enumerate(invalid):
        cnf = directory / f"invalid-{index}.cnf"
        cnf.write_text(text)
        run = subprocess.run([binary, cnf], capture_output=True, text=True, timeout=30)
        assert run.returncode == 1 and "s UNSATISFIABLE" not in run.stdout
        assert "unsupported input" in run.stderr, (index, run.stdout, run.stderr)
print("PHP 3/2, 4/3, 5/4: zero decisions; six unsupported inputs rejected")

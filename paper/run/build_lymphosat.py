#!/usr/bin/env python3
"""Rebuild LymphoSAT (SAT Competition 2026) for a generic x86-64 machine.

The competition binaries need glibc 2.38 and an Ice Lake CPU. This rebuilds
every nested solver from the shipped sources through lymphosat-cxx, which
removes only the hard-coded target flags. A nested solver whose build fails
under the generic target keeps its shipped binary, and LymphoSAT's own
fallback covers it; the JSON report lists any such solver.

Run by paper/run/setup.sh after tools/solvers/fetch-winners.sh.
"""
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOLVER = ROOT / "winners/lymphosat-sc2026/green/lymphosat/src/solver"
ADAPTER = Path(__file__).resolve().parent / "lymphosat-cxx"
REPORT = Path(os.environ.get("LYMPHOSAT_BUILD_REPORT", ROOT / "paper/run/tools/lymphosat-build.json"))


def main():
    order = json.loads((SOLVER / "order.json").read_text())
    names = list(order["solver_order"])
    if order.get("fallback_solver") and order["fallback_solver"] not in names:
        names.append(order["fallback_solver"])
    real_cxx = shutil.which("g++")
    real_cc = shutil.which("gcc")
    wrappers = Path(tempfile.mkdtemp(prefix="lymphosat-cxx-"))
    build = Path(tempfile.mkdtemp(prefix="lymphosat-composite-"))
    try:
        (wrappers / "g++").symlink_to(ADAPTER)
        (wrappers / "clang++").symlink_to(ADAPTER)
        env = dict(os.environ, PATH=f"{wrappers}:{os.environ['PATH']}", CC="gcc", CXX="g++",
                   LYMPHOSAT_REAL_CXX=real_cxx, LYMPHOSAT_REAL_CLANGXX=str(ADAPTER),
                   LYMPHOSAT_REAL_CLANG=real_cc)
        rows = []
        for name in names:
            package = SOLVER / "solvers" / name / "solver"
            script = package / "build.sh"
            if not script.exists():
                rows.append({"solver": name, "result": "shipped executable retained"})
                continue
            run = subprocess.run([script], cwd=package, env=env, capture_output=True,
                                 text=True, errors="replace")
            rows.append({"solver": name,
                         "result": "portable" if run.returncode == 0 else "shipped binary retained",
                         "build_exit_code": run.returncode,
                         "diagnostic_tail": "\n".join((run.stdout + run.stderr).splitlines()[-8:])})
        composite = dict(env, CXXFLAGS="-march=x86-64-v2 -mtune=generic")
        subprocess.run(["cmake", "-S", SOLVER, "-B", build, "-DCMAKE_BUILD_TYPE=Release"],
                       env=composite, check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["cmake", "--build", build, "--target", "lymphosat_composite", "--parallel",
                        str(os.cpu_count() or 4)], env=composite, check=True, stdout=subprocess.DEVNULL)
        report = {"portable": sum(r["result"] == "portable" for r in rows),
                  "shipped_binary_retained": [r["solver"] for r in rows
                                              if r["result"] == "shipped binary retained"],
                  "nested": rows}
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(json.dumps({k: v for k, v in report.items() if k != "nested"}, indent=2))
    finally:
        shutil.rmtree(wrappers, ignore_errors=True)
        shutil.rmtree(build, ignore_errors=True)


if __name__ == "__main__":
    main()

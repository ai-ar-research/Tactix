#!/usr/bin/env python3
"""Rebuild LymphoSAT for the heterogeneous Tactix cluster.

The submission's generic-build script still lets nested build scripts hard-code
Ice Lake flags.  Rebuild each nested solver through the recorded flag filter;
an explicitly AVX-512 implementation may reject the generic target, in which
case its already rebuilt Ice Lake binary is retained and the composite's
documented signal fallback remains active.  A JSON report makes that exception
auditable.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "winners/lymphosat-sc2026/green/lymphosat/src"
SOLVER = SRC / "solver"
ADAPTER = ROOT / "tools/bench/lymphosat-generic-cxx"
REPORT = ROOT / "benchmarks/baselines/sc24-26-32g-5000s-v1/lymphosat-build.json"


def main():
    order = json.loads((SOLVER / "order.json").read_text())
    names = list(order["solver_order"])
    fallback = order.get("fallback_solver")
    if fallback and fallback not in names:
        names.append(fallback)
    wrapper_dir = Path(tempfile.mkdtemp(prefix="lymphosat-cxx-"))
    build_dir = Path(tempfile.mkdtemp(prefix="lymphosat-composite-"))
    try:
        (wrapper_dir / "g++").symlink_to(ADAPTER)
        (wrapper_dir / "clang++").symlink_to(ADAPTER)
        env = os.environ.copy()
        env.update({
            "PATH": f"{wrapper_dir}:{env['PATH']}",
            "CC": "gcc", "CXX": "g++",
            "LYMPHOSAT_REAL_CLANGXX": str(ADAPTER),
            "LYMPHOSAT_REAL_CLANG": "/cm/local/apps/gcc/13.1.0/bin/gcc",
        })
        rows = []
        for name in names:
            package = SOLVER / "solvers" / name / "solver"
            script = package / "build.sh"
            if not script.exists():
                rows.append({"solver": name, "result": "shipped executable retained"})
                continue
            run = subprocess.run([script], cwd=package, env=env,
                                 capture_output=True, text=True, errors="replace")
            rows.append({
                "solver": name,
                "result": "portable" if run.returncode == 0 else "Ice Lake binary retained",
                "build_exit_code": run.returncode,
                "diagnostic_tail": "\n".join((run.stdout + run.stderr).splitlines()[-8:]),
            })
        cenv = env.copy()
        cenv["CXXFLAGS"] = "-march=x86-64-v2 -mtune=generic"
        subprocess.run(["cmake", "-S", SOLVER, "-B", build_dir,
                        "-DCMAKE_BUILD_TYPE=Release"], env=cenv, check=True)
        subprocess.run(["cmake", "--build", build_dir, "--target",
                        "lymphosat_composite", "--parallel", "4"], env=cenv, check=True)
        report = {
            "schema": 1,
            "portable": sum(r["result"] == "portable" for r in rows),
            "ice_lake_retained": [r["solver"] for r in rows
                                  if r["result"] == "Ice Lake binary retained"],
            "adapter": str(ADAPTER.relative_to(ROOT)),
            "nested": rows,
        }
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(json.dumps({k: v for k, v in report.items() if k != "nested"}, indent=2))
    finally:
        shutil.rmtree(wrapper_dir, ignore_errors=True)
        shutil.rmtree(build_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main() or 0)

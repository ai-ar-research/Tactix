#!/usr/bin/env python3
"""Guard SLURM cgroup ownership of benchmark memory limits."""

import ast
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNNERS = (
    ROOT / "tools/bench/run.py",
    ROOT / "tools/bench/frozen_baseline.py",
    ROOT / "tools/bench/tactix_tierd.py",
    ROOT / "tools/bench/lymphosat_attribution.py",
    ROOT / "tools/bench/p4_cvg.py",
)


def attributes(node):
    return [child.attr for child in ast.walk(node)
            if isinstance(child, ast.Attribute)]


for runner in RUNNERS:
    tree = ast.parse(runner.read_text(), filename=str(runner))
    for call in (node for node in ast.walk(tree) if isinstance(node, ast.Call)):
        if isinstance(call.func, ast.Attribute) and call.func.attr == "setrlimit":
            assert "RLIMIT_AS" not in attributes(call), runner

run_source = (ROOT / "tools/bench/run.py").read_text()
assert "physical_cores = max(1, -(-(mem_gb or 8) // 8))" in run_source
assert "cpus = 2 * physical_cores" in run_source
assert "#SBATCH --mem={mem_gb}G" in run_source

plan = (ROOT / "DEVELOPMENT_PLAN.md").read_text()
assert "Allocate 8 GiB per **physical** core" in plan
assert "two physical/four logical CPUs" in plan
assert "SLURM `--mem=16G` task cgroup" in plan

print("SLURM memory enforcement: PASS")

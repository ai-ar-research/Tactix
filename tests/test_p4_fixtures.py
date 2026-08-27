#!/usr/bin/env python3
"""Pin the Phase 4 causal-gap subsets and their parent-tier provenance."""
import importlib.util
import pathlib
import subprocess
import sys
import tempfile
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[1]
TIERS = ROOT / "benchmarks" / "tiers"
MANIFESTS = ROOT / "tools" / "bench" / "manifests"


def rows(name):
    directory = MANIFESTS if name.startswith("P4") else TIERS
    return [line.split() for line in (directory / f"{name}.list").read_text().splitlines()
            if line.strip() and not line.startswith("#")]


p4a = rows("P4A")
p4b = rows("P4B")
assert [row[0] for row in p4a] == [
    "tierA/tr-n300-s055.cnf",
    "tierA/tr-n260-s002.cnf",
    "tierA/tr-n280-s052.cnf",
]
assert [row[0] for row in p4b] == [
    "tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz",
    "tierB/mp1-klieber2017s-0500-023-t12.cnf.xz",
    "tierB/or_randxor_k3_n540_m540.cnf.xz",
    "tierB/unsat-set-b-fclqcolor-10-07-09.sat05-1282.reshuffled-07.cnf.xz",
    "tierB/veer_axi_yosyshq_appnote_123_veer_axi-p23_transition.cnf.xz",
]
assert {tuple(row) for row in p4a} <= {tuple(row) for row in rows("A")}
assert {tuple(row) for row in p4b} <= {tuple(row) for row in rows("B")}
assert [row[1] for row in p4a] == ["UNSAT"] * 3
assert [row[1] for row in p4b].count("SAT") == 2
assert [row[1] for row in p4b].count("UNSAT") == 3

spec = importlib.util.spec_from_file_location("bench_run", ROOT / "tools" /
                                              "bench" / "run.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
assert runner.TIER_TIMEOUT["P4A"] == 30.0
assert runner.TIER_TIMEOUT["P4B"] == 300.0
assert all(runner.TIER_MEM_GB[tier] == 32
           for tier in ("A", "B", "C", "P4A", "P4B"))
assert runner.TIER_MEM_GB["D"] == 16
assert [row[0] for row in runner.read_tier_list("P4A")] == [row[0] for row in p4a]
assert [row[0] for row in runner.read_tier_list("P4B")] == [row[0] for row in p4b]

spec = importlib.util.spec_from_file_location("p4_cvg", ROOT / "tools" /
                                              "bench" / "p4_cvg.py")
p4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p4)
assert p4.resolve_fixture("tr-n300-s055")["group"] == "P4A"
assert p4.resolve_fixture("or_randxor_k3_n540_m540.cnf")["group"] == "P4B"
assert p4.resolve_fixture("T87.2.0", "C")["group"] == "C"
try:
    p4.resolve_fixture("not-a-p4-fixture")
    raise AssertionError("unknown fixture accepted")
except ValueError:
    pass
dry = subprocess.run(
    [sys.executable, str(ROOT / "tools/bench/p4_cvg.py"),
     "--fixture", "tr-n300-s055", "--tactix", "/pin/tactix",
     "--kissat", "/pin/kissat", "--out", "/tmp/p4-dry", "--dry-run"],
    check=True, text=True, stdout=subprocess.PIPE).stdout
assert "sbatch" in dry and "--worker" in dry and "--time=131" in dry
tier_c_dry = subprocess.run(
    [sys.executable, str(ROOT / "tools/bench/p4_cvg.py"),
     "--fixture", "T87.2.0", "--tier", "C", "--tactix", "/pin/tactix",
     "--kissat", "/pin/kissat", "--out", "/tmp/p4-tier-c-dry",
     "--dry-run"],
    check=True, text=True, stdout=subprocess.PIPE).stdout
assert "--tier C" in tier_c_dry
assert "--cpus-per-task=8" in tier_c_dry and "--mem=32G" in tier_c_dry
assert "--time=210" in tier_c_dry

tactix_only_dry = subprocess.run(
    [sys.executable, str(ROOT / "tools/bench/p4_cvg.py"),
     "--fixture", "T87.2.0", "--tier", "C", "--tactix", "/pin/tactix",
     "--kissat", "/pin/kissat", "--out", "/tmp/p4-tier-c-tactix-only-dry",
     "--tactix-only", "--no-trace", "--dry-run"],
    check=True, text=True, stdout=subprocess.PIPE).stdout
assert "--tactix-only" in tactix_only_dry and "--no-trace" in tactix_only_dry
assert "--time=110" in tactix_only_dry

large_memory_dry = subprocess.run(
    [sys.executable, str(ROOT / "tools/bench/p4_cvg.py"),
     "--fixture", "128_125.sanitized", "--tier", "C",
     "--tactix", "/pin/kissat", "--kissat", "/pin/kissat",
     "--out", "/tmp/p4-tier-c-large-memory-dry", "--tactix-only",
     "--no-trace", "--memory-gb", "40", "--dry-run"],
    check=True, text=True, stdout=subprocess.PIPE).stdout
assert "--cpus-per-task=10" in large_memory_dry and "--mem=40G" in large_memory_dry
assert "--memory-gb 40" in large_memory_dry

with tempfile.TemporaryDirectory(prefix="p4-check-timeout-") as directory:
    directory = pathlib.Path(directory)
    check_log = directory / "check.log"
    timeout = subprocess.TimeoutExpired(["drat-trim"], 300, output=b"partial")
    with mock.patch.object(p4.subprocess, "run", side_effect=timeout):
        ok, status = p4.verify(
            "UNSAT", directory / "input.cnf", directory / "proof.drat",
            directory / "solver.log", check_log, 30)
    assert not ok and status == "timeout"
    assert check_log.read_text() == "partial\nCHECKER TIMEOUT after 3600s\n"
print("test_p4_fixtures OK")

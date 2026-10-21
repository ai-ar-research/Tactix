#!/usr/bin/env python3
"""The full Tier-D preparer must pin the runtime's complete proof routes."""

import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "experimental_tierd", ROOT / "tools/bench/experimental_tierd.py")
TIERD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TIERD)

REFERENCE = ROOT / "benchmarks/baselines/sc24-26-32g-5000s"
baseline = json.loads((REFERENCE / "manifest.json").read_text())
assert len(baseline["instances"]) == 1166

auto = TIERD.checker_sources(REFERENCE, baseline, "auto")
assert set(auto) == {
    "checkmodel", "modelcheck.py", "drat-trim", "cake_lpr",
    "dsr-trim", "dsr-compress", "srcheck", "veripb", "cake-pb-cnf",
}
assert all(path.is_file() for path in auto.values())
for name in ("dsr-trim", "dsr-compress", "srcheck"):
    assert TIERD.sha(auto[name]) == TIERD.FROZEN_DSR_HASHES[name]

pbp = TIERD.checker_sources(REFERENCE, baseline, "pbp")
assert set(pbp) == {
    "checkmodel", "modelcheck.py", "veripb", "cake-pb-cnf",
}
assert all(path.is_file() for path in pbp.values())

dsr = TIERD.checker_sources(REFERENCE, baseline, "dsr")
assert set(dsr) == set(auto) - {"veripb", "cake-pb-cnf"}
try:
    TIERD.checker_sources(REFERENCE, baseline, "drat")
    raise AssertionError("unsupported certificate mode was accepted")
except ValueError:
    pass

wall = TIERD.scheduler_wall_seconds(5000, 45000)
assert wall == 51300
assert TIERD.slurm_time(wall) == "14:15:00"
assert wall >= 5000 + 45000
assert TIERD.scheduler_wall_seconds(1200, 16000) >= 18000

assert TIERD.solver_arguments("dsr", [
    "--controller-policy=17",
    "--controller-native-producers=tseitin-cycle",
    "--controller-families=TseitinCycleBreaking",
]) == [
    "--dsr",
    "--controller-policy=17",
    "--controller-native-producers=tseitin-cycle",
    "--controller-families=TseitinCycleBreaking",
]
assert TIERD.solver_arguments("auto", []) == ["--auto-proof"]
assert TIERD.solver_arguments("pbp", []) == []
assert TIERD.solver_arguments("dsr", [
    "--controller-policy=2",
    "--controller-families=NaryCspPrefix",
], causal_preflight=True) == [
    "--dsr",
    "--controller-policy=2",
    "--controller-families=NaryCspPrefix",
]
for artifact in ("--controller-trace=trace", "--feedback=feedback.jsonl.gz",
                 "--structural-provenance=provenance.jsonl"):
    try:
        TIERD.solver_arguments("dsr", [artifact], causal_preflight=True)
        raise AssertionError("causal preflight accepted a full telemetry artifact")
    except ValueError:
        pass
try:
    TIERD.solver_arguments("dsr", [], gf2_telemetry=True,
                           causal_preflight=True)
    raise AssertionError("causal preflight accepted full GF2 telemetry")
except ValueError:
    pass
for invalid in ([""], ["--pbp"], ["--dsr"], ["--auto-proof"], ["--drat"], ["--lrat"]):
    try:
        TIERD.solver_arguments("dsr", invalid)
        raise AssertionError("invalid extra solver argument was accepted")
    except ValueError:
        pass
try:
    TIERD.solver_arguments("drat", [])
    raise AssertionError("invalid certificate mode was accepted")
except ValueError:
    pass

print("Experimental Tier-D automatic-route protocol: PASS")

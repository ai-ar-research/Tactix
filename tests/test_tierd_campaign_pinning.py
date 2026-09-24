#!/usr/bin/env python3
"""Guard the Tier-D debug reservation and independent binary pin."""

import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
from contextlib import redirect_stdout


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "tactix_tierd", ROOT / "tools/bench/tactix_tierd.py")
TIERD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TIERD)

assert TIERD.slurm_array_spec(1166, 89) == "0-1165%89"
assert TIERD.slurm_array_spec(1166, None) == "0-1165"

winner = json.loads((ROOT / "benchmarks/baselines/sc24-26-32g-5000s/manifest.json").read_text())
slurm = TIERD.comparison_slurm(winner, 32, None, None)
assert slurm["partition"] == "cpu-dense-hwulab-q"
assert slurm["cpus_per_task"] == winner["slurm"]["cpus_per_task"] == 4
assert slurm["memory_gib"] == winner["limits"]["process_memory_gib"] == 32
assert slurm["array_throttle"] is None
assert "physical_cores" not in slurm
assert "mem_per_cpu_gib" not in slurm
assert TIERD.comparison_slurm(winner, 16, None, None)["cpus_per_task"] == 2
assert TIERD.comparison_slurm(winner, 33, None, None)["cpus_per_task"] == 5

with tempfile.TemporaryDirectory(prefix="tactix-tierd-pin-") as tmp:
    base = Path(tmp)
    source = base / "build/tactix"
    target = base / "campaign/pins/tactix"
    source.parent.mkdir()
    source.write_bytes(b"first release binary")
    source.chmod(source.stat().st_mode | stat.S_IXUSR)
    TIERD.copy_frozen_binary(source, target)

    assert source.stat().st_ino != target.stat().st_ino
    assert target.stat().st_mode & 0o222 == 0
    assert target.read_bytes() == b"first release binary"
    source.write_bytes(b"later debug rebuild")
    assert target.read_bytes() == b"first release binary"

    reference = base / "reference"
    campaign = base / "checker-campaign"
    (reference / "pins").mkdir(parents=True)
    (campaign / "pins").mkdir(parents=True)
    (reference / "pins/checkmodel").write_text("checker CLI\n")
    (reference / "pins/modelcheck.py").write_text("checker module\n")
    TIERD.pin_checker_bundle(
        reference, campaign, {"checkmodel": "pins/checkmodel"})
    assert (campaign / "pins/checkmodel").read_text() == "checker CLI\n"
    assert (campaign / "pins/modelcheck.py").read_text() == "checker module\n"

    missing = base / "missing-module"
    (missing / "pins").mkdir(parents=True)
    (missing / "pins/checkmodel").write_text("checker CLI\n")
    try:
        TIERD.pin_checker_bundle(
            missing, base / "must-fail", {"checkmodel": "pins/checkmodel"})
        raise AssertionError("checkmodel without modelcheck.py was accepted")
    except SystemExit as error:
        assert "missing checkmodel runtime module" in str(error)

    launch_campaign = base / "dry-run"
    pins = launch_campaign / "pins"
    pins.mkdir(parents=True)
    runner = pins / "runner.py"
    campaign_runner = pins / "campaign.py"
    runner.write_text("# frozen measurement runner\n")
    campaign_runner.write_text("# frozen campaign runner\n")
    manifest = {
        "tasks": [None] * len(winner["instances"]),
        "slurm": slurm,
        "measurement_runner": {"path": "pins/runner.py", "sha256": TIERD.sha256(runner)},
        "campaign_runner": {"path": "pins/campaign.py",
                            "sha256": TIERD.sha256(campaign_runner)},
    }
    (launch_campaign / "manifest.json").write_text(json.dumps(manifest))
    (launch_campaign / "validation.json").write_text('{"passed": true}')
    with patch.object(TIERD.subprocess, "run", side_effect=[
        SimpleNamespace(stdout="101\n"), SimpleNamespace(stdout="102\n")]) as sbatch, \
            redirect_stdout(io.StringIO()):
        TIERD.submit(launch_campaign)
    array = (launch_campaign / "array.sbatch").read_text()
    assert "#SBATCH --partition=cpu-dense-hwulab-q\n" in array
    assert "#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4\n" in array
    assert "#SBATCH --mem=32G\n" in array
    assert "#SBATCH --array=0-1165\n" in array
    assert "--nodelist" not in array and "--array=0-1165%" not in array
    assert sbatch.call_count == 2
    assert sbatch.call_args_list[1].args[0][2] == "--dependency=afterany:101"
    assert json.loads((launch_campaign / "launch.json").read_text())["tasks"] == 1166

print("Tier-D campaign pinning: PASS")

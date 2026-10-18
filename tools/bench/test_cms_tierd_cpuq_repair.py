"""Checks that a missing-row repair stays distinct and keeps solver pins."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cms_tierd_cpuq_repair import (array_expression, prepare,
                                   prepare_continuation)
from frozen_baseline import sha256


class CpuqRepairTests(unittest.TestCase):
    def test_sparse_missing_rows_keep_original_checked_pins(self):
        self.assertEqual(array_expression([1, 2, 4, 7, 8]), "1-2,4,7-8")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, repair = root / "base", root / "repair"
            pins, results, pending = (base / name for name in
                                      ("pins", "results", "pending"))
            for path in (pins, results, pending):
                path.mkdir(parents=True)
            checked = ("solver", "frat_xor", "cake_xlrup", "checkmodel",
                       "modelcheck.py", "libcadiback.so.0", "libcadical.so.3",
                       "libcryptominisat5.so.5.14", "anonymous_scratch.py",
                       "frozen_baseline.py", "cms-wrapper")
            for name in checked:
                (pins / name).write_bytes(name.encode())
            provenance = {
                key: {"path": f"pins/{name}", "sha256": sha256(pins / name)}
                for key, name in (("solver-binary", "solver"),
                                  ("frat-xor", "frat_xor"),
                                  ("cake-xlrup", "cake_xlrup"),
                                  ("checkmodel", "checkmodel"),
                                  ("modelcheck.py", "modelcheck.py"),
                                  ("anonymous-scratch", "anonymous_scratch.py"),
                                  ("frozen-baseline", "frozen_baseline.py"),
                                  ("cryptominisat-5.14.7", "cms-wrapper"))}
            provenance["shared-libraries"] = [
                {"path": f"pins/{name}", "sha256": sha256(pins / name)}
                for name in checked[5:8]]
            manifest = {"kind": "cryptominisat-tierd-cpuq-anonymous-baseline-v2",
                        "instances": list(range(1166)), "tasks": list(range(1166)),
                        "certificate_policy": {"retention": "none"},
                        "limits": {}, "slurm": {}, "provenance": provenance}
            (base / "manifest.json").write_text(json.dumps(manifest))
            (base / "launch.json").write_text('{"array_job_id":"12345"}')
            missing = {2, 17, 1165}
            for index in set(range(1166)) - missing:
                (results / f"{index:05d}.json").write_text("{}")
            with patch("cms_tierd_cpuq_repair.require_terminal", return_value="12345"):
                prepare(base, repair)
            amended = json.loads((repair / "manifest.json").read_text())
            self.assertEqual(amended["repair_of"]["missing_indices"], sorted(missing))
            self.assertEqual(amended["limits"]["node_scratch_ledger_slots"], 1)
            self.assertEqual(amended["slurm"]["node_scratch_participants"], 1)
            self.assertIn("#SBATCH --array=2,17,1165%8",
                          (repair / "array.sbatch").read_text())
            self.assertNotIn("#SBATCH --exclusive", (repair / "array.sbatch").read_text())
            self.assertNotIn("cleanup_scratch", (repair / "array.sbatch").read_text())
            for key in ("solver-binary", "frat-xor", "cake-xlrup", "checkmodel"):
                self.assertEqual(amended["provenance"][key], provenance[key])
            self.assertEqual(json.loads((base / "manifest.json").read_text()), manifest)
            workspace = root / "workspace"
            with patch("cms_tierd_cpuq_repair.require_terminal", return_value="12345"):
                prepare(base, workspace, workspace=True, throttle=125)
            workspace_manifest = json.loads((workspace / "manifest.json").read_text())
            self.assertEqual(workspace_manifest["repair_of"]["missing_indices"],
                             sorted(missing))
            self.assertEqual(workspace_manifest["certificate_policy"]["storage"],
                             "workspace-ephemeral")
            self.assertNotIn("workspace_quota_stop_gib",
                             workspace_manifest["limits"])
            self.assertNotIn("workspace_quota_poll_seconds",
                             workspace_manifest["limits"])
            self.assertNotIn("node_scratch_ledger_slots", workspace_manifest["limits"])
            self.assertIn("#SBATCH --array=2,17,1165%125",
                          (workspace / "array.sbatch").read_text())
            self.assertIn("trap cleanup_scratch EXIT",
                          (workspace / "array.sbatch").read_text())
            self.assertNotIn("workspace-quota", workspace_manifest["provenance"])
            self.assertFalse((workspace / "pins/workspace_quota.py").exists())

            # A successor keeps the prior checked rows without rerunning them,
            # freezes their exact inventory, and uses workspace cleanup.
            (workspace / "launch.json").write_text('{"array_job_id":"23456"}')
            (workspace / "results/00002.json").write_text('{"valid":true}')
            successor = root / "successor"
            with (patch("cms_tierd_cpuq_repair.require_terminal",
                        return_value="12345"),
                  patch("cms_tierd_cpuq_repair.require_job_terminal") as terminal):
                prepare(base, successor, workspace=True, throttle=125,
                        prior_repairs=[workspace])
            terminal.assert_called_once_with("23456")
            successor_manifest = json.loads(
                (successor / "manifest.json").read_text())
            self.assertEqual(successor_manifest["kind"],
                             "cryptominisat-tierd-cpuq-workspace-repair-v2")
            self.assertEqual(successor_manifest["repair_of"]["missing_indices"],
                             [17, 1165])
            predecessor = successor_manifest["repair_of"]["prior_repairs"][0]
            self.assertEqual(predecessor["campaign"], workspace.name)
            self.assertEqual(predecessor["published_indices"], [2])
            self.assertRegex(predecessor["results_inventory_sha256"], r"^[0-9a-f]{64}$")
            self.assertNotIn("workspace_quota_stop_gib",
                             successor_manifest["limits"])
            self.assertNotIn("workspace-quota",
                             successor_manifest["provenance"])
            self.assertIn("#SBATCH --array=17,1165%125",
                          (successor / "array.sbatch").read_text())

    def test_continuation_is_exactly_unpublished_and_removes_only_stale_claims(self):
        with tempfile.TemporaryDirectory() as directory:
            repair = Path(directory) / "repair"
            for name in ("pins", "results", "pending", "claims", "scratch"):
                (repair / name).mkdir(parents=True)
            manifest = {
                "kind": "cryptominisat-tierd-cpuq-workspace-repair-v1",
                "campaign": "repair",
                "certificate_policy": {"retention": "none",
                                       "storage": "workspace-ephemeral"},
                "repair_of": {"missing_indices": [2, 4, 8, 10]},
                "slurm": {"array_throttle": 125},
            }
            (repair / "manifest.json").write_text(json.dumps(manifest))
            (repair / "launch.json").write_text(
                json.dumps({"array_job_id": "12345"}))
            (repair / "pins/frozen_baseline.py").write_text("runner")
            (repair / "results/00002.json").write_text("{}")
            (repair / "claims/00002.json").write_text("published claim")
            (repair / "claims/00004.json").write_text("stale four")
            (repair / "claims/00008.json").write_text("stale eight")
            checked = []
            round_dir = prepare_continuation(
                repair, terminal_check=lambda job: checked.append(job))
            self.assertEqual(checked, ["12345"])
            record = json.loads((round_dir / "round.json").read_text())
            self.assertEqual(record["missing_indices"], [4, 8, 10])
            self.assertEqual([x["task"] for x in record["stale_claims_removed"]],
                             [4, 8])
            self.assertIn("#SBATCH --array=4,8,10%125",
                          (round_dir / "array.sbatch").read_text())
            self.assertIn("trap cleanup_scratch EXIT",
                          (round_dir / "array.sbatch").read_text())
            self.assertTrue((repair / "claims/00002.json").is_file())
            self.assertFalse((repair / "claims/00004.json").exists())
            self.assertFalse((repair / "claims/00008.json").exists())

            # The outer shell owns a final deletion attempt even when the
            # immutable Python runner exits before publishing cleanup state.
            (repair / "pins/frozen_baseline.py").write_text(
                "import sys\nsys.exit(7)\n")
            abandoned = repair / "scratch/00004"
            abandoned.mkdir()
            (abandoned / "proof.out").write_text("partial")
            environment = os.environ.copy()
            environment["SLURM_ARRAY_TASK_ID"] = "4"
            ran = subprocess.run(["bash", str(round_dir / "array.sbatch")],
                                 env=environment, capture_output=True, text=True)
            self.assertEqual(ran.returncode, 7, ran.stderr)
            self.assertFalse(abandoned.exists())

    def test_continuation_refuses_live_or_unclean_predecessor(self):
        with tempfile.TemporaryDirectory() as directory:
            repair = Path(directory) / "repair"
            for name in ("results", "pending", "claims", "scratch"):
                (repair / name).mkdir(parents=True)
            manifest = {
                "kind": "cryptominisat-tierd-cpuq-workspace-repair-v1",
                "certificate_policy": {"retention": "none",
                                       "storage": "workspace-ephemeral"},
                "repair_of": {"missing_indices": [1]},
                "slurm": {"array_throttle": 125},
            }
            (repair / "manifest.json").write_text(json.dumps(manifest))
            (repair / "launch.json").write_text('{"array_job_id":"99"}')
            with self.assertRaisesRegex(RuntimeError, "still live"):
                prepare_continuation(
                    repair,
                    terminal_check=lambda job: (_ for _ in ()).throw(
                        RuntimeError("prior array 99 is still live")))
            (repair / "pending/00001.json").write_text("{}")
            with self.assertRaisesRegex(RuntimeError, "cleanup incomplete"):
                prepare_continuation(repair, terminal_check=lambda job: None)

    def test_continuation_refuses_legacy_active_quota_killer(self):
        with tempfile.TemporaryDirectory() as directory:
            repair = Path(directory) / "repair"
            for name in ("results", "pending", "claims", "scratch"):
                (repair / name).mkdir(parents=True)
            manifest = {
                "kind": "cryptominisat-tierd-cpuq-workspace-repair-v1",
                "certificate_policy": {"retention": "none",
                                       "storage": "workspace-ephemeral"},
                "repair_of": {"missing_indices": [1]},
                "limits": {"workspace_quota_stop_gib": 750},
                "slurm": {"array_throttle": 125},
                "provenance": {"workspace-quota": {"sha256": "legacy"}},
            }
            (repair / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(RuntimeError, "active quota killer"):
                prepare_continuation(repair, terminal_check=lambda job: None)


if __name__ == "__main__":
    unittest.main()

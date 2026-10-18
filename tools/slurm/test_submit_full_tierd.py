import unittest
from pathlib import Path
import sys
import json
import subprocess
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import submit_full_tierd as launcher
from submit_full_tierd import array_indices, array_partition, workspace_contract


class ArrayAdmissionTest(unittest.TestCase):
    def test_full_and_declared_repair(self):
        self.assertEqual(array_indices("#SBATCH --array=0-1165%8\n", {}),
                         list(range(1166)))
        repair = {"repair_of": {"missing_indices": [12, 13, 15, 41]}}
        self.assertEqual(array_indices("#SBATCH --array=12-13,15,41%8\n", repair),
                         [12, 13, 15, 41])

    def test_partial_or_mismatched_array_cannot_bypass_full_run_contract(self):
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12-13\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165%0\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165%126\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12-13,15%8\n",
                          {"repair_of": {"missing_indices": [12, 13, 15, 41]}})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12,12%8\n",
                          {"repair_of": {"missing_indices": [12]}})

    def test_prepared_continuation_can_be_a_strict_repair_subset(self):
        repair = {"repair_of": {"missing_indices": [12, 13, 15, 41]}}
        self.assertEqual(
            array_indices("#SBATCH --array=13,41%125\n", repair, [13, 41]),
            [13, 41])
        with self.assertRaisesRegex(RuntimeError, "prepared missing rows"):
            array_indices("#SBATCH --array=13,15,41%125\n", repair, [13, 41])

    def test_cpu_q_is_mandatory(self):
        self.assertEqual(array_partition("#SBATCH --partition=cpu-q\n"),
                         "cpu-q")
        with self.assertRaisesRegex(RuntimeError, "must use cpu-q"):
            array_partition("#SBATCH --partition=hwu\n")

    def test_workspace_contract_rejects_node_local_storage(self):
        campaign = Path("/home/hwu/SAT-solving/benchmarks/runs/example")
        workspace_contract(campaign, "export TMPDIR=/home/hwu/SAT-solving/x\n",
                           {"certificate_policy": {"storage": "workspace-ephemeral"}})
        workspace_contract(campaign, "", {"scratch_directory": str(campaign / "scratch")})
        with self.assertRaisesRegex(RuntimeError, "node-local"):
            workspace_contract(campaign, "export TMPDIR=/tmp\n",
                               {"certificate_policy": {"storage": "workspace-ephemeral"}})
        with self.assertRaisesRegex(RuntimeError, "lacks"):
            workspace_contract(campaign, "", {})

    def test_submission_starts_whole_array_and_read_only_monitor(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory) / "campaign"
            campaign.mkdir()
            (campaign / "manifest.json").write_text(json.dumps({
                "certificate_policy": {"storage": "workspace-ephemeral"}}))
            array = campaign / "array.sbatch"
            array.write_text(
                "#!/usr/bin/env bash\n#SBATCH --partition=cpu-q\n"
                "#SBATCH --array=0-1165%125\n")
            calls = []

            def run(command, **_kwargs):
                calls.append(command)
                job = "100" if len(calls) == 1 else "200"
                return subprocess.CompletedProcess(command, 0, job + "\n", "")

            with (patch.object(launcher, "read_usage", return_value=(500.0, "test")),
                  patch.object(launcher.subprocess, "run", side_effect=run)):
                launcher.submit(campaign, array)
            self.assertEqual(calls[0][:2], ["sbatch", "--parsable"])
            self.assertNotIn("--hold", calls[0])
            self.assertTrue(all(call[0] == "sbatch" for call in calls))
            launch = json.loads((campaign / "launch.json").read_text())
            self.assertEqual(launch["array_job_id"], "100")
            self.assertEqual(launch["monitor_job_id"], "200")
            monitor = campaign / "storage-monitor"
            self.assertIn("--job 100", (monitor / "monitor.sbatch").read_text())
            self.assertNotIn("scontrol", (monitor / "quota_monitor.py").read_text())


if __name__ == "__main__":
    unittest.main()

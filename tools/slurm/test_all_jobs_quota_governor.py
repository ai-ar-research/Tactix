"""Tests for the dynamic all-user Slurm quota governor."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.slurm import all_jobs_quota_governor as GOVERNOR


class FakeSlurm:
    def __init__(self):
        self.control = []

    def __call__(self, command, **_kwargs):
        if command[0] == "squeue" and "-t" not in command:
            output = ("999|RUNNING\n200|RUNNING\n200|PENDING\n"
                      "300|PENDING\n")
        elif command[0] == "squeue":
            output = ("999|Priority\n200_2|Priority\n"
                      "300|JobHeldUser\n")
        elif command[0] == "scontrol":
            self.control.append(command[1:])
            output = ""
        else:
            raise AssertionError(command)
        return subprocess.CompletedProcess(command, 0, output, "")


class AllJobsQuotaGovernor(unittest.TestCase):
    def test_holds_only_pending_nonself_and_never_running(self):
        with tempfile.TemporaryDirectory() as root:
            slurm = FakeSlurm()
            finished = GOVERNOR.sample(
                Path(root), "hwu", {"999"}, 750, 700, 40, run=slurm,
                usage_reader=lambda: (800.0, "test"), now="now")
            self.assertFalse(finished)
            self.assertEqual(slurm.control, [["hold", "200_2"]])
            state = json.loads(
                (Path(root) / "quota-all-jobs-state.json").read_text())
            self.assertEqual(state, {"held": ["200_2"], "mode": "held"})
            record = json.loads(
                (Path(root) / "quota-all-jobs.log.jsonl").read_text())
            self.assertNotIn("999", record["all_user_job_states"])
            self.assertEqual(record["changed_tasks"], ["200_2"])
            self.assertNotIn("200_1", json.dumps(record))

    def test_releases_only_its_own_holds_below_lower_threshold(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            (directory / "quota-all-jobs-state.json").write_text(json.dumps(
                {"mode": "held", "held": ["200_2"]}))
            slurm = FakeSlurm()
            GOVERNOR.sample(
                directory, "hwu", {"999"}, 750, 700, 40, run=slurm,
                usage_reader=lambda: (650.0, "test"), now="now")
            self.assertEqual(slurm.control, [["release", "200_2"]])
            self.assertNotIn("300", json.dumps(slurm.control))


if __name__ == "__main__":
    unittest.main()

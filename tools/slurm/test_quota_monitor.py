"""Read-only storage monitor tests; no real Slurm or quota calls."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import quota_monitor as monitor


class MonitorTest(unittest.TestCase):
    def test_sample_records_usage_without_scheduler_mutation(self):
        calls = []

        def run(command, **_kwargs):
            calls.append(command)
            return subprocess.CompletedProcess(command, 0, "123_4\n", "")

        with tempfile.TemporaryDirectory() as directory:
            done = monitor.sample(
                Path(directory), "123", "hwu", run=run,
                usage_reader=lambda _user: (526.0, "test"), now="now")
            self.assertFalse(done)
            record = json.loads(
                (Path(directory) / "quota.log.jsonl").read_text())
            self.assertEqual(record["usage_gib"], 526.0)
            self.assertEqual(calls, [["squeue", "-h", "-j", "123", "-o", "%i"]])

    def test_monitor_finishes_after_array_leaves_queue(self):
        def run(command, **_kwargs):
            return subprocess.CompletedProcess(command, 1, "", "Invalid job id")

        with tempfile.TemporaryDirectory() as directory:
            self.assertTrue(monitor.sample(
                Path(directory), "123", "hwu", run=run,
                usage_reader=lambda _user: (600.0, "test"), now="now"))


if __name__ == "__main__":
    unittest.main()

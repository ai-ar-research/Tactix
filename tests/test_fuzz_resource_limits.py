#!/usr/bin/env python3
"""Resource ceilings must classify incomplete fuzz cases, never fake crashes."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "tools" / "fuzz" / "memory_runner.py"
sys.path.insert(0, str(ROOT / "tools" / "fuzz"))
import fuzz  # noqa: E402


def invoke(code, memory_mib=64, timeout=5):
    with tempfile.TemporaryDirectory() as directory:
        status = Path(directory) / "status.json"
        result = subprocess.run([
            sys.executable, str(RUNNER), "--status", str(status),
            "--memory-mib", str(memory_mib), "--timeout", str(timeout),
            "--cpu-seconds", "10", "--", sys.executable, "-c", code,
        ], capture_output=True, text=True, timeout=15)
        return result, json.loads(status.read_text())


class MemoryRunner(unittest.TestCase):
    def test_normal_exit_is_preserved(self):
        result, status = invoke("raise SystemExit(10)")
        self.assertEqual(result.returncode, 10)
        self.assertEqual(status["returncode"], 10)
        self.assertFalse(status["memory_exceeded"])
        self.assertFalse(status["timed_out"])

    def test_resident_memory_limit_is_distinct(self):
        result, status = invoke(
            "import time; x=bytearray(128*1024*1024); time.sleep(10)",
            memory_mib=32)
        self.assertEqual(result.returncode, 86)
        self.assertTrue(status["memory_exceeded"])
        self.assertFalse(status["timed_out"])
        self.assertGreater(status["peak_rss_kib"], 32 * 1024)

    def test_wall_timeout_is_distinct(self):
        result, status = invoke("import time; time.sleep(10)", timeout=.1)
        self.assertEqual(result.returncode, 124)
        self.assertTrue(status["timed_out"])
        self.assertFalse(status["memory_exceeded"])

    def test_descendant_memory_counts_toward_the_same_limit(self):
        code = (
            "import subprocess,sys,time; "
            "subprocess.Popen([sys.executable,'-c',"
            "'import time; x=bytearray(96*1024*1024); time.sleep(10)']); "
            "time.sleep(10)")
        result, status = invoke(code, memory_mib=64)
        self.assertEqual(result.returncode, 86)
        self.assertTrue(status["memory_exceeded"])
        self.assertGreater(status["peak_rss_kib"], 64 * 1024)


class FuzzResourceDispatch(unittest.TestCase):
    def test_checker_oom_is_resource_not_proof_or_crash(self):
        calls = 0

        def limited(command, timeout, memory_mib, env=None):
            nonlocal calls
            calls += 1
            if calls == 1:
                Path(command[-1]).write_text(
                    "pseudo-Boolean proof version 3.0\n"
                    "output NONE ;\nconclusion UNSAT : -1 ;\n"
                    "end pseudo-Boolean proof ;\n")
                return dict(returncode=20, stdout="s UNSATISFIABLE\n",
                            stderr="", timed_out=False,
                            memory_exceeded=False,
                            memory_limit_mib=memory_mib, peak_rss_kib=100,
                            cgroup_oom_kills=0, runner_error=None)
            return dict(returncode=-9, stdout="", stderr="",
                        timed_out=False, memory_exceeded=True,
                        memory_limit_mib=memory_mib,
                        peak_rss_kib=memory_mib * 1024 + 1,
                        cgroup_oom_kills=0, runner_error=None)

        with tempfile.TemporaryDirectory() as directory:
            cnf = Path(directory) / "input.cnf"
            cnf.write_text("p cnf 1 2\n1 0\n-1 0\n")
            with patch.object(fuzz, "run_limited_cmd", side_effect=limited):
                outcome = fuzz.run_case(
                    str(cnf), ["tactix", "--pbp"], None, 5, False,
                    directory)
        kinds = [kind for kind, _ in outcome.failures]
        self.assertEqual(kinds, ["resource"])
        self.assertEqual(outcome.checker_runs[-1]["status"],
                         "resource-memory")
        self.assertNotIn("proof", kinds)
        self.assertNotIn("crash", kinds)

    def test_checker_replay_rejects_modified_retained_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            artifact = bundle / "case.cnf"
            artifact.write_text("p cnf 1 1\n1 0\n")
            metadata = bundle / "meta.json"
            metadata.write_text(json.dumps({
                "artifacts": {"case.cnf": fuzz.file_sha256(artifact)},
                "checker_runs": [{
                    "command": ["/bin/true", str(artifact)],
                    "status": "resource-memory",
                }],
            }))
            artifact.write_text("p cnf 1 1\n-1 0\n")
            arguments = SimpleNamespace(
                meta=str(metadata), run_index=None, timeout=1, memory_mib=64)
            self.assertEqual(fuzz.cmd_replay_checker(arguments), 2)

    def test_checker_replay_uses_copies_and_closes_only_terminal_stage(self):
        self.assertTrue(fuzz.terminal_checker_stage(
            "cake-pb-cnf", ["cake-pb-cnf", "proof.pbp"]))
        self.assertFalse(fuzz.terminal_checker_stage(
            "veripb", ["veripb", "proof.pbp"]))
        self.assertFalse(fuzz.terminal_checker_stage(
            "drat-trim", ["drat-trim", "input.cnf", "proof.drat",
                          "-L", "proof.lrat"]))
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            cnf = bundle / "case.cnf"
            proof = bundle / "proof.kernel.pbp"
            cnf.write_text("p cnf 1 2\n1 0\n-1 0\n")
            proof.write_text("retained kernel proof\n")
            artifacts = {
                path.name: fuzz.file_sha256(path) for path in (cnf, proof)
            }
            metadata = bundle / "meta.json"
            metadata.write_text(json.dumps({
                "artifacts": artifacts,
                "checker_runs": [{
                    "command": [fuzz.CAKE_PB, str(cnf), str(proof)],
                    "status": "resource-memory",
                }],
            }))
            seen = []

            def verified(command, timeout, memory_mib, env=None):
                seen.extend(command)
                Path(command[-1]).write_text("disposable checker scratch\n")
                return dict(returncode=0, stdout="s VERIFIED UNSAT\n",
                            stderr="", timed_out=False,
                            memory_exceeded=False,
                            memory_limit_mib=memory_mib, peak_rss_kib=100,
                            cgroup_oom_kills=0, runner_error=None)

            arguments = SimpleNamespace(
                meta=str(metadata), run_index=None, timeout=1, memory_mib=64)
            with patch.object(fuzz, "run_limited_cmd", side_effect=verified):
                self.assertEqual(fuzz.cmd_replay_checker(arguments), 0)
            self.assertNotEqual(Path(seen[-1]).parent, bundle)
            self.assertEqual(fuzz.file_sha256(proof), artifacts[proof.name])


if __name__ == "__main__":
    unittest.main()

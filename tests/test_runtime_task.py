"""Reject unchecked verdicts, bad exits and changed pins on either runtime arm."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
with patch.object(sys, "argv", ["runtime_task.py", str(ROOT / "benchmarks/runs/test"), "0"]):
    spec = importlib.util.spec_from_file_location("runtime_task", ROOT / "tools/bench/runtime_task.py")
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)


class RuntimeTaskTest(unittest.TestCase):
    def run_case(self, arm, verdict="SAT", solver_exit=10, checker_exit=0,
                 checker_text="s VERIFIED\n", tampered=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            campaign = root / "benchmarks/runs/test"
            for name in ("pins", "claims", "results", "provisional"):
                (campaign / name).mkdir(parents=True)
            cache = root / "benchmarks/cache"
            cache.mkdir()
            (cache / "test.xz").write_bytes(b"fixture")
            (campaign / "pins/solver").write_bytes(b"pin")
            manifest = dict(arms={arm: dict(path="pins/solver", arguments=[], sha256="pin")},
                            input_sha256={"test.xz": runner.sha256(cache / "test.xz")},
                            pin_sha256={"pins/solver": "bad" if tampered else runner.sha256(campaign / "pins/solver")})
            (campaign / "manifest.json").write_text(json.dumps(manifest))
            (campaign / "matrix.tsv").write_text(
                "task\trank\tarm\ttier\tinstance\texpected\n"
                f"0\t1\t{arm}\tD\ttest.xz\t{verdict}\n")
            calls = []

            def measured(command, stdout, stderr, metrics, timeout):
                calls.append(command)
                stderr.write_text("")
                if len(calls) == 1:
                    stdout.write_text("s " + ("SATISFIABLE" if verdict == "SAT" else "UNSATISFIABLE") + "\n")
                    Path(command[-1]).write_bytes(b"proof")
                    code = solver_exit
                else:
                    stdout.write_text(checker_text)
                    code = checker_exit
                return dict(exit_code=code, wall_seconds=1, peak_rss_kb=1, timed_out=False)

            with patch.object(runner, "ROOT", root), patch.object(runner, "CAMPAIGN", campaign), \
                 patch.object(sys, "argv", ["runtime_task.py", str(campaign), "0"]), \
                 patch.dict(os.environ, {"SLURM_JOB_ID": "test", "SLURM_TMPDIR": temporary}), \
                 patch.object(runner.subprocess, "run"), patch.object(runner, "measured", measured):
                code = runner.main()
            result = json.loads((campaign / "results/000.json").read_text())
            self.assertTrue(result["artifact_deleted"])
            return code, result, calls

    def test_both_arms_are_checked(self):
        for arm in ("tactix", "kissat"):
            code, result, calls = self.run_case(arm)
            self.assertEqual(code, 0)
            self.assertTrue(result["valid"])
            self.assertEqual(len(calls), 2)
            self.assertEqual(Path(calls[1][0]).name, "checkmodel")

    def test_bad_model_or_exit_is_not_valid(self):
        for arm in ("tactix", "kissat"):
            for kwargs in (dict(checker_exit=1), dict(solver_exit=0)):
                code, result, _ = self.run_case(arm, **kwargs)
                self.assertEqual(code, 1)
                self.assertFalse(result["valid"])

    def test_unsat_requires_verification_marker_and_exit(self):
        for arm in ("tactix", "kissat"):
            for text, check_exit, valid in (("s VERIFIED\n", 0, True),
                                           ("s NOT VERIFIED\n", 0, False),
                                           ("s VERIFIED\n", 1, False), ("", 0, False)):
                code, result, calls = self.run_case(arm, "UNSAT", 20, check_exit, text)
                self.assertEqual(result["valid"], valid)
                self.assertEqual(code, 0 if valid else 1)
                self.assertEqual(Path(calls[1][0]).name, "drat-trim")

    def test_changed_pin_stops_before_solver(self):
        for arm in ("tactix", "kissat"):
            code, result, calls = self.run_case(arm, tampered=True)
            self.assertEqual(code, 1)
            self.assertEqual(result["verdict"], "INFRA_ERROR")
            self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()

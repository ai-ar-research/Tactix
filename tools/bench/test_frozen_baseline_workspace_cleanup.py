"""CMS-style workspace scratch is removed after solver and checker timeouts."""

import json
import lzma
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import frozen_baseline as runner


class WorkspaceTimeoutCleanupTest(unittest.TestCase):
    def campaign(self, root, solver_text, solve_limit=0.05):
        campaign, cache = root / "campaign", root / "cache"
        for name in ("pins", "results", "pending", "claims", "scratch"):
            (campaign / name).mkdir(parents=True, exist_ok=True)
        cache.mkdir()
        (cache / "case.cnf.xz").write_bytes(lzma.compress(b"p cnf 1 1\n1 0\n"))
        solver = campaign / "pins/solver"
        solver.write_text("#!/usr/bin/env bash\nset -eu\n" + solver_text)
        solver.chmod(0o755)
        manifest = {
            "certificate_policy": {"retention": "none",
                                   "storage": "workspace-ephemeral"},
            "limits": {"solve_wall_seconds": solve_limit,
                       "checker_pipeline_wall_seconds": 1,
                       "scheduler_memory_gib": 32},
            "tasks": [{"instance": 0, "solver": 0}],
            "instances": [{"rel": "case.cnf.xz", "expected": "UNKNOWN"}],
            "solvers": [{"name": "cms", "kind": "positional",
                         "certificate": "frat-xor",
                         "executable": "pins/solver",
                         "vbs_eligible": "SAT+UNSAT"}],
            "checkers": {}, "provenance": {},
        }
        (campaign / "manifest.json").write_text(json.dumps(manifest))
        return campaign, cache

    def test_solver_timeout_removes_expanded_input_and_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign, cache = self.campaign(root, "sleep 5\n")
            with patch.object(runner, "CACHE", cache):
                self.assertEqual(runner.run_one(campaign, 0), 0)
            result = json.loads((campaign / "results/00000.json").read_text())
            self.assertTrue(result["solve"]["timed_out"])
            self.assertTrue(result["scratch_deleted"])
            self.assertFalse(any((campaign / "scratch").iterdir()))

    def test_checker_timeout_removes_all_workspace_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign, cache = self.campaign(
                root, 'printf "s UNSATISFIABLE\\n"\nprintf proof > "$2"\n', 2)

            def timed_out_check(_man, _campaign, _solver, _cnf, _proof, work):
                derived = Path(work) / "proof.xlrup"
                derived.write_text("partial")
                stage = runner.measured(
                    ["bash", "-c", "sleep 5"], Path(work) / "check.out",
                    Path(work) / "check.err", 0.05, Path(work) / "check.time",
                    dict(os.environ, TMPDIR=str(Path(work) / "tmp")))
                return {"result": "not-verified", "stages": [stage]}, [derived]

            with (patch.object(runner, "CACHE", cache),
                  patch.object(runner, "check_proof", side_effect=timed_out_check)):
                self.assertEqual(runner.run_one(campaign, 0), 1)
            result = json.loads((campaign / "results/00000.json").read_text())
            self.assertTrue(result["proof_check"]["stages"][0]["timed_out"])
            self.assertTrue(result["scratch_deleted"])
            self.assertFalse(any((campaign / "scratch").iterdir()))


if __name__ == "__main__":
    unittest.main()

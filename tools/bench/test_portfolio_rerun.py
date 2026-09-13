import copy
from contextlib import redirect_stdout
import csv
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

from portfolio_rerun import (
    MEMORY_MIB,
    SOLVE_SECONDS,
    adapter_lymphosat,
    adapter_satsuma,
    batch_text,
    bound_launch,
    build_matrix,
    checked_valid,
    classify_result,
    sha256,
    verify_selection,
)


class PortfolioRerunTests(unittest.TestCase):
    def targets(self):
        return [
            dict(
                target=i,
                instance=f"i{i}.cnf.xz",
                expected="SAT" if i < 10 else "UNSAT",
                baseline_solver="lymphosat" if i < 10 else "satsuma-kissat",
                baseline_task=i,
                tactix_task=i,
            )
            for i in range(20)
        ]

    def test_matrix_is_instance_major_and_exactly_paired(self):
        rows = build_matrix(self.targets())
        self.assertEqual(len(rows), 40)
        self.assertEqual([row["task"] for row in rows], list(range(40)))
        for target in range(20):
            pair = rows[2 * target : 2 * target + 2]
            self.assertEqual([row["side"] for row in pair], ["tactix", "baseline"])
            self.assertEqual({row["instance"] for row in pair}, {f"i{target}.cnf.xz"})

    def test_batch_is_full_width_and_exact_resource_shape(self):
        text = batch_text(Path("/tmp/repo/benchmarks/runs/campaign"), 40)
        for line in (
            "#SBATCH --array=0-39",
            "#SBATCH --cpus-per-task=12",
            "#SBATCH --mem=32768M",
            "#SBATCH --partition=cpu-dense-hwulab-q",
        ):
            self.assertIn(line, text)
        self.assertNotIn(
            "%",
            next(
                line for line in text.splitlines() if line.startswith("#SBATCH --array")
            ),
        )
        self.assertNotIn("--nodelist", text)
        self.assertIn("#SBATCH --chdir=/tmp/repo", text)

    def test_launch_binding_rejects_other_jobs_and_tasks(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            (campaign / "manifest.json").write_text("{}")
            record = {
                "job_id": "123",
                "manifest_sha256": sha256(campaign / "manifest.json"),
            }
            (campaign / "launch.json").write_text(json.dumps(record))
            environment = {
                "SLURM_JOB_ID": "124",
                "SLURM_ARRAY_JOB_ID": "123",
                "SLURM_ARRAY_TASK_ID": "7",
            }
            with patch.dict(os.environ, environment, clear=True):
                self.assertEqual(bound_launch(campaign, task=7, wait_seconds=0), record)
                with self.assertRaisesRegex(ValueError, "task identity"):
                    bound_launch(campaign, task=8, wait_seconds=0)
                os.environ["SLURM_ARRAY_JOB_ID"] = "999"
                with self.assertRaisesRegex(ValueError, "job identity"):
                    bound_launch(campaign, task=7, wait_seconds=0)

            (campaign / "validation-launch.json").write_text(json.dumps(record))
            with patch.dict(os.environ, {"SLURM_JOB_ID": "123"}, clear=True):
                self.assertEqual(
                    bound_launch(campaign, validation=True, wait_seconds=0), record
                )
                os.environ["SLURM_JOB_ID"] = "999"
                with self.assertRaisesRegex(ValueError, "validation job identity"):
                    bound_launch(campaign, validation=True, wait_seconds=0)

    def test_only_checked_solve_or_clean_full_timeout_is_scoreable(self):
        solve = {"exit_code": 10, "timed_out": False, "wall_seconds": 0.1}
        self.assertTrue(checked_valid("SAT", "SAT", solve, True))
        self.assertFalse(checked_valid("UNSAT", "SAT", solve, True))
        timeout = {
            "verdict": "UNKNOWN",
            "valid": False,
            "solve": {
                "exit_code": None,
                "timed_out": True,
                "wall_seconds": SOLVE_SECONDS,
                "peak_rss_kb": None,
            },
            "memory_enforcement": "slurm_task_cgroup",
            "proof_check": None,
            "model_check": None,
            "artifact_deleted": True,
            "derived_artifacts_deleted": True,
            "cleanup_pending": False,
        }
        self.assertEqual(classify_result(timeout), "timeout")
        for path, value in (("artifact_deleted", False), ("cleanup_pending", True)):
            bad = copy.deepcopy(timeout)
            bad[path] = value
            self.assertEqual(classify_result(bad), "invalid")
        timeout["solve"]["wall_seconds"] = SOLVE_SECONDS - 0.001
        self.assertEqual(classify_result(timeout), "invalid")

    def test_satsuma_adapter_preserves_preprocess_then_solver_order(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            (campaign / "manifest.json").write_text(
                json.dumps(
                    {
                        "arms": {
                            "satsuma-kissat": {
                                "path": "pins/satsuma-kissat",
                                "transport_shim": "pins/procfd_realpath.so",
                            }
                        }
                    }
                )
            )
            calls = []
            environments = []

            def fake_run(command, **unused):
                calls.append(list(map(str, command)))
                environments.append(unused.get("env"))
                return SimpleNamespace(returncode=0 if len(calls) == 1 else 20)

            with (
                patch("portfolio_rerun.subprocess.run", side_effect=fake_run),
                redirect_stdout(io.StringIO()),
            ):
                rc = adapter_satsuma(
                    campaign,
                    "satsuma-kissat",
                    "input",
                    campaign / "proof",
                    campaign / "output",
                )
            self.assertEqual(rc, 20)
            self.assertTrue(calls[0][0].endswith("/satsuma"))
            self.assertIn("--proof-file", calls[0])
            self.assertTrue(calls[1][0].endswith("/solver/kissat/build/kissat"))
            self.assertTrue(calls[1][1].endswith("/output"))
            self.assertTrue(
                environments[1]["LD_PRELOAD"].endswith("/procfd_realpath.so")
            )

    def test_lymphosat_adapter_uses_first_matching_exact_subsolver(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            (campaign / "manifest.json").write_text(
                json.dumps({"arms": {"lymphosat": {"path": "pins/lymphosat"}}})
            )
            proof, profile = campaign / "proof", campaign / "profile"
            attempt_out, attempt_err = (
                campaign / "attempt.out",
                campaign / "attempt.err",
            )
            calls = []

            def fake_run(command, **unused):
                command = list(map(str, command))
                calls.append(command)
                if "--list-matchers" in command:
                    return SimpleNamespace(
                        returncode=0, stdout="first\nsecond\n", stderr=""
                    )
                if "--check-match" in command:
                    return SimpleNamespace(
                        returncode=0,
                        stdout="1\n" if command[-1] == "second" else "0\n",
                        stderr="",
                    )
                if command[0].endswith("/solvers/second/solver/solver"):
                    return SimpleNamespace(returncode=10)
                return SimpleNamespace(returncode=0)

            with patch("portfolio_rerun.subprocess.run", side_effect=fake_run):
                rc = adapter_lymphosat(
                    campaign,
                    "lymphosat",
                    "input",
                    proof,
                    profile,
                    attempt_out,
                    attempt_err,
                )
            self.assertEqual(rc, 10)
            attempts = [call[0] for call in calls if "/solvers/" in call[0]]
            self.assertEqual(
                attempts,
                [str(campaign / "pins/lymphosat/solver/solvers/second/solver/solver")],
            )

    def test_selection_verification_is_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline, tactix = root / "baseline", root / "tactix"
            (root / "tools/bench").mkdir(parents=True)
            baseline.mkdir()
            tactix.mkdir()
            (tactix / "results").mkdir()
            (root / "tools/bench/portfolio_targets.py").write_text("# selector\n")
            (baseline / "cache.sha256").write_text("")
            solvers = [dict(name="lymphosat"), dict(name="satsuma-kissat")]
            bm = {
                "tasks": [{} for _ in range(20)],
                "solvers": solvers,
                "cache_manifest_sha256": sha256(baseline / "cache.sha256"),
            }
            (baseline / "manifest.json").write_text(json.dumps(bm))
            baseline_results = []
            selected = []
            matrix = []
            for task in range(20):
                verdict = "SAT" if task < 10 else "UNSAT"
                solver = "lymphosat" if verdict == "SAT" else "satsuma-kissat"
                instance = f"i{task}.cnf.xz"
                baseline_results.append(
                    {
                        "task": task,
                        "instance": instance,
                        "solver": solver,
                        "verdict": verdict,
                        "valid": True,
                        "artifact_deleted": True,
                        "derived_artifacts_deleted": True,
                        "cleanup_pending": False,
                        "solve": {"wall_seconds": float(task + 1)},
                    }
                )
                matrix.append(
                    {
                        "task": task,
                        "arm": "tactix",
                        "instance": instance,
                        "expected": verdict,
                    }
                )
                record = {
                    "verdict": "UNKNOWN",
                    "artifact_deleted": True,
                    "cleanup_pending": False,
                    "solve": {"timed_out": True, "wall_seconds": 1200.0},
                }
                record_path = tactix / "results" / f"{task:03d}.json"
                record_path.write_text(json.dumps(record))
                selected.append(
                    {
                        "instance": instance,
                        "verdict": verdict,
                        "solvers": [solver],
                        "fastest_solver": solver,
                        "fastest_seconds": float(task + 1),
                        "baseline_tasks": [task],
                        "tactix_task": task,
                        "tactix_record": str(record_path),
                    }
                )
            baseline_aggregate = {
                "complete": True,
                "results": baseline_results,
                "manifest_sha256": sha256(baseline / "manifest.json"),
            }
            (baseline / "results.json").write_text(json.dumps(baseline_aggregate))
            (baseline / "validation.json").write_text("{}")
            tactix_manifest = {
                "resource_contract": {
                    "solve_timeout": SOLVE_SECONDS,
                    "memory_mib": MEMORY_MIB,
                },
                "dataset_authority": {"sha256": sha256(baseline / "manifest.json")},
            }
            (tactix / "manifest.json").write_text(json.dumps(tactix_manifest))
            with (tactix / "matrix.tsv").open("w", newline="") as target:
                writer = csv.DictWriter(
                    target, fieldnames=list(matrix[0]), delimiter="\t"
                )
                writer.writeheader()
                writer.writerows(matrix)
            evidence_paths = [
                baseline / "manifest.json",
                baseline / "results.json",
                baseline / "validation.json",
                tactix / "manifest.json",
                tactix / "matrix.tsv",
                root / "tools/bench/portfolio_targets.py",
                *(tactix / "results" / f"{task:03d}.json" for task in range(20)),
            ]
            selection = {
                "schema": "tactix-winner-coverage-selection-v1",
                "selection_final": True,
                "seconds": SOLVE_SECONDS,
                "memory_kib": MEMORY_MIB * 1024,
                "per_status": 10,
                "tactix_classes": {
                    "solver_pending_or_missing": 0,
                    "timeout_cleanup_pending": 0,
                },
                "selected": selected,
                "evidence_sha256": {str(path): sha256(path) for path in evidence_paths},
            }
            selection_path = root / "selection.json"
            selection_path.write_text(json.dumps(selection))
            _, _, _, targets = verify_selection(
                selection_path, baseline, tactix, root, recompute=False
            )
            self.assertEqual(len(targets), 20)
            selection["tactix_classes"]["solver_pending_or_missing"] = 1
            selection_path.write_text(json.dumps(selection))
            with self.assertRaisesRegex(ValueError, "unresolved"):
                verify_selection(
                    selection_path, baseline, tactix, root, recompute=False
                )
            selection["tactix_classes"]["solver_pending_or_missing"] = 0
            selection["selected"][0]["fastest_solver"] = "satsuma-kissat"
            selection_path.write_text(json.dumps(selection))
            with self.assertRaisesRegex(ValueError, "fastest baseline"):
                verify_selection(
                    selection_path, baseline, tactix, root, recompute=False
                )


if __name__ == "__main__":
    unittest.main()

"""Reject unchecked verdicts, bad exits and changed pins on either runtime arm."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/bench"))
with patch.object(sys, "argv", ["runtime_task.py", str(ROOT / "benchmarks/runs/test"), "0"]):
    spec = importlib.util.spec_from_file_location("runtime_task", ROOT / "tools/bench/runtime_task.py")
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)


class RuntimeTaskTest(unittest.TestCase):
    def run_case(self, arm, verdict="SAT", solver_exit=10, checker_exit=0,
                 checker_text="s VERIFIED\n", tampered=False,
                 omit_modelcheck=False, certificate="drat"):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            campaign = root / "benchmarks/runs/test"
            for name in ("pins", "claims", "results", "provisional"):
                (campaign / name).mkdir(parents=True)
            cache = root / "benchmarks/cache"
            cache.mkdir()
            (cache / "test.xz").write_bytes(b"fixture")
            (campaign / "pins/solver").write_bytes(b"pin")
            checker_names = ["checkmodel", "modelcheck.py"]
            checker_names += (["drat-trim"] if certificate == "drat" else
                              ["dsr-trim", "dsr-compress", "srcheck",
                               "drat-trim", "cake_lpr"])
            for name in checker_names:
                (campaign / "pins" / name).write_bytes(b"pin")
            pin_sha256 = {
                "pins/solver": "bad" if tampered else runner.sha256(campaign / "pins/solver"),
                "pins/checkmodel": runner.sha256(campaign / "pins/checkmodel"),
                "pins/modelcheck.py": runner.sha256(campaign / "pins/modelcheck.py"),
            }
            for name in checker_names:
                pin_sha256[f"pins/{name}"] = runner.sha256(
                    campaign / "pins" / name)
            if omit_modelcheck:
                del pin_sha256["pins/modelcheck.py"]
            manifest = dict(arms={arm: dict(path="pins/solver", arguments=[],
                                           sha256="pin", certificate=certificate)},
                            input_sha256={"test.xz": runner.sha256(cache / "test.xz")},
                            pin_sha256=pin_sha256,
                            resource_contract={"solve_timeout": 1,
                                               "checker_timeout": 1})
            (campaign / "manifest.json").write_text(json.dumps(manifest))
            (campaign / "matrix.tsv").write_text(
                "task\trank\tarm\ttier\tinstance\texpected\n"
                f"0\t1\t{arm}\tD\ttest.xz\t{verdict}\n")
            calls = []

            def decompress(command, **kwargs):
                self.assertEqual(command[:2], ['xz', '-dc'])
                kwargs['stdout'].write(b'p cnf 1 2\n1 0\n-1 0\n' if verdict == 'UNSAT'
                                       else b'p cnf 1 1\n1 0\n')

            def measured(command, stdout, stderr, metrics, timeout, environment=None):
                calls.append(command)
                stderr.write_text("")
                if len(calls) == 1:
                    stdout.write_text(("c proof-route dsr-trestle\n"
                                       if verdict == "UNSAT" and certificate == "dsr" else "") +
                                      "s " + ("SATISFIABLE" if verdict == "SAT" else "UNSATISFIABLE") + "\n")
                    # Binary DSR with a repeated pivot is a witnessed SR line;
                    # the solver declares the Trestle route for this proof.
                    Path(command[-1]).write_bytes(
                        bytes([1, 2, 2, 0]) if certificate == "dsr" else b"proof")
                    code = solver_exit
                else:
                    name = Path(command[0]).name
                    if certificate == "dsr" and name == "dsr-trim":
                        stdout.write_text("s VERIFIED UNSAT\n")
                        Path(command[-1]).write_bytes(b"binary lsr")
                    elif certificate == "dsr" and name == "dsr-compress":
                        stdout.write_text("")
                        Path(command[-1]).write_bytes(b"text lsr")
                    elif certificate == "dsr" and name == "srcheck":
                        stdout.write_text("s VERIFIED UNSAT\n")
                    else:
                        stdout.write_text(checker_text)
                    code = checker_exit
                return dict(exit_code=code, wall_seconds=1, peak_rss_kb=1, timed_out=False)

            with patch.object(runner, "ROOT", root), patch.object(runner, "CAMPAIGN", campaign), \
                 patch.object(sys, "argv", ["runtime_task.py", str(campaign), "0"]), \
                 patch.dict(os.environ, {"SLURM_JOB_ID": "test", "SLURM_TMPDIR": temporary}), \
                 patch.object(runner.subprocess, "run", side_effect=decompress), patch.object(runner, "measured", measured):
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

    def test_dsr_uses_the_reference_pipeline_and_parameters(self):
        code, result, calls = self.run_case(
            "tactix", "UNSAT", 20, certificate="dsr")
        self.assertEqual(code, 0)
        self.assertTrue(result["valid"])
        self.assertEqual(
            [Path(call[0]).name for call in calls[1:]],
            ["dsr-trim", "dsr-compress", "srcheck"])
        # Exactly the Satsuma+Kissat reference invocation: dsr-trim takes no
        # options (backward checking), dsr-compress -d expands its binary LSR,
        # and Trestle reads that text LSR.
        self.assertEqual(len(calls[1]), 4)
        self.assertFalse([arg for arg in map(str, calls[1][1:])
                          if arg.startswith("-")])
        self.assertEqual(str(calls[2][1]), "-d")
        self.assertEqual(str(calls[2][2]), str(calls[1][-1]))
        self.assertEqual(len(calls[3]), 3)
        self.assertEqual(str(calls[3][-1]), str(calls[2][-1]))
        self.assertEqual(result["proof_check"]["pipeline"],
                         "dsr-trim-dsr-compress-trestle")
        self.assertEqual([stage["result"] for stage in
                          result["proof_check"]["stages"]],
                         ["verified", "verified", "verified"])

    def test_changed_pin_stops_before_solver(self):
        for arm in ("tactix", "kissat"):
            code, result, calls = self.run_case(arm, tampered=True)
            self.assertEqual(code, 1)
            self.assertEqual(result["verdict"], "INFRA_ERROR")
            self.assertEqual(calls, [])

    def test_missing_modelcheck_dependency_stops_before_solver(self):
        code, result, calls = self.run_case("tactix", omit_modelcheck=True)
        self.assertEqual(code, 1)
        self.assertEqual(result["verdict"], "INFRA_ERROR")
        self.assertIn("modelcheck.py", result["error"])
        self.assertEqual(calls, [])

    def test_pb_spelling_on_anonymous_scratch(self):
        from anonymous_scratch import AnonymousScratch
        campaign = ROOT / 'benchmarks/runs/20260923-am337-proof-lifetime-v2'
        scratch = AnonymousScratch('/tmp')
        try:
            cnf, proof = scratch / 'original.cnf', scratch / 'proof.pbp'
            original = 'c leading\np cnf 1 2\n1\n0 -1 0\n'
            cnf.write_text(original)
            proof.write_text('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                             'output NONE ;\nconclusion UNSAT : -1 ;\n'
                             'end pseudo-Boolean proof ;\n')
            result = runner.check_unsat_proof(campaign, 'auto', cnf, proof,
                                               scratch, 60, 'pbp')
            self.assertEqual(result['result'], 'verified', result)
            self.assertEqual(result['checker_input']['original_sha256'],
                             runner.sha256(cnf))
            self.assertEqual(cnf.read_text(), original)
            self.assertEqual((scratch / 'checker.cnf').read_text(),
                             'p cnf 1 2\n1 0\n-1 0\n')
            self.assertEqual([stage['result'] for stage in result['stages']],
                             ['verified', 'verified'])
            cnf.write_text('p cnf 1 1\n1 0\n')
            self.assertEqual(runner.check_unsat_proof(
                campaign, 'auto', cnf, proof, scratch, 60, 'pbp')['result'], 'not-verified')
        finally:
            scratch.close()
        self.assertFalse(scratch.exists())


class ExperimentalCheckerProfileTest(unittest.TestCase):
    def test_fixed44_capacity_profile_is_explicit_and_exclusive(self):
        import experimental_tierd as builder
        arguments = SimpleNamespace(
            scratch_capacity_profile='fixed44-exclusive-v1',
            cpus_per_task=4, partition='cpu-q', array_throttle=None,
            certificate='auto', gf2_telemetry=False,
            retain_failed_proof=False, causal_preflight=False)
        rows = [dict(task=index) for index in range(44)]
        selected = builder.selected_scratch_capacity(
            arguments, builder.FIXED44_SELECTION, rows)
        self.assertEqual(selected, builder.FIXED44_SCRATCH_CAPACITY)
        self.assertEqual(
            selected['maximum_file_bytes'] * selected['maximum_live_handles'] +
            selected['reserve_bytes'], selected['minimum_free_bytes'])
        for field, value in (
                ('cpus_per_task', 5), ('partition', 'cpu-dense-hwulab-q'),
                ('array_throttle', 1), ('certificate', 'dsr'),
                ('gf2_telemetry', True), ('retain_failed_proof', True),
                ('causal_preflight', True)):
            changed = SimpleNamespace(**vars(arguments))
            setattr(changed, field, value)
            with self.subTest(field=field), self.assertRaises(ValueError):
                builder.selected_scratch_capacity(
                    changed, builder.FIXED44_SELECTION, rows)
        with self.assertRaises(ValueError):
            builder.selected_scratch_capacity(arguments, None, rows)
        with self.assertRaises(ValueError):
            builder.selected_scratch_capacity(
                arguments, builder.FIXED44_SELECTION, rows[:-1])

    def test_explicit_long_profile_preserves_independent_checker_and_defaults(self):
        import experimental_tierd as builder
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference, frozen, long = (root / name for name in ('ref', 'frozen', 'long'))
            for directory in (reference, frozen, long):
                directory.mkdir()
            baseline = {'checkers': {}, 'provenance': {}}
            for name in ('checkmodel', 'modelcheck.py', 'drat-trim', 'cake-lpr',
                         'veripb', 'cake-pb-cnf'):
                path = reference / name
                path.write_text(name)
                baseline['checkers'][name] = name
                baseline['provenance'][name] = {'sha256': builder.sha(path)}
            frozen_hashes, long_hashes = {}, {}
            for name in ('dsr-trim', 'dsr-compress', 'srcheck'):
                path = frozen / name
                path.write_text('frozen-' + name)
                frozen_hashes[name] = builder.sha(path)
            for name in ('dsr-trim', 'dsr-compress'):
                path = long / ('compress' if name == 'dsr-compress' else name)
                path.write_text('long-' + name)
                long_hashes[name] = builder.sha(path)
            with patch.multiple(builder, FROZEN_DSR=frozen, LONG_DSR=long,
                                FROZEN_DSR_HASHES=frozen_hashes,
                                LONG_DSR_HASHES=long_hashes):
                original = builder.checker_sources(reference, baseline, 'auto')
                selected = builder.checker_sources(reference, baseline, 'auto', 'long64')
                self.assertEqual(original['dsr-trim'], frozen / 'dsr-trim')
                self.assertEqual(selected['dsr-trim'], long / 'dsr-trim')
                self.assertEqual(selected['dsr-compress'], long / 'compress')
                for name in set(original) - set(long_hashes):
                    self.assertEqual(selected[name], original[name])
                for name in ('dsr-trim', 'compress'):
                    path = long / name
                    saved = path.read_bytes()
                    path.write_bytes(b'tampered')
                    with self.assertRaisesRegex(ValueError, 'pin mismatch'):
                        builder.checker_sources(reference, baseline, 'auto', 'long64')
                    path.write_bytes(saved)
                (long / 'compress').unlink()
                with self.assertRaisesRegex(ValueError, 'pin mismatch'):
                    builder.checker_sources(reference, baseline, 'auto', 'long64')
                self.assertEqual(builder.checker_sources(reference, baseline, 'auto'), original)
                with self.assertRaisesRegex(ValueError, 'unsupported DSR checker profile'):
                    builder.checker_sources(reference, baseline, 'auto', 'unqualified')


if __name__ == "__main__":
    unittest.main()

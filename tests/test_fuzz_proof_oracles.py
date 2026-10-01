#!/usr/bin/env python3
"""Exercise the actual fuzz dispatch with fixed proofs and real checkers."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/fuzz'))
import fuzz


def synthetic_route_hint(proof):
    from proof_oracle import certificate_route
    return {'pbp': 'pbp', 'drat': 'dsr-cake_lpr',
            'dsr': 'dsr-trestle'}[certificate_route(proof)]


class FuzzProofOracles(unittest.TestCase):
    def test_pbp_leading_blanks_on_every_executing_surface(self):
        from proof_oracle import check_routed
        from runtime_task import check_unsat_proof
        from anonymous_scratch import AnonymousScratch
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        text = ('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                'output NONE ;\nconclusion UNSAT : -1 ;\n'
                'end pseudo-Boolean proof ;\n')
        for prefix in ('\n', ' \t\r\n', 'c leading comment\n\n'):
            for valid in (False, True):
                raw = prefix + ('p cnf 1 2\n1 0\n-1 0\n' if valid else 'p cnf 1 1\n1 0\n')
                result = self.fixture(raw, text, auto=True)
                self.assertEqual(not result.failures, valid, result.failures)
                for stage in result.checker_runs:
                    self.assertIn('checker.cnf', str(stage['command']))
                with tempfile.TemporaryDirectory() as directory:
                    work = Path(directory)
                    cnf, proof = work/'original.cnf', work/'certificate.pbp'
                    cnf.write_text(raw)
                    proof.write_text(text)
                    original = cnf.read_bytes()
                    checked = check_routed(None, None, None, None, None,
                        cnf, proof, work, 60, veripb=pins/'veripb', cake_pb=pins/'cake-pb-cnf')
                    self.assertEqual(checked[0], valid, checked)
                    for anonymous in (False, True):
                        scratch = AnonymousScratch(directory) if anonymous else work
                        try:
                            checked = check_unsat_proof(campaign, 'auto', cnf, proof,
                                                        scratch, 60, 'pbp')
                            self.assertEqual(checked['result'] == 'verified', valid, checked)
                        finally:
                            if anonymous: scratch.close()
                    self.assertEqual(cnf.read_bytes(), original)
                    self.assertEqual(proof.read_text(), text)

    def test_fuzz_large_wait_has_checked_verdict_and_timeout(self):
        import proof_oracle
        with patch.object(proof_oracle, 'MAX_CAPTURE_WAIT_SECONDS', .04):
            result = fuzz.run_cmd([sys.executable, '-c', 'print("s VERIFIED")'], 2147483647)
            self.assertEqual(result, (0, 's VERIFIED\n', ''))
            result = fuzz.run_cmd([sys.executable, '-c',
                'import time; print("partial",flush=True); time.sleep(60)'], .4)
            self.assertIsNone(result[0])
            self.assertEqual(result[1], 'partial\n')

    def test_input_restatement_maps_rat_hints_and_is_independently_checked(self):
        from proof_oracle import canonical_checker_cnf, restate_duplicate_input_lrat
        import subprocess
        cake = ROOT/'benchmarks/runs/20260923-am341-unwatch/pins/cake_lpr'
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, spelling_path = work/'input.cnf', work/'checker.cnf'
            raw, lifted = work/'raw.lrat', work/'lifted.lrat'
            cnf.write_text('p cnf 5 7\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n'
                           '4 5 0\n4 -5 0\n-3 -3 4 0\n')
            spelling = canonical_checker_cnf(cnf, spelling_path)
            # RAT addition 3 requires the negative hint to input clause 7.
            # Deletions name both input and derived clauses; labels repeat.
            original = '8 3 0 -7 5 6 0\n8 d 7 8 0\n9 1 0 1 2 0\n10 0 9 3 4 0\n'
            raw.write_text(original)
            output, count = restate_duplicate_input_lrat(spelling_path, spelling, raw, lifted)
            self.assertEqual(count, 1)
            self.assertEqual(output, lifted)
            expected = ('8 -3 4 0 7 0\n8 d 7 0\n9 3 0 -8 5 6 0\n9 d 8 9 0\n'
                        '10 1 0 1 2 0\n11 0 10 3 4 0\n')
            self.assertEqual(lifted.read_text(), expected)
            self.assertEqual(raw.read_text(), original)
            def check():
                result = subprocess.run([str(cake), str(spelling_path), str(lifted)],
                                        capture_output=True, text=True, timeout=10)
                return 's VERIFIED UNSAT' in result.stdout and result.returncode == 0
            self.assertTrue(check())
            # Break the used final chain, not an irrelevant auxiliary lemma.
            lifted.write_text(expected.replace('11 0 10 3 4 0', '11 0 9 3 4 0'))
            self.assertFalse(check())

    def test_routed_restatement_corruption_is_rejected_by_cake_lpr(self):
        import proof_oracle as oracle
        import runtime_task
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        actual = oracle.restate_duplicate_input_lrat
        def corrupt(cnf, spelling, source, target):
            path, count = actual(cnf, spelling, source, target)
            self.assertEqual(count, 1)
            path.write_text(path.read_text().replace('3 1 0 1 0', '3 1 0 2 0', 1))
            return path, count
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'proof.dsr'
            cnf.write_text('p cnf 1 2\n1 1 0\n-1 0\n')
            proof.write_text('0\n')
            with patch.object(oracle, 'restate_duplicate_input_lrat', side_effect=corrupt):
                result = oracle.check_routed(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                                             pins/'drat-trim', pins/'cake_lpr', cnf, proof, work, 10)
            self.assertFalse(result[0], result)
            self.assertEqual(result[1], 'cake_lpr')
            with patch.object(runtime_task, 'restate_duplicate_input_lrat', side_effect=corrupt):
                result = runtime_task.check_unsat_proof(campaign, 'auto', cnf, proof,
                                                        work, 10, synthetic_route_hint(proof))
            self.assertEqual(result['result'], 'not-verified', result)
            self.assertEqual(result['stages'][-1]['name'], 'cake_lpr')

    def test_routed_duplicate_inputs_require_checked_restatements(self):
        from proof_oracle import check_routed
        from runtime_task import check_unsat_proof
        from anonymous_scratch import AnonymousScratch
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'proof.dsr'
            for raw in ('p cnf 1 2\n1 1 0\n-1 0\n',
                        'p cnf 3 5\n1\nc duplicate on next line\n01 0 -1 2 0\n-1 3 0\n-2 -3 0\n2 3 0\n',
                        'p cnf 2 4\n1 1 -1 0\n2 2 0\n-2 -2 0\n1 1 -1 0\n'):
                for payload in (b'0\n', b'\x01\x00'):
                    with self.subTest(raw=raw, payload=payload):
                        cnf.write_text(raw)
                        proof.write_bytes(payload)
                        result = check_routed(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                                              pins/'drat-trim', pins/'cake_lpr', cnf, proof, work, 10)
                        self.assertTrue(result[0], result)
                        self.assertEqual(result[3], 'drat')
                        for anonymous in (False, True):
                            scratch = AnonymousScratch(directory) if anonymous else work
                            try:
                                result = check_unsat_proof(campaign, 'auto', cnf, proof,
                                                           scratch, 10, synthetic_route_hint(proof))
                                self.assertEqual(result['result'], 'verified', result)
                                self.assertGreater(sum(s.get('input_restatements', 0)
                                                       for s in result['stages']), 0)
                            finally:
                                if anonymous: scratch.close()
                        self.assertEqual(cnf.read_text(), raw)

    def test_native_dsr_legal_layouts_on_all_checker_surfaces(self):
        from proof_oracle import check_dsr
        from runtime_task import check_unsat_proof
        import run as tier
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        layouts = (
            b'p cnf 1 2\r\n1 0\r\n-1 0\r\n',
            b'c header\r\np cnf 1 2\n1\r\nc inside clause\r\n0 -1 0\n',
            b'p cnf 1 2\n\t01\n  c indented comment\n0\n-01 0\n\n',
        )
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'input.dsr'
            for raw in layouts:
                for payload in (b'1 1 0\n0\n', b'\x01\x02\x02\x00\x01\x00'):
                    with self.subTest(raw=raw, payload=payload):
                        cnf.write_bytes(raw)
                        proof.write_bytes(payload)
                        result = check_dsr(pins/'dsr-trim', pins/'dsr-compress',
                                           pins/'srcheck', cnf, proof, work, 10,
                                           every_lemma=True)
                        self.assertTrue(result[0], result)
                        result = tier.check_proof('dsr', str(cnf), str(proof), 10,
                                                  route=synthetic_route_hint(proof))
                        self.assertTrue(result[0], result)
                        result = check_unsat_proof(campaign, 'auto', cnf, proof, work,
                                                   10, synthetic_route_hint(proof))
                        self.assertEqual(result['result'], 'verified', result)
                        self.assertEqual(result['stages'][0]['name'], 'dsr-trim')
                        self.assertEqual(Path(result['stages'][0]['command'][1]).name, 'checker.cnf')
                        self.assertEqual(cnf.read_bytes(), raw)
                        # Same proof and layout, but no contradictory input unit.
                        cnf.write_bytes(raw.replace(b'-1', b'1').replace(b'-01', b'01'))
                        self.assertFalse(check_dsr(pins/'dsr-trim', pins/'dsr-compress',
                                                  pins/'srcheck', cnf, proof, work, 10,
                                                  every_lemma=True)[0])
                        self.assertFalse(tier.check_proof('dsr', str(cnf), str(proof), 10,
                                                          route=synthetic_route_hint(proof))[0])
                        self.assertEqual(check_unsat_proof(campaign, 'auto', cnf, proof,
                                                          work, 10, synthetic_route_hint(proof))['result'], 'not-verified')

    def test_routed_empty_input_requires_checked_lrat_bridge(self):
        from proof_oracle import check_routed
        from runtime_task import check_unsat_proof
        from anonymous_scratch import AnonymousScratch
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'input.dsr'
            for raw in ('p cnf 0 1\n0\n',
                        'p cnf 2 3\n1\nc in clause\n2 0 000\n-1 0\n',
                        'p cnf 1 3\n0 1 0 0\n'):
                for payload in (b'0\n', b'\x01\x00', b''):
                    with self.subTest(raw=raw, payload=payload):
                        cnf.write_text(raw)
                        proof.write_bytes(payload)
                        result = check_routed(pins/'dsr-trim', pins/'dsr-compress',
                                              pins/'srcheck', pins/'drat-trim', pins/'cake_lpr',
                                              cnf, proof, work, 30)
                        self.assertTrue(result[0], result)
                        self.assertEqual(result[3], 'drat')
                        for anonymous in (False, True):
                            scratch = AnonymousScratch(directory) if anonymous else work
                            try:
                                result = check_unsat_proof(campaign, 'dsr', cnf, proof,
                                                           scratch, 30, synthetic_route_hint(proof))
                                self.assertEqual(result['result'], 'verified', result)
                                self.assertEqual([s['name'] for s in result['stages']],
                                                 ['drat-trim', 'cake_lpr'])
                                self.assertGreater(result['stages'][0]['produced_bytes'], 0)
                            finally:
                                if anonymous: scratch.close()
                        self.assertEqual(cnf.read_text(), raw)

    def test_empty_input_bridge_does_not_bypass_cake_lpr(self):
        import proof_oracle
        import runtime_task
        campaign = ROOT/'benchmarks/runs/20260923-am341-unwatch'
        pins = campaign/'pins'
        actual_bridge = proof_oracle.bridge_trivial_input_lrat
        def corrupt_hint(spelling, lrat, returncode, stdout, stderr=''):
            changed = actual_bridge(spelling, lrat, returncode, stdout, stderr)
            self.assertTrue(changed)
            Path(lrat).write_text('4 0 1 0\n')  # clause 1 is nonunit, not the empty clause 2
            return changed
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'input.dsr'
            cnf.write_text('p cnf 2 3\n1 2 0\n0\n-1 0\n')
            proof.write_text('0\n')
            with patch.object(proof_oracle, 'bridge_trivial_input_lrat', side_effect=corrupt_hint):
                result = proof_oracle.check_routed(
                    pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck', pins/'drat-trim',
                    pins/'cake_lpr', cnf, proof, work, 30)
            self.assertFalse(result[0])
            self.assertEqual(result[1], 'cake_lpr', result)
            with patch.object(runtime_task, 'bridge_trivial_input_lrat', side_effect=corrupt_hint):
                result = runtime_task.check_unsat_proof(campaign, 'dsr', cnf, proof,
                                                        work, 30, synthetic_route_hint(proof))
            self.assertEqual(result['result'], 'not-verified', result)
            self.assertEqual(result['stages'][-1]['name'], 'cake_lpr')
            self.assertEqual(result['stages'][-1]['result'], 'rejected')

    def test_conversion_predicate_uses_clause_values_not_layout(self):
        fixtures = [
            ('p cnf 1 2\n1 01 0\n-1 0\n', True),
            ('p cnf 1 2\n1\nc inside clause\n1 0 -1 0\n', True),
            ('p cnf 1 3\n1 0 1 0 -1 0\n', False),
            ('p cnf 1 2\n1 0 000\n', True),
            ('p cnf 1 2\n1\nc legal comment\n0 -1 0\n', False),
        ]
        with tempfile.TemporaryDirectory() as directory:
            cnf = Path(directory)/'input.cnf'
            for raw, must_skip in fixtures:
                with self.subTest(raw=raw):
                    cnf.write_text(raw)
                    self.assertEqual(bool(fuzz.conversion_crosscheck_unsound(cnf)), must_skip)
                    result = self.fixture(raw, '0\n', lrat=False, convert=True)
                    self.assertFalse(result.failures, result.failures)
                    self.assertEqual(bool(result.skips), must_skip)

    def test_tier_runner_requires_exact_checker_specific_success_and_exit(self):
        import importlib.util
        import subprocess
        spec = importlib.util.spec_from_file_location('strict_tier_runner', ROOT/'tools/bench/run.py')
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with tempfile.TemporaryDirectory() as directory:
            cnf, proof = Path(directory)/'sat.cnf', Path(directory)/'false.proof'
            cnf.write_text('p cnf 2 1\n1 2 0\n')
            proof.write_text('0\n')
            for fmt, token, wrong in [('drat', 's VERIFIED', 's VERIFIED UNSAT'),
                                      ('lrat', 's VERIFIED UNSAT', 's VERIFIED')]:
                for code, output, ok in [
                    (0, token, True), (0, '\r' + token + '\n', True),
                    (1, token, False), (-11, token, False), (0, 'x' + token, False),
                    (0, wrong, False), (0, token + '\ns NOT VERIFIED', False),
                    (0, token + '\nc Checking failed at line: 1', False),
                    (0, '', False),
                ]:
                    with self.subTest(fmt=fmt, code=code, output=output):
                        done = subprocess.CompletedProcess([], code, output, '')
                        with patch.object(runner.subprocess, 'run', return_value=done):
                            result = runner.check_proof(fmt, str(cnf), str(proof), 10)
                        self.assertEqual(result[0], ok, result)

    def test_trivial_drat_exit_requires_a_real_input_empty_clause(self):
        import importlib.util
        from runtime_task import check_unsat_proof
        spec = importlib.util.spec_from_file_location('trivial_tier_runner', ROOT/'tools/bench/run.py')
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work/'pins').mkdir()
            checker = work/'pins/drat-trim'
            checker.write_text("#!/bin/sh\nprintf 'c trivial UNSAT\\ns VERIFIED\\n'\nexit 1\n")
            checker.chmod(0o755)
            cnf, proof = work/'input.cnf', work/'proof.drat'
            proof.write_text('0\n')
            for raw, accepted in [
                ('p cnf 1 1\n1 0\n', False),
                ('p cnf 1 2\n1 0\n-1 0\n', False),
                ('p cnf 1 1\n1\nc 0\n0\n', False),
                ('p cnf 0 1\nc genuine empty\n000\n', True),
                ('p cnf 1 2\n1 0 0\n', True),
            ]:
                with self.subTest(raw=raw):
                    cnf.write_text(raw)
                    with patch.object(runner, 'DRAT_TRIM', str(checker)):
                        result = runner.check_proof('drat', str(cnf), str(proof), 10)
                    self.assertEqual(result[0], accepted, result)
                    result = check_unsat_proof(work, 'drat', cnf, proof, work, 10)
                    self.assertEqual(result['result'] == 'verified', accepted, result)
                    result = self.fixture(raw, '0\n', lrat=False,
                                          checker_response=(1, 'c trivial UNSAT\ns VERIFIED\n', ''))
                    self.assertEqual(not result.failures, accepted, result.failures)

    def test_checker_spelling_preserves_clause_sequence(self):
        from proof_oracle import canonical_checker_cnf
        import hashlib
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory)/'raw.cnf', Path(directory)/'checker.cnf'
            raw = b' c before\r\n p\tcnf 3 3\r\n1 1\n c middle\n-2 0 -1 3 0\n0\n'
            source.write_bytes(raw)
            metadata = canonical_checker_cnf(source, output)
            self.assertEqual(output.read_bytes(), b'p cnf 3 3\n1 1 -2 0\n-1 3 0\n0\n')
            self.assertEqual(source.read_bytes(), raw)
            self.assertEqual(metadata['original_sha256'], hashlib.sha256(raw).hexdigest())
            self.assertEqual(metadata['checker_sha256'], hashlib.sha256(output.read_bytes()).hexdigest())
            self.assertEqual(metadata['declared_variables'], metadata['checker_variables'])
            self.assertEqual(metadata['first_empty_clause'], 3)
            with self.assertRaises(ValueError):
                canonical_checker_cnf(source, source)
            self.assertEqual(source.read_bytes(), raw)
            source.write_text('p cnf 0 1\n0\n')
            metadata = canonical_checker_cnf(source, output)
            self.assertEqual(output.read_text(), 'p cnf 1 1\n0\n')
            self.assertEqual((metadata['declared_variables'], metadata['checker_variables']), (0, 1))
            source.write_text('p cnf 00000000000001 00000000000001\n00000000000001 -0000000\n')
            canonical_checker_cnf(source, output)
            self.assertEqual(output.read_text(), 'p cnf 1 1\n1 0\n')

    def test_checker_spelling_rejects_malformed_original(self):
        from proof_oracle import canonical_checker_cnf, check_pb
        bad_inputs = ['', '1 0\n', 'p cnf -1 0\n', 'p cnf 1 2 extra\n',
                      'p cnf 1 1\n2 0\n', 'p cnf 1 1\n1\n',
                      'p cnf 1 2\n1 0\n', 'p cnf 1 1\n1 0 0\n',
                      'p cnf 1 1\n1 c not a comment\n0\n',
                      'p cnf 1 1\np cnf 1 1\n1 0\n', 'p cnf 0 1\n1 0\n',
                      'p cnf 1 1\n--1 0\n', 'p cnf 1 1\n+1 0\n']
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            for bad in bad_inputs:
                with self.subTest(bad=bad):
                    source = work/'input.cnf'; source.write_text(bad)
                    with self.assertRaises(ValueError):
                        canonical_checker_cnf(source, work/'checker.cnf')
                    with patch('subprocess.run') as checker:
                        result = check_pb('veripb', 'cake', source, work/'p', work)
                        self.assertFalse(result[0])
                        self.assertEqual(result[1], 'cnf')
                        checker.assert_not_called()

    def test_pinned_checkers_accept_original_layout_variants(self):
        from proof_oracle import check_dsr, check_pb
        pins = ROOT/'benchmarks/runs/20260923-am337-proof-lifetime-v2/pins'
        layouts = ['p cnf 1 2\n1\n0\n-1 0\n',
                   'p cnf 1 2\n1 0 -1 0\n',
                   'p cnf 1 2\n1 0\nc between clauses\n-1 0\n',
                   '\np cnf 1 2\n1 0\n-1 0\n',
                   'c prefix\r\n \t\r\np cnf 1 2\r\n1 0\r\n-1 0\r\n']
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, dsr, pbp = work/'input.cnf', work/'proof.dsr', work/'proof.pbp'
            dsr.write_text('0\n')
            pbp.write_text('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                           'output NONE ;\nconclusion UNSAT : -1 ;\n'
                           'end pseudo-Boolean proof ;\n')
            for raw in layouts:
                cnf.write_text(raw)
                result = check_pb(pins/'veripb', pins/'cake-pb-cnf', cnf, pbp, work, 60)
                self.assertTrue(result[0], result)
                result = check_dsr(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                                   cnf, dsr, work, 60, every_lemma=True)
                self.assertTrue(result[0], result)
                self.assertEqual(cnf.read_bytes(), raw.encode())
            cnf.write_text('p cnf 0 1\n0\n')
            dsr.write_bytes(b'\x01\x00')
            result = check_dsr(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                               cnf, dsr, work, 60, every_lemma=True)
            self.assertTrue(result[0], result)
            # The unused header identity cannot justify an empty lemma on SAT.
            for raw in ['p cnf 1 1\n1\n0\n', 'p cnf 0 0\n']:
                cnf.write_text(raw)
                self.assertFalse(check_dsr(pins/'dsr-trim', pins/'dsr-compress',
                                          pins/'srcheck', cnf, dsr, work, 60,
                                          every_lemma=True)[0])
                self.assertFalse(check_pb(pins/'veripb', pins/'cake-pb-cnf',
                                         cnf, pbp, work, 60)[0])

    def test_fuzz_dispatch_spells_all_dsr_checker_inputs(self):
        raw = 'p cnf 1 2\n1 0\nc between clauses\n-1 0\n'
        result = self.fixture(raw, '0\n', dsr=True)
        self.assertFalse(result.failures, result.failures)
        for run in result.checker_runs:
            if Path(run['command'][0]).name == 'srcheck':
                self.assertEqual(Path(run['command'][1]).name, 'checker.cnf')
            elif Path(run['command'][0]).name == 'dsr-trim':
                self.assertIn('checker.cnf', str(run['command']))
                self.assertNotIn('input.cnf', str(run['command']))
        self.assertFalse(self.fixture('p cnf 0 1\n0\n', '0\n', dsr=True).failures)

    def test_startup_pb_requires_both_pinned_checkers(self):
        from proof_oracle import (certificate_route, check_routed,
                                  proof_check_status, pb_stages,
                                  stage_environment)
        pins = ROOT/'benchmarks/baselines/sc24-26-32g-5000s/pins'
        self.assertTrue((pins/'veripb').is_file())
        self.assertTrue((pins/'cake-pb-cnf').is_file())
        self.assertEqual(stage_environment('cake-pb-cnf')['CML_HEAP_SIZE'],
                         '24576')
        self.assertEqual(pb_stages('vp', 'cp', 'cnf', 'pbp', 'kernel'),
                         [('veripb', ['vp', '-u', '--cnf', '--elaborate',
                                      'kernel', 'cnf', 'pbp']),
                          ('cake-pb-cnf', ['cp', 'cnf', 'kernel'])])
        for checker, token in [('veripb', 's VERIFIED UNSATISFIABLE'),
                               ('cake-pb-cnf', 's VERIFIED UNSAT')]:
            self.assertEqual(proof_check_status(checker, 0, token, ''), 'verified')
            for rc, output in [(1, token), (0, 'VERIFIED'),
                               (0, token + '\ns INVALID')]:
                self.assertNotEqual(proof_check_status(checker, rc, output, ''),
                                    'verified')
        with tempfile.TemporaryDirectory(prefix='pb-oracle-fixed-') as directory:
            work = Path(directory)
            cnf, proof = work/'i.cnf', work/'not-an-extension.pbp'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            text = ('pseudo-Boolean proof version 3.0\n'
                    'rup >= 1 ;\noutput NONE ;\n'
                    'conclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n')
            proof.write_text(text)
            self.assertEqual(certificate_route(proof), 'pbp')
            def check(**kwargs):
                return check_routed(None, None, None, None, None,
                                    cnf, proof, work, timeout=60, **kwargs)
            self.assertFalse(check()[0], 'missing PB pins must fail closed')
            accepted = check(veripb=pins/'veripb', cake_pb=pins/'cake-pb-cnf')
            self.assertTrue(accepted[0], accepted)
            # Success-looking elaboration is insufficient if CakePB rejects.
            actual = __import__('subprocess').run
            def reject_kernel(command, **kwargs):
                if str(command[0]) == str(pins/'cake-pb-cnf'):
                    return __import__('subprocess').CompletedProcess(
                        command, 1, 's VERIFIED UNSAT\n', 'bad kernel')
                return actual(command, **kwargs)
            with patch('subprocess.run', side_effect=reject_kernel):
                rejected = check(veripb=pins/'veripb', cake_pb=pins/'cake-pb-cnf')
            self.assertFalse(rejected[0])
            self.assertEqual(rejected[1], 'cake-pb-cnf')
            # Original-CNF soundness, truncated proof, and a bad unused lemma.
            cnf.write_text('p cnf 1 1\n1 0\n')
            self.assertFalse(check(veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')[0])
            proof.write_text(text.split('conclusion')[0])
            self.assertFalse(check(veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')[0])
            cnf.write_text('p cnf 3 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n')
            proof.write_text(text.replace('rup >= 1 ;',
                                          'rup 1 x3 >= 1 ;\nrup >= 1 ;'))
            self.assertFalse(check(veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')[0])

    def fixture(self, cnf_text, proof_text, lrat=True, checker_response=None,
                convert=False, dsr=False, auto=False):
        with tempfile.TemporaryDirectory(prefix='fuzz-oracle-fixed-') as directory:
            work = Path(directory); cnf = work/'input.cnf'; cnf.write_text(cnf_text)
            actual = fuzz.run_cmd
            def run(command, timeout, **kwargs):
                if command[0] == 'fixed-proof-emitter':
                    Path(command[-1]).write_text(proof_text)
                    return 20, 's UNSATISFIABLE\n', ''
                if checker_response is not None: return checker_response
                return actual(command, timeout, **kwargs)
            solver = ['fixed-proof-emitter'] + (
                ['--auto-proof'] if auto else ['--dsr', '--no-binary'] if dsr else
                (['--lrat'] if lrat else ['--drat']))
            with patch.object(fuzz, 'run_cmd', side_effect=run):
                result = fuzz.run_case(str(cnf), solver, None, 10, convert, str(work))
            self.assertTrue(result.checker_runs or
                            any('explicit clausal' in detail for _, detail in result.failures))
            if result.failures:
                bundle = Path(fuzz.save_bundle(str(work/'failures'), 0, 0, 'fixed-checker-control',
                                              str(cnf), result, solver, None, 10, convert, False))
                proof_name = ('proof.dsr' if dsr or auto else
                              ('proof.lrat' if lrat else 'proof.drat'))
                self.assertEqual((bundle/proof_name).read_text(), proof_text)
                self.assertEqual(json.loads((bundle/'meta.json').read_text())['checker_runs'], result.checker_runs)
            return result

    def test_fuzz_automatic_pb_and_strict_formats(self):
        proof = ('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                 'output NONE ;\nconclusion UNSAT : -1 ;\n'
                 'end pseudo-Boolean proof ;\n')
        cnf = 'p cnf 1 2\n1 0\n-1 0\n'
        accepted = self.fixture(cnf, proof, auto=True)
        self.assertFalse(accepted.failures, accepted.failures)
        self.assertEqual([Path(run['command'][0]).name for run in accepted.checker_runs],
                         ['veripb', 'cake-pb-cnf'])
        for options in ({'dsr': True}, {'lrat': True}, {'lrat': False}):
            rejected = self.fixture(cnf, proof, **options)
            self.assertTrue(rejected.failures)
            self.assertFalse(rejected.checker_runs)
        self.assertTrue(self.fixture('p cnf 1 1\n1 0\n', proof, auto=True).failures)
        self.assertTrue(self.fixture(cnf, proof.split('conclusion')[0], auto=True).failures)

    def test_native_nonconflicting_empty_lemma_rejected(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '2 0 1 0\n')
        self.assertIn('proof', [kind for kind, _ in r.failures])
        self.assertEqual(Path(r.checker_runs[0]['command'][0]).name, 'cake_lpr')
        self.assertEqual(r.checker_runs[0]['status'], 'rejected')

    def test_native_actual_conflict_accepted(self):
        self.assertFalse(self.fixture('p cnf 1 2\n1 0\n-1 0\n', '3 0 1 2 0\n').failures)

    def test_drat_nonconflicting_empty_lemma_rejected(self):
        self.assertTrue(self.fixture('p cnf 1 1\n1 0\n', '0\n', lrat=False).failures)

    def test_drat_actual_conflict_accepted(self):
        self.assertFalse(self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', lrat=False).failures)

    def test_drat_layout_false_proofs_rejected_and_true_proofs_accepted(self):
        # AM339 handoff: raw drat-trim parses comments as empty clauses with
        # binary proofs, and %i interprets leading-zero decimal literals as octal.
        for raw, unsat in [
                ('p cnf 2 2\n1 2 0\nc interior comment\n-1 2 0\n', False),
                ('p cnf 10 2\n8 0\n-010 0\n', False),
                ('p cnf 1 2\n1\nc split clause\n0 -1 0\n', True),
                ('p cnf 10 2\n10 0\n-010 0\n', True),
                ('p cnf 0 1\nc genuine empty clause\n0\n', True)]:
            for proof in ('0\n', 'a\x00'):
                for convert in (False, True):
                    with self.subTest(raw=raw, proof=proof, convert=convert):
                        result = self.fixture(raw, proof, lrat=False, convert=convert)
                        self.assertEqual(not result.failures, unsat, result.failures)
                        for run in result.checker_runs:
                            self.assertEqual(Path(run['command'][1]).name, 'checker.cnf')

    def test_runtime_drat_layouts_and_routed_elaboration(self):
        from runtime_task import check_unsat_proof
        from proof_oracle import check_routed, drat_stages
        campaign = ROOT/'benchmarks/runs/20260923-am337-proof-lifetime-v2'
        pins = campaign/'pins'
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'original.cnf', work/'input.proof'
            for raw, unsat in [
                    ('p cnf 2 2\n1 2 0\nc interior comment\n-1 2 0\n', False),
                    ('p cnf 10 2\n8 0\n-010 0\n', False),
                    ('p cnf 1 2\n1\nc split clause\n0 -1 0\n', True),
                    ('p cnf 10 2\n10 0\n-010 0\n', True),
                    ('p cnf 0 1\nc genuine empty clause\n0\n', True)]:
                cnf.write_text(raw)
                for encoding in (b'0\n', b'a\x00'):
                    with self.subTest(raw=raw, proof=encoding):
                        proof.write_bytes(encoding)
                        result = check_unsat_proof(campaign, 'drat', cnf, proof, work, 60)
                        self.assertEqual(result['result'] == 'verified', unsat, result)
                        self.assertEqual(cnf.read_text(), raw)
                # Routed witness-free DSR keeps CakeLPR as final authority.
                proof.write_bytes(b'\x01\x00')
                result = check_routed(pins/'dsr-trim', pins/'dsr-compress', pins/'srcheck',
                                      pins/'drat-trim', pins/'cake_lpr', cnf, proof, work, 60)
                # Genuine input emptiness now gets a checked LRAT input hint.
                self.assertEqual(result[0], unsat, result)
        self.assertEqual(drat_stages('DT', 'CL', 'raw', 'p', 'l', checker_cnf='canonical'),
                         [('drat-trim', ['DT', 'canonical', 'p', '-L', 'l']),
                          ('cake_lpr', ['CL', 'canonical', 'l'])])

    def test_tier_runner_drat_reader_uses_original_clause_sequence(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('bench_runner', ROOT/'tools/bench/run.py')
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        pins = ROOT/'benchmarks/runs/20260923-am337-proof-lifetime-v2/pins'
        with tempfile.TemporaryDirectory() as directory, patch.object(
                runner, 'DRAT_TRIM', str(pins/'drat-trim')):
            cnf, proof = Path(directory)/'original.cnf', Path(directory)/'proof.drat'
            for raw, unsat in [
                    ('p cnf 2 2\n1 2 0\nc interior comment\n-1 2 0\n', False),
                    ('p cnf 10 2\n8 0\n-010 0\n', False),
                    ('p cnf 1 2\n1\nc split clause\n0 -1 0\n', True),
                    ('p cnf 10 2\n10 0\n-010 0\n', True)]:
                for encoding in (b'0\n', b'a\x00'):
                    with self.subTest(raw=raw, proof=encoding):
                        cnf.write_text(raw)
                        proof.write_bytes(encoding)
                        result = runner.check_proof('drat', str(cnf), str(proof), 60)
                        self.assertEqual(result[0], unsat, result)
                        self.assertEqual(cnf.read_text(), raw)
            cnf.write_text('p cnf 1 1\n2 0\n')
            with patch.object(runner.subprocess, 'run') as checker:
                self.assertFalse(runner.check_proof('drat', str(cnf), str(proof), 60)[0])
                checker.assert_not_called()

    def test_drat_empty_input_checker_exit_one(self):
        r = self.fixture('p cnf 1 1\n0\n', '', lrat=False)
        self.assertFalse(r.failures)
        self.assertEqual(r.checker_runs[0]['exit_code'], 1)

    def test_drat_trivial_text_requires_actual_empty_input(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '0\n', lrat=False,
                         checker_response=(1, 'c trivial UNSAT\ns VERIFIED\n', ''))
        self.assertTrue(r.failures)

    def test_default_dsr_pipeline_accepts_actual_conflict(self):
        r = self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', dsr=True)
        self.assertFalse(r.failures)
        self.assertEqual([Path(run['command'][0]).name
                         for run in r.checker_runs],
                         ['drat-trim', 'cake_lpr', 'drat-trim'])
        # Hybrid acceptance first, then the every-lemma forward audit of the
        # same lossless DRAT conversion; witness-free DSR never takes the
        # forward dsr-trim elaboration (F2).
        self.assertNotIn('-f', r.checker_runs[0]['command'])
        self.assertIn('-f', r.checker_runs[2]['command'])
        self.assertEqual(r.checker_runs[2]['command'][2],
                         r.checker_runs[0]['command'][2])

    def test_certificate_routing_by_witness(self):
        sys.path.insert(0, str(ROOT/'tools/bench'))
        import proof_oracle as oracle
        with tempfile.TemporaryDirectory(prefix='route-') as directory:
            work = Path(directory)
            ascii_plain = work/'plain.dsr'; ascii_plain.write_text('1 2 0\nd 1 2 0\n0\n')
            ascii_witness = work/'witness.dsr'; ascii_witness.write_text('1 2 1 0\n0\n')
            self.assertFalse(oracle.dsr_proof_has_witness(ascii_plain))
            self.assertTrue(oracle.dsr_proof_has_witness(ascii_witness))
            self.assertEqual(oracle.certificate_route(ascii_plain), 'drat')
            self.assertEqual(oracle.certificate_route(ascii_witness), 'dsr')
            # binary: tag 1 = add, 2 = delete; literals 1 -> 2, -1 -> 3 (2*var+sign)
            plain = bytes([1, 2, 4, 0, 2, 2, 4, 0, 1, 0])
            witness = bytes([1, 2, 4, 2, 0, 1, 0])
            bin_plain = work/'plain.bin'; bin_plain.write_bytes(plain)
            bin_witness = work/'witness.bin'; bin_witness.write_bytes(witness)
            self.assertTrue(oracle.dsr_proof_is_binary(bin_plain))
            self.assertFalse(oracle.dsr_proof_has_witness(bin_plain))
            self.assertTrue(oracle.dsr_proof_has_witness(bin_witness))
            drat = work/'plain.drat'
            self.assertTrue(oracle.dsr_proof_as_drat(bin_plain, drat))
            self.assertEqual(drat.read_bytes(), bytes([ord('a'), 2, 4, 0, ord('d'), 2, 4, 0, ord('a'), 0]))
            cadical = work/'cadical.bin'
            cadical.write_bytes(bytes([ord('a'), 2, 4, 0, ord('d'), 2, 4, 0, 1, 0]))
            self.assertEqual(oracle.certificate_route(cadical), 'drat')
            self.assertTrue(oracle.dsr_proof_as_drat(cadical, drat))
            self.assertEqual(drat.read_bytes(), bytes([ord('a'), 2, 4, 0, ord('d'), 2, 4, 0, ord('a'), 0]))
            self.assertFalse(oracle.dsr_proof_as_drat(bin_witness, work/'never.drat'))
            copied = work/'plain.ascii.drat'
            self.assertTrue(oracle.dsr_proof_as_drat(ascii_plain, copied))
            self.assertEqual(copied.read_text(), ascii_plain.read_text())
            self.assertEqual(oracle.drat_stages('DT', 'CL', 'c', 'p', 'l'),
                             [('drat-trim', ['DT', 'c', 'p', '-L', 'l']), ('cake_lpr', ['CL', 'c', 'l'])])
            # the reference's checker amendment: cake_lpr runs with the 24576 MiB CakeML heap
            self.assertEqual(oracle.stage_environment('cake_lpr')['CML_HEAP_SIZE'], '24576')
            self.assertNotIn('CML_HEAP_SIZE', {k: v for k, v in oracle.stage_environment('drat-trim').items() if k == 'CML_HEAP_SIZE' and v == '24576' and 'CML_HEAP_SIZE' not in __import__('os').environ})

    def test_routed_acceptance_runs_the_drat_owner_pipeline(self):
        sys.path.insert(0, str(ROOT/'tools/bench'))
        import proof_oracle as oracle
        here = ROOT/'tools/checkers/bin'
        with tempfile.TemporaryDirectory(prefix='routed-') as directory:
            work = Path(directory)
            cnf = work/'i.cnf'; cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof = work/'p.dsr'; proof.write_bytes(bytes([1, 0]))  # add the empty clause
            verified, stage, output, route = oracle.check_routed(
                here/'dsr-trim', here/'dsr-compress', here/'srcheck', here/'drat-trim', here/'cake_lpr',
                cnf, proof, work)
            self.assertEqual(route, 'drat')
            self.assertTrue(verified, (stage, output[-500:]))
            # a witnessed certificate is never handed to drat-trim
            witnessed = work/'w.dsr'; witnessed.write_bytes(bytes([1, 2, 4, 2, 0, 1, 0]))
            self.assertEqual(oracle.certificate_route(witnessed), 'dsr')

    def test_reference_dsr_stages(self):
        from proof_oracle import reference_dsr_stages
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'binary.dsr'
            binary.write_bytes(b'a\x02\x00a\x00')
            ascii_proof = Path(directory) / 'ascii.dsr'
            ascii_proof.write_text('1 0\nd 1 0\n0\n')
            stages = reference_dsr_stages('T', 'C', 'S', 'f.cnf', binary,
                                          'b.lsr', 't.lsr')
            self.assertEqual(stages, [
                ('dsr-trim', ['T', 'f.cnf', binary, 'b.lsr']),
                ('dsr-compress', ['C', '-d', 'b.lsr', 't.lsr']),
                ('srcheck', ['S', 'f.cnf', 't.lsr'])])
            self.assertEqual(
                reference_dsr_stages('T', 'C', 'S', 'f.cnf', ascii_proof,
                                     'b.lsr', 't.lsr'),
                [('dsr-trim', ['T', 'f.cnf', ascii_proof, 't.lsr']),
                 ('srcheck', ['S', 'f.cnf', 't.lsr'])])

    def test_every_lemma_stages_elaborate_forward(self):
        from proof_oracle import every_lemma_dsr_stages
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'binary.dsr'
            binary.write_bytes(b'a\x02\x00a\x00')
            self.assertEqual(
                every_lemma_dsr_stages('T', 'C', 'S', 'f.cnf', binary,
                                       'b.lsr', 't.lsr'),
                [('dsr-trim', ['T', '-f', 'f.cnf', binary, 'b.lsr']),
                 ('dsr-compress', ['C', '-d', 'b.lsr', 't.lsr']),
                 ('srcheck', ['S', 'f.cnf', 't.lsr'])])

    def test_every_lemma_audit_rejects_an_unused_invalid_lemma(self):
        # `(5 v 6)` is neither RUP nor RAT and the refutation never uses it:
        # the reference pipeline trims it away, the forward audit refuses it.
        r = self.fixture('p cnf 8 6\n1 2 0\n1 -2 0\n-1 3 0\n-1 -3 0\n'
                         '-5 7 0\n-6 8 0\n', '5 6 0\n1 0\n0\n', dsr=True)
        self.assertIn('proof', [kind for kind, _ in r.failures])
        self.assertTrue(any('every-lemma' in why for _, why in r.failures))

    def test_default_dsr_rejects_nonconflicting_empty_lemma(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '0\n', dsr=True)
        self.assertIn('proof', [kind for kind, _ in r.failures])
        self.assertEqual(Path(r.checker_runs[0]['command'][0]).name,
                         'drat-trim')

    def test_conversion_dispatches_to_cake(self):
        r = self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', lrat=False, convert=True)
        self.assertFalse(r.failures)
        self.assertEqual([Path(r['command'][0]).name for r in r.checker_runs],
                         ['drat-trim','drat-trim','cake_lpr'])

    def test_no_false_success_from_status_or_text(self):
        for mode, token in ((True, 's VERIFIED UNSAT'), (False, 's VERIFIED')):
            for code, out, err in ((1, token, ''), (None, token, ''), (0, 'prefix '+token, ''),
                                   (0, token+'\ns NOT VERIFIED', ''), (0, '', ''),
                                   (0, token, 'Checking failed at 2')):
                with self.subTest(lrat=mode, code=code, stdout=out, stderr=err):
                    r = self.fixture('p cnf 1 1\n1 0\n', '2 0 1 0\n' if mode else '0\n',
                                     lrat=mode, checker_response=(code, out, err))
                    self.assertTrue(r.failures)

    def test_nonterminating_repro_uses_persistent_harness(self):
        with tempfile.TemporaryDirectory(prefix='fuzz-timeout-repro-') as directory:
            root = Path(directory)
            cnf = root/'input.cnf'; cnf.write_text('p cnf 1 0\n')
            outcome = fuzz.CaseOutcome(); outcome.verdict = 'TIMEOUT'
            replay = root/'harness'
            with patch.dict('os.environ', {
                    'TACTIX_FUZZ_REPLAY_HARNESS': str(replay),
                    'TACTIX_FUZZ_HARNESS_SHA256': 'abc123'}):
                bundle = Path(fuzz.save_nonterminating(
                    str(root/'out'), 4, 9, 'fixed-timeout', str(cnf), outcome,
                    ['solver', '--random-policy=9'], 5.0))
            self.assertIn(str(replay/'fuzz.py'), (bundle/'repro.sh').read_text())
            self.assertEqual(
                json.loads((bundle/'meta.json').read_text())['harness_sha256'],
                'abc123')


if __name__ == '__main__': unittest.main()

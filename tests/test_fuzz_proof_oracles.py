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


class FuzzProofOracles(unittest.TestCase):
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
                         ['dsr-trim', 'srcheck', 'dsr-trim', 'srcheck'])
        # reference pipeline first, then the every-lemma forward audit
        self.assertNotIn('-f', r.checker_runs[0]['command'])
        self.assertIn('-f', r.checker_runs[2]['command'])
        # The Satsuma+Kissat reference invocation: dsr-trim takes no options
        # and Trestle reads a text LSR.  This fixture is diagnostic ASCII DSR,
        # for which dsr-trim writes that text LSR itself; a binary certificate
        # adds the `dsr-compress -d` expansion (test_reference_dsr_stages).
        self.assertFalse([arg for arg in r.checker_runs[0]['command'][1:]
                          if str(arg).startswith('-')])
        self.assertEqual(len(r.checker_runs[-1]['command']), 3)
        self.assertEqual(r.checker_runs[-1]['command'][-1],
                         r.checker_runs[0]['command'][-1])

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
                         'dsr-trim')

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

#!/usr/bin/env python3
"""Both Tier D runners choose one independent route from actual DSR content."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/bench'))
import portfolio_rerun as winner
import runtime_task as tactix


class HybridCheckerRoutes(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='hybrid-checkers-')
        self.addCleanup(self.temp.cleanup)
        self.campaign = Path(self.temp.name)
        pins = self.campaign / 'pins'
        pins.mkdir()
        dsr = ROOT / 'benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins'
        baseline = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/pins'
        for target, source in {
            'dsr-trim': dsr / 'dsr-trim',
            'dsr-compress': dsr / 'dsr-compress',
            'srcheck': dsr / 'srcheck',
            'drat-trim': baseline / 'drat-trim',
            'cake_lpr': baseline / 'cake-lpr',
            'cake-lpr': baseline / 'cake-lpr',
        }.items():
            (pins / target).symlink_to(source)
        self.manifest = {
            'checkers': {name: 'pins/' + name for name in
                         ('dsr-trim', 'dsr-compress', 'srcheck',
                          'drat-trim', 'cake-lpr')},
            'pin_digests': {'pins/' + name: winner.sha256(pins / name) for name in
                            ('dsr-trim', 'dsr-compress', 'srcheck',
                             'drat-trim', 'cake-lpr')},
            'solver_environment': winner.clean_environment(),
        }
        self.cnf = self.campaign / 'original.cnf'
        self.cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')

    def test_witness_free_uses_drat_then_cake_lpr(self):
        for tag in (1, ord('a')):
            with self.subTest(tag=tag):
                proof = self.campaign / 'plain.dsr'
                proof.write_bytes(bytes([tag, 0]))
                checked = tactix.check_unsat_proof(
                    self.campaign, 'dsr', self.cnf, proof, self.campaign, 30,
                    reported_route='dsr-cake_lpr')
                self.assertEqual(checked['result'], 'verified', checked)
                self.assertEqual([stage['name'] for stage in checked['stages']],
                                 ['drat-trim', 'cake_lpr'])
                checked = winner.check_proof(
                    self.campaign, self.manifest, 'dsr', self.cnf, proof,
                    self.campaign, 30, route_contract='dsr-hybrid')
                self.assertEqual(checked['result'], 'verified', checked)
                self.assertEqual([stage['name'] for stage in checked['stages']],
                                 ['drat-trim', 'cake-lpr'])

    def test_witnessed_uses_no_option_dsr_then_trestle(self):
        proof = self.campaign / 'witness.dsr'
        proof.write_bytes(bytes([ord('a'), 2, 2, 0, 1, 0]))
        checked = tactix.check_unsat_proof(
            self.campaign, 'dsr', self.cnf, proof, self.campaign, 30,
            reported_route='dsr-trestle')
        self.assertEqual(checked['result'], 'verified', checked)
        self.assertEqual([stage['name'] for stage in checked['stages']],
                         ['dsr-trim', 'dsr-compress', 'srcheck'])
        self.assertEqual(len(checked['stages'][0]['command']), 4)
        checked = winner.check_proof(
            self.campaign, self.manifest, 'dsr', self.cnf, proof,
            self.campaign, 30, route_contract='dsr-hybrid')
        self.assertEqual(checked['result'], 'verified', checked)
        self.assertEqual([stage['name'] for stage in checked['stages']],
                         ['dsr-trim', 'dsr-compress', 'srcheck'])
        self.assertEqual(len(checked['stages'][0]['command']), 4)

    def test_mismatch_and_false_proof_fail_closed(self):
        proof = self.campaign / 'witness.dsr'
        proof.write_bytes(bytes([1, 2, 2, 0, 1, 0]))
        checked = tactix.check_unsat_proof(
            self.campaign, 'dsr', self.cnf, proof, self.campaign, 30,
            reported_route='dsr-cake_lpr')
        self.assertEqual(checked['result'], 'not-verified')
        self.assertEqual(checked['stages'][0]['name'], 'route')
        false_cnf = self.campaign / 'sat.cnf'
        false_cnf.write_text('p cnf 1 1\n1 0\n')
        false_proof = self.campaign / 'false.dsr'
        false_proof.write_bytes(bytes([1, 0]))
        checked = winner.check_proof(
            self.campaign, self.manifest, 'dsr', false_cnf, false_proof,
            self.campaign, 30, route_contract='dsr-hybrid')
        self.assertEqual(checked['result'], 'not-verified', checked)
        self.assertEqual(checked['stages'][0]['name'], 'drat-trim')


if __name__ == '__main__':
    unittest.main()

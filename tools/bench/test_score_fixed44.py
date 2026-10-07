#!/usr/bin/env python3
"""Adversarial coverage-accounting checks; no solver execution."""
import copy
import csv
import json
from pathlib import Path
import tempfile
import unittest

from score_fixed44 import (ROOT, SELECTION, SELECTION_SHA256, checked_classification,
                           load, merge, sha, summarize)


class Fixed44Scoring(unittest.TestCase):
    def setUp(self):
        with SELECTION.open(newline='') as stream:
            self.selection = list(csv.DictReader(stream, delimiter='\t'))
        self.matrix = [dict(task=i, rank=i + 1, arm='tactix',
                            instance=r['instance'], expected=r['verdict'])
                       for i, r in enumerate(self.selection)]

    def checked(self, task):
        row = self.matrix[task]
        verdict = row['expected']
        checker = dict(result='verified', exit_code=0, timed_out=False, wall_seconds=1.)
        if verdict == 'UNSAT':
            checker['stages'] = [dict(name=n, **checker)
                                 for n in ('dsr-trim', 'dsr-compress', 'srcheck')]
        return dict(**row, verdict=verdict, expected_match=True, valid=True,
                    artifact_deleted=True, derived_artifacts_deleted=True,
                    cleanup_pending=False,
                    solve=dict(wall_seconds=1., timed_out=False,
                               exit_code=10 if verdict == 'SAT' else 20),
                    **{'model_check' if verdict == 'SAT' else 'proof_check': checker})

    def test_twelve_verified_is_coverage_not_qualification(self):
        report = summarize(self.selection, self.matrix,
                           {i: self.checked(i) for i in range(12)})
        self.assertEqual(report['independently_verified'], 12)
        self.assertEqual(report['expected_instances'], 44)
        self.assertTrue(report['twelve_verified_results'])
        self.assertFalse(report['qualification_accepted'])
        self.assertIsNone(report['par2_seconds'])
        self.assertEqual(report['dispositions']['missing'], 32)

    def test_busy_cannot_count_even_with_claimed_valid_certificate(self):
        record = self.checked(0)
        record['completion'] = dict(status='incomplete', reason='scratch_node_busy', retryable=True)
        report = summarize(self.selection, self.matrix, {0: record})
        self.assertEqual(report['independently_verified'], 0)
        self.assertEqual(report['dispositions']['resource_incomplete'], 1)
        self.assertIsNone(report['par2_seconds'])

    def test_unknown_incomplete_reason_rejected(self):
        record = self.checked(0)
        record['completion'] = dict(status='incomplete', reason='invented')
        with self.assertRaises(ValueError):
            summarize(self.selection, self.matrix, {0: record})

    def test_all_canonical_final_rows_allow_par2(self):
        rows = {i: self.checked(i) for i in range(44)}
        r = rows[0]
        r.update(verdict='UNKNOWN', proof_check=None, model_check=None, valid=False)
        r['solve'] = dict(wall_seconds=5000., timed_out=True, exit_code=None)
        result = summarize(self.selection, self.matrix, rows)
        self.assertEqual(result['independently_verified'], 43)
        self.assertEqual(result['par2_seconds'], 10043.)

    def test_stage_claim_cannot_hide_abnormal_exit_or_timeout(self):
        for field, value in [('exit_code', -9), ('timed_out', True)]:
            record = self.checked(0)
            record['proof_check']['stages'][-1][field] = value
            self.assertEqual(checked_classification(self.matrix[0], record)[0],
                             'inconsistent_checker_stage')

    def test_model_claim_cannot_hide_abnormal_exit(self):
        record = self.checked(1)
        record['model_check']['exit_code'] = 1
        self.assertEqual(checked_classification(self.matrix[1], record)[0],
                         'inconsistent_checker_result')

    def test_censored_proof_not_a_closure(self):
        record = self.checked(0)
        record['proof_check']['timed_out'] = True
        self.assertEqual(checked_classification(self.matrix[0], record)[0], 'checker_timeout')

    def test_late_verification_does_not_hide_missed_deadline(self):
        for elapsed in (15000., float('nan'), None):
            record = self.checked(0)
            record['proof_check']['wall_seconds'] = elapsed
            self.assertEqual(checked_classification(self.matrix[0], record)[0],
                             'checker_time_outside_contract')

    def test_unclean_verified_result_not_a_closure(self):
        record = self.checked(0)
        record['artifact_deleted'] = False
        self.assertEqual(checked_classification(self.matrix[0], record)[0], 'residue')

    def test_replaced_missing_or_extra_instance_rejected(self):
        for kind in ('replace', 'omit', 'extra'):
            matrix = copy.deepcopy(self.matrix)
            if kind == 'replace':
                matrix[-1]['instance'] = matrix[0]['instance']
            elif kind == 'omit':
                matrix.pop()
            else:
                matrix.append(matrix[-1])
            with self.assertRaises(ValueError):
                summarize(self.selection, matrix, {})

    def test_result_identity_mismatch_rejected(self):
        record = self.checked(0)
        record['instance'] = self.matrix[1]['instance']
        with self.assertRaises(ValueError):
            summarize(self.selection, self.matrix, {0: record})

    def attempts(self):
        record = self.checked(0)
        record['completion'] = dict(status='incomplete', reason='scratch_node_busy', retryable=True)
        parent = summarize(self.selection, self.matrix, {0: record})
        child = summarize(self.selection, self.matrix, {0: self.checked(0)})
        for report, name in ((parent, 'parent'), (child, 'child')):
            report.update(campaign=str(ROOT / name), manifest_sha256=name + '-manifest',
                          record_sha256={'results/000.json': name + '-result'},
                          coherence={'candidate': 'same'}, candidate={'sha256': 'same'},
                          selection_sha256='same', retry_attempt=None)
            for cell in report['rows']:
                cell['source_campaign'] = report['campaign']
        child['retry_attempt'] = dict(schema='tactix-fixed44-retry-attempt-v1', attempt=1,
            parent=dict(campaign='parent', manifest_sha256='parent-manifest'), allowed_tasks=[0],
            predecessors=[dict(task=0, result='parent/results/000.json',
                result_sha256='parent-result', completion_status='incomplete',
                completion_reason='scratch_node_busy', retryable=True)])
        return parent, child

    def test_retry_counts_verified_once_and_retains_attempt_bindings(self):
        parent, child = self.attempts()
        result = merge([parent, child])
        self.assertEqual(result['independently_verified'], 1)
        self.assertEqual(result['expected_instances'], 44)
        self.assertEqual(len(result['attempts']), 2)
        self.assertEqual(parent['rows'][0]['status'], 'resource_incomplete')

    def test_retry_cannot_combine_different_candidates(self):
        parent, child = self.attempts()
        child['coherence']['candidate'] = 'different'
        with self.assertRaisesRegex(ValueError, 'coherent candidate'):
            merge([parent, child])

    def test_retry_cannot_reuse_a_verified_row(self):
        parent, child = self.attempts()
        parent['rows'][0].update(status='checked_UNSAT', detail=None, retryable=False)
        with self.assertRaisesRegex(ValueError, 'non-retryable'):
            merge([parent, child])

    def test_retry_requires_exact_parent_and_record_hash(self):
        for field in ('manifest_sha256', 'result_sha256'):
            parent, child = self.attempts()
            target = (child['retry_attempt']['parent'] if field == 'manifest_sha256'
                      else child['retry_attempt']['predecessors'][0])
            target[field] = 'wrong'
            with self.assertRaises(ValueError):
                merge([parent, child])

    def test_unlaunched_retry_keeps_original_incomplete_evidence(self):
        parent, child = self.attempts()
        child['rows'][0]['status'] = 'missing'
        result = merge([parent, child])
        self.assertEqual(result['independently_verified'], 0)
        self.assertEqual(result['dispositions']['resource_incomplete'], 1)
        self.assertIsNone(result['par2_seconds'])

    def test_loader_rejects_changed_pinned_bytes_and_binary_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            (campaign / 'pins').mkdir()
            (campaign / 'results').mkdir()
            binary = campaign / 'pins/tactix'
            binary.write_bytes(b'not executed: immutable binary fixture')
            matrix = campaign / 'matrix.tsv'
            with matrix.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(self.matrix[0]), delimiter='\t')
                writer.writeheader()
                writer.writerows(self.matrix)
            manifest = dict(selection_authority={'sha256': SELECTION_SHA256},
                source_commit='fixture', arms={'tactix': dict(path='pins/tactix', sha256=sha(binary),
                    arguments=['--auto-proof'], certificate='auto')},
                resource_contract=dict(solve_timeout=5000, checker_timeout=14400,
                                       cpus_per_task=4, memory_mib=32768),
                pin_sha256={'pins/tactix': sha(binary), 'matrix.tsv': sha(matrix)},
                input_sha256={r['instance']: 'fixture' for r in self.selection})
            (campaign / 'manifest.json').write_text(json.dumps(manifest))
            self.assertEqual(load(campaign)['dispositions'], {'missing': 44})
            record = self.checked(0)
            record.update(binary_sha256='wrong', command=[str(binary), '--auto-proof', 'in', 'out'])
            (campaign / 'results/000.json').write_text(json.dumps(record))
            with self.assertRaisesRegex(ValueError, 'binary mismatch'):
                load(campaign)
            (campaign / 'results/000.json').unlink()
            binary.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'changed pin'):
                load(campaign)


if __name__ == '__main__':
    unittest.main()

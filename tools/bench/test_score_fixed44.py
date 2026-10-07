#!/usr/bin/env python3
"""Adversarial coverage-accounting checks; no solver execution."""
import copy
import csv
import json
from pathlib import Path
import struct
import tempfile
import unittest

from score_fixed44 import (ROOT, SELECTION, SELECTION_SHA256, canonical_sha,
                           checked_classification, load, merge, parse_array, sha,
                           summarize, validate_checker_commands,
                           validate_retry_manifest)


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
                    node='node001',
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
                          selection_sha256='same', retry_attempt=None,
                          retry_metadata_sha256=None,
                          launch={'job_id': '10' if name == 'parent' else '11',
                                  'sha256': name + '-launch'},
                          pyc_supplement=None)
            for cell in report['rows']:
                cell['source_campaign'] = report['campaign']
        child['retry_attempt'] = dict(schema='tactix-fixed44-retry-attempt-v1', attempt=1,
            parent=dict(campaign='parent', manifest_sha256='parent-manifest',
                        launch_job_id='10', launch_sha256='parent-launch'), allowed_tasks=[0],
            predecessors=[dict(task=0, result='parent/results/000.json',
                result_sha256='parent-result', completion_status='incomplete',
                completion_reason='scratch_node_busy', retryable=True,
                coverage_rank=1, instance=self.matrix[0]['instance'], node='node001')],
            resource_exclusion=dict(holders=[dict(task=1, job_id='10_1',
                claim='parent/claims/001.json')]))
        child['retry_metadata_sha256'] = canonical_sha(child['retry_attempt'])
        return parent, child

    @staticmethod
    def rehash_retry(report):
        report['retry_metadata_sha256'] = canonical_sha(report['retry_attempt'])

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
            self.rehash_retry(child)
            with self.assertRaises(ValueError):
                merge([parent, child])

    def test_unlaunched_retry_keeps_original_incomplete_evidence(self):
        parent, child = self.attempts()
        child['rows'][0]['status'] = 'missing'
        result = merge([parent, child])
        self.assertEqual(result['independently_verified'], 0)
        self.assertEqual(result['dispositions']['resource_incomplete'], 1)
        self.assertIsNone(result['par2_seconds'])

    @staticmethod
    def write_bindings(campaign, manifest, passed=True):
        manifest_path = campaign / 'manifest.json'
        manifest_path.write_text(json.dumps(manifest))
        digest = sha(manifest_path)
        (campaign / 'launch.json').write_text(json.dumps(
            dict(job_id='101', manifest_sha256=digest)))
        (campaign / 'validation-launch.json').write_text(json.dumps(
            dict(job_id='100', manifest_sha256=digest)))
        (campaign / 'validation.json').write_text(json.dumps(
            dict(job_id='100', passed=passed)))

    def loader_campaign(self, directory):
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
        array = campaign / 'array.sbatch'
        array.write_text('#SBATCH --array=0-43\n')
        validate = campaign / 'validate.sbatch'
        validate.write_text('# validation fixture\n')
        manifest = dict(selection_authority={'sha256': SELECTION_SHA256},
            source_commit='fixture', arms={'tactix': dict(path='pins/tactix', sha256=sha(binary),
                arguments=['--auto-proof'], certificate='auto')},
            resource_contract=dict(solve_timeout=5000, checker_timeout=14400,
                                   cpus_per_task=4, memory_mib=32768,
                                   node_constraint=None),
            pin_sha256={'pins/tactix': sha(binary), 'matrix.tsv': sha(matrix),
                        'array.sbatch': sha(array), 'validate.sbatch': sha(validate)},
            input_sha256={r['instance']: 'fixture' for r in self.selection})
        self.write_bindings(campaign, manifest)
        return campaign, binary, manifest

    def test_loader_rejects_changed_pinned_bytes_and_binary_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, binary, _manifest = self.loader_campaign(directory)
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

    def test_loader_requires_launch_and_passed_validation_bindings(self):
        for mutation, message in (('launch', 'launch manifest'),
                                  ('validation-launch', 'validation manifest'),
                                  ('validation', 'did not pass')):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                campaign, _binary, _manifest = self.loader_campaign(directory)
                path = campaign / (mutation + '.json')
                payload = json.loads(path.read_text())
                if mutation == 'validation':
                    payload['passed'] = False
                else:
                    payload['manifest_sha256'] = 'wrong'
                path.write_text(json.dumps(payload))
                with self.assertRaisesRegex(ValueError, message):
                    load(campaign)

    def test_undeclared_pin_requires_explicit_full_pyc_supplement(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, _binary, manifest = self.loader_campaign(directory)
            source = campaign / 'pins/helper.py'
            source.write_text('VALUE = 1\n')
            manifest['pin_sha256']['pins/helper.py'] = sha(source)
            pyc = campaign / 'pins/__pycache__/helper.cpython-39.pyc'
            pyc.parent.mkdir()
            mtime = int(source.stat().st_mtime) & 0xFFFFFFFF
            pyc.write_bytes(b'PY39' + struct.pack('<III', 0, mtime, source.stat().st_size) + b'fixture')
            self.write_bindings(campaign, manifest)
            with self.assertRaisesRegex(ValueError, 'requires pyc supplement'):
                load(campaign)
            supplement = campaign / 'supplement.json'
            supplement.write_text(json.dumps(dict(
                schema='tactix-validation-created-pyc-supplement-v1',
                parent_campaign=str(campaign), validation_job_id='100',
                python_version='3.9.24 fixture', records=[dict(name='helper',
                    source_sha256=sha(source), pyc_sha256=sha(pyc),
                    source_mtime_header=mtime, source_size_header=source.stat().st_size,
                    semantic_compile_match=True)])))
            self.assertEqual(load(campaign, supplement)['dispositions'], {'missing': 44})
            payload = json.loads(supplement.read_text())
            payload['records'][0]['pyc_sha256'] = 'wrong'
            supplement.write_text(json.dumps(payload))
            with self.assertRaisesRegex(ValueError, 'cache hash'):
                load(campaign, supplement)

    def test_checker_executable_identity_checked_when_recorded(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            record = self.checked(1)
            record['model_check']['command'] = [str(campaign / 'pins/checkmodel'), 'cnf', 'model']
            validate_checker_commands(campaign, 'checked_SAT', record)
            record['model_check']['command'][0] = '/tmp/not-the-pin'
            with self.assertRaisesRegex(ValueError, 'model checker executable'):
                validate_checker_commands(campaign, 'checked_SAT', record)
            record = self.checked(0)
            for stage in record['proof_check']['stages']:
                stage['command'] = [str(campaign / 'pins' / stage['name'])]
            validate_checker_commands(campaign, 'checked_UNSAT', record)
            record['proof_check']['stages'][0]['command'][0] = '/tmp/not-the-pin'
            with self.assertRaisesRegex(ValueError, 'proof checker executable'):
                validate_checker_commands(campaign, 'checked_UNSAT', record)

    def test_retry_requires_monotonic_attempt_and_parent_launch(self):
        for mutation, message in (('attempt', 'attempt order'),
                                  ('launch_job_id', 'parent binding'),
                                  ('launch_sha256', 'parent binding')):
            with self.subTest(mutation=mutation):
                parent, child = self.attempts()
                if mutation == 'attempt':
                    child['retry_attempt']['attempt'] = 2
                else:
                    child['retry_attempt']['parent'][mutation] = 'wrong'
                self.rehash_retry(child)
                with self.assertRaisesRegex(ValueError, message):
                    merge([parent, child])

    def test_retry_metadata_mutation_and_predecessor_identity_rejected(self):
        parent, child = self.attempts()
        child['retry_attempt']['parent']['launch_job_id'] = 'wrong'
        with self.assertRaisesRegex(ValueError, 'metadata mismatch'):
            merge([parent, child])
        parent, child = self.attempts()
        child['retry_attempt']['predecessors'][0]['node'] = 'node999'
        self.rehash_retry(child)
        with self.assertRaisesRegex(ValueError, 'predecessor identity'):
            merge([parent, child])

    def test_second_retry_has_immediate_parent_and_explicit_older_predecessor(self):
        parent, first = self.attempts()
        second = copy.deepcopy(first)
        first['campaign'] = str(ROOT / 'first')
        first['manifest_sha256'] = 'first-manifest'
        first['record_sha256'] = {}
        first['launch'] = {'job_id': '11', 'sha256': 'first-launch'}
        for cell in first['rows']:
            cell['source_campaign'] = first['campaign']
        first['rows'][0]['status'] = 'missing'

        second['campaign'] = str(ROOT / 'second')
        second['manifest_sha256'] = 'second-manifest'
        second['record_sha256'] = {'results/000.json': 'second-result'}
        second['launch'] = {'job_id': '12', 'sha256': 'second-launch'}
        for cell in second['rows']:
            cell['source_campaign'] = second['campaign']
        second['retry_attempt']['attempt'] = 2
        second['retry_attempt']['parent'] = dict(
            campaign='first', manifest_sha256='first-manifest',
            launch_job_id='11', launch_sha256='first-launch')
        self.rehash_retry(second)
        result = merge([parent, first, second])
        self.assertEqual(result['independently_verified'], 1)

        second['retry_attempt']['parent'] = dict(
            campaign='parent', manifest_sha256='parent-manifest',
            launch_job_id='10', launch_sha256='parent-launch')
        self.rehash_retry(second)
        with self.assertRaisesRegex(ValueError, 'parent binding'):
            merge([parent, first, second])

    def test_retry_holder_must_bind_a_seen_launch(self):
        parent, child = self.attempts()
        child['retry_attempt']['resource_exclusion']['holders'][0]['job_id'] = '999_1'
        self.rehash_retry(child)
        with self.assertRaisesRegex(ValueError, 'holder launch binding'):
            merge([parent, child])

    def test_retry_array_exclusion_and_holder_claim_are_exact(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            base = Path(directory)
            campaign = base / 'campaign'
            campaign.mkdir()
            claim = base / 'claim.json'
            claim.write_text(json.dumps(dict(task=1, node='node001')))
            snapshot = base / 'snapshot.txt'
            snapshot.write_text('JOBID|STATE|TIME|NODELIST|NODELIST(REASON)\n'
                                '101_1|RUNNING|1:00|node001|node001\n')
            (campaign / 'array.sbatch').write_text(
                '#SBATCH --exclude=node001\n#SBATCH --array=0\n')
            holder = dict(task=1, job_id='101_1', node='node001',
                          elapsed_at_snapshot='1:00', claim=str(claim),
                          claim_sha256=sha(claim))
            resource = dict(schema='live-cooperative-scratch-holder-exclusion-v1',
                snapshot=str(snapshot), snapshot_sha256=sha(snapshot),
                excluded_nodes=['node001'], holders=[holder], reason='fixture')
            retry = dict(schema='tactix-fixed44-retry-attempt-v1', attempt=1,
                         parent={}, allowed_tasks=[0], predecessors=[],
                         resource_exclusion=resource)
            manifest = dict(resource_contract=dict(node_constraint=dict(
                schema='live-cooperative-scratch-holder-exclusion-v1',
                excluded_nodes=['node001'], reason='fixture')))
            validate_retry_manifest(campaign, manifest, retry, [0])
            (campaign / 'array.sbatch').write_text(
                '#SBATCH --exclude=node002\n#SBATCH --array=0\n')
            with self.assertRaisesRegex(ValueError, 'array exclusion'):
                validate_retry_manifest(campaign, manifest, retry, [0])
            (campaign / 'array.sbatch').write_text(
                '#SBATCH --exclude=node001\n#SBATCH --array=0\n')
            holder['claim_sha256'] = 'wrong'
            with self.assertRaisesRegex(ValueError, 'claim hash'):
                validate_retry_manifest(campaign, manifest, retry, [0])

    def test_array_parser_rejects_throttle_overlap_and_reverse(self):
        self.assertEqual(parse_array('0,2-3,5'), [0, 2, 3, 5])
        for value in ('0-3%2', '0,0', '3-2', ''):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_array(value)


if __name__ == '__main__':
    unittest.main()

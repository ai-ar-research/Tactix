#!/usr/bin/env python3
"""Non-solving protocol tests for fixed-reference full-view measurements."""
import copy
import contextlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import Mock, patch
import fixed_runtime as f
import runtime_task as rt


class FixedRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.ref = dict(solver_seconds=40.0, resource_contract=dict(solve_timeout=1200),
            records=[dict(rank=i,instance=f'{i}.cnf',input_sha256='input',expected='SAT',
                measurement=dict(solve=dict(wall_seconds=1.0,peak_rss_kb=10))) for i in range(1,41)])
        self.records = [dict(rank=i,task=i-1,instance=f'{i}.cnf',expected='SAT',arm='tactix',
            expected_match=True,solve=dict(wall_seconds=1.0,timed_out=False,exit_code=10,peak_rss_kb=10)) for i in range(1,41)]

    def test_candidate_only_contiguous_matrix(self):
        rows = f.candidate_rows(self.ref)
        self.assertEqual([r['task'] for r in rows], list(range(80)))
        self.assertEqual({r['arm'] for r in rows},
                         {'tactix-auto', 'tactix-drat'})
        self.assertEqual([r['rank'] for r in rows[::2]], list(range(1,41)))
        self.assertEqual([r['rank'] for r in rows[1::2]], list(range(1,41)))

    def test_automatic_only_matrix(self):
        rows = f.candidate_rows(self.ref, ('tactix-auto',))
        self.assertEqual(len(rows), 40)
        self.assertEqual([r['task'] for r in rows], list(range(40)))
        self.assertEqual({r['arm'] for r in rows}, {'tactix-auto'})
        self.assertEqual([r['rank'] for r in rows], list(range(1,41)))

    def test_prepared_automatic_bundle_satisfies_runtime_contract(self):
        real_root = f.ROOT
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            for rel in ('benchmarks/runs', 'benchmarks/cache', 'tools/bench',
                        'baseline/pins', 'source', 'authority', 'checker-pins'):
                (root / rel).mkdir(parents=True)
            for name in ('runtime_task.py', 'proof_oracle.py',
                         'anonymous_scratch.py'):
                shutil.copy2(real_root / 'tools/bench' / name,
                             root / 'tools/bench' / name)
            for name in ('checkmodel', 'modelcheck.py'):
                (root / 'baseline/pins' / name).write_text(name)
            input_file = root / 'benchmarks/cache/one.cnf.xz'
            input_file.write_bytes(b'input')
            (root / 'source/marker').write_bytes(b'source')
            (root / 'authority/cumulative.patch').write_bytes(b'patch')
            release, bc = root / 'release', root / 'bc'
            release.write_bytes(b'solver')
            bc.write_bytes(b'solver')
            checker_hashes = {}
            for name in f.CANONICAL_CHECKERS:
                path = root / 'checker-pins' / name
                path.write_bytes(name.encode())
                checker_hashes[name] = f.sha(path)
            authority = dict(source='source', files={'marker':f.sha(root / 'source/marker')},
                             binaries={str(path.relative_to(root)):f.sha(path)
                                       for path in (release, bc)},
                             production='test-source', test_consideration='fixture')
            (root / 'authority/manifest.json').write_text(json.dumps(authority))
            ref = dict(records=[dict(rank=1, instance='one.cnf.xz',
                                     input_sha256=f.sha(input_file), expected='SAT')])
            reference_path = root / 'reference.json'
            reference_path.write_text('{}')
            transport = root / 'transport-helper'
            transport.write_bytes(b'pinned transport fixture')
            transport.chmod(0o750)
            campaign = root / 'benchmarks/runs/canonical-checkers'
            with patch.object(f, 'ROOT', root), \
                 patch.object(f, 'MODEL_CHECKER_PINS', root / 'baseline/pins'), \
                 patch.object(f, 'CANONICAL_CHECKERS', checker_hashes), \
                 patch.object(f, 'reference', return_value=ref):
                manifest = f.prepare(campaign, root / 'authority', release, bc,
                                     reference_path=reference_path,
                                     checker_pins=root / 'checker-pins', dsr_transport=transport)
                self.assertEqual(manifest['pin_sha256']['pins/dsr-transport'], f.sha(transport))
                self.assertIn('pins/dsr-transport.sha256', manifest['pin_sha256'])
                rt.validate_runtime_pin_contract(campaign, manifest)
                self.assertEqual(manifest['schema'], 'tactix-fixed-reference-runtime-v6')
                self.assertEqual(manifest['arms']['tactix-auto']['certificate'], 'auto')
                self.assertEqual(manifest['arms']['tactix-auto']['arguments'], ['--auto-proof'])
                self.assertEqual(manifest['checker_authority']['sha256'],
                                 checker_hashes)
                (root / 'checker-pins/dsr-trim').write_bytes(b'changed')
                with self.assertRaisesRegex(ValueError, 'canonical checker'):
                    f.prepare(root / 'benchmarks/runs/changed-checker',
                              root / 'authority', release, bc,
                              reference_path=reference_path,
                              checker_pins=root / 'checker-pins')

    def test_each_control_score_requires_its_named_arm(self):
        dsr = copy.deepcopy(self.records)
        for record in dsr:
            record['arm'] = 'tactix-dsr'
        self.assertTrue(f.score_records(
            self.ref, dsr, 'tactix-dsr')['runtime_target_met'])
        with self.assertRaisesRegex(ValueError, 'identity'):
            f.score_records(self.ref, dsr, 'tactix-drat')

    def test_dsr_score_requires_the_declared_hybrid_branch(self):
        ref, rows = copy.deepcopy(self.ref), copy.deepcopy(self.records)
        ref['records'][0]['expected'] = rows[0]['expected'] = 'UNSAT'
        rows[0]['arm'] = 'tactix-dsr'
        rows[0]['solve']['exit_code'] = 20
        rows[0]['valid'] = True
        rows[0]['proof_check'] = dict(pipeline='witness-free-dsr-as-drat:drat-trim-cake_lpr')
        for row in rows[1:]:
            row['arm'] = 'tactix-dsr'
        with self.assertRaisesRegex(ValueError, 'hybrid checker pipeline'):
            f.score_records(ref, rows, 'tactix-dsr', require_canonical_dsr=True)
        rows[0]['proof_check'].update(result='verified',
            solver_reported_route='dsr-cake_lpr',
            pipeline='dsr-as-drat-drat-trim-cake_lpr',
            stages=[dict(name=name, result='verified') for name in
                    ('drat-trim', 'cake_lpr')])
        self.assertTrue(f.score_records(ref, rows, 'tactix-dsr',
                                        require_canonical_dsr=True)['runtime_target_met'])

    def test_automatic_score_requires_each_approved_proof_route(self):
        ref, rows = copy.deepcopy(self.ref), copy.deepcopy(self.records)
        for index, (pipeline, route, stages) in enumerate((
                ('veripb-cake-pb', 'pbp', ('veripb', 'cake-pb-cnf')),
                ('dsr-trim-dsr-compress-trestle', 'dsr-trestle',
                 ('dsr-trim', 'dsr-compress', 'srcheck')),
                ('dsr-as-drat-drat-trim-cake_lpr', 'dsr-cake_lpr',
                 ('drat-trim', 'cake_lpr')))):
            ref['records'][index]['expected'] = rows[index]['expected'] = 'UNSAT'
            rows[index].update(verdict='UNSAT', valid=True, expected_match=True,
                               proof_check=dict(result='verified', pipeline=pipeline,
                                   solver_reported_route=route,
                                   stages=[dict(name=name, result='verified')
                                           for name in stages]))
            rows[index]['solve']['exit_code'] = 20
        for row in rows:
            row['arm'] = 'tactix-auto'
        self.assertTrue(f.score_records(ref, rows, 'tactix-auto',
            require_automatic_routes=True)['runtime_target_met'])
        rows[2]['proof_check']['pipeline'] = 'unapproved-shortcut'
        with self.assertRaisesRegex(ValueError, 'plan section 5'):
            f.score_records(ref, rows, 'tactix-auto', require_automatic_routes=True)

    def test_complete_pending_score_not_verified(self):
        r = f.score_records(self.ref,self.records)
        self.assertEqual(r['ratio'],1.0)
        self.assertEqual(r['fixed_reference_seconds'],40.0)
        self.assertNotIn('fixed_kissat_seconds',r)
        self.assertIn('reference_seconds',r['per_rank'][0])
        self.assertNotIn('kissat_seconds',r['per_rank'][0])
        self.assertTrue(r['runtime_target_met'])
        self.assertEqual(r['checked_clean'],0)

    def test_explicit_suite_target_replaces_diagnostic_ratio(self):
        for record in self.records:
            record['solve']['wall_seconds'] = 1.05
        default = f.score_records(self.ref, self.records)
        approved = f.score_records(
            self.ref, self.records, runtime_target_seconds=42.5)
        self.assertFalse(default['runtime_target_met'])
        self.assertTrue(approved['runtime_target_met'])
        self.assertEqual(approved['runtime_target_seconds'], 42.5)

    def test_submit_records_exact_manifest_and_job(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            campaign = root / 'campaign'
            campaign.mkdir()
            manifest = {'fixed_reference': {'path': 'ref.json'}}
            (campaign / 'matrix.tsv').write_text(
                'task\trank\tarm\tinstance\texpected\n'
                + ''.join(f'{i-1}\t{i}\ttactix-dsr\t{i}.cnf\tSAT\n'
                          for i in range(1, 41)))
            (campaign / 'manifest.json').write_text('{}')
            completed = Mock(stdout='12345;cluster\n')
            with (patch.object(f, 'ROOT', root),
                  patch.object(f, 'verify_campaign', return_value=manifest),
                  patch.object(f, 'reference', return_value=self.ref),
                  patch.object(f.subprocess, 'run', return_value=completed) as run):
                with contextlib.redirect_stdout(io.StringIO()):
                    record = f.submit(campaign)
            self.assertEqual(record['job_id'], '12345')
            self.assertEqual(record['tasks'], 40)
            self.assertEqual(json.loads((campaign / 'launch.json').read_text()),
                             record)
            run.assert_called_once_with(
                ['sbatch', '--parsable', str(campaign / 'array.sbatch')],
                check=True, capture_output=True, text=True)
            with patch.object(f, 'verify_campaign', return_value=manifest), \
                 self.assertRaisesRegex(ValueError, 'already submitted'):
                f.submit(campaign)

    def test_partial_has_no_aggregate(self):
        r = f.score_records(self.ref,self.records[:-1])
        self.assertIsNone(r['ratio'])
        self.assertIsNone(r['candidate_par2'])
        self.assertFalse(r['runtime_target_met'])

    def test_duplicate_or_unknown_rank(self):
        for rank in (1,41):
            records = copy.deepcopy(self.records)
            records[-1]['rank'] = rank
            with self.subTest(rank=rank), self.assertRaises(ValueError):
                f.score_records(self.ref,records)

    def test_identity_mismatch(self):
        for key,value in dict(arm='kissat',instance='wrong',expected='UNSAT').items():
            records = copy.deepcopy(self.records)
            records[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                f.score_records(self.ref,records)

    def test_certificate_rejection(self):
        self.records[0]['valid'] = False
        with self.assertRaisesRegex(ValueError,'certificate'):
            f.score_records(self.ref,self.records)

    def test_timeout_charged_and_not_goal(self):
        self.records[0]['solve'].update(timed_out=True,exit_code=124,wall_seconds=1200)
        self.records[0]['expected_match'] = False
        r = f.score_records(self.ref,self.records)
        self.assertEqual(r['candidate_par2'],2439)
        self.assertEqual(r['expected_solves'],39)
        self.assertFalse(r['runtime_target_met'])

    def test_checker_cleanup_required(self):
        for r in self.records:
            r.update(valid=True,artifact_deleted=True,derived_artifacts_deleted=True,cleanup_pending=False)
        self.assertEqual(f.score_records(self.ref,self.records)['checked_clean'],40)
        self.records[0]['cleanup_pending'] = True
        self.assertEqual(f.score_records(self.ref,self.records)['checked_clean'],39)

    def test_final_clean_timeout_keeps_par2_penalty(self):
        self.records[0].update(valid=False, verdict='UNKNOWN', expected_match=False,
                              proof_check=None, model_check=None)
        self.records[0]['solve'].update(timed_out=True, exit_code=None, wall_seconds=1200)
        result = f.score_records(self.ref, self.records)
        self.assertEqual(result['candidate_par2'], 2439)
        self.assertEqual(result['expected_solves'], 39)
        self.assertFalse(result['runtime_target_met'])
        self.records[0]['proof_check'] = dict(result='not-verified')
        with self.assertRaisesRegex(ValueError, 'certificate'):
            f.score_records(self.ref, self.records)

    def test_invalid_time(self):
        for value in (-1,float('nan'),float('inf')):
            self.records[0]['solve']['wall_seconds'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                f.score_records(self.ref,self.records)

    def test_protocol_identity_rejected_before_records(self):
        # Exercise the actual filesystem scorer's resource/input binding checks.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            campaign = root / 'campaign'
            campaign.mkdir()
            rp = root / 'reference.json'
            rp.write_text('{}')
            resources = dict(cpus_per_task=12,memory_mib=48000,solve_timeout=1200,
                checker_timeout=16000,one_solver_per_task=True,proof_mode='DRAT')
            ref = dict(self.ref,resource_contract=resources)
            m = dict(fixed_reference=dict(path='reference.json',sha256=f.sha(rp)),
                resource_contract=resources,input_sha256={r['instance']:'input' for r in self.ref['records']})
            for key,value in [('cpus_per_task',1),('memory_mib',16000),('proof_mode','none'),('input',{})]:
                bad = copy.deepcopy(m)
                if key == 'input': bad['input_sha256'] = value
                else: bad['resource_contract'][key] = value
                (campaign / 'manifest.json').write_text(json.dumps(bad))
                with self.subTest(key=key), patch.object(f,'ROOT',root), patch.object(f,'reference',return_value=ref), self.assertRaises(ValueError):
                    f.score(campaign)

    def test_fifty_requires_all_fifty(self):
        ref = copy.deepcopy(self.ref)
        records = copy.deepcopy(self.records)
        for i in range(41,51):
            ref['records'].append(dict(rank=i,instance=f'{i}.cnf',expected='SAT',
                measurement=dict(solve=dict(wall_seconds=1.0))))
            records.append(dict(rank=i,task=i-1,instance=f'{i}.cnf',expected='SAT',arm='tactix',
                expected_match=True,solve=dict(wall_seconds=1.0,timed_out=False,exit_code=10)))
        ref['solver_seconds'] = 50.0
        self.assertFalse(f.score_records(ref, records[:40])['complete'])
        self.assertFalse(f.score_records(ref, records[:-1])['runtime_target_met'])
        result = f.score_records(ref, records)
        self.assertEqual(result['ratio'], 1.0)
        self.assertTrue(result['runtime_target_met'])
        self.assertEqual([r['rank'] for r in f.candidate_rows(ref)[::2]],
                         list(range(1,51)))
        records[-1]['valid'] = False
        with self.assertRaisesRegex(ValueError, 'certificate'):
            f.score_records(ref, records)

    def test_extended_reference_preserves_original_forty(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            original = dict(self.ref, schema='tactix-fixed-rank1-40-kissat-reference-v1')
            original['resource_contract'].update(partition='cpu-dense-hwulab-q',cpus_per_task=12,
                memory_mib=48000,checker_timeout=16000,one_solver_per_task=True)
            op = root/'old.json'; op.write_text(json.dumps(original))
            ref = copy.deepcopy(original)
            ref.update(schema='tactix-fixed-rank1-50-kissat-reference-v1',solver_seconds=50.0,
                       original_reference=dict(path='old.json',sha256=f.sha(op)))
            for i in range(41,51):
                ref['records'].append(dict(rank=i,instance=f'{i}.cnf',expected='SAT',
                    measurement=dict(solve=dict(wall_seconds=1.0))))
            rp = root/'new.json'; rp.write_text(json.dumps(ref))
            with patch.object(f,'ROOT',root):
                self.assertEqual(len(f.reference(rp)['records']),50)
                bad = copy.deepcopy(ref)
                bad['records'][0]['measurement']['solve']['wall_seconds'] = 2.0
                bad['solver_seconds'] += 1.0
                rp.write_text(json.dumps(bad))
                with self.assertRaisesRegex(ValueError,'original forty'):
                    f.reference(rp)
                bad = copy.deepcopy(ref); bad['resource_contract']['cpus_per_task'] = 4
                rp.write_text(json.dumps(bad))
                with self.assertRaisesRegex(ValueError,'resource mismatch'):
                    f.reference(rp)

    def test_b27c5_reference_is_a_fixed_32_row_vector(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            records = [dict(rank=i, tier='B' if i <= 27 else 'C',
                            instance=f'{i}.cnf', input_sha256='input',
                            expected='SAT',
                            measurement=dict(solve=dict(wall_seconds=1.0)))
                       for i in range(1, 33)]
            ref = dict(schema='tactix-fixed-b27c5-reference-v1',
                       solver_seconds=32.0,
                       resource_contract=dict(solve_timeout=1200),
                       records=records)
            path = root / 'bc.json'
            path.write_text(json.dumps(ref))
            loaded = f.reference(path)
            rows = f.candidate_rows(loaded)
            self.assertEqual(len(rows), 64)
            self.assertEqual({row['tier'] for row in rows[:54]}, {'B'})
            self.assertEqual({row['tier'] for row in rows[54:]}, {'C'})

    def test_full_b28c5_and_tactix_rank50_references_are_supported(self):
        for schema, count in [('tactix-fixed-b28c5-reference-v1', 33),
                              ('tactix-fixed-rank1-50-reference-v1', 50)]:
            with self.subTest(schema=schema), tempfile.TemporaryDirectory() as tmp:
                records = [dict(rank=i, tier='B', instance=f'{i}.cnf',
                                input_sha256='input', expected='SAT',
                                measurement=dict(solve=dict(wall_seconds=1.0)))
                           for i in range(1, count + 1)]
                ref = dict(schema=schema, solver_seconds=float(count),
                           resource_contract=dict(solve_timeout=1200),
                           records=records)
                path = Path(tmp) / 'reference.json'
                path.write_text(json.dumps(ref))
                self.assertEqual(len(f.reference(path)['records']), count)

    def test_tactix_rank_reference_is_supported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'tactix-rank.json'
            ref = copy.deepcopy(self.ref)
            ref['schema'] = 'tactix-fixed-rank1-40-reference-v1'
            path.write_text(json.dumps(ref))
            self.assertEqual(len(f.reference(path)['records']), 40)

    def test_comparison_reference_identity_includes_status_and_hash(self):
        identity = f.reference_identity(self.ref)
        for key, value in [('instance', 'other.cnf'),
                           ('input_sha256', 'other'),
                           ('expected', 'UNSAT')]:
            changed = copy.deepcopy(self.ref)
            changed['records'][0][key] = value
            with self.subTest(key=key):
                self.assertNotEqual(f.reference_identity(changed), identity)


if __name__ == '__main__':
    unittest.main()

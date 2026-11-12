"""Gate protocol boundaries: fixed reference, paper controller and Tier-D scorers.

Non-solving: every record is a synthetic fixture.
"""
import copy
import contextlib
import csv
import hashlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import Mock, patch

import gate
from gate import (candidate_arguments, checked_result, proof_cost_comparison,
                  proof_disposition, score_row, substituted_results,
                  within_runtime_target)
import runtime_task


class FixedRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.ref = dict(solver_seconds=40.0, resource_contract=dict(solve_timeout=1200),
            records=[dict(rank=i,instance=f'{i}.cnf',input_sha256='input',expected='SAT',
                measurement=dict(solve=dict(wall_seconds=1.0,peak_rss_kb=10))) for i in range(1,41)])
        self.records = [dict(rank=i,task=i-1,instance=f'{i}.cnf',expected='SAT',arm='tactix',
            expected_match=True,solve=dict(wall_seconds=1.0,timed_out=False,exit_code=10,peak_rss_kb=10)) for i in range(1,41)]

    def test_candidate_only_contiguous_matrix(self):
        rows = gate.candidate_rows(self.ref)
        self.assertEqual([r['task'] for r in rows], list(range(80)))
        self.assertEqual({r['arm'] for r in rows},
                         {'tactix-pbp', 'tactix-drat'})
        self.assertEqual([r['rank'] for r in rows[::2]], list(range(1,41)))
        self.assertEqual([r['rank'] for r in rows[1::2]], list(range(1,41)))

    def test_pbp_only_matrix(self):
        rows = gate.candidate_rows(self.ref, ('tactix-pbp',))
        self.assertEqual(len(rows), 40)
        self.assertEqual([r['task'] for r in rows], list(range(40)))
        self.assertEqual({r['arm'] for r in rows}, {'tactix-pbp'})
        self.assertEqual([r['rank'] for r in rows], list(range(1,41)))

    def test_prepared_pbp_bundle_satisfies_runtime_contract(self):
        real_root = gate.ROOT
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
            for name in gate.CANONICAL_CHECKERS:
                path = root / 'checker-pins' / name
                path.write_bytes(name.encode())
                checker_hashes[name] = gate.sha(path)
            authority = dict(source='source', files={'marker':gate.sha(root / 'source/marker')},
                             binaries={str(path.relative_to(root)):gate.sha(path)
                                       for path in (release, bc)},
                             production='test-source', test_consideration='fixture')
            (root / 'authority/manifest.json').write_text(json.dumps(authority))
            ref = dict(records=[dict(rank=1, instance='one.cnf.xz',
                                     input_sha256=gate.sha(input_file), expected='SAT')])
            reference_path = root / 'reference.json'
            reference_path.write_text('{}')
            transport = root / 'transport-helper'
            transport.write_bytes(b'pinned transport fixture')
            transport.chmod(0o750)
            campaign = root / 'benchmarks/runs/canonical-checkers'
            with patch.object(gate, 'ROOT', root), \
                 patch.object(gate, 'MODEL_CHECKER_PINS', root / 'baseline/pins'), \
                 patch.object(gate, 'CANONICAL_CHECKERS', checker_hashes), \
                 patch.object(gate, 'reference', return_value=ref):
                manifest = gate.fixed_prepare(campaign, root / 'authority', release, bc,
                                     reference_path=reference_path,
                                     checker_pins=root / 'checker-pins', dsr_transport=transport)
                self.assertEqual(manifest['pin_sha256']['pins/dsr-transport'], gate.sha(transport))
                self.assertIn('pins/dsr-transport.sha256', manifest['pin_sha256'])
                runtime_task.validate_runtime_pin_contract(campaign, manifest)
                self.assertEqual(manifest['schema'], 'tactix-fixed-reference-runtime-v7')
                self.assertEqual(gate.verify_campaign(campaign), manifest)
                self.assertEqual(manifest['arms']['tactix-pbp']['certificate'], 'pbp')
                self.assertEqual(manifest['arms']['tactix-pbp']['arguments'], ['--pbp'])
                self.assertEqual(manifest['checker_authority']['sha256'],
                                 checker_hashes)
                (root / 'checker-pins/dsr-trim').write_bytes(b'changed')
                with self.assertRaisesRegex(ValueError, 'canonical checker'):
                    gate.fixed_prepare(root / 'benchmarks/runs/changed-checker',
                              root / 'authority', release, bc,
                              reference_path=reference_path,
                              checker_pins=root / 'checker-pins')

    def test_each_control_score_requires_its_named_arm(self):
        dsr = copy.deepcopy(self.records)
        for record in dsr:
            record['arm'] = 'tactix-dsr'
        self.assertTrue(gate.score_records(
            self.ref, dsr, 'tactix-dsr')['runtime_target_met'])
        with self.assertRaisesRegex(ValueError, 'identity'):
            gate.score_records(self.ref, dsr, 'tactix-drat')

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
            gate.score_records(ref, rows, 'tactix-dsr', require_canonical_dsr=True)
        rows[0]['proof_check'].update(result='verified',
            solver_reported_route='dsr-cake_lpr',
            pipeline='dsr-as-drat-drat-trim-cake_lpr',
            stages=[dict(name=name, result='verified') for name in
                    ('drat-trim', 'cake_lpr')])
        self.assertTrue(gate.score_records(ref, rows, 'tactix-dsr',
                                        require_canonical_dsr=True)['runtime_target_met'])

    def test_pbp_score_requires_the_approved_proof_route(self):
        ref, rows = copy.deepcopy(self.ref), copy.deepcopy(self.records)
        ref['records'][0]['expected'] = rows[0]['expected'] = 'UNSAT'
        rows[0].update(verdict='UNSAT', valid=True, expected_match=True,
                       proof_check=dict(result='verified',
                           pipeline='veripb-cake-pb', solver_reported_route='pbp',
                           stages=[dict(name=name, result='verified')
                                   for name in ('veripb', 'cake-pb-cnf')]))
        rows[0]['solve']['exit_code'] = 20
        for row in rows:
            row['arm'] = 'tactix-pbp'
        self.assertTrue(gate.score_records(ref, rows, 'tactix-pbp',
            require_pbp_route=True)['runtime_target_met'])
        rows[0]['proof_check']['pipeline'] = 'unapproved-shortcut'
        with self.assertRaisesRegex(ValueError, 'whole-run PBP'):
            gate.score_records(ref, rows, 'tactix-pbp', require_pbp_route=True)

    def test_complete_pending_score_not_verified(self):
        r = gate.score_records(self.ref,self.records)
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
        default = gate.score_records(self.ref, self.records)
        approved = gate.score_records(
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
            with (patch.object(gate, 'ROOT', root),
                  patch.object(gate, 'verify_campaign', return_value=manifest),
                  patch.object(gate, 'reference', return_value=self.ref),
                  patch.object(gate.subprocess, 'run', return_value=completed) as run):
                with contextlib.redirect_stdout(io.StringIO()):
                    record = gate.submit(campaign)
            self.assertEqual(record['job_id'], '12345')
            self.assertEqual(record['tasks'], 40)
            self.assertEqual(json.loads((campaign / 'launch.json').read_text()),
                             record)
            run.assert_called_once_with(
                ['sbatch', '--parsable', str(campaign / 'array.sbatch')],
                check=True, capture_output=True, text=True)
            with patch.object(gate, 'verify_campaign', return_value=manifest), \
                 self.assertRaisesRegex(ValueError, 'already submitted'):
                gate.submit(campaign)

    def test_partial_has_no_aggregate(self):
        r = gate.score_records(self.ref,self.records[:-1])
        self.assertIsNone(r['ratio'])
        self.assertIsNone(r['candidate_par2'])
        self.assertFalse(r['runtime_target_met'])

    def test_duplicate_or_unknown_rank(self):
        for rank in (1,41):
            records = copy.deepcopy(self.records)
            records[-1]['rank'] = rank
            with self.subTest(rank=rank), self.assertRaises(ValueError):
                gate.score_records(self.ref,records)

    def test_identity_mismatch(self):
        for key,value in dict(arm='kissat',instance='wrong',expected='UNSAT').items():
            records = copy.deepcopy(self.records)
            records[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                gate.score_records(self.ref,records)

    def test_certificate_rejection(self):
        self.records[0]['valid'] = False
        with self.assertRaisesRegex(ValueError,'certificate'):
            gate.score_records(self.ref,self.records)

    def test_timeout_charged_and_not_goal(self):
        self.records[0]['solve'].update(timed_out=True,exit_code=124,wall_seconds=1200)
        self.records[0]['expected_match'] = False
        r = gate.score_records(self.ref,self.records)
        self.assertEqual(r['candidate_par2'],2439)
        self.assertEqual(r['expected_solves'],39)
        self.assertFalse(r['runtime_target_met'])

    def test_checker_cleanup_required(self):
        for r in self.records:
            r.update(valid=True,artifact_deleted=True,derived_artifacts_deleted=True,cleanup_pending=False)
        self.assertEqual(gate.score_records(self.ref,self.records)['checked_clean'],40)
        self.records[0]['cleanup_pending'] = True
        self.assertEqual(gate.score_records(self.ref,self.records)['checked_clean'],39)

    def test_final_clean_timeout_keeps_par2_penalty(self):
        self.records[0].update(valid=False, verdict='UNKNOWN', expected_match=False,
                              proof_check=None, model_check=None)
        self.records[0]['solve'].update(timed_out=True, exit_code=None, wall_seconds=1200)
        result = gate.score_records(self.ref, self.records)
        self.assertEqual(result['candidate_par2'], 2439)
        self.assertEqual(result['expected_solves'], 39)
        self.assertFalse(result['runtime_target_met'])
        self.records[0]['proof_check'] = dict(result='not-verified')
        with self.assertRaisesRegex(ValueError, 'certificate'):
            gate.score_records(self.ref, self.records)

    def test_invalid_time(self):
        for value in (-1,float('nan'),float('inf')):
            self.records[0]['solve']['wall_seconds'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                gate.score_records(self.ref,self.records)

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
            m = dict(fixed_reference=dict(path='reference.json',sha256=gate.sha(rp)),
                resource_contract=resources,input_sha256={r['instance']:'input' for r in self.ref['records']})
            for key,value in [('cpus_per_task',1),('memory_mib',16000),('proof_mode','none'),('input',{})]:
                bad = copy.deepcopy(m)
                if key == 'input': bad['input_sha256'] = value
                else: bad['resource_contract'][key] = value
                (campaign / 'manifest.json').write_text(json.dumps(bad))
                with self.subTest(key=key), patch.object(gate,'ROOT',root), patch.object(gate,'reference',return_value=ref), self.assertRaises(ValueError):
                    gate.fixed_score(campaign)

    def test_fifty_requires_all_fifty(self):
        ref = copy.deepcopy(self.ref)
        records = copy.deepcopy(self.records)
        for i in range(41,51):
            ref['records'].append(dict(rank=i,instance=f'{i}.cnf',expected='SAT',
                measurement=dict(solve=dict(wall_seconds=1.0))))
            records.append(dict(rank=i,task=i-1,instance=f'{i}.cnf',expected='SAT',arm='tactix',
                expected_match=True,solve=dict(wall_seconds=1.0,timed_out=False,exit_code=10)))
        ref['solver_seconds'] = 50.0
        self.assertFalse(gate.score_records(ref, records[:40])['complete'])
        self.assertFalse(gate.score_records(ref, records[:-1])['runtime_target_met'])
        result = gate.score_records(ref, records)
        self.assertEqual(result['ratio'], 1.0)
        self.assertTrue(result['runtime_target_met'])
        self.assertEqual([r['rank'] for r in gate.candidate_rows(ref)[::2]],
                         list(range(1,51)))
        records[-1]['valid'] = False
        with self.assertRaisesRegex(ValueError, 'certificate'):
            gate.score_records(ref, records)

    def test_extended_reference_preserves_original_forty(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            original = dict(self.ref, schema='tactix-fixed-rank1-40-kissat-reference-v1')
            original['resource_contract'].update(partition='cpu-dense-hwulab-q',cpus_per_task=12,
                memory_mib=48000,checker_timeout=16000,one_solver_per_task=True)
            op = root/'old.json'; op.write_text(json.dumps(original))
            ref = copy.deepcopy(original)
            ref.update(schema='tactix-fixed-rank1-50-kissat-reference-v1',solver_seconds=50.0,
                       original_reference=dict(path='old.json',sha256=gate.sha(op)))
            for i in range(41,51):
                ref['records'].append(dict(rank=i,instance=f'{i}.cnf',expected='SAT',
                    measurement=dict(solve=dict(wall_seconds=1.0))))
            rp = root/'new.json'; rp.write_text(json.dumps(ref))
            with patch.object(gate,'ROOT',root):
                self.assertEqual(len(gate.reference(rp)['records']),50)
                bad = copy.deepcopy(ref)
                bad['records'][0]['measurement']['solve']['wall_seconds'] = 2.0
                bad['solver_seconds'] += 1.0
                rp.write_text(json.dumps(bad))
                with self.assertRaisesRegex(ValueError,'original forty'):
                    gate.reference(rp)
                bad = copy.deepcopy(ref); bad['resource_contract']['cpus_per_task'] = 4
                rp.write_text(json.dumps(bad))
                with self.assertRaisesRegex(ValueError,'resource mismatch'):
                    gate.reference(rp)

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
            loaded = gate.reference(path)
            rows = gate.candidate_rows(loaded)
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
                self.assertEqual(len(gate.reference(path)['records']), count)

    def test_tactix_rank_reference_is_supported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'tactix-rank.json'
            ref = copy.deepcopy(self.ref)
            ref['schema'] = 'tactix-fixed-rank1-40-reference-v1'
            path.write_text(json.dumps(ref))
            self.assertEqual(len(gate.reference(path)['records']), 40)

    def test_comparison_reference_identity_includes_status_and_hash(self):
        identity = gate.reference_identity(self.ref)
        for key, value in [('instance', 'other.cnf'),
                           ('input_sha256', 'other'),
                           ('expected', 'UNSAT')]:
            changed = copy.deepcopy(self.ref)
            changed['records'][0][key] = value
            with self.subTest(key=key):
                self.assertNotEqual(gate.reference_identity(changed), identity)


def proof(elapsed, timeout=False):
    return dict(pipeline='dsr-as-drat-drat-trim-cake_lpr',
        result='not-verified' if timeout else 'verified', timed_out=timeout,
        wall_seconds=elapsed, stages=[dict(name='drat-trim', result='verified'),
            dict(name='pipeline-deadline' if timeout else 'cake_lpr',
                 result='timeout' if timeout else 'verified', timed_out=timeout)])


class CandidateArgumentTests(unittest.TestCase):
    def test_default_pbp_command_and_profile_are_exact(self):
        self.assertEqual(candidate_arguments(), [])
        self.assertEqual(candidate_arguments(['--pbp']), ['--pbp'])
        for family in ('affine-search', 'graph-labeling'):
            for mode in ('on', 'off'):
                argument = '--default-' + family + '=' + mode
                self.assertEqual(candidate_arguments([argument]), [argument])
        combined = ['--default-affine-search=off', '--default-graph-labeling=on']
        self.assertEqual(candidate_arguments(combined), combined)

    def test_rejects_empty_proof_policy_and_conflicting_overrides(self):
        for arguments in ([''], ['--drat'], ['--auto-proof'], ['--lrat'],
                          ['--random-policy'], ['--default-affine-search=maybe'],
                          ['--default-graph-labeling=maybe'],
                          ['--default-nol-walk=on'],
                          [' --default-affine-search=on'],
                          ['--default-affine-search=on --drat'],
                          ['--default-affine-search=on', '--default-affine-search=off'],
                          ['--default-affine-search=on', '--default-affine-search=on'],
                          ['--default-graph-labeling=on', '--default-graph-labeling=off'],
                          ['--default-graph-labeling=on', '--default-graph-labeling=on']):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                candidate_arguments(arguments)
        # Reject before any authority/pin read or campaign mutation.
        with self.assertRaises(ValueError):
            gate.controller_prepare('invalid', Path('/nonexistent'), ['--lrat'])

    def test_preparation_binds_exact_arguments_without_changing_baseline(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            historical = root / 'benchmarks/runs/historical'
            pins = historical / 'pins'
            pins.mkdir(parents=True)
            tools = root / 'tools/bench'; tools.mkdir(parents=True)
            for name in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
                (tools / name).write_text('# fixed non-executed protocol fixture\n')
            checkers = {'checker': None}
            for name in ('checkmodel', 'modelcheck.py', *checkers):
                (pins / name).write_text('fixed protocol artifact ' + name)
            checkers['checker'] = gate.sha(pins / 'checker')
            old = {'arms': {'tactix': {'arguments': ['--auto-proof']}}}
            source = historical / 'manifest.json'; source.write_text(json.dumps(old))
            matrix = historical / 'matrix.tsv'; matrix.write_text('instance\nfixture.cnf\n')
            cache = root / 'benchmarks/cache'; cache.mkdir()
            (cache / 'fixture.cnf').write_text('p cnf 1 1\n1 0\n')
            accepted = dict(source_manifest=str(source.relative_to(root)),
                source_manifest_sha256=gate.sha(source),
                input_sha256={'fixture.cnf': gate.sha(cache / 'fixture.cnf')},
                pin_sha256={'pins/' + name: gate.sha(pins / name)
                            for name in ('checkmodel', 'modelcheck.py', *checkers)},
                matrix_sha256=gate.sha(matrix), expected_rows=1,
                acceptance_runtime_seconds=11.0,
                resource_contract=dict(partition='fixture', cpus_per_task=2,
                                       memory_mib=8192, solve_timeout=10,
                                       one_solver_per_task=True))
            baseline = root / 'baseline.json'; baseline.write_text(json.dumps({'sets': {'wv100': accepted}}))
            release = root / 'build/pin-fixture/tactix'; release.parent.mkdir(parents=True)
            release.write_text('fixed non-executed solver fixture')
            (release.parent / 'PIN.json').write_text(json.dumps(dict(build_type='Release',
                source_commit='fixed-fixture', binaries={'tactix': gate.sha(release)})))
            baseline_before = baseline.read_bytes()
            with patch.object(gate, 'ROOT', root), \
                 patch.object(gate, 'CONTROLLER_BASELINE', baseline), \
                 patch.object(gate, 'CANONICAL_CHECKERS', checkers), \
                 patch.object(gate, 'verify_controller_paper') as verified:
                for prefix, arguments in (('legacy', []),
                                          ('enabled', ['--default-graph-labeling=on'])):
                    campaigns = gate.controller_prepare(prefix, release, arguments)
                    manifest = json.loads((Path(campaigns[0]) / 'manifest.json').read_text())
                    self.assertEqual(manifest['arms']['tactix']['arguments'],
                                     ['--pbp', *arguments])
                    self.assertEqual(manifest['resource_contract']['throttle'], None)
                    self.assertEqual(manifest['baseline']['sha256'], gate.sha(baseline))
                self.assertEqual(verified.call_count, 2)
            self.assertEqual(baseline.read_bytes(), baseline_before)
            self.assertEqual(json.loads(source.read_text()), old)


class RegressionGateTests(unittest.TestCase):
    def test_paper_runtime_target_allows_exactly_ten_percent(self):
        paper_seconds = 100.0
        ceiling = paper_seconds * 1.10
        self.assertTrue(within_runtime_target(ceiling, ceiling))
        self.assertFalse(within_runtime_target(ceiling + 0.001, ceiling))

    def test_small_baseline_uses_twenty_seconds_and_equality_passes(self):
        self.assertEqual(proof_disposition(proof(30), proof(10)), ('verified', []))
        self.assertIn('significant', proof_disposition(proof(30.001), proof(10))[1][0])

    def test_large_baseline_uses_thirty_percent_and_equality_passes(self):
        self.assertEqual(proof_disposition(proof(130), proof(100)), ('verified', []))
        self.assertTrue(proof_disposition(proof(130.001), proof(100))[1])

    def test_timeout_within_cost_bound_remains_censored_and_can_pass(self):
        self.assertEqual(proof_disposition(proof(1800, True), proof(1500)), ('censored', []))

    def test_timeout_already_exceeding_completed_baseline_limit_fails(self):
        disposition, errors = proof_disposition(proof(1800, True), proof(100))
        self.assertEqual(disposition, 'censored')
        self.assertIn('significant proof-check cost regression', errors)

    def test_censored_baseline_supplies_no_completion_cost_claim(self):
        self.assertEqual(proof_disposition(proof(1800, True), proof(57600, True)), ('censored', []))

    def test_censored_baseline_cost_comparison_is_explicitly_inconclusive(self):
        for candidate in (proof(30), proof(1800, True)):
            comparison = proof_cost_comparison(candidate, proof(57600, True))
            self.assertEqual(comparison['status'], 'inconclusive')
            self.assertIsNone(comparison['allowed_completion_seconds'])
            self.assertTrue(comparison['baseline_censored'])

    def test_cost_dispositions_distinguish_completion_censoring_and_slowdown(self):
        for candidate, baseline, expected in (
                (proof(130), proof(100), 'completed-within-bound'),
                (proof(1800, True), proof(1500), 'censored-within-bound'),
                (proof(1800, True), proof(100), 'significant-slowdown')):
            comparison = proof_cost_comparison(candidate, baseline)
            self.assertEqual(comparison['status'], expected)
            self.assertEqual(comparison['candidate_censored'], candidate['timed_out'])

    def test_missing_baseline_is_inconclusive_even_for_fast_completed_check(self):
        comparison = proof_cost_comparison(proof(1), None)
        self.assertEqual(comparison['status'], 'inconclusive')
        self.assertIsNone(comparison['allowed_completion_seconds'])

    def test_cost_disposition_does_not_compare_checker_failure_as_fast_completion(self):
        failed = proof(1)
        failed['result'] = 'not-verified'
        self.assertEqual(proof_cost_comparison(failed, proof(100))['status'], 'checker-failure')
        failed = proof(1800, True)
        failed['stages'][0]['result'] = 'rejected'
        self.assertEqual(proof_cost_comparison(failed, proof(1500))['status'], 'checker-failure')

    def test_recorded_rejection_dominates_later_deadline(self):
        rejected = proof(1800, True)
        rejected['stages'][0]['result'] = 'rejected'
        self.assertEqual(proof_disposition(rejected, None)[0], 'failed')

    def test_missing_final_independent_verifier_is_not_verified(self):
        incomplete = proof(10)
        incomplete['stages'].pop()
        self.assertEqual(proof_disposition(incomplete, None)[0], 'failed')

    def test_bad_time_or_missing_outcome_fails(self):
        for elapsed in (-1, float('nan'), float('inf'), None):
            self.assertEqual(proof_disposition(proof(elapsed), None)[0], 'failed')
        self.assertEqual(proof_disposition(None, None)[0], 'missing')

    def sample(self):
        row = dict(artifact_deleted=True, derived_artifacts_deleted=True,
            cleanup_pending=False, verdict='UNSAT', expected_match=True,
            solve=dict(exit_code=20, timed_out=False, wall_seconds=1, peak_rss_kb=100),
            proof_check=proof(1800, True), valid=False)
        accepted = dict(expected='UNSAT', require_solver_retention=True, proof_check=proof(1500))
        contract = dict(solve_timeout=1200, memory_mib=32768)
        return row, accepted, contract

    def test_censored_result_stays_unverified_while_retention_gate_passes(self):
        row, accepted, contract = self.sample()
        self.assertEqual(score_row(row, accepted, contract), ('censored', []))
        self.assertFalse(row['valid'])

    def test_saved_solver_timeout_exception_does_not_excuse_checker_error(self):
        row, accepted, contract = self.sample()
        accepted['require_solver_retention'] = False
        row['solve'].update(timed_out=True, wall_seconds=1200.005, exit_code=None)
        row['verdict'] = 'UNKNOWN'
        row['proof_check']['stages'][0]['result'] = 'rejected'
        self.assertTrue(score_row(row, accepted, contract)[1])
        row['proof_check'] = None
        self.assertEqual(score_row(row, accepted, contract), ('solver-timeout', []))
        accepted['require_solver_retention'] = True
        self.assertIn('lost retained solve', score_row(row, accepted, contract)[1])

    def test_paper_timeout_is_an_explicit_exception_but_new_timeout_fails(self):
        row, accepted, contract = self.sample()
        row['solve']['timed_out'] = True
        row['solve']['wall_seconds'] = 1200.005
        row['solve']['exit_code'] = None
        row['verdict'] = 'UNKNOWN'
        row['proof_check'] = None
        accepted['require_solver_retention'] = False
        self.assertEqual(score_row(row, accepted, contract), ('solver-timeout', []))
        accepted['require_solver_retention'] = True
        self.assertIn('lost retained solve', score_row(row, accepted, contract)[1])

    def test_solver_timeout_evidence_still_obeys_per_instance_bound(self):
        row, accepted, contract = self.sample()
        accepted['require_solver_retention'] = False
        row['solve'].update(timed_out=True, wall_seconds=1201.001, exit_code=None)
        row['verdict'] = 'UNKNOWN'
        row['proof_check'] = None
        self.assertIn('invalid solver-timeout evidence',
                      score_row(row, accepted, contract)[1])

    def test_paper_censored_proof_has_no_invented_completion_target(self):
        row, accepted, contract = self.sample()
        accepted['proof_check'] = proof(1800, True)
        self.assertEqual(score_row(row, accepted, contract), ('censored', []))

    def test_model_requires_independent_success(self):
        row, accepted, contract = self.sample()
        row.update(verdict='SAT', model_check=dict(result='verified', exit_code=0, timed_out=False))
        row['solve']['exit_code'] = 10
        accepted['expected'] = 'SAT'
        self.assertEqual(score_row(row, accepted, contract), ('model-verified', []))
        row['model_check']['exit_code'] = 1
        self.assertTrue(score_row(row, accepted, contract)[1])

    def test_cleanup_memory_solve_time_and_exit_failures(self):
        for key, value in [('wall_seconds', 1201), ('peak_rss_kb', 32768 * 1024 + 1),
                           ('exit_code', 0)]:
            row, accepted, contract = self.sample()
            row['solve'][key] = value
            self.assertTrue(score_row(row, accepted, contract)[1])
        row, accepted, contract = self.sample()
        row['cleanup_pending'] = True
        self.assertTrue(score_row(row, accepted, contract)[1])


def contract():
    return {"memory_mib": 32768, "solve_timeout": 5000,
            "checker_timeout": 28800}


def base_result(verdict="SAT"):
    result = {
        "instance": "case.cnf.xz", "expected": verdict,
        "verdict": verdict, "expected_match": True, "valid": True,
        "artifact_deleted": True, "derived_artifacts_deleted": True,
        "cleanup_pending": False,
        "solve": {"timed_out": False, "wall_seconds": 10,
                  "peak_rss_kb": 1000,
                  "exit_code": {"SAT": 10, "UNSAT": 20}[verdict]},
    }
    if verdict == "SAT":
        result["model_check"] = {"result": "verified", "exit_code": 0,
                                 "timed_out": False}
    else:
        result["proof_check"] = {
            "pipeline": "veripb-cake-pb", "result": "verified",
            "timed_out": False, "wall_seconds": 20,
            "stages": [{"name": "veripb", "result": "verified"},
                       {"name": "cake-pb-cnf", "result": "verified"}],
        }
    return result


class TierdPaperContractTests(unittest.TestCase):
    def test_independently_checked_sat_and_unsat_pass(self):
        for verdict in ("SAT", "UNSAT"):
            row = {"instance": "case.cnf.xz", "expected": verdict}
            status, seconds, errors = checked_result(
                row, base_result(verdict), contract())
            self.assertEqual((status, seconds, errors),
                             ("verified-" + verdict, 10, []))

    def test_clean_solver_timeout_is_unresolved_not_infrastructure_failure(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result.update(verdict="UNKNOWN", expected_match=False, valid=False,
                      proof_check=None)
        result["solve"].update(timed_out=True, wall_seconds=5000.005,
                               exit_code=None)
        self.assertEqual(checked_result(row, result, contract()),
                         ("solver-timeout", 5000.005, []))

    def test_clean_proof_timeout_is_censored_but_not_verified(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result["valid"] = False
        result["proof_check"] = {
            "result": "not-verified", "timed_out": True,
            "wall_seconds": 28800.001,
            "stages": [{"name": "pipeline-deadline", "result": "timeout",
                        "timed_out": True}],
        }
        status, _, errors = checked_result(row, result, contract())
        self.assertEqual(status, "censored-UNSAT-timeout")
        self.assertEqual(errors, [])

    def test_rejected_proof_and_invalid_timeout_evidence_fail(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result["valid"] = False
        result["proof_check"] = {
            "result": "not-verified", "timed_out": False,
            "wall_seconds": 3,
            "stages": [{"name": "veripb", "result": "rejected",
                        "exit_code": 1, "output_tail": "s INVALID"}],
        }
        self.assertIn("rejected", checked_result(row, result, contract())[2][0])
        result = base_result("UNSAT")
        result.update(verdict="UNKNOWN", valid=False, proof_check=None)
        result["solve"].update(timed_out=True, exit_code=20)
        self.assertIn("invalid solver-timeout evidence",
                      checked_result(row, result, contract())[2])

    def test_multiple_direct_rechecks_overlay_in_argument_order(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "base"
            base.joinpath("results").mkdir(parents=True)
            rows = [{"task": str(i), "rank": str(i + 1), "arm": "tactix",
                     "tier": "D", "instance": f"i{i}.cnf.xz", "expected": "UNSAT"}
                    for i in range(1166)]
            with base.joinpath("matrix.tsv").open("w", newline="") as out:
                writer = csv.DictWriter(out, fieldnames=rows[0], delimiter="\t")
                writer.writeheader(); writer.writerows(rows)
            arm = {"path": "pins/tactix", "sha256": "a" * 64,
                   "arguments": [], "certificate": "pbp"}
            base_manifest = {"pin_sha256": {}, "arms": {"tactix": arm}}
            base.joinpath("manifest.json").write_text(json.dumps(base_manifest))
            for task in (1, 2):
                base.joinpath("results", f"{task:03d}.json").write_text(json.dumps(
                    {"task": task, "rank": task + 1, "arm": "tactix",
                     "binary_sha256": "a" * 64, "instance": f"i{task}.cnf.xz",
                     "marker": "base"}))
            base_manifest_sha = hashlib.sha256(base.joinpath("manifest.json").read_bytes()).hexdigest()

            def make_recheck(name, source_tasks, marker):
                campaign = root / name
                campaign.joinpath("results").mkdir(parents=True)
                matrix = []
                selected = {}
                for local, source_task in enumerate(source_tasks):
                    matrix.append({"task": str(local), "source_task": str(source_task),
                                   "instance": f"i{source_task}.cnf.xz"})
                    source_path = base / "results" / f"{source_task:03d}.json"
                    selected[f"results/{source_task:03d}.json"] = hashlib.sha256(
                        source_path.read_bytes()).hexdigest()
                    campaign.joinpath("results", f"{local:03d}.json").write_text(json.dumps(
                        {"task": local, "arm": "tactix", "binary_sha256": "a" * 64,
                         "instance": f"i{source_task}.cnf.xz", "marker": marker}))
                with campaign.joinpath("matrix.tsv").open("w", newline="") as out:
                    writer = csv.DictWriter(out, fieldnames=("task", "source_task", "instance"),
                                            delimiter="\t")
                    writer.writeheader(); writer.writerows(matrix)
                campaign.joinpath("manifest.json").write_text(json.dumps({
                    "pin_sha256": {}, "arms": {"tactix": arm},
                    "resource_contract": {"memory_mib": 65536, "solve_timeout": 5000,
                                          "checker_timeout": 28800},
                    "recheck_source": {"manifest_sha256": base_manifest_sha,
                                       "selected_result_sha256": selected}}))
                return campaign

            first = make_recheck("first", (1,), "first")
            second = make_recheck("second", (1, 2), "second")
            _, _, results = substituted_results(base, [first, second])
            self.assertEqual(results[1]["marker"], "second")
            self.assertEqual(results[2]["marker"], "second")
            second.joinpath("results/001.json").unlink()
            with self.assertRaisesRegex(ValueError, "recheck result missing: second/001"):
                substituted_results(base, [first, second])


if __name__ == '__main__':
    unittest.main()

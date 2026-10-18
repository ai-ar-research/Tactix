"""A final CG audit must reject changed campaign files and comparison inputs."""
import copy
import csv
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import cms_tierd_cpuq as campaign_module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CampaignPinTests(unittest.TestCase):
    def test_summary_accepts_ordered_successor_and_rejects_changed_predecessor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, first, second = (root / name for name in
                                   ('base', 'first', 'second'))
            for campaign in (base, first, second):
                (campaign / 'results').mkdir(parents=True)
            rows = [{'rel': f'case-{index}.cnf.xz', 'expected': 'UNKNOWN'}
                    for index in range(3)]
            core_pins = {name: {'sha256': name + '-hash'} for name in
                         ('solver-binary', 'frat-xor', 'cake-xlrup', 'checkmodel')}
            base_policy = {
                'storage': 'anonymous-node-local', 'retention': 'none',
                'result_publication': 'pending-journal-then-atomic-promotion',
                'stage_cleanup': 'delete-input-after-consuming-checker-stage'}
            common = {
                'instances': rows, 'tasks': [0, 1, 2],
                'solvers': [{'name': 'cryptominisat-5.14.7'}],
                'checkers': {'same': True}, 'source_commits': {'same': True},
                'limits': {'solve_wall_seconds': 5000,
                           'checker_pipeline_wall_seconds': 45000},
                'provenance': core_pins,
            }
            base_manifest = copy.deepcopy(common)
            base_manifest['certificate_policy'] = base_policy
            (base / 'manifest.json').write_text(json.dumps(base_manifest))
            base_hash = digest(base / 'manifest.json')

            workspace_policy = dict(base_policy)
            workspace_policy.update({
                'storage': 'workspace-ephemeral',
                'terminal_cleanup':
                    'bounded-nfs-retry-in-finally-before-result-publication'})
            first_manifest = copy.deepcopy(common)
            first_manifest.update({
                'kind': 'cryptominisat-tierd-cpuq-workspace-repair-v1',
                'certificate_policy': workspace_policy,
                'slurm': {'array_throttle': 125},
                'repair_of': {'campaign': base.name,
                              'manifest_sha256': base_hash,
                              'missing_indices': [1, 2]}})
            (first / 'manifest.json').write_text(json.dumps(first_manifest))

            def unknown(path, task):
                path.write_text(json.dumps({
                    'task': task, 'instance': rows[task]['rel'],
                    'verdict': 'UNKNOWN', 'valid': False,
                    'cleanup_pending': False, 'scratch_deleted': True,
                    'artifact_deleted': True,
                    'derived_artifacts_deleted': True,
                    'retained_artifacts': []}))

            unknown(base / 'results/00000.json', 0)
            unknown(first / 'results/00001.json', 1)
            predecessor = {
                'campaign': first.name,
                'manifest_sha256': digest(first / 'manifest.json'),
                'published_indices': [1],
                'results_inventory_sha256': campaign_module.result_inventory(
                    (first / 'results').glob('*.json'))}
            second_manifest = copy.deepcopy(common)
            second_manifest.update({
                'kind': 'cryptominisat-tierd-cpuq-workspace-repair-v2',
                'certificate_policy': workspace_policy,
                'slurm': {'array_throttle': 125},
                'repair_of': {'campaign': base.name,
                              'manifest_sha256': base_hash,
                              'missing_indices': [2],
                              'prior_repairs': [predecessor]}})
            (second / 'manifest.json').write_text(json.dumps(second_manifest))
            unknown(second / 'results/00002.json', 2)

            saved = root / 'saved.csv'
            with saved.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=(
                    'instance', 'verified', 'expected', 'verdict', 'solve_wall_s'))
                writer.writeheader()
                for row in rows:
                    for _ in range(7):
                        writer.writerow({'instance': row['rel'], 'verified': 'False',
                                         'expected': 'UNKNOWN', 'verdict': 'UNKNOWN',
                                         'solve_wall_s': '1'})
            (root / 'benchmarks/tiers').mkdir(parents=True)
            with (patch.object(campaign_module, 'instances', return_value=rows),
                  patch.object(campaign_module, 'verify_campaign_pins'),
                  patch.object(campaign_module, 'SAVED', saved),
                  patch.object(campaign_module, 'ROOT', root)):
                self.assertEqual(campaign_module.summary(base, [first, second]), 0)
                analysis = json.loads((base / 'analysis.json').read_text())
                self.assertEqual([item['campaign'] for item in
                                  analysis['repair_manifests']],
                                 [first.name, second.name])
                unknown(first / 'results/00001.json', 2)
                with self.assertRaisesRegex(RuntimeError,
                                            'predecessor inventory changed'):
                    campaign_module.summary(base, [first, second])

    def test_saved_verdict_conflict_is_not_a_speed_win(self):
        result = {'expected': 'SAT', 'verdict': 'SAT'}
        arms = [{'verified': 'True', 'expected': 'SAT', 'verdict': 'SAT'},
                {'verified': 'False', 'expected': 'SAT', 'verdict': 'UNSAT'}]
        self.assertEqual(campaign_module.verified_saved_arms(result, arms), arms[:1])
        arms[0]['verdict'] = 'UNSAT'
        with self.assertRaisesRegex(RuntimeError, 'verdict disagrees'):
            campaign_module.verified_saved_arms(result, arms)

    def test_unverified_result_cannot_qualify_for_cg(self):
        manifest = {
            'solvers': [{'name': 'cryptominisat-5.14.7'}],
            'provenance': {
                'checkmodel': {'sha256': 'model-pin'},
                'frat-xor': {'sha256': 'frat-pin'},
                'cake-xlrup': {'sha256': 'cake-pin'},
            },
        }
        result = {
            'valid': True, 'expected_match': True,
            'solver': 'cryptominisat-5.14.7', 'verdict': 'UNSAT',
            'solve': {'timed_out': False, 'exit_code': 0},
            'certificate_bytes': 12, 'certificate_sha256': 'proof-digest',
            'proof_ok': True,
            'proof_check': {
                'result': 'verified', 'pipeline': 'frat-xor-cake-xlrup',
                'stages': [
                    {'name': name, 'result': 'verified', 'exit_code': 0,
                     'timed_out': False, 'input_artifact_deleted': True,
                     'anonymous_temp_deleted': True,
                     'provenance': {'sha256': digest}}
                    for name, digest in (('frat-xor', 'frat-pin'),
                                         ('cake-xlrup', 'cake-pin'))],
                'derived_artifacts': [{'deleted': True}],
            },
        }
        self.assertTrue(campaign_module.independently_checked(result, manifest))
        changed = copy.deepcopy(result)
        changed['proof_check']['stages'][1]['provenance']['sha256'] = 'wrong'
        with self.assertRaisesRegex(RuntimeError, 'independent proof check'):
            campaign_module.independently_checked(changed, manifest)
        changed = copy.deepcopy(result)
        changed['proof_check']['stages'][0]['anonymous_temp_deleted'] = False
        with self.assertRaisesRegex(RuntimeError, 'independent proof check'):
            campaign_module.independently_checked(changed, manifest)
        changed = copy.deepcopy(result)
        changed['valid'] = False
        self.assertFalse(campaign_module.independently_checked(changed, manifest))

        workspace_manifest = copy.deepcopy(manifest)
        workspace_manifest['certificate_policy'] = {'storage': 'workspace-ephemeral'}
        workspace_result = copy.deepcopy(result)
        workspace_result['proof_check']['stages'][0].pop('anonymous_temp_deleted')
        workspace_result['proof_check']['stages'][0]['temporary_deleted'] = True
        self.assertTrue(campaign_module.independently_checked(
            workspace_result, workspace_manifest))
        workspace_result['proof_check']['stages'][0]['temporary_deleted'] = False
        with self.assertRaisesRegex(RuntimeError, 'independent proof check'):
            campaign_module.independently_checked(workspace_result, workspace_manifest)

    def test_workspace_repair_accepts_legacy_and_governor_only_zero_retention(self):
        base = {'certificate_policy': {
            'storage': 'anonymous-node-local', 'retention': 'none',
            'result_publication': 'pending-journal-then-atomic-promotion',
            'stage_cleanup': 'delete-input-after-consuming-checker-stage'}}
        repair = {'kind': 'cryptominisat-tierd-cpuq-workspace-repair-v1',
                  'certificate_policy': {
                      'storage': 'workspace-ephemeral', 'retention': 'none',
                      'result_publication': 'pending-journal-then-atomic-promotion',
                      'stage_cleanup': 'delete-input-after-consuming-checker-stage',
                      'terminal_cleanup':
                      'bounded-nfs-retry-in-finally-before-result-publication'},
                  'limits': {'workspace_quota_stop_gib': 750,
                             'workspace_quota_poll_seconds': 2},
                  'slurm': {'array_throttle': 125},
                  'provenance': {'workspace-quota': {'sha256': 'pin'}}}
        self.assertTrue(campaign_module.compatible_repair_policy(base, repair))
        governor_only = copy.deepcopy(repair)
        governor_only['kind'] = 'cryptominisat-tierd-cpuq-workspace-repair-v2'
        governor_only['limits'].pop('workspace_quota_stop_gib')
        governor_only['limits'].pop('workspace_quota_poll_seconds')
        governor_only['provenance'].pop('workspace-quota')
        self.assertTrue(campaign_module.compatible_repair_policy(
            base, governor_only))
        changed = copy.deepcopy(repair)
        changed['limits']['workspace_quota_stop_gib'] = 800
        self.assertFalse(campaign_module.compatible_repair_policy(base, changed))
        changed = copy.deepcopy(governor_only)
        changed['limits']['workspace_quota_poll_seconds'] = 2
        self.assertFalse(campaign_module.compatible_repair_policy(base, changed))
        changed = copy.deepcopy(repair)
        changed['certificate_policy']['retention'] = 'proof'
        self.assertFalse(campaign_module.compatible_repair_policy(base, changed))

    def test_validates_solver_checker_and_frozen_comparison(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign = root / 'campaign'
            pins = campaign / 'pins'
            pins.mkdir(parents=True)
            for name in ('solver', 'cms-wrapper', 'frat_xor', 'cake_xlrup',
                         'checkmodel', 'modelcheck.py'):
                (pins / name).write_text(name)
            frozen = root / 'instances.list'
            cache = root / 'cache.sha256'
            saved = root / 'results.csv'
            for path in (frozen, cache, saved):
                path.write_text(path.name)
            provenance = {
                name: {'path': f'pins/{filename}', 'sha256': digest(pins / filename)}
                for name, filename in (
                    ('solver-binary', 'solver'),
                    ('cryptominisat-5.14.7', 'cms-wrapper'),
                    ('frat-xor', 'frat_xor'),
                    ('cake-xlrup', 'cake_xlrup'),
                    ('checkmodel', 'checkmodel'),
                    ('modelcheck.py', 'modelcheck.py'))}
            manifest = {
                'campaign': campaign.name,
                'instances_list_sha256': digest(frozen),
                'cache_manifest_sha256': digest(cache),
                'saved_comparison_sha256': digest(saved),
                'certificate_policy': {'retention': 'none'},
                'slurm': {'partition': 'cpu-q'},
                'provenance': provenance,
                'solvers': [{'executable': 'pins/cms-wrapper'}],
                'checkers': {'frat-xor': 'pins/frat_xor',
                             'cake-xlrup': 'pins/cake_xlrup',
                             'checkmodel': 'pins/checkmodel',
                             'modelcheck.py': 'pins/modelcheck.py'},
            }
            with (patch.object(campaign_module, 'LIST', frozen),
                  patch.object(campaign_module, 'CACHE_MANIFEST', cache),
                  patch.object(campaign_module, 'SAVED', saved)):
                campaign_module.verify_campaign_pins(campaign, manifest)
                changed = copy.deepcopy(manifest)
                changed['provenance']['solver-binary']['sha256'] = '0' * 64
                with self.assertRaisesRegex(RuntimeError, 'provenance pin changed'):
                    campaign_module.verify_campaign_pins(campaign, changed)
                changed = copy.deepcopy(manifest)
                changed['saved_comparison_sha256'] = '0' * 64
                with self.assertRaisesRegex(RuntimeError, 'frozen input'):
                    campaign_module.verify_campaign_pins(campaign, changed)
                changed = copy.deepcopy(manifest)
                changed['checkers']['cake-xlrup'] = 'pins/checkmodel'
                with self.assertRaisesRegex(RuntimeError, 'checker path changed'):
                    campaign_module.verify_campaign_pins(campaign, changed)
                changed = copy.deepcopy(manifest)
                changed['checkers']['modelcheck.py'] = 'pins/checkmodel'
                with self.assertRaisesRegex(RuntimeError, 'checker path changed'):
                    campaign_module.verify_campaign_pins(campaign, changed)
                changed = copy.deepcopy(manifest)
                changed['provenance']['checkmodel']['path'] = '../outside'
                with self.assertRaisesRegex(RuntimeError, 'provenance pin changed'):
                    campaign_module.verify_campaign_pins(campaign, changed)


if __name__ == '__main__':
    unittest.main()

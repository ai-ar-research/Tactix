"""Only independently checked CMS rows and verified saved arms qualify for CG."""
import copy
import unittest

import cms_tierd_cpuq as campaign_module


class CheckedRowTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()

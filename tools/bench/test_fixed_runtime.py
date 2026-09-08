#!/usr/bin/env python3
"""Non-solving protocol tests for fixed-reference full-view measurements."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import fixed_runtime as f


class FixedRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.ref = dict(solver_seconds=40.0, resource_contract=dict(solve_timeout=1200),
            records=[dict(rank=i,instance=f'{i}.cnf',input_sha256='input',expected='SAT',
                measurement=dict(solve=dict(wall_seconds=1.0,peak_rss_kb=10))) for i in range(1,41)])
        self.records = [dict(rank=i,task=i-1,instance=f'{i}.cnf',expected='SAT',arm='tactix',
            expected_match=True,solve=dict(wall_seconds=1.0,timed_out=False,exit_code=10,peak_rss_kb=10)) for i in range(1,41)]

    def test_candidate_only_contiguous_matrix(self):
        rows = f.candidate_rows(self.ref)
        self.assertEqual([r['task'] for r in rows], list(range(40)))
        self.assertEqual({r['arm'] for r in rows}, {'tactix'})
        self.assertEqual([r['rank'] for r in rows], list(range(1,41)))

    def test_complete_pending_score_not_verified(self):
        r = f.score_records(self.ref,self.records)
        self.assertEqual(r['ratio'],1.0)
        self.assertTrue(r['runtime_target_met'])
        self.assertEqual(r['checked_clean'],0)

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


if __name__ == '__main__':
    unittest.main()

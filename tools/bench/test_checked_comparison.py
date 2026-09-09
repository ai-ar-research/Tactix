"""Classification and complete-view controls; no solver execution."""
import copy
import unittest
from checked_comparison import classify, summarize

def record(verdict='SAT'):
    r=dict(task=0,rank=1,arm='tactix',instance='a',expected=verdict,verdict=verdict,expected_match=True,
        valid=True,artifact_deleted=True,derived_artifacts_deleted=True,cleanup_pending=False,
        solve=dict(wall_seconds=1.0,exit_code=10 if verdict=='SAT' else 20,timed_out=False,peak_rss_kb=10))
    r['model_check' if verdict=='SAT' else 'proof_check']=dict(exit_code=0,timed_out=False,result='verified',output_tail='s VERIFIED')
    return r

class CheckedComparisonTests(unittest.TestCase):
    def test_empty_matrix_is_not_vacuous_success(self):
        with self.assertRaisesRegex(ValueError,'empty'):
            summarize([],{},set(),{},1200)

    def test_checked_oracles_and_pending_are_distinct(self):
        for verdict in ('SAT','UNSAT'):
            r=record(verdict)
            self.assertTrue(classify(r,True)['checked'])
            self.assertEqual(classify(r)['status'],'pending_validation')
            self.assertFalse(classify(r)['scorable'])

    def test_cleanup_is_independent(self):
        for key,value in (('artifact_deleted',False),('derived_artifacts_deleted',False),('cleanup_pending',True)):
            r=record();r[key]=value
            self.assertEqual(classify(r,True)['status'],'residue')
            self.assertFalse(classify(r,True)['scorable'])

    def test_rejected_timeout_and_abnormal_checker(self):
        for field,value,want in (('result','not-verified','proof_rejected_or_status_mismatch'),
                                 ('timed_out',True,'checker_timeout'),
                                 ('exit_code',137,'checker_abnormal_exit'),
                                 ('output_tail','s NOT VERIFIED','proof_rejected_or_status_mismatch')):
            r=record('UNSAT');r['proof_check'][field]=value
            self.assertEqual(classify(r,True)['status'],want)
            self.assertFalse(classify(r,True)['scorable'])

    def test_clean_timeout_retained_in_par2(self):
        r=record();r.update(verdict='UNKNOWN',expected_match=False,valid=False,model_check=None)
        r['solve'].update(timed_out=True,exit_code=124,wall_seconds=1200)
        self.assertEqual(classify(r,True)['status'],'solver_timeout')
        self.assertTrue(classify(r,True)['scorable'])
        r['verdict']='SAT'
        self.assertEqual(classify(r,True)['status'],'solver_timeout_with_verdict')
        self.assertFalse(classify(r,True)['scorable'])

    def test_oom_requires_phase_evidence(self):
        self.assertEqual(classify(None,state='OUT_OF_MEMORY')['status'],'resource_failure_phase_unknown')
        r=record()
        self.assertEqual(classify(r,state='OUT_OF_MEMORY')['status'],'checker_resource_failure')
        r['solve']['exit_code']=137
        self.assertEqual(classify(r,state='OUT_OF_MEMORY')['status'],'solver_resource_failure')
        self.assertEqual(classify(r,True)['status'],'solver_abnormal_exit')

    def test_terminal_missing_records_never_count_as_success(self):
        self.assertEqual(classify(None,state='COMPLETED')['status'],'missing_solver_record_terminal')
        self.assertEqual(classify(record(),state='COMPLETED')['status'],'missing_final_terminal')
        self.assertEqual(classify({'error':'decompress failed'},True)['status'],'harness_failure')

    def test_mismatch_and_missing_checker(self):
        r=record();r['verdict']='UNSAT'
        self.assertEqual(classify(r,True)['status'],'verdict_mismatch')
        r=record();r.pop('model_check')
        self.assertEqual(classify(r,True)['status'],'missing_checker_result')
        r=record();r['valid']=False
        self.assertEqual(classify(r,True)['status'],'inconsistent_final_validation')

    def test_all_fifty_retained_aggregate_requires_every_task(self):
        matrix=[];records={}
        for rank in range(1,51):
            for arm in ('tactix','kissat'):
                task=len(matrix);r=record();r.update(rank=rank,task=task,arm=arm,instance=str(rank))
                records[task]=r
                matrix.append({k:r[k] for k in ('rank','task','arm','instance','expected')})
        summary=summarize(matrix,records,set(records),{},1200)
        self.assertEqual(summary['ratio'],1.0)
        self.assertEqual(len(summary['pairs']),50)
        pending=summarize(matrix,records,set(records)-{99},{99:'RUNNING'},1200)
        self.assertIsNone(pending['ratio']);self.assertEqual(len(pending['pairs']),50)
        records[98].update(verdict='UNKNOWN',expected_match=False,valid=False,model_check=None)
        records[98]['solve'].update(timed_out=True,exit_code=124,wall_seconds=1200)
        scored=summarize(matrix,records,set(records),{},1200)
        self.assertEqual(scored['totals_par2']['tactix'],2449)
        self.assertTrue(scored['pairs'][-1]['solve_loss'])
        bad=copy.deepcopy(records);bad[0]['instance']='wrong'
        with self.assertRaisesRegex(ValueError,'identity'):summarize(matrix,bad,set(bad),{},1200)

if __name__=='__main__':unittest.main()

"""Negative controls for the stateful API trace oracle and reducer."""
import importlib.util
import copy
import hashlib
import json
import struct
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import errno

root=Path(sys.argv.pop(1))
spec=importlib.util.spec_from_file_location('trace_fuzz',root/'tools/fuzz/ipasir_trace.py')
trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace)


class Oracles(unittest.TestCase):
    def test_native_minimization_and_after_release_reach(self):
        q=dict(formula=[[1]],assumptions=[7,-1],min_budget=-1,min_context=0)
        p1=dict(result=20,bundle_index=2,owned_result=1,assumptions=[-1],values=[],failed=[-1])
        p2=dict(result=10,bundle_index=3,owned_result=1,assumptions=[],values=[1],failed=[])
        m=dict(minimal=True,source=1,core=[-1],context=[],certificate=2,witnesses=[[-1,1]],
               probes=[p1,p2],strategy_calls=0,restorations=0,termination_limit=-1,termination_calls=0,learned=[])
        answer=dict(result=20,bundle_index=1,owned_result=1,termination_calls=0,minimization=m)
        self.assertEqual(trace.validate_minimization(q,answer),[p1,p2])
        self.assertEqual(trace.validate_owned_delivery([answer],'c owned-results-checked 3\n'),3)
        with self.assertRaises(trace.Failure):
            trace.validate_owned_delivery([answer],'c owned-results-checked 1\n')
        for update in (dict(core=[]),dict(certificate=1),dict(witnesses=[]),dict(minimal=False),
                       dict(strategy_calls=1),dict(source=2),dict(context=[7])):
            with self.assertRaises(trace.Failure):trace.validate_minimization(q,dict(answer,minimization=dict(m,**update)))
        bad=copy.deepcopy(answer);bad['minimization']['probes'][0]['result']=10
        with self.assertRaises(trace.Failure):trace.validate_minimization(q,bad)
        with self.assertRaises(trace.Failure):trace.validate_minimization(dict(q,min_budget=1),answer)
        # A retained literal's earlier SAT witness remains valid after a later
        # successful deletion weakens the tested cube.
        q=dict(formula=[[1,2]],assumptions=[-1,-2,7],min_budget=-1)
        probes=[dict(result=10,bundle_index=2,assumptions=[-2,7],values=[1,-2,7]),
                dict(result=10,bundle_index=3,assumptions=[-1,7],values=[-1,2,7]),
                dict(result=20,bundle_index=4,assumptions=[-1,-2],failed=[-1,-2])]
        m=dict(m,core=[-1,-2],probes=probes,witnesses=[[-1,0],[-2,1]],certificate=4)
        trace.validate_minimization(q,dict(answer,minimization=m))

    def test_native_minimization_context_and_unknown(self):
        q=dict(formula=[[1]],assumptions=[7,-1],min_budget=-1,min_context=2)
        m=dict(minimal=True,source=1,core=[],context=[-1],certificate=2,witnesses=[],
               probes=[dict(result=20,bundle_index=2,assumptions=[-1],failed=[-1])],
               strategy_calls=0,restorations=0,termination_limit=-1,termination_calls=0,learned=[])
        a=dict(result=20,bundle_index=1,termination_calls=0,minimization=m)
        trace.validate_minimization(q,a)
        # Empty minimized core refutes the fixed context; F alone is SAT.
        self.assertTrue(trace.satisfiable(q['formula']))
        q=dict(q,min_context=0,min_stop=1)
        m=dict(m,minimal=False,core=[7,-1],context=[],certificate=1,termination_limit=1,termination_calls=1,
               probes=[dict(result=0,bundle_index=2,assumptions=[-1],values=[],failed=[])])
        trace.validate_minimization(q,dict(a,minimization=m))
        for update in (dict(core=[-1]),dict(certificate=2),dict(termination_calls=0),dict(minimal=True)):
            with self.assertRaises(trace.Failure):trace.validate_minimization(q,dict(a,minimization=dict(m,**update)))
        with self.assertRaises(trace.Failure):trace.validate_minimization(dict(q,min_stop=-1),dict(a,minimization=m))
        for lines in (['init 0','min_budget 0 -3','solve 0'],['init 0','min_context 0 -1','solve 0'],
                      ['init 0','min_stop 0 -3','solve 0']):
            with self.assertRaises(ValueError):trace.queries(lines)

    def test_owned_results_require_after_release_reach(self):
        answers=[{'result':10,'owned_result':1},{'result':20,'owned_result':1},
                 {'result':0,'owned_result':0}]
        self.assertEqual(trace.validate_owned_delivery(answers,'c owned-results-checked 2\n'),2)
        for output in ('','c owned-results-checked 1\n',
                       'c owned-results-checked 2\nc owned-results-checked 2\n'):
            with self.assertRaises(trace.Failure):trace.validate_owned_delivery(answers,output)
        for bad in ({'result':20},{'result':20,'owned_result':0},{'result':0,'owned_result':1}):
            with self.assertRaises(trace.Failure):trace.validate_owned_delivery([bad],'c owned-results-checked 1\n')

    def test_learning_excludes_assumptions_and_private_names(self):
        query={'formula':[[1,2]],'assumptions':[-1],'learning_limit':1}
        answer={'result':10,'values':[-1,2],'learning_limit':1,'learned':[]}
        trace.validate_answer(query,answer)
        # The query forces 2; permanent F does not. An empty clause or helper
        # name is invalid too, even when supplied alongside a correct model.
        for clause in ([2],[],[77],[0]):
            with self.assertRaises(trace.Failure) as e:
                trace.validate_answer(query,dict(answer,learned=[clause]))
            self.assertEqual(e.exception.kind,'learning')
        query={'formula':[[1]],'assumptions':[],'learning_limit':1}
        trace.validate_answer(query,dict(answer,values=[1],learned=[[1]]))
        for query_update in ({'learning_budget':0},{'learning_limit':-1}):
            q=dict(query,**query_update)
            with self.assertRaises(trace.Failure):
                trace.validate_answer(q,dict(answer,values=[1],learned=[[1]],**query_update))

    def test_strategy_configuration_and_required_driver(self):
        q={'formula':[[1]],'assumptions':[],'strategy':2}
        good={'result':10,'values':[1],'strategy':2,'strategy_calls':1}
        trace.validate_answer(q,good)
        for update in ({'strategy':0},{'strategy_calls':0}):
            with self.assertRaises(trace.Failure):trace.validate_answer(q,dict(good,**update))
        queries,_=trace.queries(['init 0','strategy 0 2','learn 0 1','learn_budget 0 0',
                                'core_budget 0 4096','solve 0'])
        self.assertEqual((queries[0]['strategy'],queries[0]['learning_limit'],queries[0]['core_budget']),(2,1,4096))
        for lines in (['init 0','strategy 0 10','solve 0'],['init 0','learn_budget 0 -2','solve 0']):
            with self.assertRaises(ValueError):trace.queries(lines)

    def test_native_program_identity_and_reach(self):
        q={'formula':[[1]],'assumptions':[],'strategy':4}
        good={'result':10,'values':[1],'strategy':4,'strategy_calls':1,
              'driver_name':'cruxmiter','native_root_requests':3,'native_root_eligible':2,
              'native_root_effective':1,'native_root_rounds':1,'native_handoffs':1}
        trace.validate_answer(dict(q,strategy=9),dict(good,strategy=9,driver_name='chvatal'))
        trace.validate_answer(q,good)
        for update in ({'driver_name':'default'},{'native_root_eligible':4},
                       {'native_root_effective':3},{'native_handoffs':2},
                       {'native_root_requests':None}):
            with self.assertRaises(trace.Failure):trace.validate_answer(q,dict(good,**update))
            with self.assertRaises(trace.Failure):
                trace.validate_answer(dict(q,strategy=9),dict(dict(good,strategy=9,driver_name='chvatal'),**update))
        for strategy,name in ((6,'dpll'),(7,'cdcl-no-restarts'),(8,'scripted-default')):
            q=dict(q,strategy=strategy)
            good=dict(good,strategy=strategy,driver_name=name,native_root_requests=0,
                      native_root_eligible=0,native_root_effective=0,native_root_rounds=0,native_handoffs=0)
            trace.validate_answer(q,good)
            for update in ({'driver_name':'default'},{'native_handoffs':1},{'native_root_requests':1}):
                with self.assertRaises(trace.Failure):trace.validate_answer(q,dict(good,**update))

    def test_public_random_rejects_missing_and_fabricated_accounting(self):
        q={'formula':[[1]],'assumptions':[],'strategy':5}
        policy=dict(root=1,search=0,root_to_search=0,partial_restarts=0,full_restarts=0,
                    search_to_root=0,algebraic=0,forms=[0,0,1,0,0],effective_forms=[0,0,1,0,0],
                    rules={'BVE':[1,1]})
        good=dict(result=10,values=[1],strategy=5,strategy_calls=1,random_policy=policy)
        trace.validate_answer(q,good)
        structured = dict(policy,forms=[0,0,0,0,1],effective_forms=[0,0,0,0,1],
                          rules={'ExtendedResolution':[1,1]})
        trace.validate_answer(q,dict(good,random_policy=structured))
        for update in (None,{},dict(policy,forms=[0,0,0,0]),
                       dict(policy,effective_forms=[0,0,2,0,0]),dict(policy,root=-1),
                       dict(policy,rules={'BVE':[0,1]}),
                       dict(structured,forms=[0,0,0,0,0]),
                       dict(structured,effective_forms=[0,0,0,0,0]),
                       dict(structured,forms=[0,0,0,1])):
            with self.assertRaises(trace.Failure):
                trace.validate_answer(q,dict(good,random_policy=update))
        for strategy in (0,5,6,7,8,9):
            qs,_=trace.queries(trace.generate(314159,40,strategy=strategy))
            self.assertTrue(all(q['strategy']==strategy for q in qs))

    def test_reduced_core_requires_exact_bundle_members(self):
        with tempfile.TemporaryDirectory() as tmp:
            stem=Path(tmp)/'q'
            def member(suffix,data):Path(str(stem)+suffix).write_text(data)
            def manifest(reduced):
                suffixes=['.cnf','.proof','.core']+(['-core.cnf','-core.proof'] if reduced else [])
                hashes={s:hashlib.sha256(Path(str(stem)+s).read_bytes()).hexdigest() for s in suffixes}
                digest=hashlib.sha256(b'tactix-permanent-input-i32le-v1\n'+struct.pack('<iiii',1,0,-1,0)).hexdigest()
                member('.json',json.dumps({'query':1,'result':20,'core':'licensed-subset' if reduced else 'full-original-cube',
                       'permanent_encoding':'tactix-permanent-input-i32le-v1','permanent_sha256':digest,'sha256':hashes}))
            q={'query':1,'formula':[[1],[-1]],'assumptions':[7]}
            answer={'result':20,'failed':[]}
            member('.cnf','p cnf 7 3\n1 0\n-1 0\n7 0\n');member('.proof','');member('.core','0\n');manifest(False)
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'bundle')
            member('-core.cnf','p cnf 7 2\n1 0\n-1 0\n');member('-core.proof','');manifest(True)
            self.assertTrue(trace.validate_bundle(q,answer,stem))
            meta=json.loads(Path(str(stem)+'.json').read_text())
            member('.json',json.dumps(dict(meta,permanent_sha256='0'*64)))
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'snapshot');manifest(True)
            member('-core.cnf','p cnf 7 3\n1 0\n-1 0\n7 0\n');manifest(True)
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'snapshot')

    def test_cleanup_failure_is_incomplete_evidence(self):
        with patch.object(trace,'remove_tree',return_value=2):
            self.assertEqual(trace.cleanup_case(Path('case')),2)
        with patch.object(trace,'remove_tree',side_effect=OSError(errno.ENOTEMPTY,'persistent cleanup failure')):
            with self.assertRaises(trace.Failure) as e:trace.cleanup_case(Path('case'))
            self.assertEqual(e.exception.kind,'cleanup')

    def test_wrong_verdict(self):
        q={'formula':[[1]],'assumptions':[]}
        with self.assertRaises(trace.Failure):
            trace.validate_answer(q,{'result':20,'failed':[]})

    def test_partial_model_every_completion(self):
        q={'formula':[[1,2]],'assumptions':[]}
        trace.validate_answer(q,{'result':10,'values':[1]})
        for values in ([],[-1,-2],[1,-1],[0]):
            with self.assertRaises(trace.Failure):
                trace.validate_answer(q,{'result':10,'values':values})

    def test_failed_scope(self):
        q={'formula':[[1,2]],'assumptions':[7,-1,-2]}
        trace.validate_answer(q,{'result':20,'failed':[-1,-2]})
        for core in ([],[-1],[-2],[1,-1]):
            with self.assertRaises(trace.Failure):
                trace.validate_answer(q,{'result':20,'failed':core})

    def test_termination_is_not_a_verdict_or_a_persistent_assumption(self):
        q={'formula':[[1]],'assumptions':[-1],'termination_limit':1}
        trace.validate_answer(q,{'result':0,'termination_limit':1,'termination_calls':1,'values':[],'failed':[]})
        for answer in ({'result':0,'termination_limit':1,'termination_calls':0},
                       {'result':20,'termination_limit':1,'termination_calls':1,'failed':[-1]},
                       {'result':0,'termination_limit':1,'termination_calls':1,'values':[1]}):
            with self.assertRaises(trace.Failure):trace.validate_answer(q,answer)
        qs,_=trace.queries(['init 0','add 0 1','add 0 0','assume 0 -1',
                            'terminate 0 1','solve 0','terminate 0 -1','solve 0'])
        self.assertEqual(qs[1]['assumptions'],[])
        self.assertEqual(qs[1]['termination_limit'],-1)

    def test_assumption_consumption_and_prefix(self):
        qs,_=trace.queries(['init 0','add 0 1','add 0 0','assume 0 -1','solve 0','solve 0'])
        self.assertEqual(qs[0]['assumptions'],[-1])
        self.assertEqual(qs[1]['assumptions'],[])
        self.assertEqual(qs[1]['formula'],[[1]])
        for lines in (['init 0','add 0 1','solve 0'],['init 0','release 0','solve 0']):
            with self.assertRaises(ValueError):trace.queries(lines)

    def test_seed_replay_and_reduction(self):
        self.assertEqual(trace.generate(42),trace.generate(42))
        for seed in range(50):trace.queries(trace.generate(seed))
        lines=['init 0','add 0 1','add 0 0','assume 0 -1','solve 0','release 0']
        reduced=trace.reduce_trace(lines,lambda ls:any(q['assumptions']==[-1] for q in trace.queries(ls)[0]))
        trace.queries(reduced)
        self.assertIn('assume 0 -1',reduced)
        self.assertLess(len(reduced),len(lines))

    def test_bundle_hashes_snapshot_and_model(self):
        with tempfile.TemporaryDirectory() as tmp:
            stem=Path(tmp)/'q'
            def member(suffix,data):Path(str(stem)+suffix).write_text(data)
            def manifest():
                hashes={s:hashlib.sha256(Path(str(stem)+s).read_bytes()).hexdigest()
                        for s in ('.cnf','.proof','.model')}
                digest=hashlib.sha256(b'tactix-permanent-input-i32le-v1\n'+struct.pack('<ii',1,0)).hexdigest()
                member('.json',json.dumps({'query':1,'result':10,'sha256':hashes,
                       'permanent_encoding':'tactix-permanent-input-i32le-v1','permanent_sha256':digest}))
            q={'query':1,'formula':[[1]],'assumptions':[]}
            answer={'result':10,'values':[1]}
            member('.cnf','p cnf 1 1\n1 0\n');member('.proof','');member('.model','v 1 0\n');manifest()
            trace.validate_bundle(q,answer,stem)
            member('.model','v -1 0\n')
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'bundle')
            manifest()
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'model')
            member('.model','v 1 0\n');member('.cnf','p cnf 1 1\n-1 0\n');manifest()
            with self.assertRaises(trace.Failure) as e:trace.validate_bundle(q,answer,stem)
            self.assertEqual(e.exception.kind,'snapshot')


if __name__=='__main__':unittest.main()

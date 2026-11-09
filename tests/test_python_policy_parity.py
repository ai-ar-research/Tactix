"""Compare independently written Python/C policies, models and proof bytes."""
from pathlib import Path
import argparse
import ctypes as C
import importlib.util
import hashlib
import itertools
import json
import random
import statistics
import subprocess
import sys
import tempfile
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('library',type=Path);parser.add_argument('root',type=Path)
parser.add_argument('compiler');parser.add_argument('--out',type=Path)
parser.add_argument('--policy',choices=('cruxmiter','scripted-default'),default='cruxmiter')
args=parser.parse_args();root=args.root
sys.path.insert(0,str(root/'python'));sys.path.insert(0,str(root/'tools/bench'))
from tactix import capi
from python_policy_reference import Cruxmiter, ScriptedDefault
spec=importlib.util.spec_from_file_location('parity_proof_runner',root/'tools/bench/run.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
from proof_oracle import certificate_route
lib=capi.load_library(args.library)
rules={lib.tactix_rule_name(i).decode():i for i in range(lib.tactix_rule_count())}
cases=[[],[[1]],[[1],[-1]],[[]],[[1,2],[-1,2],[-2,3]],
       [[1,2],[1,-2],[-1,2],[-1,-2]],
       [[-3,1],[-3,2],[3,-1,-2],[-4,1],[-4,2],[4,-1,-2]],
       [list(lits) for lits in itertools.product((-1,1),(-2,2),(-3,3))],
       [[1,-1],[2,3],[-2,3]]]
for seed in range(16):
    rng=random.Random(731000+seed);formula=[]
    for i in range(24+seed):
        variables=rng.sample(range(1,7),rng.choice((2,3)))
        formula.append([v if rng.randrange(2) else -v for v in variables])
    cases.append(formula)

if args.policy=='scripted-default':
    # demo3's retained schedule witness: restarts, reduction, mode switch and
    # rephase must actually fire, rather than comparing two trivial searches.
    state=75;mask=(1<<64)-1
    def rnd():
        global state
        state^=state>>12;state^=(state<<25)&mask;state^=state>>27
        return (state*0x2545F4914F6CDD1D)&mask
    formula=[]
    for _ in range(596):
        clause=[]
        while len(clause)<3:
            variable=rnd()%140+1
            if any(abs(lit)==variable for lit in clause):continue
            clause.append(variable if rnd()&1 else -variable)
        formula.append(clause)
    cases.append(formula)

WRAPPER=r'''
#include "tactix.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
static struct action {int rule,result,form;uint64_t instance;} *actions;
static size_t action_count,action_capacity;
static int record(int rule,int result,int form,uint64_t instance) {
  if(action_count==action_capacity) {
    action_capacity=action_capacity?2*action_capacity:1024;
    actions=realloc(actions,action_capacity*sizeof *actions);assert(actions);
  }
  actions[action_count++]=(struct action){rule,result,form,instance};return result;
}
static int traced_apply(tactix *s,int rule) {
  return record(rule,tactix_apply(s,rule),0,0);
}
static int traced_instance(tactix *s,int rule,uint64_t instance) {
  return record(rule,tactix_apply_instance(s,rule,instance),1,instance);
}
#define tactix_apply traced_apply
#define tactix_apply_instance traced_instance
#if SCRIPTED_DEFAULT
#define main unused_demo3_main
#include "examples/demo3_scripted_default.c"
#undef main
#else
#include "strategy/presets/cruxmiter.c"
#endif
#undef tactix_apply
#undef tactix_apply_instance
void *reference_new(void) {
#if SCRIPTED_DEFAULT
  return malloc(1);
#else
  cruxmiter_program *p=malloc(sizeof *p);
  if(p&&!cruxmiter_begin(p)) {free(p);return NULL;}return p;
#endif
}
void reference_delete(void *p) {free(p);free(actions);actions=NULL;action_capacity=0;}
int reference_configure(tactix *s) {
#if SCRIPTED_DEFAULT
  (void)s;return 1;
#else
  return cruxmiter_configure(s);
#endif
}
int reference_run(void *p,tactix *s,const char *path) {
  (void)path;action_count=0;
#if SCRIPTED_DEFAULT
  (void)p;return scripted_solve(s);
#else
  return cruxmiter_run(p,s,NULL,NULL,NULL);
#endif
}
void reference_flush_trace(const char *path) {
  FILE *trace=fopen(path,"w");assert(trace);
  for(size_t i=0;i<action_count;++i)fprintf(trace,"%d %d %d %" PRIu64 "\n",
      actions[i].rule,actions[i].result,actions[i].form,actions[i].instance);
  assert(!fclose(trace));
}
void reference_counts(void *p,uint64_t *out) {
#if SCRIPTED_DEFAULT
  (void)p;for(int i=0;i<4;++i)out[i]=0;
#else
  cruxmiter_program *q=p;out[0]=q->requests;out[1]=q->eligible;
  out[2]=q->effective;out[3]=q->rounds;
#endif
}
'''

with tempfile.TemporaryDirectory(prefix='python-crux-parity-') as tmp:
    p=args.out or Path(tmp);p.mkdir(parents=True,exist_ok=True)
    (p/'reference.c').write_text(WRAPPER)
    subprocess.run([args.compiler,'-shared','-fPIC','-O3','-I'+str(root/'include'),
                    '-I'+str(root/'src'),'-I'+str(root),
                    '-DSCRIPTED_DEFAULT='+str(int(args.policy=='scripted-default')),
                    str(p/'reference.c'),str(args.library),'-lm',
                    '-Wl,-rpath,'+str(args.library.parent),'-o',str(p/'reference.so')],check=True)
    ref=C.CDLL(str(p/'reference.so'))
    for name,result,params in [('reference_new',C.c_void_p,[]),('reference_delete',None,[C.c_void_p]),
        ('reference_configure',C.c_int,[C.c_void_p]),('reference_flush_trace',None,[C.c_char_p]),
        ('reference_run',C.c_int,[C.c_void_p,C.c_void_p,C.c_char_p]),
        ('reference_counts',None,[C.c_void_p,C.POINTER(C.c_uint64)])]:
        function=getattr(ref,name);function.restype=result;function.argtypes=params
    rows=[];proofs=models=0;canonical={};reached_merges=0
    for i,formula in enumerate(cases):
        n=max((abs(lit) for clause in formula for lit in clause),default=0)
        satisfiable=(any(all(any(bits[abs(l)-1]==(l>0) for l in c) for c in formula)
                         for bits in itertools.product((False,True),repeat=n)) if n<=6 else None)
        cnf=p/f'{i}.cnf';cnf.write_text(f'p cnf {n} {len(formula)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in formula))
        for mi,(mode,fmt) in enumerate((('none',None),('dsr',capi.TACTIX_PROOF_DSR_ASCII),
                                       ('pbp',capi.TACTIX_PROOF_PBP),('auto',capi.TACTIX_PROOF_AUTO))):
            answers={}
            for arm in (('c','python') if (i+mi)%2 else ('python','c')):
                s=lib.tactix_init();assert s
                program=ref.reference_new() if arm=='c' else (Cruxmiter() if args.policy=='cruxmiter' else ScriptedDefault())
                stem=p/f'{i}-{mode}-{arm}'
                if arm=='c':assert ref.reference_configure(s)==1
                else:
                    for name,value in program.settings:assert lib.tactix_set_default_option(s,name.encode(),value.encode())==0
                if fmt is not None:assert lib.tactix_proof(s,str(stem.with_suffix('.proof')).encode(),fmt)==0
                for clause in formula:
                    for lit in clause:lib.tactix_add(s,lit)
                    lib.tactix_add(s,0)
                assert lib.tactix_set_guidance_publication(s,capi.TACTIX_GUIDANCE_NATIVE)==0
                start=time.perf_counter_ns()
                result=ref.reference_run(program,s,str(stem.with_suffix('.actions')).encode()) if arm=='c' else program.run(capi.SolverView(lib,s,lambda:None),rules)
                if result==30:result=lib.tactix_solve(s)
                elapsed=time.perf_counter_ns()-start
                assert result in (10,20) and (satisfiable is None or result==(10 if satisfiable else 20)),(i,mode,arm,result)
                counters=tuple(lib.tactix_stat(s,j) for j in range(1 + max(
                    value for name,value in capi.schema.CONSTANTS.items()
                    if name.startswith('TACTIX_STAT_'))))
                model=tuple(v if lib.tactix_value(s,v)>0 else -v for v in range(1,n+1)) if result==10 else ()
                if result==10:
                    assert all(any(l in model for l in c) for c in formula);models+=1
                if arm=='c':
                    ref.reference_flush_trace(str(stem.with_suffix('.actions')).encode())
                    trace=[tuple(map(int,line.split())) for line in stem.with_suffix('.actions').read_text().splitlines()]
                    counts=(C.c_uint64*4)();ref.reference_counts(program,counts);counts=tuple(counts);ref.reference_delete(program)
                else:trace=program.trace;counts=(program.requests,program.eligible,program.effective,program.rounds)
                lib.tactix_release(s)
                proof=stem.with_suffix('.proof')
                if result==20 and fmt is not None:
                    kind='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
                    checked=runner.check_proof(kind,str(cnf),str(proof),30,route=route)
                    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked;proofs+=1
                answers[arm]=(result,counters,model,trace,counts,proof.read_bytes() if fmt is not None else b'',elapsed)
            c,py=answers['c'],answers['python'];assert c[:6]==py[:6],(i,mode,c[:5],py[:5])
            if n>6:
                for counter in ('RESTARTS','REDUCTIONS','SWITCHES','REPHASED'):
                    assert c[1][getattr(capi,'TACTIX_STAT_'+counter)]>0,(i,mode,counter)
            assert c[1]==canonical.setdefault(i,c[1]),(i,mode,'proof-dependent search')
            reached_merges+=c[1][capi.TACTIX_STAT_CONGRUENCE_MERGES]
            rows.append(dict(case=i,mode=mode,result=c[0],actions=len(c[3]),native_ns=c[6],python_ns=py[6],
                counters=c[1],root_counts=c[4],model=c[2],
                action_sha256=hashlib.sha256(repr(c[3]).encode()).hexdigest(),
                proof_sha256=hashlib.sha256(c[5]).hexdigest() if fmt is not None else None,
                cnf_sha256=hashlib.sha256(cnf.read_bytes()).hexdigest()))
            if fmt is not None:
                for arm in ('c','python'):(p/f'{i}-{mode}-{arm}.proof').unlink()
    if args.policy=='cruxmiter':assert reached_merges>0,'no effective gate merging reached'
    result=dict(status='PASS',scope=args.policy+' C/Python equivalence using one engine; no native DefaultPolicy Python-port or representative performance claim.',
                pairs=len(rows),checked_models=models,checked_proofs=proofs,
                paired_median_python_over_c=statistics.median(row['python_ns']/row['native_ns'] for row in rows),rows=rows)
    (p/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='rows'},indent=2))

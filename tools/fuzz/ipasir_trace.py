#!/usr/bin/env python3
"""Stateful public-IPASIR query fuzzing, independent oracles and trace reduction.

This leg covers query/termination/learning, core budgets and runtime public-rule
strategies. Named specialized adapters and ordinary policy acceptance remain separate.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import random
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/bench'))
from directory_scratch import remove_tree


class Failure(RuntimeError):
    def __init__(self, kind, details):
        super().__init__(f'{kind}: {details}')
        self.kind, self.details = kind, details


def cleanup_case(path):
    try:
        return remove_tree(path)
    except OSError as error:
        raise Failure('cleanup',str(error)) from error


def assignments(clauses):
    variables = sorted({abs(l) for c in clauses for l in c})
    if len(variables) > 12:
        raise ValueError('small-instance oracle supports at most 12 distinct variables')
    for signs in itertools.product((-1, 1), repeat=len(variables)):
        yield {v*s for v, s in zip(variables, signs)}


def satisfies(clauses, values):
    return all(any(l in values for l in c) for c in clauses)


def satisfiable(clauses):
    return any(satisfies(clauses, values) for values in assignments(clauses))


def validate_answer(query, answer):
    for field,default in (('strategy',0),('learning_limit',-1),('learning_budget',-1),('core_budget',0),
                          ('min_budget',-2),('min_context',0),('min_stop',-2)):
        if answer.get(field,default)!=query.get(field,default):
            raise Failure('configuration',(field,query,answer))
    strategy=query.get('strategy',0)
    if (not strategy and answer.get('strategy_calls',0)) or (strategy and answer['result'] and not answer.get('strategy_calls',0)):
        raise Failure('strategy',('missing/wrong execution driver',query,answer))
    native={4:'cruxmiter',6:'dpll',7:'cdcl-no-restarts',8:'scripted-default',9:'chvatal'}.get(strategy)
    if native:
        if answer.get('driver_name')!=native:raise Failure('strategy','wrong native program')
        counts=[answer.get('native_root_'+name) for name in ('requests','eligible','effective','rounds')]
        if any(not isinstance(n,int) or n<0 for n in counts) or not counts[0]>=counts[1]>=counts[2]:
            raise Failure('strategy','invalid native request/eligibility/effect counts')
        handoffs=answer.get('native_handoffs')
        if not isinstance(handoffs,int) or not 0<=handoffs<=answer.get('strategy_calls',0):
            raise Failure('strategy','invalid native handoff count')
        if strategy not in (4,9) and (any(counts) or handoffs):
            raise Failure('strategy','search program reported unexpected root-program work or a default handoff')
    if strategy==5:
        policy=answer.get('random_policy')
        if not isinstance(policy,dict):raise Failure('strategy','missing public-random coverage')
        for field in ('root','search','root_to_search','partial_restarts','full_restarts','search_to_root','algebraic'):
            if type(policy.get(field)) is not int or policy[field]<0:
                raise Failure('strategy',('invalid public-random counter',field))
        forms,effective=policy.get('forms'),policy.get('effective_forms')
        if not isinstance(forms,list) or not isinstance(effective,list) or len(forms)!=4 or len(effective)!=4:
            raise Failure('strategy','missing public-random application forms')
        if any(type(n) is not int or n<0 for n in forms+effective) or any(a<b for a,b in zip(forms,effective)):
            raise Failure('strategy','invalid public-random form counts')
        rules=policy.get('rules')
        if not isinstance(rules,dict):raise Failure('strategy','missing public-random rules')
        for name,counts in rules.items():
            if not isinstance(name,str) or not isinstance(counts,list) or len(counts)!=2 or any(type(n) is not int or n<0 for n in counts) or counts[0]<counts[1]:
                raise Failure('strategy','invalid public-random rule counts')
        if sum(forms)!=sum(n[0] for n in rules.values()) or sum(effective)!=sum(n[1] for n in rules.values()):
            raise Failure('strategy','public-random form/rule accounting mismatch')
    validate_learning(query,answer.get('learned',[]))
    limit=query.get('termination_limit',-1)
    calls=answer.get('termination_calls',0)
    if answer.get('termination_limit',-1)!=limit:
        raise Failure('termination',('wrong callback configuration',query,answer))
    if limit>=0 and calls<=0:
        raise Failure('termination','configured callback was never polled')
    requested=limit>0 and calls>=limit
    if answer['result']==0:
        if not requested or answer.get('values') or answer.get('failed'):
            raise Failure('termination',('unrequested/invalid UNKNOWN',query,answer))
        return
    if requested or limit==1:
        raise Failure('termination',('ignored termination request',query,answer))
    if limit<0 and calls:raise Failure('termination','disabled callback was called')
    validate_semantics(query,answer)


def validate_learning(query,learned):
    f=query['formula'];maximum=query.get('learning_limit',-1)
    if learned and (maximum<0 or query.get('learning_budget',-1)==0):
        raise Failure('learning','disabled/budget-zero callback delivered a clause')
    extent=max((abs(l) for c in f for l in c),default=0)
    for clause in learned:
        if len(clause)>maximum or any(not isinstance(l,int) or not l or abs(l)>extent for l in clause):
            raise Failure('learning',('length/namespace',clause,query))
        if satisfiable(f+[[-l] for l in clause]):
            raise Failure('learning',('clause not implied by permanent F',clause,query))


def validate_semantics(query,answer):
    f,a=query['formula'],query['assumptions'];clauses=f+[[l] for l in a]
    expected=10 if satisfiable(clauses) else 20
    if answer['result'] != expected:
        raise Failure('verdict', (query, answer))
    if expected == 10:
        values = answer['values']
        if any(not l or -l in values for l in values):
            raise Failure('model', ('inconsistent values', values))
        fixed = set(values)
        # IPASIR may leave don't-cares at zero: EVERY completion must model F+A.
        for candidate in assignments(clauses):
            if any(-l in candidate for l in fixed):
                continue
            if not satisfies(clauses, candidate):
                raise Failure('model', (query, values, sorted(candidate)))
    else:
        core = answer['failed']
        if not set(core) <= set(a) or satisfiable(f+[[l] for l in core]):
            raise Failure('failed-core', (query, core))


def queries(lines):
    sessions, result, coverage = {}, [], Counter()
    for raw in lines:
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        words = line.split()
        if len(words) not in (2, 3): raise ValueError('bad trace operation')
        op, sid = words[0], int(words[1])
        if sid < 0: raise ValueError('negative session ID')
        if op == 'init':
            if len(words)!=2 or sid in sessions: raise ValueError('duplicate init')
            sessions[sid] = {'formula': [], 'pending': [], 'assumptions': [], 'query': 0,
                             'last_prefix':0,'termination_limit':-1,'strategy':0,
                             'learning_limit':-1,'learning_budget':-1,'core_budget':0,'random_seed':0,
                             'min_budget':-2,'min_context':0,'min_stop':-2}
        else:
            if sid not in sessions: raise ValueError('operation on released session')
            s = sessions[sid]
            if op == 'release':
                if len(words)!=2: raise ValueError('release arguments')
                del sessions[sid]
            elif op=='terminate':
                if len(words)!=3 or int(words[2]) < -1:raise ValueError('invalid termination control')
                s['termination_limit']=int(words[2])
            elif op in ('min_budget','min_context','min_stop'):
                if len(words)!=3:raise ValueError('missing minimization configuration')
                value=int(words[2]);minimum=0 if op=='min_context' else -2
                if not minimum<=value<=2147483647:raise ValueError('invalid minimization configuration')
                s[op]=value
            elif op in ('strategy','learn','learn_budget','core_budget','random_seed'):
                if len(words)!=3:raise ValueError('missing configuration value')
                value=int(words[2])
                if not -1<=value<=2147483647:raise ValueError('configuration overflow')
                if op=='strategy':
                    if not 0<=value<=9:raise ValueError('unknown strategy')
                    coverage['strategy-switch']+=value!=s['strategy']
                    s['strategy']=value
                elif op=='random_seed':
                    if value<0:raise ValueError('negative random seed')
                    s['random_seed']=value
                else:s[{'learn':'learning_limit','learn_budget':'learning_budget','core_budget':'core_budget'}[op]]=value
            elif op in ('add', 'assume'):
                if len(words)!=3: raise ValueError('missing literal')
                lit = int(words[2])
                if not -2147483647 <= lit <= 2147483647: raise ValueError('literal overflow')
                if op == 'assume':
                    if not lit: raise ValueError('zero assumption')
                    coverage['duplicate-assumption'] += lit in s['assumptions']
                    coverage['opposite-assumption'] += -lit in s['assumptions']
                    s['assumptions'].append(lit)
                elif lit:
                    s['pending'].append(lit)
                else:
                    s['formula'].append(s['pending']);s['pending']=[]
            elif op == 'solve':
                if len(words)!=2 or s['pending']: raise ValueError('unterminated input')
                s['query'] += 1
                q = {'session': sid, 'query': s['query'],
                     'formula': [list(c) for c in s['formula']], 'assumptions': list(s['assumptions']),
                     'termination_limit':s['termination_limit'],
                     'strategy':s['strategy'],'random_seed':s['random_seed'],'learning_limit':s['learning_limit'],
                     'learning_budget':s['learning_budget'],'core_budget':s['core_budget'],
                     'min_budget':s['min_budget'],'min_context':s['min_context'],
                     'min_stop':s['min_stop'],
                     'appended':len(s['formula'])-s['last_prefix']}
                result.append(q)
                s['last_prefix']=len(s['formula']);s['assumptions']=[]
            else: raise ValueError('unknown operation')
        coverage[op] += 1
        coverage['multiple-live-sessions'] += len(sessions)>1
    if not result: raise ValueError('trace has no solve')
    return result, coverage


def generate(seed, count=12, strategy=None,minimize=False):
    rng = random.Random(seed)
    lines = ['init 0', 'init 1',f'random_seed 0 {seed%2147483648}',f'random_seed 1 {(seed+1)%2147483648}']
    if strategy is not None:lines += [f'strategy {sid} {strategy}' for sid in (0,1)]
    pool = [1, 2, 3, 7, 17, 41]
    for n in range(count):
        sid = rng.randrange(2)
        if n and rng.randrange(5)==0:
            lines += [f'release {sid}', f'init {sid}',f'random_seed {sid} {rng.randrange(2147483648)}']
            if strategy is not None:lines += [f'strategy {sid} {strategy}']
        if rng.randrange(3)==0:
            lines += [f'terminate {sid} {rng.choice((-1,0,1,2,4,16,64))}']
        if rng.randrange(3)==0:lines += [f'strategy {sid} {rng.randrange(10) if strategy is None else strategy}']
        if rng.randrange(3)==0:lines += [f'learn {sid} {rng.choice((-1,0,1,2,4,8))}']
        if rng.randrange(4)==0:lines += [f'learn_budget {sid} {rng.choice((-1,0,1,64,4096,65536))}']
        if rng.randrange(3)==0:lines += [f'core_budget {sid} {rng.choice((0,1,64,4096,1000000))}']
        if minimize:
            lines += [f'min_budget {sid} {rng.choice((-1,0,1,2,4))}',
                      f'min_context {sid} {rng.randrange(128)}',
                      f'min_stop {sid} {rng.choice((-2,-1,0,1,4,16,64))}']
        for _ in range(rng.randrange(4)):
            width = rng.randrange(1, 5) if rng.randrange(16) else 0
            clause = [rng.choice(pool)*rng.choice((-1, 1)) for _ in range(width)]
            lines += [f'add {sid} {l}' for l in clause]+[f'add {sid} 0']
        assumptions = [rng.choice(pool)*rng.choice((-1, 1)) for _ in range(rng.randrange(5))]
        if assumptions and rng.randrange(3)==0: assumptions += [rng.choice(assumptions)]
        if assumptions and rng.randrange(4)==0: assumptions += [-assumptions[0]]
        lines += [f'assume {sid} {l}' for l in assumptions]+[f'solve {sid}']
    return lines+['release 0', 'release 1']


def limited(command, directory, name, timeout, memory):
    status = directory/(name+'-resource.json')
    wrapped = [sys.executable, str(ROOT/'tools/fuzz/memory_runner.py'),
               '--status', str(status), '--timeout', str(timeout),
               '--cpu-seconds', str(int(timeout)+5), '--memory-mib', str(memory), '--',
               *map(str, command)]
    try:
        r = subprocess.run(wrapped, capture_output=True, text=True, timeout=timeout+20)
    except subprocess.TimeoutExpired as error:
        raise Failure('harness-resource', str(error)) from error
    (directory/(name+'.stdout')).write_text(r.stdout)
    (directory/(name+'.stderr')).write_text(r.stderr)
    if not status.exists(): raise Failure('harness', ('missing memory record', r.stderr))
    record = json.loads(status.read_text())
    if record.get('memory_exceeded') or record.get('timed_out') or record.get('cgroup_oom_kills'):
        raise Failure(name+'-resource', record)
    return r


def read_cnf(path):
    clauses, clause = [], []
    for line in path.read_text().splitlines():
        if not line or line.startswith(('c', 'p')): continue
        for lit in map(int, line.split()):
            if lit: clause.append(lit)
            else: clauses.append(clause);clause=[]
    if clause: raise Failure('snapshot', 'unterminated CNF')
    return clauses


def check_certificate(prefix, timeout):
    sys.path.insert(0, str(ROOT/'tools/bench'))
    spec = importlib.util.spec_from_file_location('trace_proof_runner', ROOT/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    from proof_oracle import certificate_route
    cnf, proof = Path(str(prefix)+'.cnf'), Path(str(prefix)+'.proof')
    kind = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    free = kind=='dsr' and certificate_route(proof)=='drat'
    route = 'pbp' if kind=='pbp' else 'dsr-cake_lpr' if free else 'dsr-trestle'
    result = runner.check_proof(kind, str(cnf), str(proof), timeout, route=route)
    if result[:3] != (True, 'VERIFIED', runner.CHK_VERIFIED):
        raise Failure('certificate', result)


def validate_bundle(q, answer, stem,semantics_only=False):
    cnf=Path(str(stem)+'.cnf')
    if read_cnf(cnf)!=q['formula']+[[l] for l in q['assumptions']]:
        raise Failure('snapshot', q)
    meta=json.loads(Path(str(stem)+'.json').read_text())
    if meta['query']!=q['query'] or meta['result']!=answer['result']:
        raise Failure('snapshot', (q, meta))
    digest=hashlib.sha256(b'tactix-permanent-input-i32le-v1\n')
    for clause in q['formula']:
        for word in clause+[0]:digest.update(struct.pack('<i',word))
    if meta.get('permanent_encoding')!='tactix-permanent-input-i32le-v1' or meta.get('permanent_sha256')!=digest.hexdigest():
        raise Failure('snapshot','incorrect permanent-input identity')
    required={'.cnf','.proof','.model' if answer['result']==10 else '.core'}
    if meta.get('core','full-original-cube') not in ('full-original-cube','licensed-subset'):
        raise Failure('bundle','unknown core evidence kind')
    reduced=answer['result']==20 and meta.get('core')=='licensed-subset'
    if reduced:required.update(('-core.cnf','-core.proof'))
    if set(meta['sha256'])!=required:raise Failure('bundle','missing/extra members')
    for suffix,digest in meta['sha256'].items():
        if hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()!=digest:
            raise Failure('bundle', ('hash mismatch',suffix))
    if answer['result']==10:
        model=[int(x) for x in Path(str(stem)+'.model').read_text().split() if x!='v']
        if not model or model.pop()!=0:raise Failure('model','unterminated model artifact')
        (validate_semantics if semantics_only else validate_answer)(q,dict(answer,values=model))
    else:
        core=list(map(int,Path(str(stem)+'.core').read_text().split()))
        if not core or core.pop()!=0:raise Failure('failed-core','unterminated core artifact')
        (validate_semantics if semantics_only else validate_answer)(q,dict(answer,failed=core))
        if set(core)!=set(answer['failed']):raise Failure('failed-core','artifact/result mismatch')
        if not reduced and set(core)!=set(q['assumptions']):
            raise Failure('bundle','smaller core lacks its exact reduced certificate')
        if reduced:
            if len(set(core))>=len(set(q['assumptions'])):raise Failure('failed-core','non-strict reduced-core claim')
            if read_cnf(Path(str(stem)+'-core.cnf'))!=q['formula']+[[l] for l in core]:
                raise Failure('snapshot','wrong reduced F+C')
    return reduced


def validate_owned_delivery(answers, output):
    all_results=list(answers)
    for a in answers:
        if a.get('minimization') is not None:
            m=a['minimization']
            if not isinstance(m,dict) or not isinstance(m.get('probes'),list):
                raise Failure('minimization','invalid probe records')
            all_results+=m['probes']
    if any(not isinstance(a,dict) or a.get('result') not in (0,10,20) for a in all_results):
        raise Failure('owned-result','invalid terminal/probe result record')
    count=sum(a['result']!=0 for a in all_results)
    if any(a.get('owned_result')!=(a['result']!=0) for a in all_results):
        raise Failure('owned-result','missing or wrong owned result')
    markers=[line for line in output.splitlines() if line.startswith('c owned-results-checked ')]
    if markers!=[f'c owned-results-checked {count}']:
        raise Failure('owned-result','missing after-release value/artifact check')
    return count


def validate_minimization(q,answer):
    """Reconstruct each greedy deletion independently; no native core is trusted."""
    m=answer.get('minimization');budget=q.get('min_budget',-2)
    if answer['result']!=20 or budget==-2:
        if m is not None:raise Failure('minimization','unexpected minimization')
        return []
    if not isinstance(m,dict) or not isinstance(m.get('probes'),list):
        raise Failure('minimization','missing native reduction')
    source=answer['bundle_index']
    if m.get('source')!=source:raise Failure('minimization','wrong source identity')
    raw=q['assumptions'];mask=q.get('min_context',0)
    context=list(dict.fromkeys(l for i,l in enumerate(raw[:31]) if mask&(1<<i)))
    candidate=list(dict.fromkeys(l for l in raw if l not in context))
    if m.get('context')!=context:raise Failure('minimization','wrong fixed context')
    probes=m['probes'];position=0;witnesses=[];certificate=source
    if any(not isinstance(p,dict) or p.get('result') not in (0,10,20) for p in probes):
        raise Failure('minimization','invalid probe record')
    maximum=2**64-1 if budget==-1 else budget
    if len(probes)>maximum:raise Failure('minimization','probe budget exceeded')
    for i,p in enumerate(probes):
        if position>=len(candidate):raise Failure('minimization','unnecessary probe after completion')
        literal=candidate[position]
        proposed=context+candidate[:position]+candidate[position+1:]
        if p.get('assumptions')!=proposed:raise Failure('minimization','wrong deletion premises')
        if p['result']==20:
            validate_semantics(dict(q,assumptions=proposed),p)
            candidate.pop(position);certificate=p['bundle_index']
        elif p['result']==10:
            validate_semantics(dict(q,assumptions=proposed),p)
            witnesses.append([literal,i]);position+=1
        elif p['result']==0:
            if i!=len(probes)-1 or p.get('values') or p.get('failed'):
                raise Failure('minimization','continued after UNKNOWN or exported UNKNOWN values')
        else:raise Failure('minimization','invalid probe verdict')
    minimal=position==len(candidate)
    if m.get('core')!=candidate or type(m.get('minimal')) is not bool or m['minimal']!=minimal:
        raise Failure('minimization','wrong best core or minimality claim')
    if m.get('certificate')!=certificate or m.get('witnesses')!=witnesses:
        raise Failure('minimization','misbound certificate or deletion witness')
    if satisfiable(q['formula']+[[l] for l in context+candidate]):
        raise Failure('minimization','returned cube does not refute its context')
    unknown=bool(probes and probes[-1]['result']==0)
    if not minimal and not unknown and len(probes)!=maximum:
        raise Failure('minimization','stopped without exhausting a budget or interruption')
    calls=m.get('termination_calls');override=q.get('min_stop',-2)
    limit=q.get('termination_limit',-1) if override==-2 else override
    start=answer.get('termination_calls',0) if override==-2 else 0
    if m.get('termination_limit')!=limit or type(calls) is not int or calls<start:
        raise Failure('minimization','invalid termination counter')
    if unknown:
        if limit<=0 or calls<limit:raise Failure('minimization','unrequested UNKNOWN probe')
    elif limit>0 and calls>=limit:raise Failure('minimization','ignored interruption during minimization')
    if limit<0 and calls:raise Failure('minimization','disabled termination was called')
    entered,resumed=m.get('strategy_calls'),m.get('restorations')
    if type(entered) is not int or type(resumed) is not int or not 0<=resumed<=entered:
        raise Failure('minimization','invalid driver entry counters')
    terminal=sum(p['result']!=0 for p in probes)
    if q.get('strategy',0):
        if not terminal<=entered-resumed<=len(probes):
            raise Failure('minimization','probe driver was skipped or silently changed')
    elif entered or resumed:raise Failure('minimization','unexpected custom driver')
    validate_learning(q,m.get('learned',[]))
    if minimal:
        for literal in candidate:
            if not satisfiable(q['formula']+[[l] for l in context+candidate if l!=literal]):
                raise Failure('minimization','a claimed necessary assumption is redundant')
    return probes


def verify_query(driver,q,answer,stem,directory,timeout,solver_memory,checker_memory,reference,coverage,fresh,probe=False):
    """The same original-input oracle chain covers outer calls and native probes."""
    value=answer['result'];cnf=Path(str(stem)+'.cnf');reduced=False
    if value:reduced=validate_bundle(q,answer,stem,semantics_only=probe)
    else:
        clauses=q['formula']+[[l] for l in q['assumptions']]
        variables=max((abs(l) for c in clauses for l in c),default=0)
        cnf.write_text(f'p cnf {variables} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
        coverage['unknown-validated']+=1
    name=stem.name
    if value==20:
        check=limited([sys.executable,Path(__file__).resolve(),'--check-prefix',stem,'--root',ROOT,'--timeout',str(timeout)],
                      directory,'checker-'+name,timeout+5,checker_memory)
        if check.returncode:raise Failure('certificate',check.stderr[-4000:])
        coverage['proof-verified']+=1
        if reduced:
            check=limited([sys.executable,Path(__file__).resolve(),'--check-prefix',str(stem)+'-core','--root',ROOT,'--timeout',str(timeout)],
                          directory,'core-checker-'+name,timeout+5,checker_memory)
            if check.returncode:raise Failure('certificate',check.stderr[-4000:])
            coverage['reduced-core-verified']+=1
    elif value==10:coverage['model-verified']+=1
    if reference:
        ref=limited([reference,'-q',cnf],directory,'reference-'+name,timeout,solver_memory)
        expected=10 if satisfiable(q['formula']+[[l] for l in q['assumptions']]) else 20
        if ref.returncode!=expected:raise Failure('differential',(name,ref.returncode,ref.stderr))
        coverage['cadical-query']+=1
    if fresh:
        replay=['init 0',f"random_seed 0 {q['random_seed']}",f"strategy 0 {q['strategy']}",f"learn 0 {q['learning_limit']}",
                f"learn_budget 0 {q['learning_budget']}",f"core_budget 0 {q['core_budget']}"]
        for c in q['formula']:replay += [f'add 0 {l}' for l in c]+['add 0 0']
        replay += [f'assume 0 {l}' for l in q['assumptions']]+['solve 0','release 0']
        check_run(driver,replay,directory/('fresh-'+name),timeout,solver_memory,checker_memory,fresh=False)
        coverage['fresh-query']+=1


def check_run(driver, lines, directory, timeout, solver_memory, checker_memory, reference=None, fresh=True):
    lines=[raw.split('#',1)[0].strip() for raw in lines]
    expected, coverage = queries(lines)
    directory.mkdir()
    trace, prefix = directory/'input.trace', directory/'result'
    trace.write_text('\n'.join(lines)+'\n')
    run = limited([driver, trace, prefix], directory, 'solver', timeout, solver_memory)
    if run.returncode: raise Failure('driver', (run.returncode, run.stderr[-4000:]))
    answers = [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]
    if len(answers)!=len(expected): raise Failure('lifecycle', ('answer count', len(answers), len(expected)))
    coverage['owned-result-verified']+=validate_owned_delivery(answers,run.stdout)
    previous={};native_queries={};artifact=0
    for number, (q, answer) in enumerate(zip(expected, answers), 1):
        if answer['index']!=number or answer['session']!=q['session']:
            raise Failure('lifecycle', answer)
        validate_answer(q, answer)
        before=previous.get(q['session']) if q['query']>1 else None
        value=answer['result'];coverage[f'result-{value}']+=1
        if before is not None:
            coverage[f'{before}-to-{value}']+=1
            coverage[f'add-after-{before}']+=q['appended']
        previous[q['session']]=value
        coverage['termination-polls']+=answer.get('termination_calls',0)
        coverage['strategy-calls']+=answer.get('strategy_calls',0)
        coverage[f"strategy-{q['strategy']}-solve"]+=1
        coverage['restorations']+=answer.get('restorations',0)
        if q['strategy']==5:
            policy=answer['random_policy']
            for field in ('root','search','root_to_search','partial_restarts','full_restarts','search_to_root','algebraic'):
                coverage['random-'+field]+=policy[field]
            for i,form in enumerate(('canonical','instance','semantic','online')):
                coverage['random-form-'+form]+=policy['forms'][i]
                coverage['random-effective-form-'+form]+=policy['effective_forms'][i]
            for rule,counts in policy['rules'].items():
                coverage['random-rule-'+rule]+=counts[0]
                coverage['random-effective-rule-'+rule]+=counts[1]
        for field in ('requests','eligible','effective','rounds'):
            coverage['native-root-'+field]+=answer.get('native_root_'+field,0)
        coverage['native-handoffs']+=answer.get('native_handoffs',0)
        coverage['learned-clauses']+=len(answer.get('learned',[]))
        coverage['learned-units']+=sum(len(c)==1 for c in answer.get('learned',[]))
        coverage['learned-empty']+=sum(not c for c in answer.get('learned',[]))
        artifact+=1
        if answer.get('bundle_index')!=artifact:raise Failure('lifecycle','wrong outer bundle identity')
        native=1 if q['query']==1 else native_queries[q['session']]+1
        actual=dict(q,query=native)
        stem=Path(str(prefix)+f'-q{artifact}')
        verify_query(driver,actual,answer,stem,directory,timeout,solver_memory,checker_memory,reference,coverage,fresh)
        probes=validate_minimization(q,answer)
        m=answer.get('minimization')
        if m is not None:
            coverage['minimizations']+=1
            coverage['min-context']+=bool(m['context'])
            coverage['min-strict-reduction']+=len(m['core'])<len(set(q['assumptions'])-set(m['context']))
            coverage['min-complete']+=m['minimal']
            coverage['min-incomplete']+=not m['minimal']
            coverage['min-zero-budget']+=q['min_budget']==0
            coverage['min-hard-unsat']+=not m['core'] and not m['context']
            coverage['min-context-unsat']+=not m['core'] and bool(m['context'])
            coverage['min-witnesses']+=len(m['witnesses'])
            coverage['min-strategy-calls']+=m['strategy_calls']
            coverage['min-restorations']+=m['restorations']
            coverage['min-termination-polls']+=m['termination_calls']-(answer['termination_calls'] if q['min_stop']==-2 else 0)
            coverage['min-learned-clauses']+=len(m.get('learned',[]))
            for p in probes:
                artifact+=1;native+=1
                if p.get('bundle_index')!=artifact:raise Failure('lifecycle','wrong probe bundle identity')
                coverage['min-probes']+=1
                coverage[f"min-probe-result-{p['result']}"]+=1
                pq=dict(q,query=native,assumptions=p['assumptions'])
                verify_query(driver,pq,p,Path(str(prefix)+f'-q{artifact}'),directory,timeout,solver_memory,checker_memory,reference,coverage,fresh,probe=True)
        native_queries[q['session']]=native
    return coverage


def reduce_trace(lines, predicate):
    """Delete operation chunks while preserving a legal trace and failure class."""
    current, width = list(lines), 2
    while len(current)>1:
        size = max(1, (len(current)+width-1)//width)
        for start in range(0, len(current), size):
            candidate = current[:start]+current[start+size:]
            try: queries(candidate)
            except ValueError: continue
            if predicate(candidate):
                current=candidate;width=max(2,width-1);break
        else:
            if width>=len(current): break
            width=min(len(current),width*2)
    return current


def main():
    global ROOT
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=ROOT)
    p.add_argument('--driver', type=Path)
    p.add_argument('--reference', type=Path)
    p.add_argument('--out', type=Path)
    p.add_argument('--corpus', type=Path)
    p.add_argument('--n', type=int, default=0)
    p.add_argument('--seed', type=int, default=0)
    p.add_argument('--strategy',type=int,choices=range(10),
                   help='fix the generated-trace driver; retained corpus keeps its own drivers')
    p.add_argument('--minimize',action='store_true',help='generate native minimization budgets and fixed contexts')
    p.add_argument('--timeout', type=float, default=30)
    p.add_argument('--solver-memory-mib', type=int, default=2048)
    p.add_argument('--checker-memory-mib', type=int, default=4096)
    p.add_argument('--check-prefix', type=Path)
    p.add_argument('--shrink', type=Path)
    args=p.parse_args();ROOT=args.root.resolve()
    if args.check_prefix:
        check_certificate(args.check_prefix,args.timeout);return 0
    if not args.driver or not args.out: p.error('--driver and --out are required')
    args.driver=args.driver.resolve()
    args.reference=args.reference.resolve() if args.reference else None
    args.out.mkdir(parents=True,exist_ok=False)
    cases=[]
    if args.shrink: cases.append(('shrink-source',args.shrink.read_text().splitlines()))
    elif args.corpus:
        cases += [(f'corpus-{f.stem}',f.read_text().splitlines()) for f in sorted(args.corpus.glob('*.trace'))]
    cases += [(f'seed-{s}',generate(s,strategy=args.strategy,minimize=args.minimize)) for s in range(args.seed,args.seed+args.n)]
    if not cases:p.error('select a nonempty corpus or seed range')
    total=Counter();failure=None
    for name, lines in cases:
        try:
            total.update(check_run(args.driver,lines,args.out/name,args.timeout,
                                  args.solver_memory_mib,args.checker_memory_mib,args.reference))
            total['cleanup-retries']+=cleanup_case(args.out/name)
        except Failure as error:
            failure={'case':name,'kind':error.kind,'details':error.details}
            if args.shrink and 'resource' not in error.kind:
                attempt=0
                def fails(candidate):
                    nonlocal attempt
                    attempt+=1
                    try:check_run(args.driver,candidate,args.out/f'reduce-{attempt}',args.timeout,
                                  args.solver_memory_mib,args.checker_memory_mib,args.reference)
                    except Failure as other:return other.kind==error.kind
                    return False
                reduced=reduce_trace(lines,fails)
                (args.out/'minimized.trace').write_text('\n'.join(reduced)+'\n')
            break
    summary={'status':'FAIL' if failure else 'PASS','coverage':dict(total),'failure':failure,
             'generated_strategy':args.strategy,
             'generated_minimization':args.minimize,
             'traces_requested':len(cases),'interfaces_covered':['IPASIR-query']+(['IPASIR-terminate'] if total['termination-polls'] else [])+
                 (['IPASIR-learn'] if total['learned-clauses'] else [])+
                 (['runtime-strategy'] if total['strategy-calls'] else [])+
                 (['certified-reduced-core'] if total['reduced-core-verified'] else [])+
                 (['owned-result'] if total['owned-result-verified'] else [])+
                 (['named-cruxmiter'] if total['strategy-4-solve'] else [])+
                 (['public-random-incremental'] if total['strategy-5-solve'] else [])+
                 (['named-scripted-default'] if total['strategy-8-solve'] else [])+
                 (['named-chvatal'] if total['strategy-9-solve'] else [])+
                 (['native-minimization'] if total['minimizations'] else []),
             'interfaces_pending':['remaining-named-specialized-adapters','full-policy-acceptance'],
             'interfaces_excluded':['IPASIR-UP'],
             'resource_contract':{'solver_memory_mib':args.solver_memory_mib,
                                  'checker_memory_mib':args.checker_memory_mib,'child_timeout':args.timeout}}
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2));return 1 if failure else 0


if __name__=='__main__':
    sys.exit(main())

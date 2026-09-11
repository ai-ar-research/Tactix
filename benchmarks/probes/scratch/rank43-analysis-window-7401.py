#!/usr/bin/env python3
"""Full own replay with existing bounded search-analysis observers."""
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from bounded_trace_config import validate_trace_keys, UNREACHABLE_FINITE_STOP

OLD=ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions'
C=ROOT/'benchmarks/runs/20260911-7401-rank43-analysis-window'
WINDOW=(1,85)
CAP=250000


def read(p): return json.loads(p.read_text())


def extra_environment(prefix,path):
    return {prefix+'_'+k:v for k,v in dict(
        EVENT_TRACE=str(path), EVENT_TRACE_CONFLICT_FROM=str(WINDOW[0]),
        EVENT_TRACE_CONFLICT_TO=str(WINDOW[1]), EVENT_TRACE_LIMIT=str(CAP),
        EVENT_TRACE_STOP_AFTER_CONFLICT=str(UNREACHABLE_FINITE_STOP),
        EVENT_TRACE_CONTROL_ONLY='1', EVENT_TRACE_SKIP_WATCH='1',
        EVENT_TRACE_SKIP_IMPLY='1', EVENT_TRACE_SKIP_HEAP='1',
        EVENT_TRACE_SKIP_RANDEC_DRAWS='1', EVENT_TRACE_SKIP_ANALYSIS_TICKS='1').items()}


def fixtures():
    for prefix in ('TACTIX','KISSAT'):
        env=extra_environment(prefix,'/tmp/diagnostic-only.tsv')
        assert len(env)==11 and env[prefix+'_EVENT_TRACE_STOP_AFTER_CONFLICT']==str(UNREACHABLE_FINITE_STOP)
        assert env[prefix+'_EVENT_TRACE_CONFLICT_FROM']=='1' and env[prefix+'_EVENT_TRACE_CONFLICT_TO']=='85'
        assert all(k.startswith(prefix+'_EVENT_TRACE') for k in env)
        assert not any(k.startswith('SAT_') for k in env)
        source='\n'.join('getenv("'+key+'")' for key in env)
        assert len(validate_trace_keys(env,prefix,source))==11
        for key in env:
            try: validate_trace_keys(env,prefix,source.replace('getenv("'+key+'")',''))
            except ValueError: pass
            else: raise AssertionError('unknown observer key accepted')
    print('Bounded analysis config: both prefixes and22 missing-key rejections PASS')


def prepare():
    assert not C.exists(), 'campaign frozen once prepared'
    m=read(OLD/'manifest.json');s=read(OLD/'solver-summary-7398.json')
    assert s['first_decision_difference']['matching_rows']==4048
    assert s['projected_decision_differences']['literal_sequence_only']['matching_rows']==4049
    deps=dict(m['dependencies'])
    def bind(p,expected=None):
        d=sha(p);assert expected is None or d==expected,str(p)
        deps[str(p.relative_to(ROOT))]=d
    for rel,d in deps.copy().items():bind(ROOT/rel,d)
    for rel,d in s['hashes'].items():bind(ROOT/rel,d)
    for rel,d in m['pin_sha256'].items():bind(OLD/rel,d)
    for p in (OLD/'manifest.json',OLD/'launch.json',OLD/'solver-summary-7398.json',Path(__file__),
              ROOT/'tools/bench/bounded_trace_config.py',ROOT/'tools/bench/test_bounded_trace_config.py'):
        bind(p)
    # Source ownership of current candidate observer and the exact native pin.
    candidate_source=ROOT/'.claude/worktrees/runtime-lucky-reattach-7371/src/diagnostics/event_trace.cpp'
    native_root=ROOT/'tools/solvers/variants/kissat-imply-fast-5414'
    bind(native_root/'build/kissat',m['arms']['kissat']['sha256'])
    source_by_arm=dict(candidate=candidate_source,kissat=native_root/'src/eventtrace.c')
    for name in ('pins','logs','results','provisional','claims','traces'):(C/name).mkdir(parents=True)
    for p in (OLD/'pins').iterdir():
        if p.is_file():shutil.copy2(p,C/'pins'/p.name)
    for arm,config in m['arms'].items():
        prefix='TACTIX' if arm=='candidate' else 'KISSAT'
        config['environment']={k:v.replace(str(OLD),str(C)) for k,v in config['environment'].items()}
        env=extra_environment(prefix,C/'traces'/f'{arm}.search.tsv')
        source=source_by_arm[arm];bind(source)
        validate_trace_keys(env,prefix,source.read_text())
        blob=(C/config['path']).read_bytes()
        assert all(key.encode()+b'\0' in blob for key in env),arm
        config['environment'].update(env)
    for name in ('matrix.tsv','array.sbatch'):
        body=(OLD/name).read_text().replace(str(OLD),str(C)).replace('7397-current-rank43-decisions','7401-rank43-analysis-window')
        with (C/name).open('x') as stream:stream.write(body)
    m.update(schema='full-rank43-bounded-analysis-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        dependencies=deps,window=list(WINDOW),row_cap=CAP,
        prior_campaign=str(OLD.relative_to(ROOT)),prior_summary_sha256=sha(OLD/'solver-summary-7398.json'),
        hypothesis='First4048 full decision rows match; C82 same literal at differing levels and next decision opposite phase. Locate earlier differing conflict, learned clause, minimization or backjump transaction within C1..85 before attributing the loss to initial Congruence binary counts or late scheduling.',
        validation='Exact entire own7397 non-timing work/certificate and whole decision/inprocessing streams; candidate macro rows replay with only explicitly permitted observer additions. Complete uncapped bounded event rows, conflict/analysis-end coverage and portable clause stages precede interpretation. Full independent proofs/cleanup continue asynchronously.',
        test_consideration='No solver, observer or runtime-wrapper edits. Existing strict search-window/macro/trace-key/full-decision controls plus two-prefix22 missing-key checks. No duplicate fuzz or solver-unit tests for this diagnostic-only change.',
        limitation='Only C1..85 analysis detail; watch, implication and heap logs suppressed. Finite event stop is unreachable and no prefix stop is set. Neither timing acceptance nor fixed-reference refresh; trace-induced work/proof drift invalidates diagnosis.')
    m['pin_sha256']={str(p.relative_to(C)):sha(p) for p in [*(C/'pins').iterdir(),C/'matrix.tsv',C/'array.sbatch']}
    with (C/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2)
    for p in (C/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
    print(C)


def submit():
    assert not (C/'launch.json').exists()
    m=read(C/'manifest.json')
    for rel,d in m['pin_sha256'].items():assert sha(C/rel)==d,rel
    for group in ('dependencies','protected_campaign_hashes'):
        for rel,d in m[group].items():assert sha(ROOT/rel)==d,rel
    for task in range(2):
        p=OLD/f'results/{task:03d}.json'
        if p.exists():assert classify(read(p),True)['status']=='checked_UNSAT'
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',str(C/'array.sbatch')]
    job=subprocess.check_output(command,text=True).strip().split(';')[0];assert job.isdigit()
    with (C/'launch.json').open('x') as stream:
        json.dump(dict(job_id=job,accounting_since=since,command=command,manifest_sha256=sha(C/'manifest.json'),
                       submit_helper_sha256=sha(Path(__file__))),stream,indent=2)
    print(job,since)


if __name__=='__main__':
    assert sys.argv[1:] in (['--prepare'],['--submit'],['--self-test'])
    fixtures()
    if sys.argv[1:]==['--prepare']:prepare()
    elif sys.argv[1:]==['--submit']:submit()

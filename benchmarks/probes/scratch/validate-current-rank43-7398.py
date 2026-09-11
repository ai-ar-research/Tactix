#!/usr/bin/env python3
"""Validate complete own work/proof replay before interpreting full decisions."""
import datetime as dt
import importlib.util
import io
import itertools
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from native_work import primary_statistics
from runtime_summary import counters
from decision_prefix import first_stream_difference
from search_window import macro_replay

C = ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions'
TARGET = C/'solver-summary-7398.json'


def projected_first(left, right, fields):
    assert fields and len(fields) == len(set(fields)) and all(0 <= i < 5 for i in fields)
    for index, (a,z) in enumerate(itertools.zip_longest(left,right)):
        a = None if a is None else list(map(int,a.strip().split('\t')))
        z = None if z is None else list(map(int,z.strip().split('\t')))
        assert a is None or len(a) == 5
        assert z is None or len(z) == 5
        if a is None or z is None or any(a[i] != z[i] for i in fields):
            return dict(matching_rows=index,left=a,right=z,fields=fields)
    return None


def stream_summary(stream, final):
    count = warm = resets = 0
    first = last = regular = None
    for line in stream:
        assert line.endswith('\n'), 'truncated decision row'
        fields = line[:-1].split('\t')
        assert len(fields) == 5, 'wrong decision columns'
        row = list(map(int, fields)); c, d, level, literal, warming = row
        assert 0 <= c <= final['conflicts'] and 0 <= d <= (1 << 64)-1
        assert 1 <= level <= (1 << 32)-1 and literal != 0 and abs(literal) < (1 << 31)
        assert warming in (0,1)
        if last is not None: assert c >= last[0], 'conflict counter moved backward'
        if not warming:
            assert d > 0
            if regular is None: assert d == 1, 'missing initial normal decision'
            if regular is not None and d != regular[1]+1:
                assert c == regular[0] == 0 and d == 1, 'missing/duplicate normal decision or late reset'
                resets += 1
            regular = row
        warm += warming; count += 1
        if first is None: first = row
        last = row
    assert count and regular is not None, 'empty decision stream'
    assert regular[1] == final['decisions'], 'terminal decision count mismatch'
    return dict(rows=count, warming_rows=warm, preprocessing_counter_resets=resets,
                first=first, last=last, last_normal=regular)


def fixtures():
    good = '0\t1\t1\t2\t0\n1\t2\t2\t-3\t0\n'
    final = dict(conflicts=2, decisions=2)
    assert stream_summary(io.StringIO(good), final)['rows'] == 2
    reset = '0\t1\t1\t2\t0\n0\t1\t1\t3\t0\n0\t1\t2\t-4\t1\n1\t2\t1\t5\t0\n'
    r = stream_summary(io.StringIO(reset), final)
    assert r['preprocessing_counter_resets'] == r['warming_rows'] == 1
    cases = [('',final), (good[:-1],final), (good.replace('\t-3\t','\t0\t'),final),
        (good.replace('\t-3\t0','\t-3\t2'),final),
        (good.replace('1\t2\t2','1\t3\t2'),dict(final,decisions=3)),
        (good.replace('1\t2\t2','1\t1\t2'),dict(final,decisions=1)),
        (good.replace('0\t1\t1','2\t1\t1'),final),
        (good.replace('1\t2\t2','3\t2\t2'),final),
        (good.replace('1\t2\t2','1\t2\t0'),final),
        (good.replace('\t2\t0\n','\t2\t0\textra\n'),final),
        (good,dict(final,decisions=3)),
        ('1\t2\t2\t-3\t0\n',final)]
    for body, stats in cases:
        try: stream_summary(io.StringIO(body), stats)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('bad full stream accepted')
    print('Full decision stream: two positive and twelve negative controls PASS')
    shifted = good.replace('1\t2\t2','1\t2\t3')
    assert projected_first(io.StringIO(good),io.StringIO(shifted),(0,1,3,4)) is None
    assert projected_first(io.StringIO(good),io.StringIO(shifted),(0,1,2,3,4))['matching_rows'] == 1
    changed = shifted.replace('-3','-4')
    assert projected_first(io.StringIO(good),io.StringIO(changed),(3,))['matching_rows'] == 1
    assert projected_first(io.StringIO(good),io.StringIO(''),(3,))['left'] == [0,1,1,2,0]
    print('Four projection/length controls PASS; level-only differences remain explicit')


def validate(seal=False):
    if seal: assert not TARGET.exists(), 'successful evidence frozen'
    m = json.loads((C/'manifest.json').read_text()); hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, str(p)
        hashes[str(p.relative_to(ROOT))] = digest
    for rel, digest in m['pin_sha256'].items(): bind(C/rel,digest)
    for group in ('dependencies','protected_campaign_hashes'):
        for rel, digest in m[group].items(): bind(ROOT/rel,digest)
    for p in (C/'manifest.json', C/'launch.json', Path(__file__)):
        bind(p)
    helper = Path(__file__).with_name('localize-root-lock-rank37-6073.py'); bind(helper)
    spec = importlib.util.spec_from_file_location('boundaries7398',helper)
    b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b); b.fixtures()
    records = {}; bounds = {}; ignored = {}; streams = {}
    for task, arm in enumerate(('candidate','kissat')):
        p = C/f'results/{task:03d}.json'
        if not p.exists(): p = C/f'provisional/{task:03d}.json'
        if not p.exists():
            print(arm, 'solver record pending; no summary written'); return
        r = json.loads(p.read_text()); bind(p)
        state = classify(r,p.parent.name == 'results')
        assert state['status'] in ('pending_validation','checked_UNSAT'), state
        assert (r['task'],r['rank'],r['arm']) == (task,43,arm)
        assert r['solve']['solver_environment'] == m['arms'][arm]['environment']
        oracle = m['unprofiled_records']['43/'+arm]; op = ROOT/oracle['path']
        bind(op,oracle['sha256']); old = json.loads(op.read_text())
        for key in ('instance','expected','verdict','certificate_bytes','certificate_sha256'):
            assert r[key] == old[key], (arm,key)
        assert counters(r) == counters(old)
        if arm == 'candidate':
            assert [s for s in r['solver_statistics'] if not s.startswith('c timing')] == [s for s in old['solver_statistics'] if not s.startswith('c timing')]
        else: assert primary_statistics(r['solver_statistics']) == primary_statistics(old['solver_statistics'])
        if p.parent.name == 'results': assert r['binary_sha256'] == m['arms'][arm]['sha256']
        streams[arm] = C/'traces'/f'{arm}.decisions.tsv'
        bind(streams[arm])
        with streams[arm].open() as stream: summary = stream_summary(stream,counters(r))
        bp = C/'traces'/f'{arm}.boundaries.tsv'; bind(bp)
        assert bp.read_bytes().endswith(b'\n')
        with bp.open() as stream: bounds[arm], ignored[arm] = b.parse(stream,arm == 'kissat')
        records[arm] = dict(path=str(p.relative_to(ROOT)),state=state,work=counters(r),stream=summary)
    macro = C/'traces/candidate.macro.tsv'; bind(macro)
    body = macro.read_text(); macro_replay(body,body)
    for name in ('native_work','decision_prefix','search_window'):
        bind(ROOT/f'tools/bench/{name}.py')
    projections = {}
    for name,fields in (('without_level',(0,1,3,4)),('literal_sequence_only',(3,))):
        with streams['candidate'].open() as left, streams['kissat'].open() as right:
            projections[name] = projected_first(left,right,fields)
    out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
        records=records,first_decision_difference=first_stream_difference(streams['candidate'],streams['kissat']),
        projected_decision_differences=projections,
        boundary_difference=b.compare(bounds['candidate'],bounds['kissat']),ignored_boundary_names=ignored,
        limitation='Full own work/certificate replay and complete trace framing precede interpretation. Boundary counts are coarse, not formula or effort equality. The first decision difference may be downstream of earlier preprocessing. Independent proofs and cleanup remain acceptance requirements; no unit-cost or strength claim from unequal work.')
    if seal:
        with TARGET.open('x') as stream: json.dump(out,stream,indent=2)
        print(TARGET)
    print(json.dumps({k:out[k] for k in ('first_decision_difference','projected_decision_differences','boundary_difference')},indent=2))


if __name__ == '__main__':
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    fixtures()
    if sys.argv[1:] != ['--self-test']: validate(sys.argv[1:] == ['--seal'])

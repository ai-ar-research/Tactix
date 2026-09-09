#!/usr/bin/env python3
"""Stream full history, require parent replay, locate first retention drift."""
from collections import Counter
import hashlib
import importlib.util
import io
import itertools
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
C = ROOT / 'benchmarks/runs/20260908-6042-retention-history'
spec = importlib.util.spec_from_file_location('reduce_compare', ROOT / 'tools/bench/compare_reduce_trace.py')
reduce = importlib.util.module_from_spec(spec); spec.loader.exec_module(reduce)

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def learned(stream):
    assert next(stream).rstrip() == '# learned-stage\t1'
    assert next(stream).rstrip() == '# seq\tevent\tconflicts\tdecisions\tlevel\tsize\tliterals'
    previous = (0, 0)
    for seq, line in enumerate(stream):
        f = line.rstrip('\n').split('\t'); assert len(f) == 7
        assert int(f[0]) == seq
        event = f[1]; conflict, decisions, level, size = map(int, f[2:6])
        assert event in ('learned_raw', 'learned_sorted', 'learned_shrunk', 'learned')
        assert 0 <= conflict <= 270978 and decisions >= 0 and level >= 0 and size >= 0
        assert conflict >= previous[0] and decisions >= previous[1]
        previous = (conflict, decisions)
        lits = tuple(map(int, f[6].split(','))) if size else ()
        assert (size or f[6] == '-') and len(lits) == size and all(lits)
        assert len(set(map(abs, lits))) == size
        yield (event, conflict, decisions, level, size, lits)

def assert_window(new, old, lower, upper):
    count = 0
    for a, b in itertools.zip_longest((r for r in new if lower <= r[1] <= upper), old):
        assert a == b, ('window-replay', count, a, b)
        count += 1
    assert count
    return count

def reductions(path):
    def records(stream):
        for line in stream:
            if line.startswith('#'): continue
            f = line.rstrip('\n').split('\t'); assert len(f) == len(reduce.COLUMNS)
            yield {k: reduce.value(k, v) for k, v in zip(reduce.COLUMNS, f)}
    previous = 0
    with path.open() as stream:
        for number, group in itertools.groupby(records(stream), lambda r: r['reduction']):
            assert number > previous; previous = number
            rows = list(group)
            summaries = [r for r in rows if r['kind'] == 'summary']; assert len(summaries) == 1
            summary = summaries[0]
            clauses = [r for r in rows if r['kind'] == 'clause']
            ranks = {r['ref']: r for r in rows if r['kind'] == 'rank'}
            assert len({r['ref'] for r in clauses}) == len(clauses)
            for r in clauses:
                assert r['conflict'] == summary['conflict']
                assert tuple(sorted(r['literals'])) == r['literals'] and len(r['literals']) == r['size']
                if r['ref'] in ranks:
                    r.update({k: ranks[r['ref']][k] for k in ('rank','selected')})
            ranked = sorted((r for r in clauses if r['rank'] is not None), key=lambda r:r['rank'])
            assert [r['rank'] for r in ranked] == list(range(summary['candidates']))
            assert sum(r['selected'] for r in ranked) == summary['victims']
            assert all(r['selected'] == int(r['rank'] < summary['victims']) for r in ranked)
            yield summary, clauses, ranked

def comparison(a, b):
    x, xc, xr = a; y, yc, yr = b
    for k in ('reduction', 'conflict', 'mode', 'tier1', 'tier2'): assert x[k] == y[k], (k,x[k],y[k])
    counts = [Counter(r['literals'] for r in rows) for rows in (xc,yc)]
    extras = [counts[0] - counts[1], counts[1] - counts[0]]
    shared = counts[0] & counts[1]
    projected = []
    for rows in (xr,yr):
        remain = shared.copy(); out = []
        for r in rows:
            if remain[r['literals']]: out.append(r['literals']); remain[r['literals']] -= 1
        projected.append(out)
    fields = ('literals','size','glue','used','reason','eligibility','rank','selected')
    metadata = [Counter(tuple(r[k] for k in fields) for r in rows) for rows in (xc,yc)]
    differences = [metadata[0]-metadata[1], metadata[1]-metadata[0]]
    return dict(reduction=x['reduction'], conflict=x['conflict'], exact_clause_metadata=not any(differences),
        candidate_only_copies=sum(extras[0].values()), kissat_only_copies=sum(extras[1].values()),
        ranked_common_projection_equal=projected[0] == projected[1],
        summaries={'candidate':x,'kissat':y},
        only_rows={arm:[r for r in rows if r['literals'] in extra][:12]
                   for arm,rows,extra in zip(('candidate','kissat'),(xc,yc),extras)},
        first_metadata_difference={arm:[dict(zip(fields,key), multiplicity=n) for key,n in list(diff.items())[:3]]
                                   for arm,diff in zip(('candidate','kissat'),differences)})

def fixtures():
    head = '# learned-stage\t1\n# seq\tevent\tconflicts\tdecisions\tlevel\tsize\tliterals\n'
    data = head + '0\tlearned_raw\t0\t1\t0\t2\t1,-2\n1\tlearned\t1\t2\t0\t0\t-\n2\tlearned\t2\t3\t0\t1\t3\n'
    rows = list(learned(io.StringIO(data))); assert len(rows) == 3
    assert assert_window(iter(rows), iter(rows[1:]), 1, 2) == 2
    assert assert_window(iter(rows), iter(rows[:1]), 0, 0) == 1
    for old,new in [('1,-2','1,0'), ('1,-2','1,-1'), ('1\tlearned','4\tlearned'), ('\t0\t-','\t0\t3'), ('learned_raw','bad')]:
        try: list(learned(io.StringIO(data.replace(old,new))))
        except (AssertionError,ValueError): pass
        else: raise AssertionError('invalid learning data accepted')
    try: assert_window(iter(rows), iter(rows[1:2]), 1, 2)
    except AssertionError: pass
    else: raise AssertionError('missing old row accepted')
    assert Counter([(1,2),(1,2)]) - Counter([(1,2)]) == Counter({(1,2):1})
    print('PASS streaming learning/window/multiplicity controls', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = C / 'summary.json'; assert not target.exists()
    m = json.loads((C / 'manifest.json').read_text()); parent = ROOT / m['parent_campaign']
    for rel,digest in m['hashes'].items(): assert sha(C/rel) == digest, rel
    evidence = {}
    for arm in ('candidate','kissat'):
        p = C/'results'/f'{arm}.json'; r=json.loads(p.read_text())
        oldp=parent/'results'/f'{arm}.json'; old=json.loads(oldp.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['binary_sha256'] == old['binary_sha256'] == sha(C/'pins'/arm)
        assert r['partial_certificate'] == old['partial_certificate']
        assert set(r['trace_sha256']) == set(old['trace_sha256'])
        for name,digest in r['trace_sha256'].items():
            assert sha(C/'traces'/name) == digest, name
            if not name.endswith(('.reduce.tsv','.learned-stage.tsv')): assert digest == old['trace_sha256'][name], name
        for suffix in ('reduce.tsv','learned-stage.tsv'):
            assert sha(parent/'traces'/f'{arm}.{suffix}') == old['trace_sha256'][f'{arm}.{suffix}']
        with (C/'traces'/f'{arm}.learned-stage.tsv').open() as a, (parent/'traces'/f'{arm}.learned-stage.tsv').open() as b:
            n = assert_window(learned(a), learned(b), 240062, 247206)
        with (C/'traces'/f'{arm}.reduce.tsv').open() as a, (parent/'traces'/f'{arm}.reduce.tsv').open() as b:
            current = (line for line in a if not line.startswith('#') and 245473 <= int(line.split('\t')[3]) <= 270978)
            oldrows = (line for line in b if not line.startswith('#'))
            nr = 0
            for x,y in itertools.zip_longest(current,oldrows): assert x == y; nr += 1
            assert nr
        evidence[arm] = dict(result_sha256=sha(p),parent_result_sha256=sha(oldp),window_learning_rows=n,window_reduce_rows=nr)
        print(arm, 'full parent replay PASS', flush=True)
    comparisons=[]
    for a,b in itertools.zip_longest(reductions(C/'traces/candidate.reduce.tsv'), reductions(C/'traces/kissat.reduce.tsv')):
        assert a is not None and b is not None
        row=comparison(a,b); comparisons.append(row)
        print('reduction',row['reduction'],row['conflict'],'exact',row['exact_clause_metadata'],'extras',row['candidate_only_copies'],row['kissat_only_copies'],flush=True)
    assert comparisons
    out=dict(full_parent_replay=True,evidence=evidence,reductions=comparisons,
             first_difference=next((r for r in comparisons if not r['exact_clause_metadata']),None),
             limitation='Diagnostic only; local refs are not cross-solver identities. Callback stages and missing clauses need causal interpretation; no performance or final proof claim.')
    target.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps(out['first_difference'],indent=2))

if __name__ == '__main__': main()

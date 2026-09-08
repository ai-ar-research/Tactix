#!/usr/bin/env python3
"""Replay first, then distinguish completed insertions from pre-push updates."""
import io
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
C = ROOT / 'benchmarks/runs/20260908-5996-bve-reinsert-order'
SUBJECTS = {'5409', '5415', '5419', '5232', '148944', '331390', '225614', '60705'}
source = Path(__file__).with_name('compare-bve-heap-lifecycle-5951.py')
code = source.read_text().replace('20260908-5949-bve-heap-lifecycle', C.name)
code = code.replace("SUBJECTS={'225614','225641','225620','89339'}", 'SUBJECTS=' + repr(SUBJECTS))
code = code.replace("{key,arm+'.bve-heap.tsv'}", "{key,arm+'.bve-heap.tsv',arm+'.retry-producer.tsv'}")
code = code.replace("target.write_text(json.dumps(out,indent=2)+'\\n')", 'return out')
base = {'__file__': str(source), '__name__': 'selected_heap_base'}
exec(compile(code, str(source), 'exec'), base)

def producers(stream, arm):
    rows = []
    fields = {'conflicts', 'decisions', 'external', 'source', 'before', 'after', 'value'}
    fields |= {'retired', 'clause'} if arm == 'candidate' else {'probing'}
    for line in stream:
        parts = line.rstrip('\n').split('\t')
        assert parts[0] == ('tactix' if arm == 'candidate' else 'kissat')
        row = dict(p.split('=', 1) for p in parts[1:])
        assert len(row) == len(parts) - 1 and set(row) == fields
        assert row['conflicts'] == '271079' and row['external'] == '5409'
        assert int(row['decisions']) >= 0 and row['source']
        assert row['before'] in ('0', '1') and row['after'] == '1'
        assert row.get('retired', row.get('probing')) in ('0', '1')
        assert row['value'] in ('-1', '0', '1')
        if arm == 'candidate':
            text = row['clause']
            assert not text or text.endswith(',')
            lits = [int(x) for x in text[:-1].split(',')] if text else []
            assert all(lits) and len(set(map(abs, lits))) == len(lits)
            row['clause'] = lits
        rows.append(row)
    assert rows
    return rows

def insertions(rows, arm):
    stage = 'update-reinsert' if arm == 'candidate' else 'push'
    selected = [r for r in rows if r['stage'] == stage and r['trigger'] == r['subject']
                and r['root'] == '331390' and r['subject'] in ('5409', '5415')]
    assert len(selected) == 2 and {r['subject'] for r in selected} == {'5409', '5415'}
    assert all(int(r['pos']) >= 0 for r in selected)
    assert int(selected[1]['size']) == int(selected[0]['size']) + 1
    return selected

def fixtures():
    base['fixtures']()
    good = 'tactix\tconflicts=271079\tdecisions=12\texternal=5409\tsource=delete\tbefore=1\tafter=1\tvalue=0\tretired=0\tclause=-5409,5415,\n'
    assert producers(io.StringIO(good), 'candidate')[0]['clause'] == [-5409, 5415]
    for old, new in [('271079', '271078'), ('external=5409', 'external=5415'),
                     ('before=1', 'before=2'), ('after=1', 'after=0'),
                     ('-5409,5415,', '-5409,5409,'), ('5415,\n', '5415\n'),
                     ('tactix', 'kissat'), ('retired=0', 'retired=2')]:
        try:
            producers(io.StringIO(good.replace(old, new)), 'candidate')
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('Malformed producer accepted')
    native = good.replace('tactix', 'kissat').replace('retired=0\tclause=-5409,5415,', 'probing=1')
    assert len(producers(io.StringIO(native), 'kissat')) == 1
    rows = [dict(stage='push', trigger=s, subject=s, root='331390', pos='4', size=str(9003+i))
            for i, s in enumerate(('5415', '5409'))]
    assert insertions(rows, 'kissat')[0]['subject'] == '5415'
    for bad in (rows[:1], rows + rows[:1], [dict(rows[0], stage='update'), rows[1]],
                [dict(rows[0], pos='-1'), rows[1]], [rows[0], dict(rows[1], size='9005')]):
        try:
            insertions(bad, 'kissat')
        except AssertionError:
            pass
        else:
            raise AssertionError('Incomplete/ambiguous insertion sequence accepted')
    print('PASS: producer schema/window controls and completed-push ordering controls', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    target = C / 'heap-validation.json'
    assert not target.exists()
    out = base['main']()
    if out is None:
        return
    orders, marks = {}, {}
    for arm in ('candidate', 'kissat'):
        with (C / 'traces' / (arm + '.bve-heap.tsv')).open() as stream:
            orders[arm] = insertions(base['parse'](stream, arm), arm)
        with (C / 'traces' / (arm + '.retry-producer.tsv')).open() as stream:
            marks[arm] = producers(stream, arm)
    clause = {-246596, -5409, -47, 49, 5415, 148944, 238853}
    deleted = [r for r in marks['candidate'] if set(r['clause']) == clause]
    assert len(deleted) == 1, deleted
    # Assert the proposed witness only after full replay and schema checks.
    assert [r['subject'] for r in orders['candidate']] == ['5409', '5415']
    assert [r['subject'] for r in orders['kissat']] == ['5415', '5409']
    a, b = orders['candidate'][0], orders['kissat'][1]
    assert all(a[k] == b[k] for k in ('score', 'positive', 'negative', 'root'))
    assert int(b['size']) == int(a['size']) + 1
    out.update(completed_insertions=orders, tactix_clause_producer=deleted[0],
               producer_rows={a: len(r) for a, r in marks.items()},
               interpretation='Actual post-push order is reversed for5409/5415. Tactix producer records actual physical clause order. Native deletion pose and the origin of incoming pose difference are not yet directly traced; no implementation fix or runtime gain claimed.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k: out[k] for k in ('completed_insertions', 'tactix_clause_producer', 'interpretation')}, indent=2))

if __name__ == '__main__':
    main()

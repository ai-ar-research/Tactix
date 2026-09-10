#!/usr/bin/env python3
"""Validate complete selected history and locate the post-attempt retry mark."""
import importlib.util
import io
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
reader = Path(__file__).with_name('validate-rank32-bve-candidates-6192.py')
spec = importlib.util.spec_from_file_location('bve_history_reader', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)

def producers(content, solver, external):
    assert content and content.endswith('\n')
    keys = {'conflicts', 'decisions', 'external', 'source', 'before', 'after', 'value'}
    keys |= {'retired', 'clause'} if solver == 'tactix' else {'probing'}
    rows = []
    for line in content.splitlines():
        fields = line.split('\t')
        assert fields[0] == solver
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(row) == len(fields)-1 and set(row) == keys
        row = {k: v if k in ('source', 'clause') else int(v) for k, v in row.items()}
        assert row['external'] == external and row['source']
        assert row['value'] in (-1, 0, 1)
        for key in keys - {'source', 'clause', 'value', 'external'}:
            assert row[key] >= 0
        for key in keys & {'before', 'after', 'retired', 'probing'}:
            assert row[key] in (0, 1)
        if 'clause' in row and row['clause']:
            assert row['clause'].endswith(',')
            assert all(int(lit) != 0 for lit in row['clause'][:-1].split(','))
        if rows:
            assert row['conflicts'] >= rows[-1]['conflicts']
            assert row['decisions'] >= rows[-1]['decisions']
        rows.append(row)
    return rows

def fixtures():
    good = 'tactix\tconflicts=1\tdecisions=2\texternal=9\tsource=delete-clause\tbefore=0\tafter=1\tvalue=0\tretired=0\tclause=9,-2,\n'
    assert len(producers(good, 'tactix', 9)) == 1
    bad = [good[:-1], good.replace('tactix', 'kissat'),
           good.replace('external=9', 'external=8'),
           good.replace('before=0', 'before=2'), good.replace('value=0', 'value=2'),
           good.replace('source=delete-clause', 'source='),
           good.replace('clause=9,-2,', 'clause=0,'),
           good.replace('clause=9,-2,', 'clause=9,-2'),
           good.replace('retired=0', 'before=0'),
           good + good.replace('decisions=2', 'decisions=1')]
    for value in bad:
        try: producers(value, 'tactix', 9)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid producer journal accepted')
    h.fixtures()
    print('producer framing/schema/order positive +10 negative fixtures PASS')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260910-7024-rank35-retry-producer'
    target = c / 'producer-localization-7032.json'
    assert not target.exists()
    authority = c / 'replay-summary-7026.json'
    s = json.loads(authority.read_text())
    assert s['exact_whole_parent_schedule_trace_and_partial_proof_replay']
    hashes = dict(s['hashes'])
    for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
    for p in (authority, Path(__file__), reader): hashes[str(p.relative_to(root))] = sha(p)
    groups, marks = {}, {}
    for arm, solver in [('candidate_schedule', 'tactix'), ('kissat_schedule', 'kissat')]:
        history = (c / 'traces' / (arm + '.bve-history.tsv')).read_text().splitlines(True)
        rows = []
        # Preserve every original row; partition only contiguous conflict groups
        # to use the frozen single-conflict reader unchanged.
        for conflict, lines in itertools.groupby(history[1:],
                key=lambda line: int(dict(f.split('=', 1) for f in line.strip().split('\t')[1:])['conflicts'])):
            rows.extend(h.candidates(io.StringIO(history[0] + ''.join(lines)), solver, conflict))
        assert all(r['external'] == s['selected_external'] for r in rows)
        groups[solver] = h.attempts(rows)
        marks[solver] = producers((c / 'traces' / (arm + '.retry-producer.tsv')).read_text(), solver, s['selected_external'])
    assert len(groups['tactix']) == len(groups['kissat']) == 15
    fields = ('external', 'conflicts', 'ticks', 'limit', 'applied', 'value')
    for a, b in zip(groups['tactix'], groups['kissat']):
        for stage in ('begin', 'end'):
            assert all(a[stage][key] == b[stage][key] for key in fields)
        assert a['end']['retry'] == b['end']['eliminate'] == 0
        assert a['end']['applied'] == 0
        for key in ('roots', 'trail', 'propagate', 'value'):
            assert a['begin'][key] == a['end'][key]
    last = groups['tactix'][-1]['end']['conflicts']
    assert last == 271520
    late = {arm: [r for r in rows if r['conflicts'] > last] for arm, rows in marks.items()}
    assert not late['kissat'] and len(late['tactix']) == 1
    mark = late['tactix'][0]
    assert (mark['conflicts'], mark['decisions'], mark['before'], mark['after']) == (276460, 73339360, 0, 1)
    assert mark['source'] == 'delete-clause'
    macro = (c / 'traces/candidate_schedule.macro.tsv').read_text().splitlines()
    selected = [line for line in macro if not line.startswith('#') and line.split('\t')[3] == str(mark['conflicts'])]
    assert [line.split('\t')[2] for line in selected] == ['reduce_begin', 'reduce_end']
    assert all(int(line.split('\t')[4]) == mark['decisions'] for line in selected)
    out = dict(hashes=hashes, complete_prior_attempts=15, last_common_attempt=last,
        late_marks=late, reduce_boundary_rows=selected,
        limitation='One representative extra member, not all21. Trace locates deletion within Reduce; source inspection identifies sparse satisfied-large retirement as the repair hypothesis. Runtime benefit and complete causal closure remain unproven.')
    with target.open('x') as stream:
        json.dump(out, stream, indent=2); stream.write('\n')
    print(json.dumps({k: v for k, v in out.items() if k != 'hashes'}, indent=2))

if __name__ == '__main__': main()

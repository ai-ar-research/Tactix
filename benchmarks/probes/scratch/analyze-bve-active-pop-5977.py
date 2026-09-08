#!/usr/bin/env python3
"""Compare all repaired attempts, keeping inactivity and absolute effort visible."""
import importlib.util
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
source = Path(__file__).with_name('analyze-first-bve-attempts-5943.py')
spec = importlib.util.spec_from_file_location('attempt_parser', source)
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)
sha = parser.sha
c = root / 'benchmarks/runs/20260908-5969-bve-active-pop-rank32-prefix'
old = root / 'benchmarks/runs/20260908-5937-first-bve-frontier'
common = {'external', 'stage', 'conflicts', 'ticks', 'limit', 'applied', 'value', 'trail', 'propagate'}
extra = {'candidate': {'retry', 'retired', 'roots'},
         'kissat': {'eliminate', 'active', 'fixed', 'eliminated', 'units'}}

def parse(stream, arm):
    assert next(stream).strip() == '# bve-candidate-observer\t1'
    rows = []
    for line in stream:
        cells = line.rstrip().split('\t')
        assert cells[0] == ('tactix' if arm == 'candidate' else 'kissat')
        row = dict(x.split('=', 1) for x in cells[1:])
        assert len(row) == len(cells)-1 and set(row) == common | extra[arm]
        assert row['conflicts'] == '271079' and int(row['external']) > 0
        for key, value in row.items():
            if key in ('stage', 'value'): continue
            assert int(value) >= 0
        assert row['value'] in ('-1', '0', '1')
        for key in ({'applied', 'retry', 'retired'} if arm == 'candidate' else
                    {'applied', 'eliminate', 'active', 'fixed', 'eliminated'}):
            assert row[key] in ('0', '1')
        rows.append(row)
    return parser.attempts(rows, arm)

def fixtures():
    parser.fixtures()
    row = 'tactix\texternal=7\tstage=begin\tconflicts=271079\tticks=3\tlimit=9\tapplied=0\tretry=1\tvalue=0\tretired=0\troots=4\ttrail=4\tpropagate=4\n'
    good = '# bve-candidate-observer\t1\n' + row + row.replace('stage=begin', 'stage=end')
    assert len(parse(io.StringIO(good), 'candidate')) == 1
    for before, after in [('external=7', 'external=0'), ('retired=0', 'retired=2'),
                          ('ticks=3', 'ticks=-1'), ('roots=4', 'roots=4\troots=4'),
                          ('value=0', 'value=2'), ('conflicts=271079', 'conflicts=1')]:
        try: parse(io.StringIO(good.replace(before, after)), 'candidate')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('bad candidate journal accepted')
    print('PASS: strict journal schema and six additional negative fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = c / 'active-pop-validation.json'
    assert not target.exists()
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    current_path = c / 'results/candidate.json'
    current = json.loads(current_path.read_text())
    assert current['exit_code'] == 0 and current['scratch_deleted']
    assert current['binary_sha256'] == sha(c / 'pins/candidate')
    summary = json.loads((old / 'summary.json').read_text())
    data, bindings = {}, {str(current_path.relative_to(root)): sha(current_path)}
    for name, campaign, arm, digests in [
        ('new', c, 'candidate', current['trace_sha256']),
        ('old', old, 'candidate', summary['evidence']['candidate']['trace_sha256']),
        ('native', old, 'kissat', summary['evidence']['kissat']['trace_sha256'])]:
        p = campaign / 'traces' / (arm + '.bve-candidates.tsv')
        assert sha(p) == digests[p.name]
        bindings[str(p.relative_to(root))] = sha(p)
        with p.open() as stream: data[name] = parse(stream, arm)
    inactive = lambda groups: [dict(index=i, rows=g) for i,g in enumerate(groups)
                              if g[0]['retired'] == '1' or g[0]['value'] != '0']
    old_inactive, new_inactive = inactive(data['old']), inactive(data['new'])
    assert len(old_inactive) == 9
    assert all(g[0]['active'] == '1' and g[0]['value'] == '0' for g in data['native'])
    parent_path = root / m['retained_parent']['record']
    assert sha(parent_path) == m['retained_parent']['sha256']
    parent = json.loads(parent_path.read_text())
    ordinary = {name: current['trace_sha256'].get(name) == digest for name,digest in parent['trace_sha256'].items()}
    active_old = [g for g in data['old'] if g[0]['retired'] == '0' and g[0]['value'] == '0']
    out = dict(attempt_counts={a: len(gs) for a,gs in data.items()},
        old_inactive_count=len(old_inactive), new_inactive_attempts=new_inactive,
        first_native_difference=parser.first_difference(data['new'], data['native']),
        first_parent_active_difference=parser.first_difference(data['new'], active_old),
        ordinary_parent_trace_equality=ordinary,
        parent_partial_proof_equal=current['partial_certificate'] == parent['partial_certificate'],
        compared_fields=parser.FIELDS, hashes=bindings,
        limitation='All attempts are compared with absolute effort. Parent active projection is labelled separately, never a substitute for native comparison or full runtime/correctness gates.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k: v for k,v in out.items() if k != 'hashes'}, indent=2))
    assert not new_inactive, 'Inactive pop repair did not close its motivating case'

if __name__ == '__main__': main()

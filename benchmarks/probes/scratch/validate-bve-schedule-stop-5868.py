#!/usr/bin/env python3
"""Classify intended observer exits and explain every initial score residual."""
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from decision_prefix import first_stream_difference
c = root / 'benchmarks/runs/20260908-5862-rank31-bve-frontier'
target = c / 'schedule-diagnosis.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
evidence, schedules = {}, {}
for arm in ['candidate', 'kissat']:
    r = json.loads((c / f'results/{arm}.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert not (c / f'traces/{arm}.prefix.tsv').exists()
    for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
    binding = m['cached_controls'][arm]
    parent = root / binding['record']
    assert sha(parent) == binding['sha256']
    parent_record = json.loads(parent.read_text())
    stream = parent.parent.parent / f'traces/{arm}.decisions.tsv'
    assert sha(stream) == parent_record['trace_sha256'][stream.name]
    prefix = first_stream_difference(c / f'traces/{arm}.decisions.tsv', stream)
    assert prefix['left'] is None and prefix['right'][0] == 179823
    lines = (c / f'traces/{arm}.bve-schedule.tsv').read_text().splitlines()
    assert lines[0] == '# bve-schedule-observer\t1'
    head = dict(x.split('=', 1) for x in lines[1].split('\t')[1].split('|'))
    rows = [dict(x.split('=', 1) for x in line.split('\t')[1:]) for line in lines[2:]]
    assert len(rows) == int(head['scheduled']) == 109255
    assert [int(row['position']) for row in rows] == list(range(len(rows)))
    assert len({row['external'] for row in rows}) == len(rows)
    schedules[arm] = rows
    evidence[arm] = dict(result_sha256=sha(c / f'results/{arm}.json'), prefix=prefix, header=head)
differences = []
residual = Counter()
for a, b in zip(schedules['candidate'], schedules['kissat']):
    assert a['external'] == b['external'] and a['position'] == b['position']
    if a == b: continue
    differences.append(dict(candidate=a, native=b))
    v = int(a['external'])
    for sign, field in [(1, 'positive'), (-1, 'negative')]:
        n = int(a[field]) - int(b[field])
        assert n >= 0
        if n: residual[sign * v] += n
assert len(differences) == 6 and sum(residual.values()) == 9
old = root / 'benchmarks/runs/20260908-5815-rank31-forward-residual-formula'
old_summary = json.loads((old / 'summary.json').read_text())
path = root / 'benchmarks/probes/scratch/compare-rank32-formula-5785.py'
spec = importlib.util.spec_from_file_location('formula', path)
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)
parser.fixtures()
selected = {}
for arm in ['candidate', 'kissat']:
    path = old / f'traces/{arm}.formula.tsv'
    assert sha(path) == old_summary['evidence'][arm]['trace_sha256'][path.name]
    clauses, occurrences = [], Counter()
    for row in parser.read_rows(path):
        if row[0][0] or not any(value > 0 for lit, value in row[3]): continue
        free = [lit for lit, value in row[3] if value == 0]
        if not any(lit in residual for lit in free): continue
        clauses.append(list(row[3]))
        occurrences.update(free)
    assert occurrences == residual and len(clauses) == 3
    selected[arm] = clauses
assert selected['candidate'] == selected['kissat']
target.write_text(json.dumps(dict(evidence=evidence, score_differences=differences,
    excess_occurrences=dict(residual), explaining_satisfied_clauses=selected['candidate'],
    classification='Both solvers intentionally exit0 in standalone BVE schedule observer. Runner incorrectly required300K prefix,so array failed. No crash,timeout,rejected proof or unexplained solver exit;owned scratch cleaned. Prior progress incorrectly called the job running after it had failed.',
    conclusion='All109255 members/positions match;all six initial score differences are explained by nine stale free-literal occurrences of three root-satisfied clauses. Native deletes before connecting;5849 connects then deletes,leaving other stacks stale.',
    limitation='Captured decisions match each cached full stream up to the snapshot. No300K observer-neutral replay or partial-proof-prefix check is claimed;full partial proofs were already deleted. Old physical snapshots explain the exact residual,not full formula identity.'), indent=2) + '\n')
print('PASS: explained observer stop;109255 positions equal;six score residuals = nine stale occurrences from three clauses')

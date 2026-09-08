#!/usr/bin/env python3
"""Classify Rank31's residual using complete formula and binary snapshots."""
from collections import Counter
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
def module(name, filename):
    spec = importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
formula = module('formula_parser','compare-rank32-formula-5785.py')
binary = module('binary_parser','compare-rank32-forward-binaries-5795.py')
formula.fixtures(); binary.fixtures()
c = root / 'benchmarks/runs/20260908-5815-rank31-forward-residual-formula'
target = c / 'summary.json'
assert not target.exists()
def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
local = json.loads((root / m['localization_authority']['path']).read_text())
sets,binaries,evidence,count_checks = {},{},{},{}
for rel,digest in m['hashes'].items(): assert sha(c / rel) == digest
def rows(arm):
    with (c / 'traces' / (arm + '.formula.tsv')).open() as stream:
        assert next(stream).rstrip() in ('# tactix-formula-set\t2','# kissat-formula-set\t2')
        assert next(stream).rstrip() == '# redundant\tsize\thash\tliterals'
        for line in stream: yield formula.parse_line(line)
for arm in ('candidate','kissat'):
    p = c / 'results' / (arm + '.json')
    r = json.loads(p.read_text())
    binding = m['cached_controls'][arm]
    old_path = root / binding['record']
    assert sha(old_path) == binding['sha256']
    old = json.loads(old_path.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == old['partial_certificate']
    for rel,digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
    for rel,digest in old['trace_sha256'].items(): assert r['trace_sha256'][rel] == digest
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm+'.formula.tsv',arm+'.binaries.tsv'}
    sets[arm] = Counter(row[0] for row in rows(arm))
    boundaries = [row for row in local['target_boundaries']['repaired' if arm == 'candidate' else arm] if row['name'] == 'strengthen']
    count = Counter()
    for key,n in sets[arm].items(): count[key[0]] += n
    expected_count = {0:int(boundaries[1]['irred']),1:int(boundaries[1]['red'])}
    count_checks[arm] = dict(physical=dict(count),recorded=expected_count,matches=count == expected_count)
    # A diagnostic discrepancy is evidence,not grounds for changing the solver
    # or silently pretending its counters bound physical snapshot length.
    if count != expected_count:
        print('COUNTER/SNAPSHOT DISCREPANCY',arm,count_checks[arm],flush=True)
    with (c / 'traces' / (arm+'.binaries.tsv')).open() as stream:
        binaries[arm] = binary.parse(stream)
    assert len(binaries[arm]) == len(boundaries) == 4
    for snap,boundary in zip(binaries[arm],boundaries):
        assert (snap['stage'],snap['conflict'],snap['count']) == (boundary['stage'],int(boundary['conflicts']),int(boundary['binary']))
    evidence[arm] = dict(result_sha256=sha(p),trace_sha256=r['trace_sha256'])
pairs = []
for a,b in zip(binaries['candidate'],binaries['kissat']):
    ac,bc = Counter(a['pairs']),Counter(b['pairs'])
    pairs.append(dict(index=a['index'],stage=a['stage'],equal=ac == bc,
        candidate_only=sum((ac-bc).values()),kissat_only=sum((bc-ac).values())))
common = set(binaries['candidate'][1]['pairs']) & set(binaries['kissat'][1]['pairs'])
extras = {}
for arm,other in [('candidate','kissat'),('kissat','candidate')]:
    delta = sets[arm] - sets[other]
    witnesses = []
    for row in rows(arm):
        if row[0] not in delta: continue
        witnesses.append(dict(row=row,copies=delta[row[0]],
            common_binary_subsumers=[pair for pair in itertools.combinations(row[0][1],2) if pair in common],
            root_satisfied=any(value > 0 for _,value in row[3])))
    extras[arm] = dict(copies=sum(delta.values()),witnesses=witnesses)
out = dict(observer_parent_trace_and_proof_equal=True,evidence=evidence,
    first_exit_formula_extras=extras,binary_snapshots=pairs,formula_count_checks=count_checks,
    limitation='Complete literal multisets and physical binary copies. Metadata/root witnesses identify candidate explanations,not an accepted causal repair. No runtime baseline refresh.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k != 'evidence'},indent=2))

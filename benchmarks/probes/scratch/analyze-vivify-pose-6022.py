#!/usr/bin/env python3
"""Compare candidate order and effort within scopes after full replay passes."""
import hashlib
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6020-vivify-pose'
sp = importlib.util.spec_from_file_location('pose', Path(__file__).with_name('validate-vivify-pose-6019.py'))
mod = importlib.util.module_from_spec(sp); sp.loader.exec_module(mod)
target = c / 'candidate-analysis.json'
assert not target.exists()
summary = json.loads((c / 'summary.json').read_text()); assert summary['full_parent_replay']
rounds = {}; mutations = {}; hashes = {}
for arm in ('candidate', 'kissat'):
    p = c / 'traces' / (arm + '.vivify-pose.tsv')
    digest = mod.sha(p); assert digest == summary['evidence'][arm]['trace_sha256'][p.name]
    hashes[p.name] = digest
    with p.open() as stream: rows, pending = mod.parse(stream, arm=arm)
    assert pending is None
    rounds[arm] = {}; mutations[arm] = []
    for row in rows:
        key = (row['portable_scope'], row['scope_occurrence'])
        group = rounds[arm].setdefault(key, dict(pairs=[], budget_stop=None))
        if row['event'] == 'vivify_budget_stop': group['budget_stop'] = row; continue
        if row['event'] == 'vivify_candidate':
            group['pairs'].append(dict(begin=row)); continue
        pair = group['pairs'][-1]; pair['end'] = row
        pair['tick_delta'] = row['ticks'] - pair['begin']['ticks']
        if any(pair['begin'][name] != row[name] for name in ('pose', 'searched')):
            mutations[arm].append(pair)
    assert all('end' in pair for group in rounds[arm].values() for pair in group['pairs'])
comparisons = []
for key in sorted(set(rounds['candidate']) | set(rounds['kissat'])):
    groups = {arm: rounds[arm].get(key) for arm in rounds}
    record = dict(scope=key[0], occurrence=key[1], arms={})
    for arm, group in groups.items():
        if group is None: record['arms'][arm] = None; continue
        pairs = group['pairs']; first = pairs[0]['begin'] if pairs else group['budget_stop']
        last = pairs[-1]['end'] if pairs else first
        record['arms'][arm] = dict(tried=len(pairs), first_ticks=first['ticks'], limit=first['limit'],
            remaining_budget_at_first=first['limit'] - first['ticks'], end_ticks=last['ticks'],
            candidate_ticks=sum(p['tick_delta'] for p in pairs), budget_stop=group['budget_stop'])
    if all(groups.values()):
        a, b = groups['candidate']['pairs'], groups['kissat']['pairs']
        common = 0; first_delta = None
        for x, y in zip(a, b):
            if sorted(x['begin']['candidate']) != sorted(y['begin']['candidate']): break
            if first_delta is None and x['tick_delta'] != y['tick_delta']:
                first_delta = dict(candidate=x, kissat=y)
            common += 1
        record['common_candidate_identity_prefix'] = common
        record['first_tick_delta_difference_within_common_prefix'] = first_delta
        record['first_unmatched_candidate'] = {arm: pairs[common] if common < len(pairs) else None
            for arm, pairs in [('candidate', a), ('kissat', b)]}
    comparisons.append(record)
out = dict(summary_sha256=mod.sha(c / 'summary.json'), trace_sha256=hashes,
    scope_comparisons=comparisons, within_candidate_pose_mutations=mutations,
    limitations='Scope occurrence alignment is conditional on scope order and candidate identity; a differing candidate stops causal row alignment. Tick deltas can differ due to propagation order and do not alone prove budget errors. This is diagnostic, not a runtime benchmark.')
target.write_text(json.dumps(out, indent=2) + '\n')
for record in comparisons:
    print(record['scope'], record['occurrence'],
          {arm: None if item is None else {k: item[k] for k in ('tried', 'remaining_budget_at_first', 'candidate_ticks')}
           for arm, item in record['arms'].items()},
          'common', record.get('common_candidate_identity_prefix'))
print('within-candidate pose mutations', {arm: len(pairs) for arm, pairs in mutations.items()})

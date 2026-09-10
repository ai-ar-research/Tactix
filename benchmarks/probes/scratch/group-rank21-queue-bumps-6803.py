#!/usr/bin/env python3
"""Seal exact-clock bump membership localization from validated6800 rows."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from bump_groups import compare_groups
c = root/'benchmarks/runs/20260909-6798-rank21-queue-bumps'
target = c/'groups-6803.json'
assert not target.exists()
accepted = c/'summary-6800.json'
s = json.loads(accepted.read_text())
assert s['observer_replay_validated']
for rel, digest in s['hashes'].items(): assert sha(root/rel) == digest, rel
groups = compare_groups(s['all_bump_rows']['candidate'], s['all_bump_rows']['kissat'])
assert groups['group_counts'] == [1187, 1187]
diffs = groups['differences']
assert [d['clock'][0] for d in diffs] == [23987,23988,24045,24060,25095,25096]
for d in diffs[:4]:
    assert d['candidate_only'] == [402386] and d['reference_only'] == [402382]
for d in diffs[4:]:
    assert d['candidate_only'] == []
    assert set(d['reference_only']) == {402381,402380,402433,402378,402428}
hashes = {str(p.relative_to(root)):sha(p) for p in (accepted,Path(__file__),
    root/'tools/bench/bump_groups.py',root/'tools/bench/test_bump_groups.py')}
out = dict(hashes=hashes, **groups,
    raw_first_links_difference=s['first_links_values_difference'],
    selected_moves=s['selected_moves'],
    conclusion='Only six of1187 exact-clock request groups differ. Four early groups substitute402386 for402382; two late groups omit the same five variables, including402380, in Tactix. Membership differs, not merely ordering within an equal set.',
    next_action='Use unchanged pins and existing CONTROL_ONLY diagnostics in separate four-conflict windows23986..23989 and25094..25097. Compare conflict, resolution marks/reasons, learned clauses, analyzed admission and reason-side gate data, with complete6798 replay mandatory before interpretation.',
    limitation='Earlier raw next-link mismatch at23973 is retained, not normalized away or declared causal. Equal requests do not prove equal queue state. Late omissions explain missing restamps, but analysis/reason provenance and worse-work causality are still open. No solver fix or speedup.',
    test_consideration='Six new grouping tests cover exact clocks, missing groups, duplicate multiplicity, order-only mismatch, monotonicity and raw-state preservation; existing queue/state parsers stay frozen.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(out['conclusion'])

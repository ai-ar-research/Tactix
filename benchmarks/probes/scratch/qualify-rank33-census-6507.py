#!/usr/bin/env python3
"""Qualify raw6504 stage labels using source order and normalized candidate pairs."""
from collections import Counter
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_trace import read_binaries,read_candidates
c=root/'benchmarks/runs/20260909-6501-rank33-congruence-replay'
target=c/'stage-qualification-6507.json'; assert not target.exists()
p=c/'summary.json'; s=json.loads(p.read_text()); assert s['parent_replay']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
source=root/'.claude/worktrees/runtime-activation-stamp-6424/src/rules/congruence/congruence.cpp'
text=source.read_text(); body=text[text.index('  congruence_binary_trace(k, "entry");'):]
assert body.index('congruence_binary_trace(k, "post-binary-extraction")') < body.index('install_congruence_binary_candidates(k, binary_candidates') < body.index('extract_gates(k, gates,')
tables={}; binary={}
for arm,solver in (('candidate','tactix'),('kissat','kissat')):
    with (c/'traces'/f'{arm}.congruence-candidates.tsv').open() as inp: tables[arm]=read_candidates(inp,solver,227617)
    with (c/'traces'/f'{arm}.congruence-binaries.tsv').open() as inp: binary[arm]=read_binaries(inp,solver,227617)
def normalized(row):
    return dict(row,pair=None if row['pair'] is None else tuple(sorted(row['pair'])))
normalized_differences=[]
for call in range(2):
    first=None
    for index,(a,b) in enumerate(itertools.zip_longest(tables['candidate'][call]['rows'],tables['kissat'][call]['rows'])):
        if (None if a is None else normalized(a))!=(None if b is None else normalized(b)):
            first=dict(index=index,candidate=a,kissat=b);break
    normalized_differences.append(first)
assert normalized_differences[0] is None
t_pre=[r[:2] for r in binary['candidate'][1]['rows']]
k_entry=[r[:2] for r in binary['kissat'][1]['rows']]
assert Counter(t_pre)==Counter(k_entry)
derived=[tuple(sorted(r['pair'])) for r in tables['candidate'][0]['rows'] if r['disposition']=='extract']
assert len(derived)==85 and len(set(derived))==84
predicted=Counter(t_pre)
for pair in derived:
    if not predicted[pair]: predicted[pair]=1
native_post=Counter(r[:2] for r in binary['kissat'][2]['rows'])
assert predicted==native_post
result=dict(parent_summary_sha256=sha(p),source_sha256={str(source.relative_to(root)):sha(source)},
    first_call_candidate_rows=108890,normalized_candidate_first_differences=normalized_differences,
    clean_entry_multiset_equal=True,clean_entry_count=len(t_pre),candidate_derivations=85,unique_derivations=84,
    predicted_installed_count=sum(predicted.values()),predicted_installed_multiset_equals_native=True,
    correction='Raw6504 post-binary-extraction labels are not equivalent observation points: Tactix is pre-install and native post-install. Preserve6504 counts, but supersede its suggestion that those sites align. Tactix pre-install and native dense entry match exactly. All108890 first-call candidate records match after unordered binary-pair normalization;84 distinct planned pairs reconstruct native127503 post-install multiset.',
    limitation='Reconstructed Tactix post-install population is a source-backed prediction, not observed solver state. No missing-inference claim or repair follows. Capture actual post-install and post-gate populations with a neutral observer before causal attribution; full large-clause identity and ordered closure remain unproved.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

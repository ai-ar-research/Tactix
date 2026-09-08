#!/usr/bin/env python3
"""Freeze causal conclusions and the complete BC solver view, not acceptance."""
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
target = root / 'benchmarks/reports/2026-09-08-forward-caller-checkpoint.json'
assert not target.exists()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
c31 = runs / '20260908-5856-forward-caller-rank31-prefix'
c32 = runs / '20260908-5857-forward-caller-rank32-prefix'
s31 = json.loads((c31 / 'summary.json').read_text())
s32 = json.loads((c32 / 'summary.json').read_text())
assert s31['comparisons']['parent']['decisions']['complete_equal']
assert s31['comparisons']['parent']['decisions']['matching_rows'] == 96534506
assert s31['comparisons']['kissat']['first_forward_bve_difference']['ordinal'] == 35
assert s32['matching_decisions'] == 61154590
parent32 = runs / '20260908-5837-forward-admission-rank32-prefix/results/candidate.json'
r32 = json.loads((c32 / 'results/candidate.json').read_text())
p32 = json.loads(parent32.read_text())
assert r32['trace_sha256']['candidate.decisions.tsv'] == p32['trace_sha256']['candidate.decisions.tsv']
assert r32['partial_certificate'] == p32['partial_certificate']
bc = runs / '20260908-5855-forward-caller-BC-gate'
bm = json.loads((bc / 'manifest.json').read_text())
for rel, digest in bm['pin_sha256'].items(): assert sha(bc / rel) == digest
groups = defaultdict(list)
hashes = {}
for p in (bc / 'provisional').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['expected_match'] and not r['solve']['timed_out'] and r['solve']['exit_code'] in (10, 20)
    groups[r['arm']].append(r)
    hashes[str(p.relative_to(root))] = sha(p)
assert {k: len(v) for k, v in groups.items()} == {'candidate_B': 28, 'current_B': 28, 'candidate_C': 5, 'current_C': 5}
totals = {k: sum(r['solve']['wall_seconds'] for r in rs) for k, rs in groups.items()}
finals = list((bc / 'results').glob('*.json'))
for p in finals:
    r = json.loads(p.read_text())
    assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    hashes[str(p.relative_to(root))] = sha(p)
for p in [c31 / 'summary.json', c32 / 'summary.json', c31 / 'results/candidate.json',
          c32 / 'results/candidate.json', parent32]:
    hashes[str(p.relative_to(root))] = sha(p)
view = dict(expected_solves=66, checked_clean=len(finals), totals=totals,
    ratios={tier: totals['candidate_' + tier] / totals['current_' + tier] for tier in ['B', 'C']},
    combined=sum(totals['candidate_' + tier] for tier in ['B', 'C']) / sum(totals['current_' + tier] for tier in ['B', 'C']))
target.write_text(json.dumps(dict(bc=view, hashes=hashes,
    rank31='Full96534506 decisions replay5830 through300K. Caller removes the three-clause forward-end residual;first native count difference is now BVE end. This repair did not change the observed decisions.',
    rank32='Full decisions and partial certificate bytes replay5830 through300K. First native split remains225987 after61154590 matching decisions.',
    next_action='Validate paired5862/job1674504 BVE schedule/candidate observer neutrality,then first changed lifecycle;score full5858 once all40 solves finish. No reference refresh.',
    limitation='BC single-window provisional screen,not full TierC/D or final promotion. Partial diagnostic certificates are provenance,not checked UNSAT evidence. Fixed34+6 actual-code proofs cover candidate behavior;current-source fuzz remains pending.'), indent=2) + '\n')
print(json.dumps(view, indent=2))

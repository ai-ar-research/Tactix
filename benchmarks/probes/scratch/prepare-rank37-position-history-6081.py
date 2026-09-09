#!/usr/bin/env python3
"""Observe existing macro-position history on the replay-validated heap probe."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6076-root-lock-rank37-choice'
c = root / 'benchmarks/runs/20260908-6081-rank37-position-history'
assert not c.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()
v = json.loads((parent / 'state-analysis.json').read_text())
assert v['change_counts'] == dict(candidate_only=0,kissat_only=0,score=0,assigned=0,active=0,position=254)
for rel,digest in v['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((parent / 'manifest.json').read_text())
for rel,digest in m['hashes'].items():
    assert sha(parent / rel) == digest, rel
runner = (parent / 'run.py').read_text().replace('tactix-6076-', 'tactix-6081-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env.update({prefix + '_INPROCESS_SCORE_TRACE': str(c / 'traces' / (arm + '.scores.tsv')),\n    'SAT_INPROCESS_SCORE_EXTERNAL': '116362', 'SAT_INPROCESS_SCORE_CONFLICT_FROM': '0'})\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
for name in ('pins','logs','traces','results'):
    (c / name).mkdir(parents=True)
for arm in ('candidate','kissat'):
    shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
    (c / 'pins' / arm).chmod(0o555)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent),str(c)).replace('6076-rank37-choice','6081-rank37-position'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(parent.relative_to(root)),
    parent_summary_sha256=sha(parent / 'choice-validation.json'), position_authority_sha256=sha(parent / 'state-analysis.json'),
    hypothesis='Equal complete membership/scores/assignment/active flags but254 positions differ at last-common272979. Trace116362 position fromstartup at existing common macro boundaries to bracket heap topology producer;do not change tie comparison.',
    validation='Both arms replay all6076 ordinary1M,heap/choice traces and partial proof bytes before interpreting score/position history. Retain unmatched boundary sites;common labels/counters/occurrence bracket but do not prove a cause.',
    native_rerun_reason='Existing native6076 lacks earlier macro-position history needed to bracket first ordering divergence. Diagnostic only,not fixed timing refresh.',
    test_consideration='No new solver code or observer. Reuse existing standalone macro score/position logger and15 strict parser fixtures plus6080 heap-difference fixtures. Require full replay;current-source fuzz remains separately unverified.',
    score_external=116362,score_compare_through=272979)
m['hashes']={name:sha(c / name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)

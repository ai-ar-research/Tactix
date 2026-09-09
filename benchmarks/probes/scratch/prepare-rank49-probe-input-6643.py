#!/usr/bin/env python3
"""Capture the actual population feeding the one-conflict Probe deadline gap."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6639-rank49-search-window'
c = root / 'benchmarks/runs/20260909-6643-rank49-probe-input'
assert not c.exists()
s = json.loads((old/'summary-6641.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6639-','tactix-6643-')
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor)==1
runner=runner.replace(anchor,"""
env.update(SAT_EVENT_TRACE_BINARY_SET_PATH=str(c/'traces'/(arm+'.binaries.tsv')),
           SAT_EVENT_TRACE_BINARY_SET_MATCH='probe_end', SAT_EVENT_TRACE_BINARY_SET_CONFLICT='2988',
           SAT_EVENT_TRACE_BINARY_SET_ONCE='1',
           SAT_EVENT_TRACE_FORMULA_SET_PATH=str(c/'traces'/(arm+'.formula.tsv')),
           SAT_EVENT_TRACE_FORMULA_SET_MATCH='probe_end', SAT_EVENT_TRACE_FORMULA_SET_CONFLICT='2988',
           SAT_EVENT_TRACE_FORMULA_SET_POSE='1', SAT_EVENT_TRACE_FORMULA_SET_META='1',
           SAT_EVENT_TRACE_FORMULA_SET_VALUES='1')
"""+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for arm in ('candidate','kissat'):
    shutil.copy2(old/'pins'/arm,c/'pins'/arm)
    p=old/f'results/{arm}.json';m['dependencies'][str(p.relative_to(root))]=sha(p)
    r=json.loads(p.read_text())
    for name,digest in r['trace_sha256'].items():m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
p=old/'summary-6641.json';m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6639-rank49-window','6643-probe-input'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),cached_controls=str(old.relative_to(root)),
    snapshot_boundary=dict(event='probe_end',conflicts=2988,decisions=15773),
    purpose='Six conflict/learned transactions agree through9273, then Tactix Probe runs one conflict earlier. First Probe exit has8031 large irred in both and342/338 binary copies: reference scale predicts deadlines9272/9273 exactly. Capture complete binaries and large rows to distinguish unique clauses from duplicate multiplicity before any repair.',
    validation='Require all6639 per-arm traces including search window and normalized-observer outputs plus partial-proof bytes/hash exactly. Validate whole snapshot schemas/declared counts and identities at2988; compare all multiplicities and root values. Do not infer duplicate semantics from counts alone.',
    test_consideration='No solver/observer edits. Reuse binary_snapshot and5785 full formula fixtures; search_window7 new tests cover observer additions/framing. Keep actual population accounting separate from a proved causal repair.',
    limitation='Missing early population observation, not native timing refresh. Existing global search trace retained for exact replay; no source change. Preserve strongerRank33,Rank32,compact-stamp,activation and SP-SLOT-5.')
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2);stream.write('\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)

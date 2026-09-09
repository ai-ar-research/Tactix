#!/usr/bin/env python3
"""Pin the last-common Rank37 heap and one variable's assignment history."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-6070-root-lock-rank37-prefix'
native = runs / '20260908-5478-rank37-bound-extended'
template = runs / '20260908-5917-reduce-root-next-choice'
c = runs / '20260908-6076-root-lock-rank37-choice'
assert not c.exists()

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

s = json.loads((old / 'summary.json').read_text())
assert s['matching_decisions'] == 77120545
assert s['suffix_comparison']['left'] == [272979, 76953349, 388, 116360, 0]
assert s['suffix_comparison']['right'] == [272979, 76953349, 388, 116362, 0]
m = json.loads((old / 'manifest.json').read_text())
tm = json.loads((template / 'manifest.json').read_text())
for rel, digest in tm['hashes'].items():
    assert sha(template / rel) == digest, rel
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
for name in ('pins', 'traces', 'logs', 'results'):
    (c / name).mkdir(parents=True)
for arm, origin in (('candidate', old), ('kissat', native)):
    r = json.loads((origin / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert sha(origin / 'pins' / arm) == r['binary_sha256']
    shutil.copy2(origin / 'pins' / arm, c / 'pins' / arm)
    (c / 'pins' / arm).chmod(0o555)
runner = (template / 'run.py').read_text().replace('tactix-5917-', 'tactix-6076-')
for a, b in (('271085', '272979'), ('67954349', '76953348'), ('29066', '116362')):
    assert a in runner
    runner = runner.replace(a, b)
anchor = "str(m.get('common_decisions', 1))"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "str(m['common_decisions'] if arm == 'candidate' else 1)")
assert "else 100000" in runner and "'_EVENT_TRACE'" in runner
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6070-root-lock-rank37', '6076-rank37-choice').replace('--array=0-0', '--array=0-1')
assert '--array=0-1' in batch and '--cpus-per-task=12' in batch and '--mem=48000M' in batch
(c / 'array.sbatch').write_text(batch)
binding = lambda p: dict(record=str(p.relative_to(root)), sha256=sha(p))
m['cached_controls'] = {arm: binding(origin / 'results' / (arm + '.json')) for arm, origin in (('candidate', old), ('kissat', native))}
m['parent_summary'] = dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json'))
m['resource_contract']['array'] = '0-1'
m['hypothesis'] = 'At first new272979/76953349 split,distinguish assignment/score/membership from physical heap order for116360 versus116362. Observe after last-common76953348,not after divergence.'
m['validation'] = 'Both arms must replay every cached ordinary1M trace and partial proof byte. Candidate stream0/native100K and per-arm common-prefix requests reproduce each cache exactly. Require full heap and nonempty116362 choice-state rows.'
m['native_rerun_reason'] = 'Cached native5478 lacks full last-common heap and116362 assignment/reason state at272979. Missing causal fields only;fixed runtime vector unchanged.'
m['test_consideration'] = 'Existing tested standalone heap/choice observers,unchanged binaries.22 benchmark tests and6073 boundary controls pass;exact ordinary/proof replay is the observer-neutrality gate. No solver change or new fuzz campaign required.'
m['limitation'] = 'No final checked proof,runtime claim,source promotion or field alignment by native sentinels. Normal full-run checks and cpu-q campaign continue unchanged.'
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)

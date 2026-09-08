#!/usr/bin/env python3
"""Freeze the tested read-only mark observer and replay the Rank32 witness."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5798-forward-unit'
observer = runs / '20260908-5817-forward-mark-observer'
parent = runs / '20260908-5771-reuse-target-rank32-extended'
c = runs / '20260908-5822-forward-mark-rank32-replay'
source = root / '.claude/worktrees/runtime-forward-mark-observer-5817'
binary = root / 'build/pin-5817-forward-mark-trace/tactix'

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

a = json.loads((authority / 'manifest.json').read_text())
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(parent / rel) == digest, rel
changed = []
for rel, digest in a['files'].items():
    assert sha(root / a['source'] / rel) == digest, rel
    if sha(source / rel) != digest:
        changed.append(rel)
assert changed == ['src/rules/strengthen/strengthen.cpp'], changed
fixed = json.loads((observer / 'fixed-control/summary.json').read_text())
assert fixed['observer_positive_negative_pass']
for rel, digest in fixed['hashes'].items():
    assert sha(root / rel) == digest, rel
assert '100% tests passed out of 92' in (observer / 'logs/ctest.log').read_text()
assert not (observer / 'manifest.json').exists() and not c.exists()
diff = subprocess.run(['diff', '-u', '--label', 'a/' + changed[0], '--label', 'b/' + changed[0],
                       root / a['source'] / changed[0], source / changed[0]],
                      capture_output=True, check=False)
assert diff.returncode == 1
(observer / 'observer.patch').write_bytes(diff.stdout)
bound = [observer / 'observer.patch', observer / 'fixed-control/summary.json',
         *sorted((observer / 'logs').glob('*.log')), binary, source / changed[0]]
om = dict(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
          parent_authority=str(authority.relative_to(root)),
          parent_manifest_sha256=sha(authority / 'manifest.json'),
          source=str(source.relative_to(root)),
          purpose='Read-only selected-variable stamps before/after canonical root notification;no rule or scheduling changes.',
          test_consideration='92/92 CTest;actual native/Tactix root-mark control;observer positive/negative selection and off/on output equality.',
          hashes={str(p.relative_to(root)): sha(p) for p in bound})
(observer / 'manifest.json').write_text(json.dumps(om, indent=2) + '\n')
c.mkdir()
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir()
shutil.copy2(binary, c / 'pins/candidate')
runner = (parent / 'run.py').read_text().replace('tactix-5771-', 'tactix-5822-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """env['TACTIX_FORWARD_MARK_TRACE'] = str(c / 'traces/candidate.marks.tsv')
env['SAT_FORWARD_MARK_CONFLICT'] = '179463'
env['TACTIX_EVENT_TRACE_FORWARD_CLAUSE'] = '-217936,-217929,-217919,-217911,-217844,-217832,217821,217824,217864,217939'
assert not (c / 'traces/candidate.marks.tsv').exists()
"""
(c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
assert '#SBATCH --array=0-1' in batch
batch = batch.replace('#SBATCH --array=0-1', '#SBATCH --array=0-0')
batch = '\n'.join('#SBATCH --job-name=5822-root-mark-rank32' if line.startswith('#SBATCH --job-name=') else line
                  for line in batch.split('\n'))
(c / 'array.sbatch').write_text(batch)
m.update(created_at=om['created_at'], parent_campaign=str(parent.relative_to(root)),
         candidate_authority=str(observer.relative_to(root)), candidate_source=om['source'],
         source_patch_sha256=sha(observer / 'observer.patch'),
         hypothesis='Does root notification supply the second admission mark in the real Rank32 forward witness? Fixed controls prove possibility,not provenance.',
         validation='All ordinary300K trace hashes and partial certificate must exactly match5771 candidate;then read selected before/after mark records. No final proof or timing claim.',
         contract='One candidate-only diagnostic task,12CPU/48000MiB,1200s solver/25min allocation;no throttle,node constraint,or Kissat rerun.',
         resource_contract=dict(m['resource_contract'], array='0-0'),
         test_consideration=om['test_consideration'],
         cached_controls={arm: dict(record=str((parent / ('results/' + arm + '.json')).relative_to(root)),
                                   sha256=sha(parent / ('results/' + arm + '.json'))) for arm in ('candidate', 'kissat')},
         observer_manifest_sha256=sha(observer / 'manifest.json'))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/candidate')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins/candidate', binary):
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

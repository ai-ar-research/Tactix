#!/usr/bin/env python3
"""Locate the largest adverse change without weakening its existing closure."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
old = root/'benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix'
c = root/'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
assert not c.exists()
om = json.loads((old/'manifest.json').read_text())
for rel,digest in om['hashes'].items():
    assert sha(old/rel) == digest
prior = root/'benchmarks/runs/20260907-5424-rank35-rephase-prefix'
pm = json.loads((prior/'manifest.json').read_text())
for rel,digest in pm['hashes'].items():
    assert sha(prior/rel) == digest
assert sha(root/pm['input']) == pm['input_sha256_compressed']
for folder in ('pins','logs','traces','results'):
    (c/folder).mkdir(parents=True)
authorities = {}
for arm, stem in (('candidate','6357-compact-stamp'),('previous','6305-substitute-collapse')):
    authority = root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'
    a = json.loads(authority.read_text())
    for rel,digest in a['files'].items():
        assert sha(root/a['source']/rel) == digest
    pin = root/f'build/pin-{stem}-trace/tactix'
    assert sha(pin) == a['binaries'][str(pin.relative_to(root))]
    shutil.copy2(pin, c/'pins'/arm)
    authorities[arm] = dict(path=str(authority.relative_to(root)), sha256=sha(authority))
runner = (old/'run.py').read_text().replace("('candidate', 'kissat')", "('candidate', 'previous')").replace('tactix-6365-', 'tactix-6376-')
# The old selected-producer sink has one fixed filename; this diagnostic needs
# neither its output nor a shared sink across two Tactix processes.
runner = '\n'.join(line for line in runner.splitlines() if not line.startswith((
    "env['TACTIX_RETRY_PRODUCER_TRACE']", "env['SAT_RETRY_PRODUCER_FROM']",
    "env['SAT_RETRY_PRODUCER_TO']", "env['SAT_RETRY_PRODUCER_EXTERNAL']")))+'\n'
assert 'RETRY_PRODUCER' not in runner
(c/'run.py').write_text(runner)
batch = (old/'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0\n','--array=0-1\n').replace('6365-compact-stamp-prefix','6376-rank35-stamp-prefix')
assert '--array=0-1\n' in batch
(c/'array.sbatch').write_text(batch)
selection = root/'benchmarks/reports/2026-09-09-compact-stamp-full50-6371/summary.json'
record = next(r for r in json.loads(selection.read_text())['rows'] if r['rank']==35)
assert record['delta_parent_seconds'] > 170
m = dict(rank=35, input=pm['input'], input_sha256_compressed=pm['input_sha256_compressed'],
    stop_conflict=100000, stream_from=0, common_decisions=18516218,
    prior_common_hash='a6eb1df4f4e0e591', authorities=authorities,
    resource_contract=dict(om['resource_contract'],array='0-1'),
    prior_closure=dict(path=str((prior/'summary.json').relative_to(root)),sha256=sha(prior/'summary.json')),
    selection=dict(path=str(selection.relative_to(root)),sha256=sha(selection)),
    purpose='Test whether6357 preserves the old Rank35 Rephase decision closure, then localize its earliest changed choice versus6305 through100K.',
    validation='Verify each complete prefix, trace framing/counts, independent pin hashes and scratch deletion. Compare all current/previous decision rows; report inherited13,833 closure separately. An inherited-prefix change is localization evidence, not an assumed solver failure.',
    test_consideration='Same frozen diagnostic runner with independent arm trace filenames; remove unused selected-producer sink. Existing decision-prefix/framing tests cover schema, overlap, missing rows and truncation. No new solver instrumentation.',
    limitation='Cached native used only as an earlier closure authority, not fresh timing or full-state proof. Both stopped partial proofs are hash-bound and deleted, not accepted final certificates.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

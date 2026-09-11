#!/usr/bin/env python3
"""Confirm current clause-arena allocation behavior with the proven observer."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260911-7310-clause-memory-rank31'
assert not c.exists(), 'preserve successful or failed preparation'
prior = root / 'benchmarks/runs/20260911-7299-proc-memory-rank31'
m = json.loads((prior / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
bind(prior / 'manifest.json')
bind(prior / 'summary.json')
bind(prior / 'resident-analysis-7300.json')
for name in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c / name).mkdir(parents=True)
for name in ('run.py', 'syscall_memory_task.py', 'runtime_task.py', 'modelcheck.py',
             'drat-trim', 'checkmodel', 'strace'):
    bind(prior / 'pins' / name, m['pin_sha256']['pins/' + name])
    shutil.copy2(prior / 'pins' / name, c / 'pins' / name)
shutil.copy2(prior / 'matrix.tsv', c / 'matrix.tsv')
runtime = root / 'benchmarks/runs/20260911-7306-clause-realloc-runtime'
rm = json.loads((runtime / 'manifest.json').read_text())
bind(runtime / 'manifest.json')
bind(runtime / 'wave0-submission.json')
m['release_records'] = {}
for arm, stem, authority, task in (
    ('candidate', '7302-clause-realloc', '7302-clause-realloc', 4),
    ('previous', '7267-dense-realloc', '7271-dense-realloc', 5)):
    p = root / f'benchmarks/runs/20260911-{authority}-build/manifest.json'
    build = json.loads(p.read_text())
    bind(p)
    for rel, digest in build['files'].items():
        bind(root / build['source'] / rel, digest)
    binary = root / f'build/pin-{stem}-release/tactix'
    bind(binary, build['binaries'][str(binary.relative_to(root))])
    assert sha(binary) == rm['arms'][arm]['sha256']
    shutil.copy2(binary, c / 'pins' / arm)
    m['arms'][arm] = dict(path='pins/' + arm, sha256=sha(binary), arguments=[],
        source_commit=build['source_commit'], source_build=str(binary.parent.relative_to(root)))
    p = runtime / f'results/{task:03d}.json'
    final = p.exists()
    if not final:
        p = runtime / f'provisional/{task:03d}.json'
    r = json.loads(p.read_text())
    assert classify(r, final)['status'] in ('pending_validation', 'checked_UNSAT')
    assert r['arm'] == arm and r['rank'] == 31
    bind(p)
    m['release_records'][arm] = dict(path=str(p.relative_to(root)), sha256=sha(p),
                                    peak_rss_kib=r['solve']['peak_rss_kb'])
with (c / 'array.sbatch').open('x') as out:
    out.write((prior / 'array.sbatch').read_text().replace(str(prior), str(c)))
bind(Path(__file__))
bind(Path(__file__).with_name('validate-clause-memory-7310.py'))
assert m['release_records']['candidate']['peak_rss_kib'] == 483992
assert m['release_records']['previous']['peak_rss_kib'] == 562920
for key in ('preflight', 'prior_preflight_failure'):
    m.pop(key, None)
m.update(schema='tactix-external-clause-memory-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(), evidence_sha256=hashes,
    preflight_authority=dict(path=str((prior / 'manifest.json').relative_to(root)),
                            sha256=sha(prior / 'manifest.json'),
                            reason='Observer/tracer/runner copied byte-exact from the passed48MiB process-identity/RSS/smaps preflight and exact-Release reproduction7299. No new observation path.'),
    hypothesis='First ordinary7306 pair lowers Rank31 RSS by78928KiB with exact work/certificates but0.74% longer wall time. Confirm changed clause-arena growth and current high-water ownership under exact new7302 versus7267 Release binaries; do not infer global heap cause or runtime win.',
    acceptance='Exact whole own Release work/certificates including physical counters. Compare external GNUtime and solver VmHWM explicitly with483992/562920KiB before attributing this Release improvement. Unknown records, nonmonotonic HWM readings, gaps and non-atomic smaps stay explicit. All independent proofs required; proceed provisionally without waiting.',
    test_consideration='Existing tested byte-identical external observers and parsing controls, plus7302 full120/120/119 suites,32768-clause actual-parent replay,7303 twenty sanitizer legs,7304 156proofs/156models/two negative controls and all66 BC solver outcomes. No diagnostic or runtime harness change.',
    limitation='Diagnostic only. No score, sampler-as-atomic-peak assumption, allocator-liveness inference, unrestricted correctness/fuzz or promotion. Original7295/7299 campaigns and7306 timed waves remain unchanged.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p)
    for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

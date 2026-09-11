#!/usr/bin/env python3
"""Observe the largest repeated memory excess against exact native Release."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha, reference
from checked_comparison import classify
c = root / 'benchmarks/runs/20260911-7315-rank25-native-memory'
assert not c.exists(), 'preserve successful or failed preparation'
prior = root / 'benchmarks/runs/20260911-7310-clause-memory-rank31'
m = json.loads((prior / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
bind(prior / 'manifest.json')
bind(prior / 'summary.json')
bind(prior / 'resident-analysis-7311.json')
ref = reference()
native = ref['records'][24]
assert native['rank'] == 25 and native['expected'] == 'SAT'
build_path = root / 'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
b = json.loads(build_path.read_text()); bind(build_path)
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for directory in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c / directory).mkdir(parents=True)
for name in ('run.py', 'syscall_memory_task.py', 'runtime_task.py', 'modelcheck.py',
             'drat-trim', 'checkmodel', 'strace'):
    bind(prior / 'pins' / name, m['pin_sha256']['pins/' + name])
    shutil.copy2(prior / 'pins' / name, c / 'pins' / name)
native_binary = root / ref['source_campaign'] / ref['solver']['path']
m['arms'], m['release_records'] = {}, {}
for arm, solver, binary, digest, args, oracle in (
    ('candidate', 'tactix', root / 'build/pin-7302-clause-realloc-release/tactix',
     b['binaries']['build/pin-7302-clause-realloc-release/tactix'], [],
     dict(path='benchmarks/runs/20260911-7306-clause-realloc-runtime/results/002.json')),
    ('previous', 'kissat', native_binary, ref['solver']['sha256'],
     ref['solver']['arguments'], native['final_authority'])):
    bind(binary, digest)
    shutil.copy2(binary, c / 'pins' / arm)
    p = root / oracle['path']; bind(p, oracle.get('sha256'))
    r = json.loads(p.read_text())
    assert classify(r, True)['checked'] and r['rank'] == 25
    assert r['instance'] == native['instance'] and r['expected'] == 'SAT'
    m['arms'][arm] = dict(path='pins/' + arm, sha256=digest, arguments=args,
        source_commit=b['source_commit'] if solver == 'tactix' else ref['solver']['source_commit'],
        source_build=str(binary.parent.relative_to(root)), solver=solver)
    m['release_records'][arm] = dict(path=str(p.relative_to(root)), sha256=sha(p),
        peak_rss_kib=r['solve']['peak_rss_kb'], solver=solver)
assert m['release_records']['candidate']['peak_rss_kib'] == 2484880
assert m['release_records']['previous']['peak_rss_kib'] == 888820
with (c / 'matrix.tsv').open('x', newline='') as out:
    writer = csv.DictWriter(out, fieldnames=['task','rank','arm','tier','instance','expected'], delimiter='\t', lineterminator='\n')
    writer.writeheader()
    for task, arm in enumerate(('candidate', 'previous')):
        writer.writerow(dict(task=task, rank=25, arm=arm, tier='D', instance=native['instance'], expected='SAT'))
with (c / 'array.sbatch').open('x') as out:
    out.write((prior / 'array.sbatch').read_text().replace(str(prior), str(c)).replace('7295-memory-syscalls-rank31', '7315-rank25-native-memory'))
bind(root / 'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
for name in ('prepare-rank25-memory-7315', 'validate-rank25-memory-7315', 'rank25-memory-contract-7315'):
    bind(Path(__file__).with_name(name + '.py'))
for name in ('native_work.py','test_native_work.py'):
    bind(root / 'tools/bench' / name)
m.update(schema='tactix-native-external-rank25-memory-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(), evidence_sha256=hashes,
    input_sha256={native['instance']: native['input_sha256']},
    hypothesis='Rank25 current7302 peak2484880KiB exceeds fixednative888820KiB by1596060KiB despite being faster. Locate actual resident peak and large mapping owners in both unchanged binaries before choosing a broad memory representation repair.',
    acceptance='Require exact own Release work/certificate fingerprints using raw Tactix counters and every primary native statistic; no cross-solver identity assumed. Reproduce both own Release peaks before attribution. Independently check both original SAT models and require zero residue.',
    test_consideration='No solver or observer change. Byte-identical proven external observer/protocol/tracer, strict memory parsers and native primary-statistic tests. New contract fixtures preserve all Tactix counters, detect six native work/certificate changes, reject malformed/truncated native sections and wrong solver schema.',
    limitation='Diagnostic only; no runtime score/reference refresh, atomic smaps, live allocator-object census, unsupported cost attribution or promotion. Frozen full50 and cpu-q campaigns unchanged; pending checks permit provisional work, not acceptance.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c/'pins').iterdir(), c/'matrix.tsv', c/'array.sbatch']}
with (c / 'manifest.json').open('x') as out: json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

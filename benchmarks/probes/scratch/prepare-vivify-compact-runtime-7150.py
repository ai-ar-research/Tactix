#!/usr/bin/env python3
"""Predeclare three six-task31/35/38 waves for compact canonical Vivify."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7150-vivify-compact-runtime'
assert not c.exists()
gate = root / 'benchmarks/runs/20260910-7149-vivify-compact-BC-gate'
summary = gate / 'neutrality-7149.json'
g = json.loads(summary.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33 and g['all_work_and_certificates_equal']
for rel, digest in g['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((gate / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(gate / rel) == digest, rel
full = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50'
fm = json.loads((full / 'manifest.json').read_text())
protected = json.loads((full / 'launch.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest, rel
authorities, arms, oracles, inputs, seeds = {}, {}, {}, {}, {}
for arm, date, stem in [('candidate', '20260910', '7144-vivify-compact'),
                         ('previous', '20260910', '7030-collect-retry')]:
    path = root / f'benchmarks/runs/{date}-{stem}-build/manifest.json'
    build = json.loads(path.read_text())
    for rel, digest in build['files'].items(): assert sha(root / build['source'] / rel) == digest, rel
    binary = root / f'build/pin-{stem}-release/tactix'
    assert sha(binary) == build['binaries'][str(binary.relative_to(root))]
    authorities[arm] = dict(path=str(path.relative_to(root)), sha256=sha(path))
    arms[arm] = dict(path='pins/' + arm, sha256=sha(binary), arguments=[],
        source_commit=build['source_commit'], source_build=str(binary.parent.relative_to(root)))
for rank in (31, 35, 38):
    path = full / f'results/{rank-1:03d}.json'
    r = json.loads(path.read_text())
    assert classify(r, True)['status'] == 'checked_UNSAT' and r['rank'] == rank
    oracles[str(rank)] = dict(path=str(path.relative_to(root)), sha256=sha(path))
    inputs[r['instance']] = fm['input_sha256'][r['instance']]
    seeds[rank] = dict(rank=rank, tier='D', instance=r['instance'], expected='UNSAT')
for name in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(gate / 'pins' / name, c / 'pins' / name)
for arm, info in arms.items(): shutil.copy2(root / info['source_build'] / 'tactix', c / info['path'])
rows = []
for wave in range(3):
    for rank in (31, 35, 38):
        for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
            rows.append(dict(task=len(rows), arm=arm, **seeds[rank]))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (gate / 'array.sbatch').read_text().replace(str(gate), str(c)).replace(
    '--array=0-65', '--array=0-17').replace('7149-vivify-compact-BC', '7150-vivify-compact-runtime')
(c / 'array.sbatch').write_text(batch)
dependencies = [summary, full / 'manifest.json', Path(__file__),
    root / 'benchmarks/runs/20260910-7148-vivify-compact-proofs/summary.json',
    root / 'benchmarks/runs/20260910-7147-vivify-compact-sanitizers/summary.json']
out = dict(schema='tactix-vivify-compact-three-wave-runtime-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, authorities=authorities, input_sha256=inputs, collect_runtime_context=True,
    resource_contract=dict(m['resource_contract'], array='0-17'), protected_campaign_hashes=protected,
    previous_work_oracles=oracles, dependencies={str(p.relative_to(root)): sha(p) for p in dependencies},
    witnesses={'31': 'largest unshelved absolute gap; adverse and transfer control',
               '35': 'primary complete Vivify unit-cost witness',
               '38': 'long-run ordinary CDCL throughput transfer control'},
    submission_contract='Three sequential full-width six-task waves0-5,6-11,12-17. Advance after expected solver outcomes and exact own/cross-arm work/certificates, never wait for proof checking. Middle wave reverses arms; no node constraint/throttle.',
    hypothesis='Compact canonical homogeneous Vivify: redundant arena poses and reference sorting; ordinary irredundant stable count radix, logical ties preserved. No rule or strategy changes. Require at least15% lower complete Vivify Rank35 sampled cycles, at least1% lower whole-runtime paired medians31 and35, and no runtime/RSS regression31/35/38. Separate exact-work profiles own complete-Vivify attribution. Preserve all three waves and stop/reassess if premise fails.',
    acceptance='All18 expected independently checked/clean outcomes. Both arms must match frozen7030 non-timing stats/certificate and repeat own wave0 fingerprints. Report all paired ratios, median/range and within-arm spread; absolute claim requires<=2%. No repeat-to-select or fixed50 refresh.',
    test_consideration='115/115/114 full suites;96 ordering and1728 transaction/model cases exact7030; six DebugASan/UBSan/LSan fixtures twice;96 independent DRAT/LRAT positives and192 negatives; B28+C5 exact-work solver gate. Runtime runner unchanged; existing fingerprint/timer checks retain18 tasks, reversed middle wave, repeated own/cross-arm work and malformed-input rejection.',
    limitation='Isolated/unfuzzed; no promotion. Full50/memory/consolidation gates remain. Explicit user resumed after old Sep10 06:00 cutoff.')
out['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

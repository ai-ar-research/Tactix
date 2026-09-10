#!/usr/bin/env python3
"""Profile the repaired Rank35 search; reuse the already checked native profile."""
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
c = root / 'benchmarks/runs/20260910-7060-rank35-residual-profile'
assert not c.exists()
old = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles'
authority = root / 'benchmarks/runs/20260910-7030-collect-retry-build'
runtime = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime'
om = json.loads((old / 'manifest.json').read_text())
am = json.loads((authority / 'manifest.json').read_text())
rm = json.loads((runtime / 'manifest.json').read_text())
deps = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or expected == digest, p
    deps[str(p.relative_to(root))] = digest
for rel, digest in om['pin_sha256'].items(): bind(old / rel, digest)
for rel, digest in am['files'].items(): bind(root / am['source'] / rel, digest)
for rel, digest in am['binaries'].items(): bind(root / rel, digest)
for p in (old / 'manifest.json', authority / 'manifest.json', runtime / 'manifest.json',
          runtime / 'solver-checkpoint-7044.json', runtime / 'matched-native-work-7059.json', Path(__file__)):
    bind(p)
oracle = runtime / 'provisional/002.json'; bind(oracle)
assert classify(json.loads(oracle.read_text()), False)['status'] == 'pending_validation'
for p in (runtime / 'results').glob('*.json'):
    assert classify(json.loads(p.read_text()), True)['checked'], p
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate/results'
assert len(list(gate.glob('*.json'))) == 66
for p in gate.glob('*.json'):
    state = classify(json.loads(p.read_text()), True)
    assert state['checked'] and state['clean'], p
    bind(p)
native_record = old / 'results/003.json'; bind(native_record)
native = json.loads(native_record.read_text())
assert classify(native, True)['checked'] and native['rank'] == 35
for name, report in native['solve']['profile']['reports'].items():
    assert report['exit'] == 0
    bind(old / f'profiles/003-{name}.txt', report['sha256'])
assert '# Total Lost Samples: 0' in (old / 'profiles/003-self.txt').read_text()
bind(old / 'profiles/003.perf.data.xz', native['solve']['profile']['compressed_sha256'])
for name in ('pins','logs','results','provisional','claims','profiles'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py','run.py','checkmodel','modelcheck.py','drat-trim'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / 'build/pin-7030-collect-retry-release/tactix', c / 'pins/candidate')
with (old / 'matrix.tsv').open(newline='') as stream:
    reader = csv.DictReader(stream, delimiter='\t'); fields = reader.fieldnames
    rows = [row for row in reader if row['rank'] == '35' and row['arm'] == 'candidate']
assert len(rows) == 1; rows[0]['task'] = '0'
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
assert batch.count('--array=0-3') == 1
batch = batch.replace('--array=0-3','--array=0-0').replace('7002-largest-gap','7060-rank35-residual')
(c / 'array.sbatch').write_text(batch)
m = dict(om, created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms={'candidate':dict(rm['arms']['candidate'], path='pins/candidate')},
    resource_contract=dict(om['resource_contract'], array='0-0'),
    dependencies=deps, input_sha256={rows[0]['instance']: om['input_sha256'][rows[0]['instance']]},
    unprofiled_records={'35/candidate':dict(path=str(oracle.relative_to(root)),sha256=sha(oracle))},
    cached_native_profile=dict(path=str(native_record.relative_to(root)), sha256=sha(native_record)),
    selection=[dict(rank=35, role='Residual engineering cost after source-backed search repair')],
    current_authority=dict(path=str((authority / 'manifest.json').relative_to(root)), sha256=sha(authority / 'manifest.json')),
    purpose='One unchanged7030 Rank35 sampled run, not another native baseline or runtime repetition. Prior repaired work matches27 major native counters and1M decisions; choose a material inprocessing or CDCL implementation-cost target.',
    hypothesis='The residual7030 overhead is in inprocessing implementation or representation rather than the formerly doubled search. Compare dominant exclusive/callpath samples with checked native7002; different clock/scope/work residuals forbid raw timer subtraction.',
    validation='Require expected UNSAT and exact entire own non-timing statistics, proof bytes/hash, inherited immutable candidate SHA, zero lost samples, complete hash-bound self/callpath reports and compressed profile. Independent proof and scratch cleanup; pending proof permits investigation only.',
    test_consideration='No solver, observer or profile wrapper change; unchanged maintained runtime tests and7059 correspondence fixtures. Existing own-repeat fingerprint fixtures cover full statistics/certificate equality. No new solver unit tests needed.',
    parity='Preserve all candidate work/proofs and scheduling admission/effort.27 matched major native counters and1M choices support a residual-cost diagnostic but not universal CDCL or full-state parity. Preserve4/32/33/49/46; Rank31 later adverse transfer proceeds separately.',
    repetition='One candidate-only diagnostic, not timing acceptance. A resulting optimization needs the required independent three-wave predecessor comparison plus B28+C5/full50 and correctness gates.',
    limitation='Single diagnostic sampling run, not three-wave timing acceptance. Native profile is cached/cross-window. Samples select a mechanism; no unit price or memory claim from perf peak RSS.')
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
for rel, digest in m['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
with (c / 'manifest.json').open('x') as stream: json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

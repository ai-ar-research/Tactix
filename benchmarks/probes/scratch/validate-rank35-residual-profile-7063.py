#!/usr/bin/env python3
"""Require complete own work/proof replay before interpreting the repaired profile."""
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import read_timers
c = root / 'benchmarks/runs/20260910-7060-rank35-residual-profile'
target = c / 'solver-summary-7063.json'
assert not target.exists()
p = c / 'results/000.json'; final = p.exists()
if not final: p = c / 'provisional/000.json'
if not p.exists():
    print('Profile solver outcome pending; no output written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies','protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for path in (c / 'manifest.json', c / 'launch.json', p, Path(__file__)): bind(path)
r = json.loads(p.read_text()); state = classify(r, final)
assert state['status'] in ('pending_validation', 'checked_UNSAT'), state
assert r['rank'] == 35 and r['task'] == 0 and r['arm'] == 'candidate'
if final: assert r['binary_sha256'] == m['arms']['candidate']['sha256']
oracle = m['unprofiled_records']['35/candidate']; old_path = root / oracle['path']
bind(old_path, oracle['sha256']); old = json.loads(old_path.read_text())
helper = Path(__file__).with_name('submit-collect-retry-wave-7042.py'); bind(helper)
spec = importlib.util.spec_from_file_location('own_repeat', helper)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
assert h.fingerprint(r) == h.fingerprint(old), 'profile changed work or certificate'
profile = r['solve']['profile']
assert set(profile['reports']) == {'self','callpaths'}
for name, record in profile['reports'].items():
    assert record['exit'] == 0
    bind(c / f'profiles/000-{name}.txt', record['sha256'])
data = c / 'profiles/000.perf.data.xz'
bind(data, profile['compressed_sha256'])
assert data.stat().st_size == profile['compressed_bytes'] > 0
body = (c / 'profiles/000-self.txt').read_text()
assert '# Total Lost Samples: 0' in body and 'candidate' in body
np = root / m['cached_native_profile']['path']; bind(np, m['cached_native_profile']['sha256'])
native = json.loads(np.read_text())
assert classify(native, True)['checked'] and classify(native, True)['clean']
out = dict(hashes=hashes, state=state, profile=profile, candidate_timers=read_timers(r['solver_statistics']),
    all_non_timing_statistics_and_certificate_exact=True, record=str(p.relative_to(root)),
    cached_native_record=str(np.relative_to(root)), self_report_head=body.splitlines()[:55],
    decision='Use repaired7030 profile to select the dominant residual inprocessing or search implementation cost.27 major native counters and1M prefix match, but no full-state/common-clock unit-cost claim.',
    limitation='One sampled candidate and cached native window; not timing acceptance or proof acceptance while pending. Perf peak RSS is not solver memory. Existing immutable binaries, source and checkers unchanged.')
with target.open('x') as stream: json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print('\n'.join(body.splitlines()[:30]))

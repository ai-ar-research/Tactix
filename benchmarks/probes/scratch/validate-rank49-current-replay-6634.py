#!/usr/bin/env python3
"""Retain every macro difference while verifying the current short search replay."""
from collections import Counter
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import first_stream_difference, read_prefix
c = root / 'benchmarks/runs/20260909-6632-rank49-current-replay'
target = c / 'summary-6634.json'
assert not target.exists()
if not (c / 'results/candidate.json').exists():
    print('Current prefix pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
old = root / m['cached_controls']
hashes = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    hashes[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for name in ('manifest.json', 'launch.json', 'results/candidate.json'):
    bind(c / name)
r = json.loads((c / 'results/candidate.json').read_text())
p = json.loads((old / 'results/candidate.json').read_text())
assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == 'candidate'
assert r['binary_sha256'] == m['hashes']['pins/candidate']
assert r['partial_certificate'] == p['partial_certificate']
assert set(r['trace_sha256']) == set(p['trace_sha256'])
trace_equal = {}
for name, digest in r['trace_sha256'].items():
    bind(c / 'traces' / name, digest)
    trace_equal[name] = digest == p['trace_sha256'][name]
assert all(equal for name, equal in trace_equal.items() if name != 'candidate.macro.tsv')
before = (old / 'traces/candidate.macro.tsv').read_text()
after = (c / 'traces/candidate.macro.tsv').read_text()
assert before.endswith('\n') and after.endswith('\n')
a, b = before.splitlines(), after.splitlines()
assert a[:2] == b[:2] and len(a) == len(b)
fields = a[1][2:].split('\t'); differences = []; counts = Counter()
for index, (x, y) in enumerate(zip(a[2:], b[2:])):
    x, y = x.split('\t'), y.split('\t')
    assert len(x) == len(y) == len(fields)
    changes = {field: [u, v] for field, u, v in zip(fields, x, y) if u != v}
    if changes:
        counts.update(changes.keys())
        differences.append(dict(index=index, earlier=dict(zip(fields, x)), current=dict(zip(fields, y)), changes=changes))
prefix = read_prefix(c / 'traces/candidate.prefix.tsv')
assert prefix['conflict'] == 100000 and prefix['count'] == 622123
comparison = first_stream_difference(c / 'traces/candidate.decisions.tsv', old / 'traces/kissat.decisions.tsv')
control = json.loads((old / 'summary.json').read_text())
assert comparison == control['decisions']
out = dict(rank=49, stop=100000, partial_proof_equal=True, trace_equal=trace_equal,
    macro_rows=len(a)-2, macro_changed_fields=dict(counts), macro_differences=differences,
    current_prefix=prefix, native_decision_comparison=comparison,
    full_control_comparison=m['full_control_comparison'], hashes=hashes,
    conclusion='Current100K decisions, inprocess boundaries, prefix and partial proof exactly match6201. Macro differences are retained in full, not ignored. Native first decision mismatch therefore still applies; no claim of full macro identity, whole-formula equality or cause from an earlier binary-count difference.',
    next_action='Use this short100K witness to capture the implication/conflict-analysis window before native9273/current9274. Preserve current compact-stamp/activation repairs; the one startup stamp difference is not evidence to revert them. No new timing denominator or final-proof acceptance.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k != 'hashes'}, indent=2))

#!/usr/bin/env python3
"""Freeze early diagnostics without converting pending proofs to acceptance."""
import datetime
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from congruence_timing import parse_cumulative, deltas

c = root / 'benchmarks/runs/20260909-6539-rank32-early-congruence-time'
target = c / 'summary-6541.json'
assert not target.exists()
hashes = {}
def read(path):
    hashes[str(path.relative_to(root))] = sha(path)
    return json.loads(path.read_text())

m = read(c / 'manifest.json')
for group in ('authorities',):
    for entry in m[group].values():
        assert sha(root / entry['path']) == entry['sha256'], entry
        hashes[entry['path']] = entry['sha256']
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
    hashes[str((c / rel).relative_to(root))] = digest
oracle = read(root / m['authorities']['work_oracle']['path'])
old = read(root / 'benchmarks/runs/20260909-6535-rank32-congruence-time/provisional/000.json')
r = read(c / 'provisional/000.json')
nt = lambda x: [s for s in x['solver_statistics'] if not s.startswith('c timing')]
assert oracle['valid'] and oracle['proof_check']['result'] == 'verified'
for candidate in (old, r):
    assert candidate['validation_pending'] and 'valid' not in candidate
    assert candidate['verdict'] == 'UNSAT' and candidate['solve']['exit_code'] == 20
    assert not candidate['solve']['timed_out'] and candidate['expected_match']
    assert nt(candidate) == nt(oracle)
    for key in ('rank', 'instance', 'certificate_sha256', 'certificate_bytes'):
        assert candidate[key] == oracle[key], key
assert 'solver_stderr_tail' not in old
assert isinstance(r['solver_stderr_tail'], list)
rows = parse_cumulative('\n'.join(r['solver_stderr_tail']), 38)
for rel in ('tools/bench/congruence_timing.py',
            '.claude/worktrees/runtime-activation-stamp-6424/src/rules/congruence/congruence.cpp',
            '.claude/worktrees/runtime-activation-stamp-6424/src/rules/factor/factor.cpp',
            'tools/solvers/variants/kissat-macro-heap-6090/src/congruence.c',
            'tools/solvers/variants/kissat-macro-heap-6090/src/factor.c',
            'tools/solvers/variants/cadical-extdiag/src/congruence.cpp'):
    hashes[rel] = sha(root / rel)
result = dict(
    observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hashes=hashes, cumulative_rows=rows, per_call_deltas=deltas(rows),
    solver_seconds=r['solve']['wall_seconds'], final_cumulative_ms=rows[-1],
    index_fraction_of_instrumented_solve=rows[-1]['index'] / (1000 * r['solve']['wall_seconds']),
    index_fraction_of_instrumented_congruence=rows[-1]['index'] / rows[-1]['apply'],
    exact_own_checked_work_and_proof=True,
    certificate_sha256=r['certificate_sha256'], certificate_bytes=r['certificate_bytes'],
    validation_pending=True,
    decision='Indexing is 1.969 seconds, only part of which is avoidable tail scanning. Deprioritize it as the primary Rank32 gap-closing lever; no solver edit or timing acceptance.',
    limitation='Two diagnostic captures, first with a provisional stderr omission. No native rerun or reference refresh. Stage counters overlap and carry timing overhead; neither their sum nor unmatched native CPU timers establishes equal-operation unit cost. Exact own proof identity does not replace each run\'s independent checking.',
)
with target.open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(target)
print(json.dumps(rows[-1], sort_keys=True))

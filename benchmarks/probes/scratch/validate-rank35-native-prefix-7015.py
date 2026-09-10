#!/usr/bin/env python3
"""Validate actual current/native traces before interpreting the first split."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, validate_suffix_accounting, first_stream_difference

c = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
target = c / 'summary-7015.json'
assert not target.exists(), 'successful summary frozen'
if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
    print('Stopped diagnostic pending; no summary written')
    sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(path, expected=None):
    actual = sha(path)
    assert expected is None or actual == expected, path
    hashes[str(path.relative_to(root))] = actual
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for path in (c / 'manifest.json', c / 'launch.json', Path(__file__), Path(__file__).with_name('validate-rank35-native-prefix-7013.py')):
    bind(path)
prefixes, inherited, records, counts = {}, {}, {}, {}
for arm in ('candidate', 'kissat'):
    record_path = c / 'results' / (arm + '.json')
    bind(record_path)
    r = json.loads(record_path.read_text())
    assert r['arm'] == arm and r['exit_code'] == 0, (arm, r)
    assert r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    assert r['partial_certificate']['bytes'] > 0
    expected = ('prefix', 'decisions', 'boundaries', 'macro') if arm == 'candidate' else ('prefix', 'decisions', 'boundaries')
    assert set(r['trace_sha256']) == {arm + '.' + suffix + '.tsv' for suffix in expected}
    # Cached native pin predates the macro observer; it never recognized this
    # environment sink. Preserve the absence instead of inventing native rows.
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    p = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    assert p['solver'] == ('tactix' if arm == 'candidate' else 'kissat')
    assert p['conflict'] == m['stop_conflict']
    inherited[arm] = p['common_reached'] == 1 and p['common'] == m['common_decisions'] and p['common_hash'] == m['prior_common_hash']
    prefixes[arm], records[arm] = p, r
comparison = None
if all(inherited.values()):
    for arm in ('candidate', 'kissat'):
        p = prefixes[arm]
        validate_common(p, stop=m['stop_conflict'], common=m['common_decisions'], digest=m['prior_common_hash'])
        old = root / ('benchmarks/runs/20260909-6376-rank35-stamp-prefix/traces/candidate.decisions.tsv' if arm == 'candidate' else 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix/traces/kissat.decisions.tsv')
        # Both cached suffix/full streams end below the threshold. Bind the
        # complete file, then establish zero overlap without inventing a row.
        bind(old)
        last_old = list(map(int, subprocess.check_output(['tail', '-n', '1', str(old)], text=True).split()))
        assert last_old[0] < m['stream_from'] and last_old[3] == p['common_last']
        stream = c / 'traces' / (arm + '.decisions.tsv')
        count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
        first = list(map(int, subprocess.check_output(['head', '-n', '1', str(stream)], text=True).split()))
        last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split()))
        validate_suffix_accounting(p, first, last, count, common_count=m['common_decisions'], overlap_count=0, stream_from=m['stream_from'])
        assert first[1] == last_old[1] + 1
        counts[arm] = count
    comparison = first_stream_difference(c / 'traces/candidate.decisions.tsv', c / 'traces/kissat.decisions.tsv')
    same = all(prefixes['candidate'][key] == prefixes['kissat'][key] for key in ('count', 'hash', 'end_decisions', 'first', 'last'))
    assert comparison['complete_equal'] == same
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), rank=35,
           prefixes=prefixes, inherited100k=inherited, suffix_rows=counts,
           decisions=comparison, matching_rows_from_start=m['common_decisions'] + comparison['matching_rows'] if comparison else None,
           hashes=hashes, records=records,
           prior_validator_failure='7013 required a macro trace from the older native pin, which lacks that observer. Both solvers stopped cleanly; no summary was written. Fresh7015 retains the exact native three-trace contract and makes no native macro claim.',
           missing_observer='Native macro boundary absent; native inprocessing boundaries remain available. A later observer replay would need its own complete neutrality validation.',
           decision='Localize preceding portable macro boundary if split; extend if equal. Earlier changed common hash takes priority over suffix comparison.',
           limitation='Bounded trace diagnostic, not proof acceptance, whole-formula identity or a runtime claim. A first difference alone does not establish which arm is stronger.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('inherited100k', 'decisions', 'matching_rows_from_start')}, indent=2))

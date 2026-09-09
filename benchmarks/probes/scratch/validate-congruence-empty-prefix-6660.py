#!/usr/bin/env python3
"""Freeze Rank49's repaired physical first-Probe population and decision stream."""
from collections import Counter
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_stream_accounting, first_stream_difference
from binary_snapshot import read_snapshots
from search_window import read_search
c = root / 'benchmarks/runs/20260909-6658-rank49-congruence-empty-prefix'
target = c / 'summary-6660.json'; assert not target.exists()
m = json.loads((c / 'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for rel in ('manifest.json', 'launch.json', 'results/candidate.json'): bind(c / rel)
r = json.loads((c / 'results/candidate.json').read_text())
assert r['exit_code'] == 0 and r['scratch_deleted'] and r['binary_sha256'] == m['hashes']['pins/candidate']
for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
native = root / 'benchmarks/runs/20260909-6648-rank49-binary-life'
nr = json.loads((native / 'results/kissat.json').read_text()); bind(native / 'results/kissat.json')
for name, digest in nr['trace_sha256'].items(): bind(native / 'traces' / name, digest)
streams = {}
for arm, campaign in (('candidate', c), ('kissat', native)):
    path = campaign / f'traces/{arm}.decisions.tsv'
    prefix = read_prefix(campaign / f'traces/{arm}.prefix.tsv'); assert prefix['conflict'] == 100000
    lines = path.read_text().splitlines(); assert lines
    validate_stream_accounting(prefix, list(map(int, lines[0].split('\t'))), list(map(int, lines[-1].split('\t'))), len(lines))
    streams[arm] = path
decision = first_stream_difference(streams['candidate'], streams['kissat'])
assert decision == dict(matching_rows=618727, left=None, right=None, complete_equal=True)
parser = Path(__file__).with_name('compare-rank32-formula-5785.py')
spec = importlib.util.spec_from_file_location('formula5785', parser)
formula = importlib.util.module_from_spec(spec); spec.loader.exec_module(formula); formula.fixtures(); bind(parser)
large = {}; binaries = {}; probes = {}
for arm, campaign in (('candidate', c), ('kissat', native)):
    large[arm] = formula.read_rows(campaign / f'traces/{arm}.formula.tsv')
    with (campaign / f'traces/{arm}.binaries.tsv').open() as stream:
        snaps = read_snapshots(stream, 'tactix' if arm == 'candidate' else 'kissat', 2988, [('probe_end', '-')])
    assert len(snaps) == 1; binaries[arm] = snaps[0]['rows']
    search = read_search((campaign / f'traces/{arm}.search.tsv').read_text(), solver='tactix' if arm == 'candidate' else 'kissat', window=(9268,9275), cap=250000)
    probes[arm] = [dict(conflicts=row['conflicts'], decisions=row['decisions'], binary=row['binary'], irred=row['irred'])
                   for row in search if row['event'] == 'probe_begin']
assert Counter(binaries['candidate']) == Counter(binaries['kissat']) and len(binaries['candidate']) == 338
assert probes['candidate'][1]['conflicts'] == probes['kissat'][1]['conflicts'] == 9274
assert probes['candidate'][1]['decisions'] == probes['kissat'][1]['decisions'] == 57338
parent = root / m['cached_controls']
def boundaries(p): return [line for line in p.read_text().splitlines() if not line.startswith('#')]
before = boundaries(parent / 'traces/candidate.boundaries.tsv')
after = boundaries(c / 'traces/candidate.boundaries.tsv')
assert before[:31] == after[:31]
assert before[30].split('\t')[2:6] == ['Congruence', 'begin', '2988', '15773']
out = dict(rank=49, bounded_stop_clean=True, decision_difference=decision,
    pre_repair_boundary_rows_equal=31, binary_multisets_equal=True, binary_copies=338,
    full_large_rows_equal=large['candidate'] == large['kissat'], large_row_counts={a:len(v) for a,v in large.items()},
    large_key_multisets_equal=Counter(row[0] for row in large['candidate']) == Counter(row[0] for row in large['kissat']),
    probe_starts=probes, hashes=hashes,
    limitation='Complete100K decision identity and firstProbe formula comparison, not full runtime or proof acceptance. Changed candidate partial proof is expected. Fixed references and old controls untouched.')
with target.open('x') as stream: json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','probe_starts')}, indent=2))

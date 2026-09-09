#!/usr/bin/env python3
"""Validate complete initial decision streams, keeping coarse bounds separate."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from decision_prefix import first_stream_difference, read_prefix
from fixed_runtime import sha
spec = importlib.util.spec_from_file_location('boundaries',
    Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
boundaries = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boundaries)
boundaries.fixtures()

for number, rank in ((6139, 21), (6140, 4)):
    c = root / f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'
    target = c / 'summary.json'
    assert not target.exists(), target
    if not all((c / 'results' / f'{arm}.json').exists() for arm in ('candidate', 'kissat')):
        print(number, 'pending'); continue
    m = json.loads((c / 'manifest.json').read_text())
    hashes = {}
    def bind(p):
        hashes[str(p.relative_to(root))] = sha(p)
    for rel, digest in m['hashes'].items():
        assert sha(c / rel) == digest, rel
    for name in ('manifest.json', 'launch.json'):
        bind(c / name)
    prefixes, projected, ignored = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        path = c / 'results' / f'{arm}.json'
        r = json.loads(path.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        assert r['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate']['bytes'] > 0
        bind(path)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name
            assert sha(p) == digest, p
            bind(p)
        prefix_path = c / 'traces' / f'{arm}.prefix.tsv'
        stream = c / 'traces' / f'{arm}.decisions.tsv'
        boundary_path = c / 'traces' / f'{arm}.boundaries.tsv'
        for p in (prefix_path, stream, boundary_path):
            assert p.name in r['trace_sha256'], p
        prefix = read_prefix(prefix_path)
        assert prefix['conflict'] == m['stop_conflict']
        assert prefix['common'] == prefix['common_reached'] == 1
        count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
        assert count == prefix['count'] > 0
        with stream.open() as f:
            first = list(map(int, f.readline().split('\t')))
        last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split('\t')))
        assert len(first) == len(last) == 5 and first[0] == 0
        assert first[3] == prefix['first'] == prefix['common_last'] and last[3] == prefix['last']
        assert last[0] <= m['stop_conflict'] and last[1] == prefix['end_decisions']
        # Startup/warm-up resets solver decision statistics, but not the
        # observer's cumulative row count. Keep the original unsigned field;
        # it is not a useful absolute starting decision ordinal in this case.
        assert prefix['start_decisions'] == (prefix['end_decisions'] - count) % (1 << 64)
        prefixes[arm] = prefix
        with boundary_path.open() as f:
            projected[arm], ignored[arm] = boundaries.parse(f, arm == 'kissat')
    difference = first_stream_difference(c / 'traces/candidate.decisions.tsv', c / 'traces/kissat.decisions.tsv')
    if difference['complete_equal']:
        assert difference['matching_rows'] == prefixes['candidate']['count'] == prefixes['kissat']['count']
        for field in prefixes['candidate']:
            if field != 'solver':
                assert prefixes['candidate'][field] == prefixes['kissat'][field], field
    search = {arm: [r for r in rows if r['conflicts'] > 0] for arm, rows in projected.items()}
    result = dict(rank=rank, stop=m['stop_conflict'], prefixes=prefixes, decisions=difference,
        boundaries_all=boundaries.compare(projected['candidate'], projected['kissat']),
        boundaries_search=boundaries.compare(search['candidate'], search['kissat']),
        ignored_boundary_names=ignored, hashes=hashes,
        limitation='Exact selected decision stream comparison,not full-solve trajectory,unit-cost acceptance or checked final proof. Coarse boundary ordinals/counts do notproveformula/pose/effort parity. Startup resets explain unsigned start_decisions wrap; useactualrowcount,notthatfieldasanabsoluteordinal. Additionalobserver payloads mustreplaytheseordinarytraces beforecausalinterpretation.')
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(number, json.dumps(dict(decisions=difference, boundaries_all=result['boundaries_all'],
                                 boundaries_search=result['boundaries_search'])), flush=True)

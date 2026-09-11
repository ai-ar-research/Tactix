#!/usr/bin/env python3
"""Read published profile outcomes, never treating pending proofs as checked."""
import csv
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from perf_symbol_cost import read_report
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('bve_wave', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
c = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'

def read_profiles():
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    p = c / 'manifest.json'; m = json.loads(p.read_text()); bind(p)
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items(): bind(root / rel, digest)
    with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert [int(r['task']) for r in matrix] == list(range(6))
    assert all(r['rank'] == '35' and r['expected'] == 'UNSAT' for r in matrix)
    launched = set()
    for wave in range(3):
        p = c / f'wave{wave}-submission.json'
        if not p.exists(): continue
        launch = json.loads(p.read_text()); bind(p)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        assert launch['tasks'] == list(range(2 * wave, 2 * wave + 2))
        launched.update(launch['tasks'])
    rows, states, parsed = {}, {}, {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text()); bind(p); task = r['task']
            assert task == int(p.stem) and task in launched
            for key in ('arm', 'instance', 'expected'): assert r[key] == matrix[task][key], (p, key)
            assert r['rank'] == 35
            s = classify(r, folder == 'results')
            assert s['status'] in ('pending_validation', 'checked_UNSAT'), (p, s)
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            oracle = m['unprofiled_records']['35/' + r['arm']]
            previous = root / oracle['path']; bind(previous, oracle['sha256'])
            assert h.fingerprint(r) == h.fingerprint(json.loads(previous.read_text())), (p, 'work/proof mismatch')
            if task in rows:
                assert h.fingerprint(r) == h.fingerprint(rows[task]) and r['solve'] == rows[task]['solve']
            rows[task], states[task] = r, s
    for task, r in rows.items():
        profile = r['solve']['profile']; tables = {}
        assert set(profile['reports']) == {'self', 'callpaths'}
        for name, report in profile['reports'].items():
            assert report['exit'] == 0
            p = c / f'profiles/{task:03d}-{name}.txt'; bind(p, report['sha256'])
            with p.open() as stream: tables[name] = read_report(stream, name == 'callpaths')
            assert '# Total Lost Samples: 0' in p.read_text()
        assert tables['self']['total_cycles'] == tables['callpaths']['total_cycles'] > 0
        p = c / f'profiles/{task:03d}.perf.data.xz'; bind(p, profile['compressed_sha256'])
        assert p.stat().st_size == profile['compressed_bytes'] > 0
        parsed[task] = dict(total_cycles=tables['self']['total_cycles'])
    for p in (Path(__file__), reader, root / 'tools/bench/perf_symbol_cost.py'): bind(p)
    return dict(manifest=m, hashes=hashes, rows=rows, states=states, flat_reports=parsed, launched=sorted(launched))

def main():
    assert sys.argv[1:] in ([], ['--seal'])
    out = read_profiles()
    checked = sum(s['checked'] for s in out['states'].values())
    print(f'Profiles {len(out["rows"])}/6 exact unprofiled work/proofs, {checked}/6 checked-clean')
    if '--seal' in sys.argv:
        target = c / 'solver-summary-7152.json'
        assert not target.exists() and len(out['rows']) == 6
        with target.open('x') as stream:
            json.dump(dict(hashes=out['hashes'], states=out['states'], flat_reports=out['flat_reports'],
                solver_outcomes=6, checked_clean=checked, exact_unprofiled_work_and_proofs=True,
                scope='All three predeclared sampled pairs. Whole-lifecycle attribution still required; pending proofs are unverified. Unprofiled7150 owns total-runtime and RSS gates.'), stream, indent=2)
        print(target)
if __name__ == '__main__': main()

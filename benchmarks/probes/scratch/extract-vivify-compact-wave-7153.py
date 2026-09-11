#!/usr/bin/env python3
"""Conserve one published paired profile wave at complete phase boundaries."""
import importlib.util
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples, summarize_phases
from perf_function_envelope import function_envelope
def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m
gate = module('profile_gate', 'validate-vivify-compact-profiles-7152.py')
replay = module('profile_components', 'replay-bve-lifecycle-reader-7131.py')
assert len(sys.argv) == 2 and sys.argv[1] in ('0', '1', '2')
wave = int(sys.argv[1])
c = root / 'benchmarks/runs/20260910-7153-vivify-compact-samples' / f'wave{wave}'
assert not c.exists(), 'successful/failed evidence retained; do not overwrite'
data = gate.read_profiles(); m = data['manifest']
tasks = list(range(2 * wave, 2 * wave + 2))
if not all(task in data['rows'] for task in tasks):
    print('Profile pair pending; no output created'); sys.exit(0)
assert {data['rows'][task]['arm'] for task in tasks} == {'candidate', 'previous'}
hashes = dict(data['hashes'])
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
p = root / 'benchmarks/reports/2026-09-10-bve-lifecycle-reader-replay-7131.json'
r = json.loads(p.read_text()); bind(p)
assert r['status'] == 'PASS' and r['exact_frozen_attribution']
for rel, digest in r['hashes'].items(): bind(root / rel, digest)
p = root / 'benchmarks/reports/2026-09-10-vivify-envelope-replay-7154.json'
r = json.loads(p.read_text()); bind(p)
assert r['status'] == 'PASS'
for rel, digest in r['hashes'].items(): bind(root / rel, digest)
c.mkdir(parents=True)
arms, envelopes, records, deleted = {}, {}, {}, 0
for task in tasks:
    record = data['rows'][task]; arm = record['arm']; profile = record['solve']['profile']
    compressed = gate.c / f'profiles/{task:03d}.perf.data.xz'
    directory = Path(tempfile.mkdtemp(prefix='expanded-', dir=c))
    raw = directory / 'profile.data'
    with lzma.open(compressed, 'rb') as src, raw.open('xb') as dst: shutil.copyfileobj(src, dst)
    assert sha(raw) == profile['sha256'] and raw.stat().st_size == profile['bytes']
    command = ['perf', 'script', '-i', str(raw), '-F', 'period,ip,sym,dso']
    output = c / (arm + '.samples.txt.xz'); stderr = c / (arm + '.stderr')
    with stderr.open('xb') as err, lzma.open(output, 'xb') as dst:
        process = subprocess.Popen(command, cwd=root, stdout=subprocess.PIPE, stderr=err)
        shutil.copyfileobj(process.stdout, dst)
        process.stdout.close()
        assert process.wait() == 0
    assert not stderr.read_text().strip(), stderr
    bind(output); bind(stderr)
    sites = {int(k): v for k, v in m['call_sites'][arm]['sites'].items()}
    with lzma.open(output, 'rt') as stream:
        summary = summarize_phases(samples(stream), str(gate.c / m['arms'][arm]['path']),
            {m['phase_entry']: None}, m['phase_caller'], sites, replay.COMPONENTS)
    assert summary['samples'] > 0
    assert summary['total_period'] == data['flat_reports'][task]['total_cycles']
    assert not summary['unresolved_callers'], 'phase ancestry unresolved; do not assign it to search'
    assert all(summary['phase_periods'].get(name, 0) > 0 for name in ('initial_probe', 'probe', 'eliminate'))
    with lzma.open(output, 'rt') as stream:
        envelope = function_envelope(samples(stream), str(gate.c / m['arms'][arm]['path']),
            'tactix::vivify_apply(', {m['phase_entry']: None}, m['phase_caller'], sites)
    assert envelope['samples'] == summary['samples']
    assert envelope['total_period'] == summary['total_period']
    assert envelope['selected_period'] > 0
    for phase_name, components in summary['components'].items():
        assert envelope['by_phase'].get(phase_name, 0) == components.get('vivify', 0), phase_name
    envelopes[arm] = envelope
    arms[arm] = summary
    records[arm] = dict(task=task, state=data['states'][task], command=command,
        profile_sha256=profile['sha256'], source_binary_sha256=m['arms'][arm]['sha256'],
        sample_stream=str(output.relative_to(root)))
    # Only this new, verified expansion is redundant. Retain the compressed
    # source profile and extracted stream; never touch the active checker.
    assert raw.parent == directory and directory.parent == c and list(directory.iterdir()) == [raw]
    assert sha(raw) == profile['sha256']
    deleted += raw.stat().st_size
    raw.unlink(); directory.rmdir()
    print(arm, 'samples', summary['samples'], 'complete Vivify cycles', envelope['selected_period'], flush=True)
for p in (Path(__file__), Path(replay.__file__), root / 'tools/bench/perf_callstack.py', root / 'tools/bench/perf_function_envelope.py', root / 'tools/bench/test_perf_function_envelope.py'): bind(p)
ratios = {name: arms['candidate']['phase_periods'].get(name, 0) / value
          for name, value in arms['previous']['phase_periods'].items() if value > 0}
with (c / 'summary.json').open('x') as stream:
    json.dump(dict(wave=wave, hashes=hashes, arms=arms, records=records, phase_ratios=ratios,
        function_envelopes=envelopes,
        complete_vivify_ratio=envelopes['candidate']['selected_period'] / envelopes['previous']['selected_period'],
        temporary_expansions_deleted_bytes=deleted,
        limitation='One predeclared pair of complete function-scope sampled periods. Not a repetition median, exact invocation timing, proof acceptance, or solver RSS. Preserve all waves and unknown non-phase ancestry; inclusive costs overlap.'), stream, indent=2)
print('Phase ratios', ratios)
print('Removed only verified new temporary expansions, bytes:', deleted)

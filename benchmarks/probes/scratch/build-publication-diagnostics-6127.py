#!/usr/bin/env python3
"""Build fresh diagnostic pins without mutating submitted source or binaries."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
authority = root / 'benchmarks/runs/20260908-6107-substitute-publication'
manifest = json.loads((authority / 'manifest.json').read_text())
source = root / manifest['source']
c = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
for rel, digest in manifest['files'].items():
    assert sha(source / rel) == digest, rel
for rel, digest in manifest['binaries'].items():
    assert sha(root / rel) == digest, rel
inputs = {str(p.relative_to(source)): sha(p)
          for folder in ('src', 'include', 'tests')
          for p in (source / folder).rglob('*') if p.is_file()}
inputs['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
(c / 'logs').mkdir(parents=True)

def run(name, command):
    with (c / 'logs' / (name + '.log')).open('x') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    assert result.returncode == 0, (name, result.returncode)
    print(name, 'PASS', flush=True)

def build(number, kind, option):
    out = root / f'build/pin-{number}-publication-{kind}'
    assert not out.exists(), out
    run(kind + '-configure', ['cmake', '-S', str(source), '-B', str(out),
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_CONFLICT_STOP=OFF', option])
    run(kind + '-build', ['cmake', '--build', str(out), '-j6'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return str(out.relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(build, 6127, 'trace', '-DTACTIX_ENABLE_EVENT_TRACE=ON'),
               pool.submit(build, 6128, 'journal', '-DTACTIX_ENABLE_RULE_JOURNAL=ON')]
    builds = dict(f.result() for f in futures)
for rel, digest in inputs.items():
    assert sha(source / rel) == digest, rel
for rel, digest in manifest['binaries'].items():
    assert sha(root / rel) == digest, rel
summary = dict(source=manifest['source'], authority=str(authority.relative_to(root)),
    authority_sha256=sha(authority / 'manifest.json'), source_sha256=inputs,
    binaries=builds, logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()},
    purpose='Unchanged6107 source: build and test event-trace and rule-journal configurations before diagnostic use.',
    test_consideration='No source change; both complete fixed CTest suites required. Runtime observer neutrality still requires full work/model replay against6115.',
    limitation='Diagnostic builds do not establish runtime improvement, memory acceptance, current-source fuzz or promotion.')
(c / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(builds, indent=2), flush=True)

#!/usr/bin/env python3
"""Attribute existing checked Rank39 propagation samples without running solvers."""
import hashlib
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5544-small-rank39-profile'
target = root / 'benchmarks/runs/20260908-5671-propagation-annotation'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
summary = json.loads((parent / 'summary.json').read_text())
record = json.loads((parent / 'solver-checkpoint.json').read_text())
manifest = json.loads((parent / 'manifest.json').read_text())
assert summary['checked_clean'] == 2
target.mkdir()
symbols = {
    'candidate': [('main', 'tactix::kernel_propagate(tactix::Kernel&, unsigned int)'),
                  ('subsolver', 'tactix::SubSolver::propagate()')],
    'kissat': [('main', 'kissat_search_propagate')],
}
results = {}
for task, arm in enumerate(('candidate','kissat')):
    profile = record['arms'][arm]['profile']
    compressed = parent / f'profiles/{task:03}.perf.data.xz'
    assert sha(compressed) == profile['compressed_sha256']
    assert sha(parent / 'pins' / arm) == manifest['arms'][arm]['sha256']
    with tempfile.TemporaryDirectory(prefix=arm+'-annotation-', dir=target) as directory:
        data = Path(directory) / 'profile.data'
        with lzma.open(compressed, 'rb') as src, data.open('wb') as dst:
            shutil.copyfileobj(src, dst)
        assert sha(data) == profile['sha256']
        for label, symbol in symbols[arm]:
            name = arm + '-' + label
            command = ['perf', 'annotate', '--stdio', '--no-source', '-i', str(data), '--symbol', symbol]
            r = subprocess.run(command, cwd=root, capture_output=True, text=True)
            (target / (name + '.txt')).write_text(r.stdout)
            (target / (name + '.stderr')).write_text(r.stderr)
            assert r.returncode == 0 and r.stdout.strip() and 'no samples' not in r.stderr, r.stderr
            results[name] = dict(command=command,exit=r.returncode,
                stdout_sha256=sha(target / (name + '.txt')),stderr_sha256=sha(target / (name + '.stderr')))
(target / 'manifest.json').write_text(json.dumps(dict(parent=str(parent.relative_to(root)),
    parent_manifest_sha256=sha(parent/'manifest.json'),parent_summary_sha256=sha(parent/'summary.json'),
    profiles=results,temporary_data_deleted=True,
    test_consideration='Configuration-only extension of the existing annotation helper: exact compressed/decompressed profile and pin hashes plus nonempty successful symbol output validate use. No solver or parser semantics changed; no new solver unit test.',
    limitation='Old5522 checked profile. It prioritizes a current-source hypothesis,not current cost attribution or a speedup; source and machine-code applicability must be checked.'),indent=2)+'\n')
print('PASS: three annotations from two checked profiles; temporary expansions removed')

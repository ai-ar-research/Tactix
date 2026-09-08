#!/usr/bin/env python3
"""Attribute existing clean samples; never execute a solver or touch fuzz data."""
import hashlib
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260907-5425-rephase-root-profile'
target = root / 'benchmarks/runs/20260907-5443-inprocessing-annotation'
assert not target.exists()
target.mkdir()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''):
            h.update(block)
    return h.hexdigest()

record = json.loads((parent / 'solver-checkpoint.json').read_text())
profile = record['arms']['candidate']['profile']
compressed = parent / 'profiles/000.perf.data.xz'
assert sha(compressed) == profile['compressed_sha256']
manifest = json.loads((parent / 'manifest.json').read_text())
assert sha(parent / 'pins/candidate') == manifest['arms']['candidate']['sha256']
symbols = {
    'replace': 'tactix::kernel_replace_large_clause_in_place',
    'rebuild': 'tactix::kernel_rebuild_large_watches',
}
results = {}
with tempfile.TemporaryDirectory(prefix='annotation-', dir=target) as directory:
    data = Path(directory) / 'profile.data'
    with lzma.open(compressed, 'rb') as src, data.open('wb') as dst:
        shutil.copyfileobj(src, dst)
    assert sha(data) == profile['sha256']
    for name, symbol in symbols.items():
        command = ['perf', 'annotate', '--stdio', '--no-source', '-i', str(data),
                   '--symbol', symbol]
        result = subprocess.run(command, cwd=root, capture_output=True, text=True)
        (target / (name + '.txt')).write_text(result.stdout)
        (target / (name + '.stderr')).write_text(result.stderr)
        results[name] = dict(command=command, exit=result.returncode,
                             stdout_sha256=sha(target / (name + '.txt')),
                             stderr_sha256=sha(target / (name + '.stderr')))
        assert result.returncode == 0, result.stderr
(target / 'manifest.json').write_text(json.dumps(dict(
    parent=str(parent.relative_to(root)), parent_manifest_sha256=sha(parent/'manifest.json'),
    compressed_sha256=sha(compressed), profiles=results, temporary_data_deleted=True,
    scope='Read-only attribution of existing clean samples, no timing or correctness claim.'), indent=2)+'\n')
print(json.dumps(results, indent=2))

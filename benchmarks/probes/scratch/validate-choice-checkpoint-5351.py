#!/usr/bin/env python3
"""Capture checkpoint validation without changing any submitted runner/pin."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
campaign = root / 'benchmarks/runs/20260907-5351-bve-schedule'
target = campaign / 'validation.json'
assert not target.exists()
commands = {
    'release-ctest': ['ctest', '--test-dir', 'build/release', '--output-on-failure', '-j', '8'],
    'doc-size': [sys.executable, 'tools/docs/check-doc-size.py'],
    'progress': [sys.executable, 'tools/docs/check-progress.py'],
}
results = {}
for name, command in commands.items():
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    output = result.stdout + result.stderr
    (campaign / 'logs' / (name + '.log')).write_text(output)
    assert result.returncode == 0, (name, output)
    if name == 'release-ctest':
        assert '100% tests passed out of 77' in output
    results[name] = dict(command=command, exit=0, output_sha256=hashlib.sha256(output.encode()).hexdigest())
    print(name, 'PASS', flush=True)
binary_hash = hashlib.sha256((root / 'build/release/tactix').read_bytes()).hexdigest()
assert binary_hash == 'de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f'
target.write_text(json.dumps(dict(commands=results, production_sha256=binary_hash,
    progress_lines=len((root / 'PROGRESS.md').read_text().splitlines()),
    interpretation='Production unchanged; 77/77 tests and all 30 documentation budgets pass. Existing near-cap warnings and historical progress-check warnings are not waived.'), indent=2) + '\n')

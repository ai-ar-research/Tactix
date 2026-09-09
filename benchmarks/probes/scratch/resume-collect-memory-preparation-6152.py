#!/usr/bin/env python3
"""Resume only the verified, unsubmitted partial artifact-copy scaffold."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6148-collect-memory'
parent = root / 'benchmarks/runs/20260908-6129-publication-memory'
m = json.loads((parent / 'manifest.json').read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert c.is_dir() and not (c / 'manifest.json').exists() and not (c / 'launch.json').exists()
for folder in ('logs', 'results', 'provisional', 'claims'):
    assert not list((c / folder).iterdir()), folder
assert {p.name for p in c.iterdir()} == {'pins', 'logs', 'results', 'provisional', 'claims'}
for p in (c / 'pins').iterdir():
    assert p.is_file() and sha(p) == m['pin_sha256'][str(p.relative_to(c))], p
source = Path(__file__).with_name('prepare-collect-memory-run-6148.py')
code = source.read_text()
start = code.index("assert not (build / 'manifest.json').exists()")
end = code.index("parent = root / 'benchmarks/runs/20260908-6129-publication-memory'")
code = code[:start] + code[end:]
code = code.replace('assert not c.exists()', 'assert c.is_dir()')
code = code.replace('(c / name).mkdir(parents=True)', '(c / name).mkdir(parents=True, exist_ok=True)')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

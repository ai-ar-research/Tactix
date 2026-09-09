#!/usr/bin/env python3
"""Preserve raw perf formatting locally; land its normalized hash-bound summary."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-09-learned-sort-landing-recovery.json'
assert not target.exists()
original = root / 'benchmarks/reports/2026-09-09-learned-sort-landing.json'
m = json.loads(original.read_text())
for rel, digest in m['hashes'].items():
    assert sha(root / rel) == digest, rel
raw = [f'benchmarks/runs/20260909-6166-rank4-release-profile/profiles/{task:03d}-{kind}.txt'
       for task in range(2) for kind in ('self', 'callpaths')]
for rel in raw:
    assert rel in m['hashes']
    assert subprocess.run(['git', 'cat-file', '-e', 'HEAD:' + rel], cwd=root,
                          capture_output=True).returncode != 0
subprocess.run(['git', 'restore', '--staged', '--', *raw], cwd=root, check=True)
target.write_text(json.dumps(dict(original_landing_sha256=sha(original),
    failure='git diff --check rejected trailing padding in four raw perf text reports; no solver or checker failed.',
    correction='Only the four newly staged raw text reports were unstaged; raw bytes remain unchanged and hash-bound locally. Tracked summary retains normalized full self-sample rows. No frozen artifact was rewritten or deleted.',
    local_raw_profiles={rel: sha(root / rel) for rel in raw},
    report=m['andrew_report'], no_push=True), indent=2) + '\n')
subprocess.run(['git', 'add', '-f', '--', str(Path(__file__).relative_to(root)),
                str(target.relative_to(root))], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Test private learned-sort identity passes after checked Rank 4 profiles'], cwd=root, check=True)
owned = [r for r in m['hashes'] if r not in raw] + [str(original.relative_to(root)),
    str(target.relative_to(root)), str(Path(__file__).relative_to(root))]
assert not subprocess.run(['git', 'status', '--porcelain', '--', *owned], cwd=root,
    capture_output=True, text=True, check=True).stdout
print('PASS: coherent local checkpoint; raw profiles unchanged, no push or promotion')

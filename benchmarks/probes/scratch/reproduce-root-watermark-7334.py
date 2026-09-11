#!/usr/bin/env python3
"""Bind two failing actual-library regressions before the migration repair."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
campaign = root / 'benchmarks/runs/20260911-7334-root-watermark-regression'
assert not campaign.exists()
campaign.mkdir()
parent = root / '.claude/worktrees/runtime-root-fold-7330'
test = root / '.claude/worktrees/runtime-root-watermark-7333/tests/test_root_watermark.cpp'
objects = sorted((root / 'build/pin-7330-root-fold-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
binary = campaign / 'parent-test-root-watermark'
command = ['/cm/local/apps/gcc/13.1.0/bin/g++', '-std=c++20', '-O3', '-DNDEBUG',
           '-fno-exceptions', '-fno-rtti', '-I' + str(parent / 'src'),
           '-I' + str(parent / 'include'), str(test), *map(str, objects), '-o', str(binary)]
records = []
for label, args in [('build', command), ('watermark', [str(binary), 'watermark']),
                    ('late', [str(binary), 'late'])]:
    result = subprocess.run(args, cwd=campaign, capture_output=True, text=True)
    record = dict(label=label, command=args, returncode=result.returncode,
                  stdout=result.stdout, stderr=result.stderr)
    with (campaign / (label + '.json')).open('x') as out:
        json.dump(record, out, indent=2)
    records.append(record)
    if label == 'build':
        assert result.returncode == 0, record
    else:
        assert result.returncode == -6 and 'Assertion' in result.stderr, record
    print(label, result.returncode, flush=True)
hashes = {str(p.relative_to(root)): sha(p)
          for p in [Path(__file__), test, *objects, *campaign.iterdir()] if p.is_file()}
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='REPRODUCED', hashes=hashes, records=records,
                   classification='Two migration regressions in isolated7330: storage folding suppresses a subsequent new-root Eliminate invitation; archiving a late true-level root changes the logical prefix watermark without an inference. No real benchmark submitted; not a proof rejection.'), out, indent=2)

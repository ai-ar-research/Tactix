#!/usr/bin/env python3
"""Correct heap path attribution and quantify a source-backed storage lead."""
from collections import Counter
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from memory_syscalls import analyze, parse
from native_work import primary_statistics
assert sys.argv[1:] in ([], ['--seal'])
target = root / 'benchmarks/reports/2026-09-11-rank25-root-storage-7320.json'
if '--seal' in sys.argv: assert not target.exists(), 'successful analysis frozen'
c = root / 'benchmarks/runs/20260911-7315-rank25-native-memory'
source = root / '.claude/worktrees/runtime-clause-realloc-7301'
hashes = {}
def bind(p, digest=None):
    value = sha(p)
    assert digest is None or value == digest, p
    hashes[str(p.relative_to(root))] = value
report = c / 'resident-analysis-7316.json'
bind(report)
r = json.loads(report.read_text())
for rel, digest in r['hashes'].items(): bind(root / rel, digest)
events, unknown = parse((c / 'logs/000.memory-syscalls.txt').read_text())
data = analyze(events, unknown, c / 'pins/candidate', 4096)
cut = r['arms']['candidate']['maximum_snapshot']['smaps_before_ns']
changes = [x for x in data['heap_changes'] if x['time_us'] * 1000 < cut]
by_owner = Counter()
for x in changes: by_owner[x['owner']] += x['delta_bytes']
assert sum(by_owner.values()) == 871895040
assert any('failed_literal_apply_chained_binary' in owner for owner in by_owner)
# A relative path cannot match these absolute strace frames. Preserve the
# failed interpretation as an explicit negative control, never normalize raw.
bad = analyze(events, unknown, Path('benchmarks/runs') / c.name / 'pins/candidate', 4096)
assert all(x['owner'] == '<no solver frame>' for x in bad['heap_changes'])
fixture = Path(__file__).with_name('rank25-layout-7320.cpp')
out = root / 'build/rank25-layout-7320'
compiler = '/cm/local/apps/gcc/13.1.0/bin/g++'
if not out.exists():
    subprocess.run([compiler, '-std=c++20', '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti',
                    '-I'+str(source/'src'), '-I'+str(source/'include'), str(fixture), '-o', str(out)], check=True)
layout_output = subprocess.check_output([str(out)], text=True)
layout = {k:int(v) for k,v in (l.split('=') for l in layout_output.splitlines())}
assert layout['ordered_offset'] == 96 and layout['ordered_arena_pointer_offset'] == 112
t = json.loads((c / 'results/000.json').read_text())
n = json.loads((c / 'results/001.json').read_text())
native = primary_statistics(n['solver_statistics'])
line = next(s for s in t['solver_statistics'] if s.startswith('c retired '))
vars_final = int(re.search(r' of (\d+)$', line)[1])
full = root / 'benchmarks/reports/2026-09-11-clause-realloc-full50-7312/summary.json'
bind(full)
row = next(x for x in json.loads(full.read_text())['rows'] if x['rank'] == 25)
for p in (Path(__file__), fixture, out, source/'src/kernel/kernel.cpp',
          source/'src/kernel/kernel.hpp', source/'src/kernel/occ.hpp',
          source/'src/kernel/connected_occurrence.hpp', root/'kissat/src/compact.c'):
    bind(p)
result = dict(hashes=hashes, layout=layout, final_tactix_internal_variables=vars_final,
    selected_final_storage_bytes=vars_final*layout['selected_bytes_per_internal_var'],
    native_statistics=native, rank25_full50=row,
    heap_extent_deltas_by_requesting_stack=by_owner.most_common(),
    heap_extent_changes=changes,
    correction='7319 report falsely said heap-growth stacks had no solver frame. The exploratory query supplied a relative executable path; absolute-path replay resolves them. Raw logs, measured RSS, work/certificates and7318 VMA join remain valid.',
    decision='Root-fixed identities retain full hot array slots in Tactix; native compact.c folds fixed variables into one representative. This is a major structural lead, not a quantified explanation of the entire peak. Confirm peak-time variable populations and live/retained bytes before a cold-storage migration.',
    limitation='Selected final storage is a source/sizeof subtotal, not RSS or peak saved bytes. Heap deltas identify requesting stacks, not current live malloc objects. Different solver trajectories and phase boundaries remain. No solver source or scored pin changed.')
print('layout', layout, 'final vars', vars_final, 'selected final bytes', result['selected_final_storage_bytes'])
print('largest heap requests', by_owner.most_common(5))
print('native', {k:native[k] for k in ('conflicts','decisions','units','eliminated','eliminate_resolutions')})
if '--seal' in sys.argv:
    with target.open('x') as f: json.dump(result, f, indent=2)
    print(target)

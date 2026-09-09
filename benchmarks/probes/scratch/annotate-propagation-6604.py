#!/usr/bin/env python3
"""Read checked cached samples for the unchanged propagation/assignment body."""
from pathlib import Path
source = Path(__file__).with_name('annotate-retention-6530.py')
code = source.read_text()
for old, new in {
    '20260909-6530-rank32-retention-annotation': '20260909-6604-rank32-propagation-annotation',
    'runtime-activation-stamp-6424': 'runtime-material-schedule-6579',
    'source_kernel_watch_exact_current': 'source_propagation_assignment_sections_exact_current',
}.items():
    assert code.count(old) == 1
    code = code.replace(old, new)
begin = code.index("for rel in ('src/kernel/kernel.cpp'")
end = code.index('target.mkdir()', begin)
code = code[:begin] + '''import hashlib
sections = (
    ('src/kernel/kernel.cpp', '// --- propagation engine', 'void kernel_propagate_beyond_conflicts'),
    ('src/kernel/kernel.hpp', 'kernel_assign(Kernel &k,', 'void kernel_backtrack(Kernel &'),
)
for rel, start, stop in sections:
    texts = [(tree / rel).read_text() for tree in (source, current)]
    parts = [s[s.index(start):s.index(stop, s.index(start))] for s in texts]
    assert parts[0] == parts[1], rel
    source_hashes[rel] = dict(source=sha(source / rel), current=sha(current / rel),
        start=start, stop=stop, section_sha256=hashlib.sha256(parts[0].encode()).hexdigest())
''' + code[end:]
begin, end = code.index('symbols = {'), code.index('\noutputs = {}')
code = code[:begin] + '''symbols = {
    'candidate': {'search': 'tactix::kernel_propagate(tactix::Kernel&, unsigned int)',
                  'probing': 'tactix::kernel_propagate_probing(tactix::Kernel&, unsigned int)'},
    'kissat': {'search': 'kissat_search_propagate', 'probing': 'kissat_probing_propagate'},
}''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

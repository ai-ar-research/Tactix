#!/usr/bin/env python3
"""Preserve complete allocation-syscall counts for the exact Release pair."""
from pathlib import Path
base = Path(__file__).with_name('analyze-memory-syscalls-7297.py')
code = base.read_text()
changes = {
    '20260911-7295-memory-syscalls-rank31': '20260911-7310-clause-memory-rank31',
    'mapping-analysis-7297.json': 'mapping-analysis-7313.json',
    "{'candidate': 562920, 'previous': 495156}": "{'candidate': 483992, 'previous': 562920}",
    "for p in (Path(__file__), root / 'tools/bench/memory_syscalls.py',":
        "for p in (Path(__file__), Path(__file__).with_name('analyze-memory-syscalls-7297.py'), root / 'tools/bench/memory_syscalls.py',",
    'Exact Release RSS reproduction makes the external trace usable. Candidate retained heap and later clause-arena growth are the next lead; externally observe process-specific RSS/smaps before assigning the resident peak or editing allocation policy.':
        'Exact Release RSS reproduced on both arms. Compare the whole syscall inventory, not only successful arena mremaps: allocator activity elsewhere can change despite identical solver work.7311 locates the new RSS peak in WatchLists growth. More syscalls do not establish their elapsed cost or prove the allocator policy causing them. Full50 must retain all runtime/memory tradeoffs.',
    'The high-water wall-time location is unknown.':
        'The independent7311 resident analysis brackets peak detection; virtual extents here are not live allocations or an atomic RSS snapshot.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'),
     {'__file__': str(Path(__file__)), '__name__': '__main__'})

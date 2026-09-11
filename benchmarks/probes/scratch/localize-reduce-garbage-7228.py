#!/usr/bin/env python3
"""Partition the larger GC child below the already conserved sparse collector."""
from pathlib import Path
base = Path(__file__).with_name('localize-reduce-collection-7217.py')
code = base.read_text()
for before,after in {
    '2026-09-11-reduce-collection-7217.json':'2026-09-11-reduce-garbage-7228.json',
    '2026-09-11-reduce-envelope-7216.json':'2026-09-11-reduce-collection-7217.json',
    "selected = 'tactix::kernel_sparse_collect_root('":"selected = 'tactix::kernel_collect_garbage('",
    "selected = 'kissat_sparse_collect'":"selected = 'flush_watched_clauses_by_literal'",
    "entry = 'tactix::reduce_apply_refs('":"entry = 'tactix::kernel_sparse_collect_root('",
    "entry = 'kissat_reduce'":"entry = 'kissat_sparse_collect'",
    "inside = (s for s in samples(stream) if any(f.dso == binary and f.symbol.startswith(entry) for f in s.frames))":
        "inside = (s for s in samples(stream) if any(f.dso == binary and f.symbol.startswith(entry) for f in s.frames) and any(f.dso == binary and f.symbol.startswith('kissat_reduce' if native else 'tactix::reduce_apply_refs(') for f in s.frames))",
    'Nested collector partition only below each retained Reduce envelope. Parent/direct-child/leaf scopes overlap and cannot be summed. Different data representation, inlining and wrapper duties preclude exact cross-solver operation prices. No new execution or source edit.':
        'GC child partition only below sparse collection inside each retained Reduce envelope. Native watch flushing is a narrower responsibility, not a matching whole-GC scope. All parent/direct-child/leaf scopes overlap; no additive total or cross-solver operation price. Exact parent conservation mandatory. No new execution or source edit.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

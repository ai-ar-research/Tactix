#!/usr/bin/env python3
"""Use complete demangled symbols: perf annotate does not prefix-match names."""
from pathlib import Path

source = Path(__file__).with_name('annotate-inprocessing-5443.py').read_text()
changes = {
    '20260907-5443-inprocessing-annotation': '20260907-5444-inprocessing-annotation',
    "'tactix::kernel_replace_large_clause_in_place'":
        "'tactix::kernel_replace_large_clause_in_place(tactix::Kernel&, unsigned int, unsigned long, unsigned int const*, unsigned int, unsigned int, char const*, bool, unsigned int)'",
    "'tactix::kernel_rebuild_large_watches'":
        "'tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)'",
    'assert result.returncode == 0, result.stderr':
        "assert result.returncode == 0 and result.stdout.strip() and 'no samples' not in result.stderr, result.stderr",
}
for old, new in changes.items():
    assert source.count(old) == 1
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

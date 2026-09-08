#!/usr/bin/env python3
"""Attribute the existing clean Sweep rewrite samples before choosing a repair."""
from pathlib import Path
source = Path(__file__).with_name('annotate-inprocessing-5443.py').read_text()
source = source.replace('20260907-5443-inprocessing-annotation',
                        '20260908-5472-sweep-rewrite-annotation')
start = source.index('symbols = {')
end = source.index('\nresults = {}', start)
source = source[:start] + '''symbols = {
    'rewrite': 'tactix::SweepSubstitutionSession::rewrite_large_in_place(tactix::Kernel const&, unsigned int, unsigned int const*, unsigned int)',
}''' + source[end:]
source = source.replace('assert result.returncode == 0, result.stderr',
                        "assert result.returncode == 0 and result.stdout.strip() and 'no samples' not in result.stderr, result.stderr")
exec(compile(source, __file__, 'exec'))

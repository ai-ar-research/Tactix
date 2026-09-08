#!/usr/bin/env python3
"""Stage only explicit audited normal evidence and bounded owner launch instructions."""
from pathlib import Path
source = Path(__file__).with_name('stage-factor-support-5607.py').read_text()
source = source.replace('2026-09-08-factor-support-validation.json', '2026-09-08-factor-followup-validation.json')
source = source.replace('2026-09-08-factor-root-support-repair.md', '2026-09-08-factor-full-and-congruence.md')
source = source.replace('2026-09-08-factor-support-main-ctest.log', '2026-09-08-factor-followup-main-ctest.log')
source = source.replace("'PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',", "'PROGRESS.md',")
source = source.replace("'benchmarks/probes/scratch/reference-factor-root-support-5595.c'", "'benchmarks/probes/scratch/claude-periodic-fuzz-5610.txt'")
source = source.replace('range(5592, 5610)', 'range(5610, 5618)')
source = source.replace("if '-5597-' in c.name:", "if any('-' + str(n) + '-' in c.name for n in (5597,5611,5613)):")
exec(compile(source, __file__, 'exec'))

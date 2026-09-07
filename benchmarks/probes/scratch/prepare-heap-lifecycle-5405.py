#!/usr/bin/env python3
"""Narrow the common-heap bracket and record native retirement ordering."""
from pathlib import Path
source = Path(__file__).with_name('prepare-heap-bracket-5404.py').read_text()
source = source.replace('20260907-5404-rank35-heap-bracket', '20260907-5405-rank35-heap-lifecycle')
source = source.replace('requested = [3005, 3071, 8000, 10553, 11837, 13564]', 'requested = [12500, 13000, 13562]')
source = source.replace("'tactix-5404-'", "'tactix-5405-'")
source = source.replace("'--array=0-11'", "'--array=0-5'")
source = source.replace("(c / 'run.py').write_text(runner)", "runner = runner.replace('r = dict(point=point,', \"env[prefix + '_DEACT_TRACE'] = str(c / 'traces' / (label + '.deact.tsv'))\\nr = dict(point=point,\")\n(c / 'run.py').write_text(runner)")
source = source.replace('Locate the earliest differing heap among six stable-mode checkpoints before the known Rank-35 choice split; observe existing source, never alter selection.', 'Narrow the first differing physical heap between conflict 11838 and 13565; compare retirement order and common decision prefixes before blaming Rephase.')
source = source.replace('12 clean stopped runs', '6 clean stopped runs')
exec(compile(source, __file__, 'exec'))

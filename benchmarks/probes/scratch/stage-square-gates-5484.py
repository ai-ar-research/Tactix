#!/usr/bin/env python3
"""Stage only owned checkpoint evidence and explicit non-fuzz campaigns."""
from pathlib import Path
source = Path(__file__).with_name('stage-watch-measurement-5481.py').read_text()
source = source.replace('freeze-watch-measurement-5481.py', 'freeze-square-gates-5484.py')
source = source.replace('stage-watch-measurement-5481.py', 'stage-square-gates-5484.py')
source = source.replace('2026-09-08-watch-pair-measured.md', '2026-09-08-square-phase-and-checked-gates.md')
source = source.replace('2026-09-08-watch-pair-measured-validation.json', '2026-09-08-square-gates-validation.json')
source = source.replace('2026-09-08-watch-pair-measured-main-ctest.log', '2026-09-08-square-gates-main-ctest.log')
source = source.replace('5470,5471,5475,5476,5478,5479):', '5470,5471,5475,5476,5478,5479,5482):')
source = source.replace('assert all((root / p).is_file() for p in paths)', """paths += ['benchmarks/probes/scratch/' + n for n in
          ('prepare-square-phase-5482.py', 'compare-square-phase-5482.py',
           'localize-rank31-boundary-5483.py')]
c = root / 'benchmarks/runs/20260908-5482-square-phase'
paths += [str((c / n).relative_to(root)) for n in ('run.py', 'array.sbatch')]
assert all((root / p).is_file() for p in paths)""")
exec(compile(source, __file__, 'exec'))

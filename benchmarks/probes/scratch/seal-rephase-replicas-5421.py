#!/usr/bin/env python3
"""Finalize all nine replicas through the original exact timing oracle."""
from pathlib import Path

directory = Path(__file__).resolve().parent
c = directory.parents[1] / 'runs/20260907-5421-rephase-root-rank35-runtime'
count = len(list((c / 'results').glob('*.json')))
if count != 9:
    print('5421 checking', count, 'of 9'); raise SystemExit(0)
if (c / 'summary.json').exists():
    print('5421 already sealed'); raise SystemExit(0)
source = (directory / 'snapshot-rephase-runtime-5421.py').read_text()
source = source.replace("dest = c / 'solver-checkpoint.json'", "assert result['checked_clean'] == 9\ndest = c / 'summary.json'")
source = source.replace('Solver timing checkpoint; pending proofs are provisional passes for investigation only.',
                        'All nine expected UNSAT results have independently checked proofs and clean scratch.')
exec(compile(source, __file__, 'exec'))

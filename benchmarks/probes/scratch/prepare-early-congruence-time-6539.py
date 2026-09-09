#!/usr/bin/env python3
"""Corrected separate capture; prior6535 and its checker remain immutable."""
from pathlib import Path
source = Path(__file__).with_name('prepare-congruence-time-6535.py')
code = source.read_text()
for old, new in {
    '20260909-6535-rank32-congruence-time': '20260909-6539-rank32-early-congruence-time',
    "runner = (parent / 'pins/runtime_task.py').read_text()":
        "runner_source = root / 'tools/bench/runtime_task.py'\nrunner = runner_source.read_text()",
    "'prior_audit': root / 'benchmarks/reports/2026-09-09-retention-attribution-audit-6533.json',":
        "'prior_audit': root / 'benchmarks/reports/2026-09-09-retention-attribution-audit-6533.json',\n        'runner_source': runner_source,\n        'prior_capture': root / 'benchmarks/runs/20260909-6535-rank32-congruence-time/manifest.json',",
    'One diagnostic solve, not three-wave runtime acceptance; no Kissat rerun or reference refresh.':
        'Corrected second capture after6535 omitted stderr from its provisional payload.6535 checker remains running; do not mutate/restart it or inspect its scratch. No Kissat rerun, reference refresh or timing acceptance.',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

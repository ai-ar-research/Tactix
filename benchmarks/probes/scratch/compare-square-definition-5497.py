#!/usr/bin/env python3
"""Preserve the failed implication-mode expectation; validate phase successor."""
from pathlib import Path
source = Path(__file__).with_name('compare-square-definition-5496.py').read_text()
start = source.index("source = Path(__file__).with_name('compare-square-imply-5487.py')")
end = source.index('\nsnapshots = {}', start)
source = source[:start] + '''source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5497-square-definition-phase').replace('5436 pending', '5497 pending')
exec(compile(source, __file__, 'exec'))
old = root / 'benchmarks/runs/20260908-5482-square-phase'
assert result == json.loads((old / 'summary.json').read_text())
''' + source[end:]
source = source.replace("('imply.tsv', 'decisions.tsv'", "('phase.tsv', 'decisions.tsv'")
exec(compile(source, __file__, 'exec'))

#!/usr/bin/env python3
"""Fresh scoped normal-campaign audit for the validated Vivify producer interval."""
from pathlib import Path
source = Path(__file__).with_name('audit-bve-reinsert-order-5999.py')
code = source.read_text()
changes = {
    '2026-09-08-bve-reinsert-order-validation.json': '2026-09-08-vivify-pose-validation.json',
    '2026-09-08-bve-reinsert-order.md': '2026-09-08-vivify-pose-origin.md',
    '(5969, 5988, 5992, 5996, 6000)': '(5996, 6000, 6008)',
    '1673402,1675162,1675706,1675801,1676156,1676209': '1673402,1675706,1675801,1676281',
    '1675996,1676156': '1676209,1676281',
    'validate-bve-reinsert-order-5998.py': 'validate-clause-pose-series-6009.py',
    '5962 full40 remains1.116453x fixedKissat,not a runtimegain.5992 fullreplay validates first5409 reinsertion difference;5996 tests adjacent5415 order and actual Tactix clause producer. No solver change orpromotion.':
        '6008 fullreplay localizes selected-clause pose drift tosecondVivify270978;6000 rulesoutforward.6004 observer98/98/nativecontrolsPASS;fixture-only NDEBUG ABI failure closed6011 before submission.5962 remains1.116453x,no gain orpromotion.',
}
for old, new in changes.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

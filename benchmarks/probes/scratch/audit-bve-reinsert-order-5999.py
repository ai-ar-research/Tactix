#!/usr/bin/env python3
"""Fresh normal-job checkpoint; preserve prior audits and all fuzz ownership."""
from pathlib import Path
source = Path(__file__).with_name('audit-bve-runtime-5991.py')
code = source.read_text()
changes = {
    '2026-09-08-bve-runtime-validation.json': '2026-09-08-bve-reinsert-order-validation.json',
    '2026-09-08-bve-runtime-and-repeated-pop.md': '2026-09-08-bve-reinsert-order.md',
    '(5969, 5972, 5978, 5988)': '(5969, 5988, 5992, 5996, 6000)',
    '1673402,1675162,1675706,1675801,1675945': '1673402,1675162,1675706,1675801,1676156,1676209',
    '1675774,1675800,1675845,1675945': '1675996,1676156',
    'validate-bve-repeat-heap-5987.py': 'validate-bve-reinsert-order-5998.py',
    '5962 full40 measures1.116453x fixedKissat;allcertificates/otherstats exactparent except intendedcounters. No runtimegain.5978 firstoccurrence62185 heapexact doesnotcover later8966-membertarget;new5985 repeatedobserver97/97/nativecontrolsPASS,5988 runs. No promotion.':
        '5962 full40 remains1.116453x fixedKissat,not a runtimegain.5992 fullreplay validates first5409 reinsertion difference;5996 tests adjacent5415 order and actual Tactix clause producer. No solver change orpromotion.',
}
for old, new in changes.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

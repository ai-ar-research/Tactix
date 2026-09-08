#!/usr/bin/env python3
"""Fresh normal-campaign audit; retain all existing proof and fuzz ownership."""
from pathlib import Path
source = Path(__file__).with_name('audit-vivify-pose-6013.py')
code = source.read_text()
changes = {
    '2026-09-08-vivify-pose-validation.json': '2026-09-08-vivify-candidate-validation.json',
    '2026-09-08-vivify-pose-origin.md': '2026-09-08-vivify-candidate-origin.md',
    '(5996, 6000, 6008)': '(6008, 6020, 6024)',
    '1673402,1675706,1675801,1676281': '1673402,1675706,1675801,1676418,1676453',
    '1676209,1676281': '1676418,1676453',
    'validate-clause-pose-series-6009.py': 'validate-vivify-pose-6019.py',
    '6008 fullreplay localizes selected-clause pose drift tosecondVivify270978;6000 rulesoutforward.6004 observer98/98/nativecontrolsPASS;fixture-only NDEBUG ABI failure closed6011 before submission.5962 remains1.116453x,no gain orpromotion.':
        '6020 fullreplay identifies native-only learned candidate2249 as selected-pose mutator in secondVivify270978;6024 traces its earlier presence.6016 diagnostics99/99/nativecontrolsPASS.5962 remains1.116453x,no implementation change,gain orpromotion.',
}
for old, new in changes.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

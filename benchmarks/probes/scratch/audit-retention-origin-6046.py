#!/usr/bin/env python3
"""New scoped audit; frozen campaigns and prior audits remain unchanged."""
from pathlib import Path
source = Path(__file__).with_name('audit-vivify-candidate-6027.py')
code = source.read_text()
changes = {
    '2026-09-08-vivify-candidate-validation.json': '2026-09-08-retention-origin-validation.json',
    '2026-09-08-vivify-candidate-origin.md': '2026-09-08-retention-origin.md',
    '(6008, 6020, 6024)': '(6035, 6038, 6042)',
    '1673402,1675706,1675801,1676418,1676453': '1673402,1675706,1675801,1676555,1676601,1676657',
    '1676418,1676453': '1676555,1676601,1676657',
    'validate-vivify-pose-6019.py': 'validate-retention-history-6044.py',
    '6020 fullreplay identifies native-only learned candidate2249 as selected-pose mutator in secondVivify270978;6024 traces its earlier presence.6016 diagnostics99/99/nativecontrolsPASS.5962 remains1.116453x,no implementation change,gain orpromotion.':
        '6035 proves common learned clause creation245473;6038 proves its deletion atReduce54 because one extra candidate changes victim count.6042 extends the ancestry trace.6030 diagnostics100/100/nativecontrolsPASS.5962 remains1.116453x,BC66/full40/reference40 checked;no implementation change,gain orpromotion.',
}
# Only substitute the generated script's new values, preserving its old anchors.
import re
for old in changes: assert old in code, old
code = re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)), lambda m: changes[m.group()],code)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Inspect the watch list preceding the first reordered implication pair."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-reason-watch-6693.py')
code=source.read_text()
changes={
    '20260909-6688-rank46-earlier-window':'20260909-6705-rank46-propagation-split',
    '20260909-6693-rank46-reason-watch':'20260909-6708-rank46-implication-watch',
    'summary-6690.json':'summary-6707.json',
    "'tactix-6688-'":"'tactix-6705-'", "'tactix-6693-'":"'tactix-6708-'",
    '6688-rank49-prefix':'6705-rank49-prefix','6693-rank46-reason-watch':'6708-rank46-implication-watch',
    '6849':'7931','108084':'107104','399066':'393591','1195':'1267',
    '-16365':'-18659','108080..108170':'107100..107106',
    'First7931 implication rows in107100..107106 match; both next imply16358 with different12-literal reasons107104/393591/level1267. Immediately preceding shared binaries have16365 as false antecedent. Inspect complete-18659 watch list before attribution; later actual implication order diverges108122 and clause poses diverge108138.':
      'First7931 implication rows match in6707; next two implications18623/18609 swap at107104/393591/level1267, each with its own14-literal reason. Last common implication is-18659. Inspect that watch list to test whether both clauses are unit and their relative order differs. Later-19005 value difference is downstream; no cursor bug established.',
    '207 benchmark tests apply':'213 benchmark tests apply',
}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

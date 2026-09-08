#!/usr/bin/env python3
"""Scoped current normal-job audit, preserving all prior result authorities."""
from pathlib import Path
import re
source=Path(__file__).with_name('audit-forward-preclean-5885.py')
code=source.read_text()
changes={
    '2026-09-08-forward-preconnection-validation.json':'2026-09-08-composition-runtime-validation.json',
    '(5784, 5811, 5836, 5838, 5855, 5858, 5878, 5880)':'(5784, 5855, 5858, 5878, 5880, 5906, 5908, 5924, 5926)',
    '(5878, 5880)':'(5906, 5908, 5924, 5926)',
    '(5849, 5870)':'(5849, 5870, 5900, 5920)',
    '(5869, 5879, 5883)':'(5917, 5925, 5931, 5932, 5937)',
    '1673402,1673816,1674055,1674132,1674338,1674410,1674668,1674669,1674700,1674781':
        '1673402,1674338,1674410,1674668,1674700,1674929,1674992,1675144,1675162,1675374',
    '1674504,1674639,1674669,1674781':'1675145,1675329,1675330,1675374',
    '2026-09-08-forward-preconnection-cleanup.md':'2026-09-08-composition-runtime-and-formula.md',
    '20260908-5870-forward-preclean/manifest.json':'20260908-5900-reduce-root/manifest.json',
    '20260908-5869-rank32-reduce-frontier/reduce-validation.json':'20260908-5931-second-forward-begin-formula/summary.json',
    '5870 tested isolated repair;runtime/causal gates pending.5869 exact Reduce selection match narrows Rank32 next diagnosis. No goal completion or promotion.':
        'Reject5920 composition atfull1.176794x/C1.017340x;retain5900 at1.112038x. Neutral formula snapshots show172 incoming differences per arm before secondforward;new5937 on5900 brackets firstBVE. No implementation change orpromotion this turn.',
}
code=re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)),lambda m:changes[m.group()],code)
anchor="hashes = {str(classification.relative_to(root)): sha(classification)}"
assert anchor in code
code=code.replace(anchor,anchor+"\nextra_classification = root / 'benchmarks/reports/2026-09-08-trivial-checker-status-second.json'\nextra_known = json.loads(extra_classification.read_text())\nfor rel, digest in extra_known['hashes'].items(): assert sha(root / rel) == digest\nhashes[str(extra_classification.relative_to(root))] = sha(extra_classification)")
anchor="assert number == 5784 and p.stem in ('828', '829'), p"
assert anchor in code
code=code.replace(anchor,"assert number == 5784 and p.stem in ('828', '829', '1280', '1281'), p\n            incident = known if p.stem in ('828','829') else extra_known")
code=code.replace("r['instance'] == known['instance']","r['instance'] == incident['instance']")
code=code.replace("== known['hashes'][str((c / 'pins/drat-trim').relative_to(root))]","== incident['hashes'][str((c / 'pins/drat-trim').relative_to(root))]")
code=code.replace('Only exact5784 tasks828/829 matched the previously reproduced input-empty-clause checker status bug;both stay invalid/unscored.',
    'Only exact5784 tasks828/829 and1280/1281 match their separately verified input-empty-clause checker-status incidents;all four stay invalid/unscored.')
anchor="('reduce-comparator-tests', ['python3', 'tests/test_compare_reduce_trace.py'])"
assert anchor in code
code=code.replace(anchor,anchor+", ('first-bve-parser-tests', ['python3', 'benchmarks/probes/scratch/compare-first-bve-frontier-5939.py', '--self-test'])")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Audit frozen normal campaigns and current docs, excluding fuzz-owned data."""
from pathlib import Path
import re
source = Path(__file__).with_name('audit-forward-preclean-5885.py')
code = source.read_text()
changes = {
    '2026-09-08-forward-preconnection-validation.json':'2026-09-08-reduce-root-runtime-validation.json',
    '(5784, 5811, 5836, 5838, 5855, 5858, 5878, 5880)':
        '(5784, 5855, 5858, 5878, 5880, 5906, 5908, 5924, 5926)',
    '(5878, 5880)':'(5906, 5908, 5924, 5926)',
    '(5849, 5870)':'(5849, 5870, 5900, 5920)',
    '(5869, 5879, 5883)':'(5891, 5893, 5907, 5910, 5917, 5925)',
    '1673402,1673816,1674055,1674132,1674338,1674410,1674668,1674669,1674700,1674781':
        '1673402,1674338,1674410,1674668,1674700,1674929,1674992,1675144,1675145,1675162',
    '1674504,1674639,1674669,1674781':'1674840,1674849,1674930,1675028,1675097,1675145',
    '2026-09-08-forward-preconnection-cleanup.md':'2026-09-08-reduce-root-runtime.md',
    '20260908-5870-forward-preclean/manifest.json':'20260908-5920-reduce-preclean/manifest.json',
    '20260908-5869-rank32-reduce-frontier/reduce-validation.json':
        '20260908-5917-reduce-root-next-choice/choice-validation.json',
    '5870 tested isolated repair;runtime/causal gates pending.5869 exact Reduce selection match narrows Rank32 next diagnosis. No goal completion or promotion.':
        '5900 full40 improves to1.112038x fixedKissat;BC B0.744014x/C0.929844x. Rank21/37 regressions remain open. Validated5917 locates second forward/BVE discrepancy before next choice. New5920 composition adds interaction test,four complete suites and46 checked proofs;fresh BC/full/prefix gates pending. No promotion or completion.',
}
code = re.sub('|'.join(re.escape(k) for k in sorted(changes, key=len, reverse=True)),
              lambda m: changes[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})

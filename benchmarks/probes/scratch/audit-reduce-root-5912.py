#!/usr/bin/env python3
"""Scoped normal-campaign audit for the independent Reduce repair."""
from pathlib import Path
import re
source=Path(__file__).with_name('audit-forward-preclean-5885.py')
code=source.read_text()
changes={
 '2026-09-08-forward-preconnection-validation.json':'2026-09-08-reduce-root-validation.json',
 '(5784, 5811, 5836, 5838, 5855, 5858, 5878, 5880)':'(5784, 5855, 5858, 5878, 5880, 5906, 5908)',
 '(5878, 5880)':'(5906, 5908)',
 '(5849, 5870)':'(5849, 5870, 5900)',
 '(5869, 5879, 5883)':'(5891, 5893, 5907, 5910)',
 '1673402,1673816,1674055,1674132,1674338,1674410,1674668,1674669,1674700,1674781':'1673402,1674338,1674410,1674668,1674700,1674929,1674930,1674992,1675028',
 '1674504,1674639,1674669,1674781':'1674840,1674849,1674930,1675028',
 '2026-09-08-forward-preconnection-cleanup.md':'2026-09-08-reduce-root-guidance.md',
 '20260908-5870-forward-preclean/manifest.json':'20260908-5900-reduce-root/manifest.json',
 '20260908-5869-rank32-reduce-frontier/reduce-validation.json':'20260908-5891-rank32-reorder-frontier/choice-validation.json',
 '5870 tested isolated repair;runtime/causal gates pending.5869 exact Reduce selection match narrows Rank32 next diagnosis. No goal completion or promotion.':
   'Independent5900 repairs Reduce root-guidance lifecycle on5849,not regressing5870. Four suites and46proofs plusactual old/new/native controls pass;BC/full/Rank32 gates pending.5893 localizes next5870 Rank31 split394397;5910 diagnostic pending. No goal completion or promotion.',
}
code=re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)),lambda m:changes[m.group()],code)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

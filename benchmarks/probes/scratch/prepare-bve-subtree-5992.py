#!/usr/bin/env python3
"""Trace the32-position witness back through its differing subtree members."""
from pathlib import Path
source=Path(__file__).with_name('prepare-bve-sift-ancestry-5972.py')
code=source.read_text()
changes={
 '20260908-5972-bve-sift-ancestry':'20260908-5992-bve-subtree-ancestry',
 'tactix-5972-':'tactix-5992-',
 '5972-bve-sift-ancestry':'5992-bve-subtree-ancestry',
 "subjects = '225614,127295,129082,40787,107019,41090,60705,89339'":
 "subjects = '225614,60705,477747,244396,5409,3343,3531,242204'",
 '20260908-5958-bve-full-heap/summary.json':'20260908-5988-bve-repeat-heap/summary.json',
 'The first different sift choice is at index278: right child129082 in Tactix versus127295 in native. Trace both and adjacent displaced ancestors back to their first different heap mutation.':
 'At the SECOND pop462198 with8966 members,32 positions differ,all other fields agree. First different chosen child atindex278 is335015 versus477747;right child244396 versus477747. Trace these and displaced ancestors to earlier mutation. Distinguish repeated trigger occurrences by actual state,not variable identity alone.',
}
for old,new in changes.items():
 assert old in code,old
 code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Complete unsubmitted pins after correcting the fixture's Proof API call."""
from pathlib import Path
import re
source=Path(__file__).with_name('complete-forward-caller-builds-5854.py')
code=source.read_text().replace('forward-caller','bve-active-pop').replace('-final','-retry')
mapping={'5849':'5962','5850':'5963','5851':'5964','5854':'5965'}
code=re.sub('|'.join(mapping),lambda m:mapping[m.group()],code)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

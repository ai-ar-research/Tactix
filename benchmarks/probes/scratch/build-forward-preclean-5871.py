#!/usr/bin/env python3
"""Build three independent Release/trace images and an active-journal image."""
from pathlib import Path
import re

here = Path(__file__).resolve().parent
path = here / 'complete-forward-caller-builds-5854.py'
code = path.read_text().replace('forward-caller', 'forward-preclean')
mapping = {'5849': '5870', '5850': '5871', '5851': '5872', '5854': '5873'}
code = re.sub('|'.join(mapping), lambda m: mapping[m.group()], code)
exec(compile(code, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})

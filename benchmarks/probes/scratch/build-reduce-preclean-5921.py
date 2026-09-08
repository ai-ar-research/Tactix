#!/usr/bin/env python3
"""Build fresh combined images and run every discovered unit test."""
from pathlib import Path
import re
source = Path(__file__).with_name('complete-forward-caller-builds-5854.py')
code = source.read_text().replace('forward-caller', 'reduce-preclean')
mapping = {'5849': '5920', '5850': '5921', '5851': '5922', '5854': '5923'}
code = re.sub('|'.join(mapping), lambda m: mapping[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

#!/usr/bin/env python3
"""Build and test independent policy-root cleanup images."""
from pathlib import Path
import re
source = Path(__file__).with_name('complete-forward-caller-builds-5854.py')
code = source.read_text().replace('forward-caller', 'reduce-root')
mapping = {'5849': '5900', '5850': '5901', '5851': '5902', '5854': '5903'}
code = re.sub('|'.join(mapping), lambda m: mapping[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

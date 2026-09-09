#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('check-vivify-references-6273.py')
code = source.read_text().replace('20260909-6273-vivify-binary-references', '20260909-6274-vivify-binary-references')
anchor = "    command = [compiler,"
assert code.count(anchor) == 1
code = code.replace(anchor, "    if arm == 'native': fixture = fixture.with_name('test-native-vivify-binary-6274.c')\n" + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

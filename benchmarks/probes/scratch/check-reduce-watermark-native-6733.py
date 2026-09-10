#!/usr/bin/env python3
"""Successor fixes the fixture's clause pointer and available counter spelling."""
from pathlib import Path
source=Path(__file__).with_name('check-reduce-watermark-native-6732.py')
code=source.read_text()
assert code.count('6732')==2
exec(compile(code.replace('6732','6733'),str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

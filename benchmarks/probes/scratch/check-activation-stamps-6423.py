#!/usr/bin/env python3
"""Preserve6422 compiler failure; native IDX assertion requires solver symbol."""
from pathlib import Path
source=Path(__file__).with_name('check-activation-stamps-6422.py')
code=source.read_text()
for old,new in {'20260909-6422-activation-stamp-control':'20260909-6423-activation-stamp-control',
                'test-native-activation-stamps-6422.c':'test-native-activation-stamps-6423.c'}.items():
    assert code.count(old)==1; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

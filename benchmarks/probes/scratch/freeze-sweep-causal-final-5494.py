#!/usr/bin/env python3
"""Repeat final audit after fixing the PDF overflow; retain its first test copy."""
from pathlib import Path
source = Path(__file__).with_name('freeze-sweep-causal-5493.py').read_text()
source = source.replace('2026-09-08-sweep-causal-main-ctest.log',
                        '2026-09-08-sweep-causal-final-main-ctest.log')
exec(compile(source, __file__, 'exec'))

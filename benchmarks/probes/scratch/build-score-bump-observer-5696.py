#!/usr/bin/env python3
"""Build independent trace copies and the complete fixed Tactix test suite."""
from pathlib import Path
source=Path(__file__).with_name('build-factor-observer-5456.py').read_text()
source=source.replace('20260907-5456-factor-observer','20260908-5696-score-bump-observer')
source=source.replace('runtime-factor-observer-5456','runtime-score-bump-observer-5696')
source=source.replace('kissat-factor-observer-5456','kissat-score-bump-observer-5696')
source=source.replace('pin-5456-factor-observer-trace','pin-5696-score-bump-observer-trace')
exec(compile(source,__file__,'exec'))

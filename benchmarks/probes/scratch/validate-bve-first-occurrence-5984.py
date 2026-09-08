#!/usr/bin/env python3
"""Classify5978's valid first-occurrence capture, not the later target pop."""
from pathlib import Path
source = Path(__file__).with_name('compare-bve-full-heap-5960.py')
code = source.read_text()
for old, new in {'20260908-5958-bve-full-heap': '20260908-5978-bve-earlier-heap',
                 "target=C/'summary.json'": "target=C/'first-occurrence-validation.json'",
                 '122629': '462198', '225614': '435552', '4465': '20576',
                 '4472': '20576', '165965': '7827',
                 'Complete pre-pop heaps and replayed actual selected positions. Sift reconstruction explains placement but does not identify the earlier mutation or authorize tie/score changes.':
                 '5978 captured the FIRST pop of462198, at62185 members, not the later8966-member target. All ordinary traces/proof/attempts replay. This validates only the first-occurrence state;5979 incorrectly assumed the watched60705 was the tail of this pop. New observer must distinguish repeated pops before interpreting the intended witness.'}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

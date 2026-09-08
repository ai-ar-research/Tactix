#!/usr/bin/env python3
"""Exercise actual schedule construction callbacks, including every round."""
from pathlib import Path
source=Path(__file__).with_name('test-bve-heap-observer-5944.py')
code=source.read_text().replace('kissat-bve-heap-observer-5944','kissat-bve-heap-lifecycle-5948').replace('20260908-5944-bve-heap-observer-controls/repaired','20260908-5948-bve-heap-lifecycle-controls')
code=code.replace("out.mkdir(exist_ok=False)","out.mkdir(exist_ok=False)")
code=code.replace("str(Path(__file__).with_suffix('.c'))","str(Path(__file__).with_name('test-bve-heap-lifecycle-5948.c'))")
code=code.replace(",\'missed-initial\'",'')
code=code.replace('eight rejection controls','seven rejection controls plus initial-construction and per-round-reset regression')
code=code.replace('Initial control crashed before the observer: ASan located null heap->pos in kissat_enlarge_heap, called by kissat_update_heap. Match eliminate.c schedule_variables by reserving schedule capacity before scored imports. Observer source unchanged; original control binaries retained.',
                  '5944 missed native construction updates before the initial snapshot.5945/job1675510 cancelled before interpretation;both tasks terminal CANCELLED.5948 resets at schedule-begin and ignores only incomplete construction. This actual-call-sequence fixture fails on5944 and passes5948. Separate prior fixture allocation repair retained.')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Price all Sweep-copy timing waves, including adverse RSS and controls."""
from pathlib import Path
source = Path(__file__).with_name('summarize-vivify-cost-6849.py')
code = source.read_text()
for before,after in {
    '20260910-6831-vivify-snapshot-runtime':'20260910-6850-sweep-rootfree-runtime',
    'solver-checkpoint-6833.json':'solver-checkpoint-6852.json',
    'timer-cost-6849.json':'timer-cost-6868.json',
    'vivify':'sweep',
    'Vivify':'Sweep',
    'No reliable total-runtime gain: Rank32 is slower in all three pairs,39 is near neutral and38 mixed. Do not adopt6822 or repeat this campaign to select favorable noise. The local RSS decrease is recorded, not a general memory-parity claim. Pending independent certificates remain open;6730 stays current.':
        'Rank39 improves across three pairs but32 and38 are near neutral; Rank32 RSS rises. Record all timing and memory outcomes; insufficient broad tradeoff for adoption. Do not repeat to select favorable noise.6730 remains current,6838 unadopted. Pending proofs remain pending; no memory/full50/fuzz acceptance.',
    'Strict6833':'Strict6852',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

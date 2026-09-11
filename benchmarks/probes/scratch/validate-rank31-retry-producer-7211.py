#!/usr/bin/env python3
"""Exact full7073 replay plus strict selected producer records; seal once."""
from pathlib import Path
base = Path(__file__).with_name('validate-rank31-bve-priority-7075.py')
code = base.read_text()
changes = {
    '20260910-7073-rank31-bve-priority': '20260911-7210-rank31-retry-producer',
    'solver-summary-7075.json': 'solver-summary-7211.json',
    "h.fixtures(); b.fixtures()": "h.fixtures(); b.fixtures()\nproducer_reader = module('producer_reader', 'localize-collect-retry-7032.py'); producer_reader.fixtures()",
    "('macro.tsv', 'boundaries.tsv', 'bve-candidates.tsv')":
        "('macro.tsv', 'boundaries.tsv', 'bve-candidates.tsv', 'bve-heap.tsv')",
    'observed = {}': 'observed = {}; producers = {}',
    "    p = c / 'traces' / (arm + '.bve-heap.tsv'); bind(p)":
        "    journal = c / 'traces' / (arm + '.retry-producer.tsv'); bind(journal)\n    content = journal.read_text()\n    producers[arm] = producer_reader.producers(content, 'tactix', 355342) if content else []\n    assert all(0 <= v['conflicts'] <= 5187498 for v in producers[arm])\n    p = c / 'traces' / (arm + '.bve-heap.tsv'); bind(p)",
    'observed=observed,': 'observed=observed, producers=producers,',
    'Exact full Release work/certificate and all7068 macro/boundary/attempt bytes precede selected heap interpretation. These four subjects do not prove complete schedule/formula identity or establish the earliest native divergence. Proof acceptance and cleanup remain pending until published checked/clean records.':
        'Exact full own Release work/certificate and all7073 macro/boundary/attempt/heap bytes precede selected producer interpretation. Selected marks are not complete prior attempts/formula/native state. Proof acceptance and cleanup require published checked/clean results.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

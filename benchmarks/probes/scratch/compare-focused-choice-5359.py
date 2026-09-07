#!/usr/bin/env python3
"""Verify observer neutrality and retain the exact next queue-state witness."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
runs=root/'benchmarks/runs'
old=runs/'20260907-5355-empty-factor-prefix'
for number,name,job in [(5358,'next-choice',1668806),(5359,'focused-choice',1668808)]:
    c=runs/f'20260907-{number}-{name}'
    m=json.loads((c/'manifest.json').read_text())
    for rel,sha in m['hashes'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha
    states={}
    for task,arm in enumerate(('tactix','kissat')):
        log=(c/'logs'/f'{job}_{task}.log').read_text()
        assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
        decisions=(c/'traces'/(arm+'.decisions.tsv')).read_bytes()
        expected=b''.join(l for l in (old/'traces'/(arm+'.decisions.tsv')).read_bytes().splitlines(keepends=True) if int(l.split()[0])<=133243)
        assert decisions==expected
        rows=[l.split('\t') for l in (c/'traces'/(arm+'.state.tsv')).read_text().splitlines()]
        if number==5359:
            previous=runs/'20260907-5358-next-choice'
            assert (c/'traces'/(arm+'.prefix.tsv')).read_bytes()==(previous/'traces'/(arm+'.prefix.tsv')).read_bytes()
            assert [r[:10] for r in rows]==[l.split('\t') for l in (previous/'traces'/(arm+'.state.tsv')).read_text().splitlines()]
            assert all(r[10]=='0' and len(r)==18 for r in rows)
        states[arm]=[r for r in rows if int(r[1]) in (327446,327447)]
    a,b=states['tactix'][0],states['kissat'][0]
    assert a[:10]==b[:10] and a[5]=='0' and a[7]=='9582'
    summary=dict(stopped_clean=2,decision_prefix_byte_identical_5355=True,
        previous_columns_unchanged=True if number==5359 else None,
        contexts=states,
        columns=m.get('state_columns','conflicts decisions level chosen observed value assignment_level heap_position score reason'.split()),
        conclusion='Observed 21954 is equally unassigned and EVSIDS-ranked before the split. Focused queue order, not EVSIDS, is the next producer to trace.' if number==5358 else 'Both focused; queue cursor selects the same 21867 before the split, but native puts 21954 immediately before 21867 while Tactix has it earlier between 21281 and 21957. Trace queue bumps/publication before 133243; no selection hack.',
        trace_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()})
    dest=c/'summary.json'
    if dest.exists():assert json.loads(dest.read_text())==summary
    else:dest.write_text(json.dumps(summary,indent=2)+'\n')
    print(number,summary['conclusion'])

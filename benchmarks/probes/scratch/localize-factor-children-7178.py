#!/usr/bin/env python3
"""Disjoint direct-child call sites and leaves within saved Factor envelopes."""
from collections import Counter, defaultdict
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import Frame, Sample, samples

def partition(stream, binary, symbol, callsites, instruction_pcs=()):
    total = selected = 0
    children = defaultdict(lambda: dict(period=0, sites=Counter(), leaves=Counter()))
    for sample in stream:
        assert sample.period > 0 and sample.frames
        total += sample.period
        matches = [i for i, f in enumerate(sample.frames) if f.dso == binary and f.symbol == symbol]
        if not matches: continue
        assert len(matches) == 1, 'recursive envelope needs explicit policy'
        index = matches[0]; selected += sample.period
        if index:
            child = sample.frames[index-1]
            name = child.symbol + ' [' + child.dso + ']'
            pc = sample.frames[index].pc
            if pc in callsites:
                site = hex(pc)
            else:
                # An unresolved unwinder frame can precede a genuine sampled
                # instruction in the parent. Retain its weight but do not
                # invent a call-site attribution for it.
                assert child.symbol == child.dso == '[unknown]' and pc in instruction_pcs, (hex(pc), name)
                site = 'unresolved-frame@' + hex(pc)
        else:
            name, site = '<entry self>', '<self>'
        leaf = sample.frames[0]
        row = children[name]
        row['period'] += sample.period
        row['sites'][site] += sample.period
        row['leaves'][leaf.symbol + ' [' + leaf.dso + ']'] += sample.period
    assert selected == sum(r['period'] for r in children.values())
    for r in children.values(): assert r['period'] == sum(r['sites'].values()) == sum(r['leaves'].values())
    return dict(total_period=total, selected_period=selected, children=dict(children))

def self_test():
    f = lambda pc, symbol, dso='solver': Frame(pc, symbol, dso)
    stream = [Sample(2, (f(1, 'entry'),)), Sample(3, (f(2, 'child'), f(14, 'entry'))),
              Sample(5, (f(3, 'malloc', 'libc'), f(2, 'child'), f(14, 'entry'))),
              Sample(7, (f(1, 'entry', 'other'),))]
    r = partition(stream, 'solver', 'entry', {14: 'call child'})
    assert r['total_period'] == 17 and r['selected_period'] == 10
    assert r['children']['child [solver]']['period'] == 8
    assert r['children']['child [solver]']['leaves']['malloc [libc]'] == 5
    unresolved = [Sample(11, (f(0, '[unknown]', '[unknown]'), f(9, 'entry')))]
    u = partition(unresolved, 'solver', 'entry', {14: 'call child'}, {9})
    assert u['children']['[unknown] [[unknown]]']['sites'] == {'unresolved-frame@0x9': 11}
    for bad in ([Sample(0, (f(1,'entry'),))], [Sample(1, ())],
                [Sample(1,(f(2,'child'),f(15,'entry')))],
                [Sample(1,(f(1,'entry'),f(14,'entry')))], unresolved):
        try: partition(bad, 'solver', 'entry', {14: 'call child'})
        except AssertionError: pass
        else: raise AssertionError('invalid sample attribution accepted')
    print('Factor child partition: self/descendant/foreign DSO/explicit unresolved and five negative controls PASS')

def main():
    self_test(); assert sys.argv[1:] in ([], ['--self-test'])
    if sys.argv[1:]: return
    target = root / 'benchmarks/reports/2026-09-10-factor-child-sites-7178.json'
    assert not target.exists()
    pcpath = root / 'benchmarks/runs/20260910-7170-factor-pc-localization/summary.json'
    envpath = root / 'benchmarks/reports/2026-09-10-factor-envelope-7171.json'
    pcs = json.loads(pcpath.read_text()); envelope = json.loads(envpath.read_text())
    hashes = {str(p.relative_to(root)): sha(p) for p in (pcpath, envpath, Path(__file__))}
    symbol = 'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'
    binary = str(root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles/pins/candidate')
    arms = {}
    for wave in range(3):
        label = f'candidate-wave{wave}'
        inventory = pcs['arms'][label]['symbols'][symbol]['instruction_rows']
        callsites = {b['pc']-1: a['instruction'] for a,b in zip(inventory, inventory[1:])
                     if a['instruction'].startswith(('call ', 'callq '))}
        path = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}/candidate.samples.txt.xz'
        rel = str(path.relative_to(root)); hashes[rel] = sha(path)
        assert hashes[rel] == envelope['hashes'][rel]
        with lzma.open(path, 'rt') as stream:
            result = partition(samples(stream), binary, symbol, callsites, {r['pc'] for r in inventory})
        expected = envelope['arms'][label]
        assert result['total_period'] == expected['total_period']
        assert result['selected_period'] == expected['selected_period']
        assert {k:v['period'] for k,v in result['children'].items()} == expected['branches']['probe']
        result['callsites'] = {hex(k): v for k,v in callsites.items()}
        arms[label] = result
        print(label, 'exact child/envelope conservation PASS', flush=True)
    with target.open('x') as out:
        json.dump(dict(hashes=hashes, arms=arms,
            limitation='Saved callstack attribution: direct child groups are disjoint; leaves inside a child are not additional cost. Initial strict call-site reader stopped on wave1 unknown unwinder frame at0x47ffc2 (a verified non-call instruction); now explicitly retain/quarantine unknown frames instead of assigning them to a call. Other invalid caller PCs still fail. Source mapping requires reviewed assembly; inlining/allocation and code-placement confounds remain.'), out, indent=2)
    print(target)

if __name__ == '__main__': main()

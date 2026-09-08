#!/usr/bin/env python3
"""Explain native observer-only rows without relaxing parent trajectory checks."""
from pathlib import Path
import json
import sys

def validate_native_boundaries(original, observed, window=(0, 502)):
    """Only extra analysis/end rows and resulting sequence shifts are allowed."""
    for rows in (original, observed):
        assert all(len(r) == 13 and r[0] == 'kissat' and int(r[1]) == i
                   for i, r in enumerate(rows)), 'Malformed boundary/sequence'
    assert all(r[2] != 'analysis' for r in original)
    extra = [r for r in observed if r[2] == 'analysis']
    assert all(r[3] == 'end' and window[0] <= int(r[4]) <= window[1]
               for r in extra), 'Unexpected analysis event'
    retained = [r for r in observed if r[2] != 'analysis']
    normalize = lambda rows: [r[:1] + r[2:] for r in rows]
    assert normalize(original) == normalize(retained), 'Original boundary changed'
    return len(extra)

def self_test():
    def row(seq, name='restart', stage='begin', conflict='1'):
        return ['kissat', str(seq), name, stage, conflict] + ['0'] * 8
    original = [row(0), row(1, stage='end')]
    observed = [row(0, 'analysis', 'end'), row(1), row(2, stage='end')]
    assert validate_native_boundaries(original, original) == 0
    assert validate_native_boundaries(original, observed) == 1
    bad = []
    for column, value in ((2, 'unknown'), (3, 'begin'), (4, '503'), (1, '5')):
        sample = [r[:] for r in observed]
        sample[0][column] = value
        bad.append(sample)
    sample = [r[:] for r in observed]
    sample[1][7] = '1'
    bad.append(sample)
    bad.append(observed[:-1])
    for sample in bad:
        try:
            validate_native_boundaries(original, sample)
        except AssertionError:
            continue
        raise AssertionError('Negative boundary fixture was accepted')
    print('PASS: 2 positive and 6 negative normalization fixtures')

if __name__ == '__main__':
    self_test()
    if '--self-test' in sys.argv:
        raise SystemExit(0)
    source = Path(__file__).with_name('compare-square-phase-5482.py').read_text()
    anchor = "        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(parent / 'traces' / (arm + '.' + suffix))"
    assert source.count(anchor) == 1
    source = source.replace(anchor, """        if arm == 'kissat' and suffix == 'boundaries.tsv':
            def boundary_rows(path):
                return [s.split('\\t') for s in path.read_text().splitlines() if not s.startswith('#')]
            extra = validate_native_boundaries(
                boundary_rows(parent / 'traces' / (arm + '.' + suffix)),
                boundary_rows(c / 'traces' / (arm + '.' + suffix)))
            assert extra == 476
        else:
            assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(parent / 'traces' / (arm + '.' + suffix))""")
    source = source.replace('PASS: both complete streams/prefixes/boundaries unchanged; selected phase records present',
        'PASS: complete streams/prefixes and candidate boundaries byte-identical; all native original boundaries unchanged after accounting for 476 analysis/end observer rows')
    exec(compile(source, __file__, 'exec'))
    phases = {}
    for arm in ('candidate', 'kissat'):
        phase_rows = []
        for line in (c / 'traces' / (arm + '.phase.tsv')).read_text().splitlines():
            if line.startswith('#'):
                continue
            r = line.split('\t')
            if r[2] in ('phase_save', 'phase_pick'):
                phase_rows.append(dict(event=r[2], conflicts=int(r[3]),
                                       decisions=int(r[4]), level=int(r[6]), payload=r[-1]))
        phases[arm] = phase_rows
    observation = dict(parent_replay=True, native_extra_analysis_end=extra,
        native_original_boundaries=986, normalization_fixtures=8,
        original_strict_comparator='compare-square-phase-5482.py fails native boundary byte identity; retained, not edited.',
        source_explanation='Kissat eventtrace.c::kissat_event_trace_detail returns without trace_file; enabling it emits analysis_end snapshots through kissat_event_trace into the standalone boundary writer before the phase-only filter.',
        phase_rows=phases,
        trace_sha256={p.name: sha(p) for p in (c / 'traces').glob('*.phase.tsv')},
        limitation='Observer replay validation only. Phase saves differ before the first pick; assignment/reason ownership remains to localize, not an established solver defect.')
    p = c / 'observer-validation.json'
    if p.exists():
        assert json.loads(p.read_text()) == observation
    else:
        p.write_text(json.dumps(observation, indent=2) + '\n')
    print(json.dumps(observation, indent=2))

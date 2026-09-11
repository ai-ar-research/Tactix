#!/usr/bin/env python3
"""Separate clause semantics, physical order, and observer holes in core7406."""
from collections import Counter
import datetime as dt
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha

CORE = ROOT / 'benchmarks/runs/20260911-7403-rank43-native-analysis-complete/core-summary-7406.json'
TARGET = ROOT / 'benchmarks/reports/2026-09-11-rank43-conflict-semantics-7407.json'


def clause(payload, conflict=False):
    fields = {}
    for part in payload.split('|'):
        k, sep, v = part.partition('=')
        if not sep or not k or not v or k in fields:
            raise ValueError('malformed/repeated clause field')
        fields[k] = v
    required = {'order', 'set', 'levels'}
    allowed = required | ({'ref', 'id', 'glue', 'used', 'redundant'} if conflict else {'nonasserting_glue'})
    if not required <= fields.keys() or fields.keys() - allowed:
        raise ValueError('missing/unknown clause field')
    order = list(map(int, fields['order'].split(',')))
    literals = list(map(int, fields['set'].split(',')))
    if not order or 0 in order or len(set(order)) != len(order) or sorted(order) != literals:
        raise ValueError('inconsistent clause identity')
    if conflict:
        pairs = [list(map(int, x.split(':'))) for x in fields['levels'].split(',')]
        if any(len(x) != 2 for x in pairs) or [x[0] for x in pairs] != order:
            raise ValueError('inconsistent conflict level identities')
        levels = [x[1] for x in pairs]
    else:
        levels = list(map(int, fields['levels'].split(',')))
    if len(levels) != len(order) or any(v < 0 for v in levels):
        raise ValueError('invalid clause levels')
    return dict(order=order, literals=literals,
                levels_by_literal=sorted(zip(order, levels)), levels=levels)


def fixtures():
    a = clause('order=2,-3|set=-3,2|levels=2:4,-3:1|ref=binary', True)
    b = clause('order=-3,2|set=-3,2|levels=-3:1,2:4|ref=4|id=8|glue=0', True)
    assert a['order'] != b['order'] and a['levels_by_literal'] == b['levels_by_literal']
    assert clause('order=2,-3|set=-3,2|levels=4,1|nonasserting_glue=1')['levels'] == [4, 1]
    assert clause('order=2,3|set=2,3|levels=4,1')['literals'] != a['literals']
    assert clause('order=2,-3|set=-3,2|levels=4,2')['levels_by_literal'] != a['levels_by_literal']
    bad = ['order=2,-3|set=-3,4|levels=4,1', 'order=2,2|set=2,2|levels=4,1',
           'order=2,0|set=0,2|levels=4,1', 'order=2,-3|set=-3,2|levels=4',
           'order=2,-3|set=-3,2|levels=4,-1', 'order=2,-3|set=-3,2|levels=4,1|levels=4,1',
           'order=2,-3|set=-3,2|levels=4,1|unknown=1']
    for payload in bad:
        try:
            clause(payload)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid clause accepted')
    try:
        clause('order=2,-3|set=-3,2|levels=-3:1,2:4', True)
    except ValueError:
        pass
    else:
        raise AssertionError('wrong level mapping accepted')
    print('Clause semantics: order/set/level positive controls and eight negative controls PASS')


def analyze(seal=False):
    if seal:
        assert not TARGET.exists()
    authority = json.loads(CORE.read_text())
    assert authority['observer_replay_validated'] and authority['core_conflicts'] == [1, 82]
    assert authority['full_window_1_85_accepted'] is False
    hashes = {str(CORE.relative_to(ROOT)): sha(CORE), str(Path(__file__).relative_to(ROOT)): sha(Path(__file__))}
    for rel, digest in authority['hashes'].items():
        assert sha(ROOT / rel) == digest, rel
    helper = ROOT / authority['core_helper']
    assert sha(helper) == authority['core_helper_sha256']
    hashes[str(helper.relative_to(ROOT))] = sha(helper)
    spec = importlib.util.spec_from_file_location('core7406', helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    rows = {}
    for arm, campaign, cap in (
        ('candidate', '20260911-7401-rank43-analysis-window', 250000),
        ('kissat', '20260911-7403-rank43-native-analysis-complete', 500000),
    ):
        p = ROOT / 'benchmarks/runs' / campaign / 'traces' / f'{arm}.search.tsv'
        assert sha(p) == authority['hashes'][str(p.relative_to(ROOT))]
        rows[arm] = module.complete_core(p.read_text(), solver='tactix' if arm == 'candidate' else 'kissat', window=(1, 85), cap=cap)
    conflicts = {}
    for arm, rr in rows.items():
        selected = [r for r in rr if r['event'] == 'conflict']
        conflicts[arm] = {r['conflicts']: clause(r['payload'], True) for r in selected}
        assert len(conflicts[arm]) == len(selected) == 81
    assert conflicts['candidate'].keys() == conflicts['kissat'].keys()
    first = {}
    for field in ('order', 'literals', 'levels_by_literal'):
        c = next(c for c in conflicts['candidate'] if conflicts['candidate'][c][field] != conflicts['kissat'][c][field])
        first[field] = dict(conflict=c, candidate=conflicts['candidate'][c], kissat=conflicts['kissat'][c])
    assert [first[k]['conflict'] for k in ('order', 'literals', 'levels_by_literal')] == [48, 82, 82]
    stages = {}
    events = ['learned_raw', 'learned_sorted', 'learned_shrunk', 'learned']
    for arm, rr in rows.items():
        selected = [r for r in rr if r['conflicts'] == 82 and r['event'] in events]
        assert [r['event'] for r in selected] == events
        parsed = [clause(r['payload']) for r in selected]
        assert all(x == parsed[0] for x in parsed)
        begin = [r for r in rr if r['conflicts'] == 82 and r['event'] == 'analysis_begin']
        end = [r for r in rr if r['conflicts'] == 82 and r['event'] == 'analysis_end']
        assert len(begin) == len(end) == 1 and begin[0]['level'] == 115
        assert begin[0]['decisions'] == end[0]['decisions'] == 4048
        assert end[0]['level'] == max(parsed[0]['levels'][1:])
        stages[arm] = dict(clause=parsed[0], unchanged_stages=events, begin_level=115, end_level=end[0]['level'])
    assert stages['candidate']['clause']['order'] == [551069, -50235]
    assert stages['kissat']['clause']['order'] == [551069, 50237]
    source = ROOT / 'tools/solvers/variants/kissat-imply-fast-5414/src/eventtrace.c'
    assert sha(source) == authority['hashes'][str(source.relative_to(ROOT))]
    hole = {a: [dict(event=r['event'], payload_bytes=len(r['payload'].encode()))
                for r in rr if r['conflicts'] == 14 and r['event'] in events] for a, rr in rows.items()}
    assert len(hole['candidate']) == 4 and not hole['kissat']
    assert all(r['payload_bytes'] > 4096 for r in hole['candidate'])
    out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
               first_conflict_differences=first, conflict82=stages, native_formatter_hole_example=hole,
               conclusion='First conflict literal-set and literal-level-map difference is C82, before minimization. Both arms preserve their distinct raw binary learned clauses through all four stages. Backjump102/105 follows actual nonasserting levels; no evidence for changing shrink or chrono thresholds. C48 earlier clause order differs with equal literal levels and remains an upstream physical-state question.',
               next_action='Inspect incoming propagation and ordered watches for propagated -1038 before conflict82 (pre-conflict counter81). Determine clause membership, assignment and scan-order differences before selecting a repair.',
               limitations='Core1..82 only; native1..85 traces remain capped at250000/500000. All analysis transactions close but native4096-byte clause formatter omits oversized payloads (e.g.C14); no all-learned-clause equality claim. Binary physical metadata is not portable. Whole solver replay is exact; independent proof acceptance is separate. No runtime improvement or unique root cause established.')
    if seal:
        with TARGET.open('x') as stream:
            json.dump(out, stream, indent=2)
    print(json.dumps(dict(first={k: v['conflict'] for k, v in first.items()}, conflict82=stages), indent=2))


if __name__ == '__main__':
    assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
    fixtures()
    if sys.argv[1:] != ['--self-test']:
        analyze(sys.argv[1:] == ['--seal'])

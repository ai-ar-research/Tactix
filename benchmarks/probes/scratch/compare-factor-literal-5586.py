#!/usr/bin/env python3
"""Validate short replay and compare connected occurrences, including duplicates."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5585-rank32-factor-literal'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def payload(text):
    fields = text.split('|')
    result = dict(field.split('=', 1) for field in fields)
    assert len(result) == len(fields)
    return result

def journal_projection(current, previous, expected):
    extra = [r for r in current if r[1:3] == ['analysis', 'end'] and r[3] != '19375']
    assert len(extra) == expected
    assert [r for r in current if not (r[1:3] == ['analysis', 'end'] and r[3] != '19375')] == previous
    return extra

def journal(path):
    rows = []
    for line in path.read_text().splitlines():
        if not line or line.startswith('#'): continue
        row = line.split('\t')
        assert len(row) == 13 and int(row[1]) == len(rows)
        rows.append(row[:1] + row[2:])
    return rows

def groups(content, selected=41096):
    assert 'truncat' not in content.lower()
    result, sequence = [], 0
    allowed = {'factor_mark_transition', 'binary_add', 'factor_fresh'}
    for line in content.splitlines():
        if not line or line.startswith('#'): continue
        row = line.split('\t')
        assert len(row) == 22 and int(row[1]) == sequence
        sequence += 1
        event = row[2]
        if event == 'trace_begin':
            assert sequence == 1
            continue
        if event in allowed: continue
        assert int(row[3]) == 19375
        d = payload(row[-1])
        assert int(d['external']) == selected
        if event == 'factor_literal_state':
            assert set(d) == {'external', 'active', 'marked', 'value', 'score'}
            state = {k: int(v) for k, v in d.items()}
            assert state['active'] in (0, 1) and state['marked'] in (0, 1, 2)
            assert state['value'] in (-1, 0, 1) and state['score'] >= 0
            result.append(dict(state=state, entries=[]))
        else:
            assert event == 'factor_literal_entry' and result
            assert set(d) == {'external', 'ordinal', 'kind', 'live', 'id', 'lits'}
            assert int(d['ordinal']) == len(result[-1]['entries'])
            lits = tuple(sorted(map(int, d['lits'].split(','))))
            assert all(lits) and selected in lits
            assert d['kind'] in ('binary', 'large') and d['live'] in ('0', '1')
            assert len(lits) == 2 if d['kind'] == 'binary' else len(lits) >= 3
            assert int(d['id']) >= 0
            result[-1]['entries'].append((d['kind'], int(d['live']), lits))
    assert result
    for g in result: assert len(g['entries']) == g['state']['score']
    return result

def fixtures():
    def row(n, event, p):
        return '\t'.join(['tactix', str(n), event, '19375'] + ['0'] * 17 + [p]) + '\n'
    state = 'external=41096|active=1|marked=1|value=0|score=2'
    entry = 'external=41096|ordinal=0|kind=binary|live=1|id=0|lits=41096,-7'
    good = row(0, 'factor_literal_state', state) + row(1, 'factor_literal_entry', entry) + row(2, 'factor_literal_entry', entry.replace('ordinal=0', 'ordinal=1'))
    assert len(groups(good)[0]['entries']) == 2
    assert Counter(groups(good)[0]['entries']) == {('binary', 1, (-7, 41096)): 2}
    for bad in ('', good.replace('score=2', 'score=1'), good.replace('ordinal=1', 'ordinal=0'),
                good.replace('19375', '19374'), good.replace('live=1', 'live=2'),
                good.replace('external=41096', 'external=7'), good + '# truncated\n',
                good.replace('value=0', 'value=0|value=0')):
        try: groups(bad)
        except (AssertionError, ValueError, KeyError): pass
        else: raise AssertionError('Invalid occurrence trace accepted')
    old = [['kissat', 'factor', 'begin', '19375']]
    extra = ['kissat', 'analysis', 'end', '100']
    assert journal_projection([extra] + old, old, 1) == [extra]
    assert journal_projection(old, old, 0) == []
    for bad in (old, [extra, extra] + old, [extra, ['kissat', 'factor', 'end', '19375']],
                [['kissat', 'analysis', 'end', '19375']] + old):
        try: journal_projection(bad, old, 1)
        except AssertionError: pass
        else: raise AssertionError('Changed original boundary accepted')
    print('PASS: four positive/twelve negative occurrence and projection fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
        print('5585 results pending'); return
    m = json.loads((c / 'manifest.json').read_text())
    parent = root / m['parent_campaign']
    for name, digest in m['hashes'].items(): assert sha(c / name) == digest
    assert sha(root / m['input']) == m['input_sha256_compressed']
    evidence, data, observer_projection = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/' + arm]
        assert r['partial_certificate'] == old['partial_certificate'] and r['partial_certificate']['bytes'] > 0
        for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        # The parent selected Sweep events; this probe selects Factor events.
        # All independent whole decision and boundary observers must replay.
        for suffix in ('decisions.tsv', 'prefix.tsv', 'boundaries.tsv', 'macro.tsv'):
            name = arm + '.' + suffix
            if name not in old['trace_sha256']:
                assert arm == 'kissat' and suffix == 'macro.tsv'
                continue
            assert sha(parent / 'traces' / name) == old['trace_sha256'][name]
            if r['trace_sha256'][name] != old['trace_sha256'][name]:
                assert name == 'kissat.boundaries.tsv', name
                # This invocation selected literal events but did not narrow
                # the global detail window. Native analysis-end boundaries
                # therefore appear outside the parent's19375 detail window.
                # Preserve the parent's19375 analysis and every original row.
                extra = journal_projection(journal(c / 'traces' / name),
                                           journal(parent / 'traces' / name), 18601)
                assert all(0 <= int(row[3]) <= 19377 for row in extra)
                observer_projection[name] = dict(extra_analysis_end=len(extra),
                    extra_sha256=hashlib.sha256(json.dumps(extra).encode()).hexdigest(),
                    preserved='Every original row in original order, including analysis at19375; all whole decision/prefix/proof hashes match.')
        data[arm] = groups((c / 'traces' / (arm + '.events.tsv')).read_text(), m['selected_literal'])
        assert len(data[arm]) == 2
        evidence[arm] = dict(result_sha256=sha(p), traces=r['trace_sha256'])
    comparisons = []
    for a, b in zip(data['candidate'], data['kissat']):
        ac, bc = Counter(a['entries']), Counter(b['entries'])
        comparisons.append(dict(candidate_state=a['state'], kissat_state=b['state'],
            physical_order_equal=a['entries'] == b['entries'],
            candidate_only=[dict(entry=k, copies=v) for k, v in sorted((ac-bc).items())],
            kissat_only=[dict(entry=k, copies=v) for k, v in sorted((bc-ac).items())]))
    result = dict(parent_replay=True, parser_fixtures=16, evidence=evidence,
        observer_projection=observer_projection,
        pass_comparisons=comparisons,
        limitation='Initial occurrence snapshots identify connected material, not the cause of different admission/peeling or the eventual runtime effect. No solver source change or checked final verdict in this stopped diagnostic.')
    p = c / 'literal-localization.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(comparisons, indent=2))

if __name__ == '__main__': main()

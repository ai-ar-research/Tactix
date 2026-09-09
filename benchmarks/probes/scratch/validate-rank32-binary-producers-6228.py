#!/usr/bin/env python3
"""Fail-closed selected-binary snapshot framing and complete parent replay."""
from collections import Counter
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
SUBJECTS = {337674, 390671, 399915, 409823, 444046}

def snapshots(stream, solver):
    assert next(stream) == '# ' + solver + '-binary-set-snapshot\t1\n'
    assert next(stream) == '# snapshot\ta\tb\n'
    result = []
    current = None
    for line in stream:
        assert line.endswith('\n')
        fields = line.rstrip('\n').split('\t')
        if fields[0] == '# begin':
            assert current is None and len(fields) == 7
            _, seq, who, event, conflict, count, payload = fields
            assert who == solver and int(seq) == len(result) and int(conflict) == 319386
            assert payload.startswith('name=') and '|stage=' in payload
            current = dict(sequence=int(seq), event=event, count=int(count), payload=payload, pairs=[])
        elif fields[0] == '# end':
            assert current is not None and fields == ['# end', str(current['sequence'])]
            assert len(current['pairs']) == current['count'] and current['pairs'] == sorted(current['pairs'])
            result.append(current)
            current = None
        else:
            assert current is not None and len(fields) == 3
            seq, a, b = map(int, fields)
            assert seq == current['sequence'] and a and b and a <= b
            assert abs(a) in SUBJECTS or abs(b) in SUBJECTS
            current['pairs'].append([a, b])
    assert current is None and result
    return result

def fixtures():
    text = '# tactix-binary-set-snapshot\t1\n# snapshot\ta\tb\n# begin\t0\ttactix\tslot_boundary\t319386\t2\tname=Substitute|stage=begin\n0\t-337674\t2\n0\t-337674\t2\n# end\t0\n'
    assert len(snapshots(io.StringIO(text), 'tactix')[0]['pairs']) == 2
    bads = [text.rsplit('# end', 1)[0], text.replace('\t2\tname=', '\t1\tname='),
            text.replace('# end\t0', '# end\t1'), text.replace('319386', '1'),
            text.replace('-337674', '0'), text.replace('-337674', '-4'),
            text.replace('0\t-337674\t2', '1\t-337674\t2'), text[:-1]]
    for text in bads:
        try: snapshots(io.StringIO(text), 'tactix')
        except (AssertionError, ValueError, StopIteration): pass
        else: raise AssertionError('invalid framing accepted')
    print('PASS: snapshot duplicate preservation and eight negative controls')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6224-rank32-binary-producers'
    assert not (c / 'summary.json').exists()
    if not all((c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('Both terminal records required; no partial interpretation.'); return
    m = json.loads((c / 'manifest.json').read_text())
    parent_path = root / m['parent_summary']['path']
    assert sha(parent_path) == m['parent_summary']['sha256']
    parent = parent_path.parent
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    states = {}; transitions = {}; lifecycle = {}; hashes = {}
    for arm, solver in (('candidate', 'tactix'), ('kissat', 'kissat')):
        p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / p.name).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate']
        added = set(r['trace_sha256']) - set(old['trace_sha256'])
        expected = {arm + '.binary-sets.tsv', arm + '.binary-lifecycle.tsv'}
        # Native macro events are named substitute_begin/end, not slot_boundary.
        # Only its forward adapter synthesizes slot_boundary. Do not pretend
        # the frozen selector captures native Probe snapshots if none exist.
        assert added == expected or (arm == 'kissat' and added == {arm + '.binary-lifecycle.tsv'})
        for name, digest in old['trace_sha256'].items():
            assert sha(parent / 'traces' / name) == digest == r['trace_sha256'][name]
        hashes[str(p.relative_to(root))] = sha(p)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name; assert sha(p) == digest
            hashes[str(p.relative_to(root))] = digest
        p = c / 'traces' / (arm + '.binary-sets.tsv')
        if p.exists():
            with p.open() as stream: states[arm] = snapshots(stream, solver)
        else: states[arm] = []
        transitions[arm] = []
        for before, after in zip(states[arm], states[arm][1:]):
            a = Counter(map(tuple, before['pairs'])); b = Counter(map(tuple, after['pairs']))
            if a != b:
                transitions[arm].append(dict(before=before['payload'], after=after['payload'],
                    deleted=list((a-b).elements()), added=list((b-a).elements())))
        lifecycle[arm] = (c / 'traces' / (arm + '.binary-lifecycle.tsv')).read_text().splitlines()
    summary = dict(parent_replay=True, snapshots=states, transitions=transitions, lifecycle=lifecycle, hashes=hashes,
        limitation='Selected physical pairs only. Native slot_boundary selector misses macro Probe boundaries (native names differ); its snapshots may be absent or only forward boundaries. Tactix snapshots and native selected-pair lifecycle are separate evidence, not a paired snapshot comparison. No full formula equivalence, repaired trajectory, or timing claim.')
    (c / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(transitions, indent=2))

if __name__ == '__main__': main()

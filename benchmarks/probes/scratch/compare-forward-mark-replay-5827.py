#!/usr/bin/env python3
"""Validate complete replay neutrality before interpreting real root marks."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5822-forward-mark-rank32-replay'
target = c / 'summary.json'
assert not target.exists()

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def parse(text):
    frames = []
    for line in text.splitlines():
        fields = line.split('\t')
        attrs = dict(f.split('=', 1) for f in fields[1:])
        assert len(attrs) == len(fields) - 1
        if fields[0] == 'boundary':
            assert set(attrs) == {'stage', 'round', 'watermark', 'epoch', 'fixed_head', 'root_fixed'}
            stage = attrs.pop('stage')
            assert stage in ('before-sync', 'after-sync')
            frames.append(dict(stage=stage, **{k: int(v) for k, v in attrs.items()}, variables=[]))
        else:
            assert fields[0] == 'variable' and frames
            assert set(attrs) == {'external', 'value', 'stamp', 'marked', 'trail'}
            row = {k: int(v) for k, v in attrs.items()}
            assert row['external'] and row['value'] in (-1, 0, 1) and row['marked'] in (0, 1)
            assert row['marked'] == (row['stamp'] > frames[-1]['watermark'])
            assert row['external'] not in {r['external'] for r in frames[-1]['variables']}
            frames[-1]['variables'].append(row)
    assert frames and len(frames) % 2 == 0
    for before, after in zip(frames[::2], frames[1::2]):
        assert (before['stage'], after['stage']) == ('before-sync', 'after-sync')
        assert before['round'] == after['round'] and before['watermark'] == after['watermark']
        assert [r['external'] for r in before['variables']] == [r['external'] for r in after['variables']]
        assert before['variables'] and before['root_fixed'] == after['root_fixed'] == after['fixed_head']
    return frames

sample = ('boundary\tstage=before-sync\tround=1\twatermark=1\tepoch=2\tfixed_head=0\troot_fixed=1\n'
          'variable\texternal=1\tvalue=-1\tstamp=1\tmarked=0\ttrail=0\n'
          'boundary\tstage=after-sync\tround=1\twatermark=1\tepoch=3\tfixed_head=1\troot_fixed=1\n'
          'variable\texternal=1\tvalue=-1\tstamp=3\tmarked=1\ttrail=0\n')
assert len(parse(sample)) == 2
for bad in [sample.split('boundary\tstage=after-sync')[0], sample.replace('value=-1', 'value=2'),
            sample.replace('marked=0', 'marked=1'), sample.replace('stage=after-sync', 'stage=before-sync'),
            sample.replace('external=1', 'external=0'), sample.replace('root_fixed=1', 'root_fixed=2')]:
    try:
        parse(bad)
    except (AssertionError, ValueError):
        pass
    else:
        raise AssertionError('Malformed mark frame accepted')
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
r = json.loads((c / 'results/candidate.json').read_text())
binding = m['cached_controls']['candidate']
old_path = root / binding['record']
assert sha(old_path) == binding['sha256']
old = json.loads(old_path.read_text())
assert r['exit_code'] == 0 and r['scratch_deleted']
assert r['binary_sha256'] == sha(c / 'pins/candidate')
assert r['partial_certificate'] == old['partial_certificate']
for rel, digest in r['trace_sha256'].items():
    assert sha(c / 'traces' / rel) == digest, rel
for rel, digest in old['trace_sha256'].items():
    assert r['trace_sha256'][rel] == digest == sha(old_path.parent.parent / 'traces' / rel), rel
assert set(r['trace_sha256']) - set(old['trace_sha256']) == {'candidate.marks.tsv'}
frames = parse((c / 'traces/candidate.marks.tsv').read_text())
assert len(frames) == 4 and all(len(frame['variables']) == 10 for frame in frames)
before, after = frames[:2]
assert before['round'] == 9
changed = [(a, b) for a, b in zip(before['variables'], after['variables']) if a != b]
assert len(changed) == 1 and changed[0][0]['external'] == 217821
assert (changed[0][0]['marked'], changed[0][1]['marked'], changed[0][1]['value']) == (0, 1, -1)
assert sum(v['marked'] for v in before['variables']) == 1
assert sum(v['marked'] for v in after['variables']) == 2
result = dict(observer_parent_trace_and_partial_proof_equal=True,
              complete_trace_sha256=r['trace_sha256'], result_sha256=sha(c / 'results/candidate.json'),
              cached_parent_sha256=binding['sha256'], frames=frames, first_sync_changed=changed,
              parser_negative_controls=6,
              conclusion='At Rank32 conflict179463,canonical root synchronization alone marks root-false217821,raising selected-clause admission from one to two marked variables. Existing native witness filters that same clause with one subsume mark. This establishes the origin of this admission difference,not its net runtime effect.',
              next_action='Isolate the default forward-admission treatment of root notifications while preserving other consumers and public single-instance capability;check fixed native parity,proof/model tests,Rank32 causal prefix,BC and full40.',
              limitation='Observer-only stopped300K replay;no new runtime result or checked final proof. Root notification has other consumers;do not globally delete shared touch updates without auditing them.')
target.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: complete ordinary trace/partial proof replay;root217821 alone changes selected admission1->2')

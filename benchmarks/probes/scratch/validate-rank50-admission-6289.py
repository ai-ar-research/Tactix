#!/usr/bin/env python3
"""Fail closed on changed observations before interpreting admission operands."""
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, first_stream_difference

FIELDS = dict(tactix='conflicts decisions answer initialized preprocess watermarks rounds deadline marks marks_limit fixed fixed_limit blocked episodes'.split(),
              kissat='conflicts decisions answer enabled irredundant last_reduce deadline eliminate eliminate_limit subsume subsume_limit'.split())
BOOLS = {'answer', 'initialized', 'preprocess', 'watermarks', 'blocked', 'enabled'}

def parse(text, solver, conflict):
    assert text and text.endswith('\n'), 'empty/truncated admission stream'
    records = []
    for line in text.splitlines():
        fields = line.split('\t')
        assert fields.pop(0) == solver and len(fields) == len(FIELDS[solver])
        r = {}
        for key, field in zip(FIELDS[solver], fields):
            name, value = field.split('=')
            assert name == key and value.isascii() and value.isdecimal()
            r[key] = int(value)
            assert r[key] < 1 << 64
            if key in BOOLS: assert r[key] in (0, 1)
        assert r['conflicts'] == conflict
        if solver == 'tactix':
            gated = r['watermarks'] and r['rounds'] != 0 and r['marks'] <= r['marks_limit'] and r['fixed'] <= r['fixed_limit']
            answer = r['initialized'] and not (r['preprocess'] and conflict) and conflict != 0 and conflict >= r['deadline'] and not gated
        else:
            answer = r['enabled'] and r['irredundant'] > 0 and r['last_reduce'] != conflict and conflict >= r['deadline'] and (r['eliminate'] > r['eliminate_limit'] or r['subsume'] > r['subsume_limit'])
        assert r['answer'] == bool(answer), 'recorded answer disagrees with actual guard operands'
        records.append(r)
    return records

def fixtures():
    good = 'kissat\tconflicts=9\tdecisions=8\tanswer=1\tenabled=1\tirredundant=1\tlast_reduce=0\tdeadline=9\teliminate=0\teliminate_limit=0\tsubsume=1\tsubsume_limit=0\n'
    assert parse(good, 'kissat', 9)[0]['answer'] == 1
    bad = ['', good[:-1], good.replace('kissat', 'tactix'), good.replace('conflicts=9', 'conflicts=8'),
           good.replace('answer=1', 'answer=2'), good.replace('answer=1', 'answer=0'), good.replace('deadline=9', 'deadline=10'),
           good.replace('subsume=1', 'subsume=0'), good.replace('last_reduce=0', 'last_reduce=9'),
           good.replace('decisions=8', 'decisions=-1'), good.replace('decisions=8', 'decisions=18446744073709551616'),
           good.replace('subsume_limit=0', 'subsume=0')]
    for text in bad:
        try: parse(text, 'kissat', 9)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid admission fixture accepted')
    print('PASS: twelve negative admission parser fixtures')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6286-rank50-admission'
    target = c / 'summary.json'; assert not target.exists()
    cached = root / 'benchmarks/runs/20260909-6211-rank50-extended-prefix'
    m = json.loads((c/'manifest.json').read_text())
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p)
        if expected is not None: assert digest == expected, p
        hashes[str(p.relative_to(root))] = digest
        return digest
    for rel, digest in m['hashes'].items(): bind(c/rel, digest)
    bind(c/'manifest.json'); bind(c/'launch.json')
    bind(root/m['observer_build']['path'], m['observer_build']['sha256'])
    old_summary = json.loads((cached/'summary.json').read_text())
    bind(cached/'summary.json')
    results = {}; prefixes = {}
    for arm in ('candidate', 'candidate_control', 'kissat'):
        p = c/'results'/f'{arm}.json'; bind(p); r = json.loads(p.read_text()); results[arm] = r
        assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['binary_sha256'] == sha(c/'pins'/arm)
        assert r['partial_certificate']['bytes'] > 0
        for name, digest in r['trace_sha256'].items(): bind(c/'traces'/name, digest)
        for suffix in ('decisions', 'boundaries', 'prefix'):
            assert f'{arm}.{suffix}.tsv' in r['trace_sha256']
        prefixes[arm] = read_prefix(c/'traces'/f'{arm}.prefix.tsv')
        validate_common(prefixes[arm], stop=1000000, common=139633, digest='236dcfa1a362af4d')
    # Full byte identity, including all ordinary trace payloads and partial
    # proof, against a same-source candidate control and cached native replay.
    replays = {}
    for arm, parent, parent_arm in (('candidate', c, 'candidate_control'), ('kissat', cached, 'kissat')):
        old_path = parent/'results'/f'{parent_arm}.json'; bind(old_path)
        old = json.loads(old_path.read_text()); r = results[arm]
        assert old['exit_code'] == 0 and old['scratch_deleted']
        assert r['partial_certificate'] == old['partial_certificate']
        current = {name.split('.', 1)[1]: digest for name, digest in r['trace_sha256'].items() if not name.endswith('.admission.tsv')}
        prior = {name.split('.', 1)[1]: digest for name, digest in old['trace_sha256'].items()}
        assert current == prior, (arm, 'observer changed ordinary trace')
        for name, digest in old['trace_sha256'].items():
            p = parent/'traces'/name; bind(p, digest)
            if arm == 'kissat': assert old_summary['hashes'][str(p.relative_to(root))] == digest
        replays[arm] = dict(full_trace_sha256=current, partial_certificate=r['partial_certificate'])
    admissions = {arm: parse((c/'traces'/f'{arm}.admission.tsv').read_text(), 'tactix' if arm == 'candidate' else 'kissat', m['admission_conflict']) for arm in ('candidate', 'kissat')}
    spec = importlib.util.spec_from_file_location('boundaries', Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module); module.fixtures()
    rows = {}
    for arm in ('candidate', 'kissat'):
        with (c/'traces'/f'{arm}.boundaries.tsv').open() as stream: rows[arm], _ = module.parse(stream, arm == 'kissat')
    result = dict(replays=replays, admissions=admissions, prefixes=prefixes,
                  decisions=first_stream_difference(c/'traces/candidate.decisions.tsv', c/'traces/kissat.decisions.tsv'),
                  boundaries=module.compare(rows['candidate'], rows['kissat']), hashes=hashes,
                  limitation='Read-only gate inputs on fully replayed bounded observations, not complete formula identity or final-proof validation. Observer runs cannot establish runtime gain. Semantic candidate6269 remains isolated and unfuzzed.')
    target.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({key: result[key] for key in ('admissions', 'decisions', 'boundaries')}, indent=2))

if __name__ == '__main__': main()

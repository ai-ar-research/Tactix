#!/usr/bin/env python3
"""Freeze selected entry evidence only after complete parent replay validation."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

root = Path(__file__).resolve().parents[3]

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            digest.update(block)
    return digest.hexdigest()

def snapshots(content, arm):
    result, current = [], None
    for line in content.splitlines():
        fields = line.split('\t')
        if line.startswith('# begin\t'):
            assert current is None and len(fields) == 7
            assert int(fields[1]) == len(result) and fields[2] == arm
            assert fields[4] == '19375'
            assert (fields[3], fields[6]) == (
                ('slot_boundary', 'name=Sweep|stage=begin') if arm == 'tactix'
                else ('sweep_begin', '-'))
            current = []
            expected = int(fields[5])
        elif line.startswith('# end\t'):
            assert current is not None and int(fields[1]) == len(result)
            assert len(current) == expected
            result.append(current)
            current = None
        elif line and not line.startswith('#'):
            assert current is not None and len(fields) == 3
            assert int(fields[0]) == len(result)
            pair = tuple(map(int, fields[1:]))
            assert any(abs(lit) in (836, 1236) for lit in pair)
            current.append(pair)
    assert current is None and result
    return result

def fixtures():
    good = '# begin\t0\ttactix\tslot_boundary\t19375\t1\tname=Sweep|stage=begin\n0\t-841\t836\n# end\t0\n'
    assert snapshots(good, 'tactix') == [[(-841, 836)]]
    native = good.replace('tactix', 'kissat').replace('slot_boundary', 'sweep_begin').replace('name=Sweep|stage=begin', '-')
    assert snapshots(native, 'kissat') == [[(-841, 836)]]
    for invalid in (good.replace('19375', '19374'),
                    good.replace('stage=begin', 'stage=end'),
                    good.replace('\t1\tname=', '\t2\tname='),
                    good.replace('# end\t0\n', ''),
                    good.replace('836', '837'),
                    good.replace('# end\t0', '# end\t1')):
        try:
            snapshots(invalid, 'tactix')
        except AssertionError:
            pass
        else:
            raise AssertionError('Malformed snapshot accepted')
    print('PASS: two positive/six negative snapshot fixtures', flush=True)

def main():
    fixtures()
    campaign = root / 'benchmarks/runs/20260908-5505-rank32-sweep-eligibility'
    destination = campaign / 'eligibility-validation.json'
    assert not destination.exists()
    manifest = json.loads((campaign / 'manifest.json').read_text())
    parent = root / manifest['parent_campaign']
    for name, digest in manifest['hashes'].items():
        assert sha(campaign / name) == digest
    assert sha(root / manifest['input']) == manifest['input_sha256_compressed']
    authority = manifest['selected_authority']
    assert sha(root / authority['path']) == authority['sha256']
    frozen, selected, formulas = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        result = json.loads((campaign / 'results' / f'{arm}.json').read_text())
        old = json.loads((parent / 'results' / f'{arm}.json').read_text())
        assert result['exit_code'] == 0 and result['scratch_deleted']
        assert result['partial_certificate']['bytes'] > 0
        assert sha(campaign / 'pins' / arm) == result['binary_sha256']
        for name, digest in result['trace_sha256'].items():
            assert sha(campaign / 'traces' / name) == digest
        for suffix in ('decisions', 'prefix', 'boundaries'):
            name = f'{arm}.{suffix}.tsv'
            digest = sha(parent / 'traces' / name)
            assert digest == old['trace_sha256'][name]
            assert digest == result['trace_sha256'][name]
        # Validate the exact current wire format, never infer snapshots.
        content = (campaign / 'traces' / f'{arm}.binary.tsv').read_text()
        selected[arm] = snapshots(content, 'tactix' if arm == 'candidate' else arm)
        rows = (campaign / 'traces' / f'{arm}.formula.tsv').read_text().splitlines()[2:]
        assert len(rows) == 11
        for row in rows:
            fields = row.split('\t')
            assert len(fields) == 5
            literals = list(map(int, fields[3].split(',')))
            assert len(literals) == int(fields[1])
            assert all(abs(lit) != 836 for lit in literals)
            assert any(abs(lit) == 1236 for lit in literals)
        formulas[arm] = rows
        frozen[arm] = {'result_sha256': sha(campaign / 'results' / f'{arm}.json'),
                       'traces': result['trace_sha256']}
    assert formulas['candidate'] == formulas['kissat']
    assert selected['candidate'] == selected['kissat'] == [
        [(-841, 836), (-796, 836), (783, 836), (834, 836)],
        [(-826, 836), (-796, 836), (783, 836), (834, 836)]]
    destination.write_text(json.dumps({
        'created_at': datetime.now(timezone.utc).isoformat(),
        'manifest_sha256': sha(campaign / 'manifest.json'),
        'parent_replay': 'entire per-arm decisions/prefix/boundaries identical',
        'selected_binary_snapshots': selected,
        'large_formula': '11 identical clauses involving1236, none involving836',
        'conclusion': '836 has four positive and zero negative live occurrences at entry in both arms; native retained eligibility declines it.',
        'limitation': 'Not complete physical-watch/active-flag parity; does not establish that skipping836 alone explains subsequent units or runtime.',
        'evidence': frozen,
    }, indent=2) + '\n')
    print(destination)

if __name__ == '__main__':
    main()

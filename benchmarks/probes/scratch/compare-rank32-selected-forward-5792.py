#!/usr/bin/env python3
"""Seal selected action evidence only after complete neutral parent replay."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5791-rank32-selected-forward'
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def main():
    target = c / 'summary.json'
    if target.exists():
        print('Already sealed:', target)
        return
    if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
        print('Pending terminal results; no validation claim.')
        return
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
    assert sha(root / m['witness_authority']['path']) == m['witness_authority']['sha256']
    records, evidence = {}, {}
    for arm in ('candidate', 'kissat'):
        rp = c / 'results' / (arm + '.json')
        r = json.loads(rp.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'], arm
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate'], arm
        for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest, rel
        for rel, digest in old['trace_sha256'].items():
            assert r['trace_sha256'][rel] == digest == sha(parent / 'traces' / rel), rel
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.forward.tsv'}
        rows = (c / 'traces' / (arm + '.forward.tsv')).read_text().splitlines()
        assert rows[0] == '# ' + ('tactix' if arm == 'candidate' else arm) + '-forward-clause\t1'
        if arm == 'candidate':
            assert any(row.startswith('result\t') for row in rows[1:]), arm
        else:
            assert any('stage=discover' in row for row in rows[1:]), arm
            assert any('stage=result-' in row or 'stage=discover-filtered' in row for row in rows[1:]), arm
        records[arm] = rows[1:]
        evidence[arm] = dict(result_sha256=sha(rp), trace_sha256=r['trace_sha256'],
                             partial_certificate=r['partial_certificate'])
    out = dict(observer_parent_trace_and_proof_equal=True, selected_clause=m['selected_clause'],
               evidence=evidence, records=records,
               limitation='Selected action/partner records do not alone prove complete binary-watch or scheduling-flag identity, causal runtime gain, or final proof acceptance.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(dict(selected_clause=out['selected_clause'], records=records), indent=2))

if __name__ == '__main__': main()

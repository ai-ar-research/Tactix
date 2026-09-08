#!/usr/bin/env python3
"""Classify an exhausted checker allowance as unverified, never accepted."""
import copy
import hashlib
import json
from pathlib import Path
import sys

def checker_timeout(r, allowance):
    check = r.get('proof_check')
    return (r.get('valid') is False and r.get('expected') == 'UNSAT' and
            r.get('verdict') == 'UNSAT' and r.get('expected_match') is True and
            r.get('solve', {}).get('exit_code') == 20 and
            r.get('solve', {}).get('timed_out') is False and
            isinstance(check, dict) and check.get('timed_out') is True and
            check.get('exit_code') is None and check.get('result') == 'not-verified' and
            isinstance(check.get('wall_seconds'), (int, float)) and
            check['wall_seconds'] >= allowance)

def fixtures():
    good = dict(valid=False, expected='UNSAT', verdict='UNSAT', expected_match=True,
                solve=dict(exit_code=20, timed_out=False),
                proof_check=dict(timed_out=True, exit_code=None, result='not-verified', wall_seconds=16000.004))
    assert checker_timeout(good, 16000)
    for scope, key, value in [(None, 'valid', True), (None, 'expected_match', False),
                              ('solve', 'exit_code', -11), ('solve', 'timed_out', True),
                              ('proof_check', 'timed_out', False), ('proof_check', 'exit_code', 1),
                              ('proof_check', 'result', 'verified'), ('proof_check', 'wall_seconds', 15999)]:
        bad = copy.deepcopy(good)
        (bad if scope is None else bad[scope])[key] = value
        assert not checker_timeout(bad, 16000)
    print('PASS: checker-timeout classification and eight rejection controls', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    root = Path(__file__).resolve().parents[3]
    c = root / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
    target = root / 'benchmarks/reports/2026-09-08-checker-timeout-384.json'
    assert not target.exists()
    p = c / 'results/384.json'
    m = json.loads((c / 'manifest.json').read_text())
    r = json.loads(p.read_text())
    assert checker_timeout(r, m['resource_contract']['checker_timeout'])
    assert r['binary_sha256'] == m['arms']['tactix']['sha256']
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
    assert sha(c / 'pins/runtime_task.py') == m['pin_sha256']['pins/runtime_task.py']
    target.write_text(json.dumps(dict(task=384, instance=r['instance'], solver_seconds=r['solve']['wall_seconds'],
        proof_check=r['proof_check'], hashes={str(x.relative_to(root)): sha(x) for x in (p, c/'manifest.json', c/'pins/runtime_task.py')},
        disposition='Checker exhausted16000s allowance;certificate remains unverified and this result remains invalid/unsolved. This is not a proved rejected derivation, solver crash or empty-input status incident. No rerun, runner change or acceptance relaxation.',
        test_consideration='Eight negative classifier controls keep solver failure, mismatch, real checker exits and premature termination out of timeout classification.'), indent=2)+'\n')
    print(target)

if __name__ == '__main__': main()

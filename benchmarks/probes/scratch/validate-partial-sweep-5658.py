#!/usr/bin/env python3
"""Use complete selected snapshots only after validating the entire observed prefix."""
import json
from pathlib import Path
import runpy
base = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha = base['root'], base['sha']

def prefix(current, previous):
    count = 0
    with current.open('rb') as src, previous.open('rb') as ref:
        for line in src:
            assert line.endswith(b'\n'), ('Incomplete observed row', current, count)
            assert next(ref) == line, (current, count)
            count += 1
    assert count > 0
    return count

def validate_partial(c):
    m = json.loads((c / 'manifest.json').read_text())
    parent = root / m['parent_campaign']
    assert sha(parent / 'summary.json') == m['parent_summary_sha256']
    for name, digest in m['hashes'].items(): assert sha(c / name) == digest
    assert sha(root / m['input']) == m['input_sha256_compressed']
    records = {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 124 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['partial_certificate']['bytes'] > 0
        assert r['binary_sha256'] == m['hashes']['pins/' + arm]
        counts = {}
        for name, digest in r['trace_sha256'].items():
            assert sha(c / 'traces' / name) == digest
            if name.endswith('.sweep.tsv'): continue
            assert sha(parent / 'traces' / name) == old['trace_sha256'][name]
            if name.endswith(('.formula.tsv', '.binary.tsv')):
                assert digest == old['trace_sha256'][name]
            else:
                assert name.endswith(('.decisions.tsv', '.boundaries.tsv', '.macro.tsv'))
                counts[name] = prefix(c / 'traces' / name, parent / 'traces' / name)
        records[arm] = dict(result_sha256=sha(p), traces=r['trace_sha256'], observed_parent_prefix_lines=counts,
            status='1200s diagnostic timeout;complete observed rows replay parent;selected snapshots identical;no final stop or proof identity claim.')
    return records

source = Path(__file__).with_name('compare-rank33-sweep-events-5645.py').read_text()
source = source.replace("evidence = support['validate'](c)", 'evidence = validate_partial(c)')
source = source.replace('parent_full_replay=True', 'parent_full_replay=False,complete_observed_parent_prefix=True')
source = source.replace('All parent traces and partial proof bytes replay exactly.',
    'Both diagnostic arms timed out at1200s. Every complete observed decision/boundary/macro row replays its parent prefix and selected formula/binary snapshots match exactly;partial proof identity and full1M stop are NOT established. Selected closed untruncated Sweep windows only,not acceptance timing or a checked final verdict.')
source = source.replace("p = c / 'summary.json'", "p = c / 'partial-localization.json'")
exec(compile(source, __file__, 'exec'))

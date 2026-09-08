#!/usr/bin/env python3
"""Use corrected native-only successors and retain original candidate snapshots."""
from collections import Counter
import itertools
import json
from pathlib import Path
import runpy
support = runpy.run_path(str(Path(__file__).with_name('compare-rank32-formula-5785.py')))
root, parent, sha, read_rows = (support[k] for k in ('root', 'parent', 'sha', 'read_rows'))
support['fixtures']()
boundary = json.loads((parent / 'boundary-localization.json').read_text())
for number, previous, stage in ((5786, 5782, 'begin'), (5787, 5783, 'end')):
    c = root / f'benchmarks/runs/20260908-{number}-rank32-native-forward-{stage}-formula'
    original = root / f'benchmarks/runs/20260908-{previous}-rank32-forward-{stage}-formula'
    target = c / 'summary.json'
    if target.exists():
        print(stage, 'already sealed', flush=True)
        continue
    if not (c / 'results/kissat.json').exists():
        print(stage, 'native stop pending', flush=True)
        continue
    evidence, data = {}, {}
    for arm, campaign in (('candidate', original), ('kissat', c)):
        m = json.loads((campaign / 'manifest.json').read_text())
        for rel, digest in m['hashes'].items(): assert sha(campaign / rel) == digest
        rp = campaign / 'results' / (arm + '.json')
        r = json.loads(rp.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['binary_sha256'] == old['binary_sha256'] == sha(campaign / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate']
        for rel, digest in r['trace_sha256'].items(): assert sha(campaign / 'traces' / rel) == digest
        for rel, digest in old['trace_sha256'].items():
            assert r['trace_sha256'][rel] == digest == sha(parent / 'traces' / rel)
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.formula.tsv'}
        data[arm] = read_rows(campaign / 'traces' / (arm + '.formula.tsv'))
        expected = boundary['selected_boundary_pairs'][arm][0 if stage == 'begin' else 1]
        assert Counter(row[0][0] for row in data[arm]) == {0: int(expected['irred']), 1: int(expected['red'])}
        evidence[arm] = dict(campaign=str(campaign.relative_to(root)), result_sha256=sha(rp), traces=r['trace_sha256'])
    a, b = data['candidate'], data['kissat']
    ac, bc = Counter(row[0] for row in a), Counter(row[0] for row in b)
    a_only, b_only = ac - bc, bc - ac
    keys_equal = [r[0] for r in a] == [r[0] for r in b]
    first = next((dict(index=i, candidate=x, kissat=y) for i, (x, y) in enumerate(itertools.zip_longest(a, b)) if x != y), None)
    def witnesses(counts, rows):
        return [dict(copies=counts[row[0]], row=row) for row in rows if row[0] in counts][:30]
    out = dict(stage=stage, observer_parent_trace_and_proof_equal=True, evidence=evidence,
               large_clause_multisets_equal=ac == bc, physical_clause_order_equal=keys_equal,
               candidate_only_copies=sum(a_only.values()), kissat_only_copies=sum(b_only.values()),
               candidate_only=witnesses(a_only, a), kissat_only=witnesses(b_only, b),
               first_physical_difference=first,
               pose_differences=sum(x[1] != y[1] for x, y in zip(a, b)) if keys_equal else None,
               metadata_differences=sum(x[2] != y[2] for x, y in zip(a, b)) if keys_equal else None,
               root_value_differences=sum(x[3] != y[3] for x, y in zip(a, b)) if keys_equal else None,
               original_observer_failure='Native selector used strengthen_'+stage+';native standalone snapshots require name=Strengthen|stage='+stage+'. Original runs retained;only missing native snapshots repeated after actual-code positive/negative controls.',
               limitation='Large-clause snapshots only;binary watch order and scheduling flags absent. Witnesses capped at30 rows but copy counts are complete. Raw metadata differences are recorded,not all fields assumed semantically portable.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k: v for k, v in out.items() if k != 'evidence'}, indent=2), flush=True)

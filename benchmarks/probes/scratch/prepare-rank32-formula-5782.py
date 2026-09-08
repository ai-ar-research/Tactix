#!/usr/bin/env python3
"""Snapshot common entry and differing exit using existing immutable observers."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
base = json.loads((parent / 'manifest.json').read_text())
for rel, digest in base['hashes'].items(): assert sha(parent / rel) == digest
localization = parent / 'boundary-localization.json'
assert json.loads(localization.read_text())['first_portable_difference']['ordinal'] == 33
for number, stage in ((5782, 'begin'), (5783, 'end')):
    c = root / f'benchmarks/runs/20260908-{number}-rank32-forward-{stage}-formula'
    c.mkdir(exist_ok=False)
    for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
    for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
    runner = (parent / 'run.py').read_text().replace('tactix-5771-', f'tactix-{number}-')
    anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
    assert runner.count(anchor) == 1
    extra = f'''env['SAT_EVENT_TRACE_FORMULA_SET_PATH'] = str(c / 'traces' / (arm + '.formula.tsv'))
env['SAT_EVENT_TRACE_FORMULA_SET_CONFLICT'] = '179463'
env['SAT_EVENT_TRACE_FORMULA_SET_MATCH'] = 'strengthen_{stage}' if arm == 'kissat' else 'name=Strengthen|stage={stage}'
env['SAT_EVENT_TRACE_FORMULA_SET_ORDINAL'] = '0'
env['SAT_EVENT_TRACE_FORMULA_SET_POSE'] = '1'
env['SAT_EVENT_TRACE_FORMULA_SET_META'] = '1'
env['SAT_EVENT_TRACE_FORMULA_SET_VALUES'] = '1'
'''
    (c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
    (c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)))
    m = dict(base)
    m.update(parent_campaign=str(parent.relative_to(root)), selected_stage=stage,
             hypothesis='Rank32 Strengthen179463 processes its whole stream,not a unit or quota exit. Identify the eight outgoing large-clause difference and establish incoming literal sets/physical pose/root values before choosing a general repair.',
             validation='Both arms ordinary300K traces and partial proofs exactly replay5771. Selected first Strengthen boundary formula snapshot must exist and match recorded live large-clause counts.',
             localization_authority=dict(path=str(localization.relative_to(root)), sha256=sha(localization)),
             native_rerun_reason='Missing physical formula/pose/root-value snapshots at the first differing component boundary; no runtime baseline refresh.',
             test_consideration='Configuration-only existing immutable5742/native pins. Four prefix-parser tests and complete parent replay validate observer scope; no changed solver code or new solver unit test.',
             cached_native_campaign=str(parent.relative_to(root)))
    m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat')}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    print(c)

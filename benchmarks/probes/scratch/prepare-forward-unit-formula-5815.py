#!/usr/bin/env python3
"""Resolve the three-clause Rank31 residual with paired full snapshot context."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5810-forward-unit-rank31-prefix'
c = root / 'benchmarks/runs/20260908-5815-rank31-forward-residual-formula'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
summary = json.loads((parent / 'summary.json').read_text())
first = summary['comparisons']['kissat']['first_forward_bve_difference']
assert first['ordinal'] == 33 and first['repaired']['conflicts'] == '179823'
assert int(first['repaired']['irred']) - int(first['control']['irred']) == 3
for rel,digest in m['hashes'].items(): assert sha(parent / rel) == digest
native_record = root / m['cached_controls']['kissat']['record']
native = native_record.parent.parent / 'pins/kissat'
native_control = root / 'benchmarks/runs/20260908-5794-rank32-forward-binaries'
assert sha(native) == json.loads((native_control / 'manifest.json').read_text())['hashes']['pins/kissat']
assert json.loads((native_control / 'summary.json').read_text())['observer_parent_trace_and_proof_equal']
c.mkdir(exist_ok=False)
for folder in ('pins','logs','results','traces'): (c / folder).mkdir()
shutil.copy2(parent / 'pins/candidate',c / 'pins/candidate')
shutil.copy2(native,c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-5810-','tactix-5815-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = '''env['SAT_EVENT_TRACE_FORMULA_SET_PATH'] = str(c / 'traces' / (arm + '.formula.tsv'))
env['SAT_EVENT_TRACE_FORMULA_SET_CONFLICT'] = '179823'
env['SAT_EVENT_TRACE_FORMULA_SET_MATCH'] = 'name=Strengthen|stage=end'
env['SAT_EVENT_TRACE_FORMULA_SET_ORDINAL'] = '0'
env['SAT_EVENT_TRACE_FORMULA_SET_POSE'] = '1'
env['SAT_EVENT_TRACE_FORMULA_SET_META'] = '1'
env['SAT_EVENT_TRACE_FORMULA_SET_VALUES'] = '1'
env['SAT_EVENT_TRACE_BINARY_SET_PATH'] = str(c / 'traces' / (arm + '.binaries.tsv'))
env['SAT_EVENT_TRACE_BINARY_SET_CONFLICT'] = '179823'
env['SAT_EVENT_TRACE_BINARY_SET_MATCH'] = 'name=Strengthen|stage='
'''
(c / 'run.py').write_text(runner.replace(anchor,extra+anchor))
batch = (parent / 'array.sbatch').read_text().replace(str(parent),str(c))
assert '#SBATCH --array=0-0' in batch
batch = batch.replace('#SBATCH --array=0-0','#SBATCH --array=0-1').replace('5810-forward-unit-rank31','5815-rank31-residual')
(c / 'array.sbatch').write_text(batch)
m.update(parent_campaign=str(parent.relative_to(root)),
    hypothesis='Classify the three outgoing large-clause count residual after recovered unit continuation: compare complete large clauses,roots,pose and binary multiplicities; determine missing work versus valid/stronger simplification before another repair.',
    validation='Both ordinary300K traces and partial proofs must exactly replay their cached arm;require first exit formula counts and four framed binary snapshots to match recorded boundaries.',
    native_rerun_reason='The cached native decision/boundary trace lacks physical clause and binary snapshots for this Rank31 residual;this is missing diagnostic information,not a baseline refresh.',
    test_consideration='Configuration-only immutable candidate/native pins. Native pin matches already validated5794 snapshot binary; corrected standalone slot selectors from5786 reused. Snapshot parser six malformed controls,decision-prefix eight tests,and exact parent replay required.',
    localization_authority=dict(path=str((parent / 'summary.json').relative_to(root)),sha256=sha(parent / 'summary.json')),
    resource_contract=dict(m['resource_contract'],array='0-1'),
    contract='Two separate proof-on diagnostic tasks,12CPU/48000MiB each,25min allocation,1200s solve,no node constraint/throttle;not acceptance timing.')
m['cached_controls']['candidate'] = dict(record=str((parent / 'results/candidate.json').relative_to(root)),sha256=sha(parent / 'results/candidate.json'))
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in (c / 'run.py',c / 'array.sbatch',c / 'pins/candidate',c / 'pins/kissat')}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

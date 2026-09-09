#!/usr/bin/env python3
"""Validate complete observer replay before interpreting material events."""
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common
from material_trace import parse_material

c = root / 'benchmarks/runs/20260909-6553-rank50-material-producers'
target = c / 'summary-6555.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    if expected is not None: assert digest == expected, p
    hashes[str(p.relative_to(root))] = digest
    return digest
def read(p, expected=None):
    bind(p, expected)
    return json.loads(p.read_text())
m = read(c / 'manifest.json')
launch = read(c / 'launch.json')
assert launch['manifest_sha256'] == sha(c / 'manifest.json')
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
build = read(root / m['observer_build']['path'], m['observer_build']['sha256'])
for rel, digest in build['files'].items(): bind(root / build['source'] / rel, digest)
old = root / m['cached_control']['campaign']
prior = read(old / 'results/kissat.json', m['cached_control']['result_sha256'])
old_summary = read(old / 'summary.json', m['cached_control']['summary_sha256'])
r = read(c / 'results/kissat.json')
assert r['slurm_job'].split('_')[0] == launch['job_id']
assert r['exit_code'] == 0 and r['scratch_deleted']
assert r['binary_sha256'] == sha(c / 'pins/kissat')
assert r['partial_certificate'] == prior['partial_certificate']
ordinary = {n:d for n,d in r['trace_sha256'].items() if n != 'kissat.material.tsv'}
assert ordinary == prior['trace_sha256'], 'observer changed ordinary trace'
for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
for name, digest in prior['trace_sha256'].items():
    p = old / 'traces' / name
    bind(p, digest)
    assert old_summary['hashes'][str(p.relative_to(root))] == digest
prefix = read_prefix(c / 'traces/kissat.prefix.tsv')
validate_common(prefix, stop=1000000, common=139633, digest='236dcfa1a362af4d')
rows = parse_material((c / 'traces/kissat.material.tsv').read_text(),
                      m['material_from'], m['material_to'], m['admission_conflict'])
path = Path(__file__).with_name('validate-rank50-admission-6289.py')
spec = importlib.util.spec_from_file_location('admission6289', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
admissions = module.parse((c / 'traces/kissat.admission.tsv').read_text(), 'kissat', m['admission_conflict'])
assert admissions
invitation = admissions[0]
assert invitation['answer'] == 1 and invitation['subsume'] == rows[-1]['count']
assert invitation['subsume_limit'] == 2508 and invitation['eliminate'] == invitation['eliminate_limit']
# The same conflict is observed again after Eliminate consumes the invitation.
# Validate those records too; they are neither new invitations nor exclusions.
assert len(admissions) == 55
assert all(a['answer'] == 0 and a['deadline'] > a['conflicts']
           and a['subsume'] == a['subsume_limit'] == 2524 for a in admissions[1:])
for rel in ('tools/bench/material_trace.py', 'tools/bench/test_material_trace.py',
            'benchmarks/probes/scratch/validate-rank50-material-6555.py'):
    bind(root / rel)
result = dict(hashes=hashes, prefix=prefix, material_events=rows, admissions=admissions,
    first_invitation=invitation, post_invitation_calls=len(admissions) - 1,
    ordinary_replay_exact=True, ordinary_trace_sha256=ordinary,
    partial_certificate=r['partial_certificate'], scratch_deleted=r['scratch_deleted'],
    limitation='Real native false-to-true producer events on a byte-exact bounded replay, not a Tactix repair or final proof acceptance. Adjacent events alone do not prove they originate from one clause. Source tags identify immediate marking callers, not the complete upstream semantic call stack. Fixed Kissat timings remain untouched.')
with target.open('x') as out:
    json.dump(result, out, indent=2)
    out.write('\n')
print(json.dumps(dict(events=rows, admissions=admissions), indent=2))

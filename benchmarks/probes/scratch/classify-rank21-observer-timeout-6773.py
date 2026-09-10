#!/usr/bin/env python3
"""Classify the bounded observer failure; validate its completed native control."""
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from certificate_prefix import hash_prefix
from search_window import read_search
c = root/'benchmarks/runs/20260909-6757-rank21-search-window'
old = root/'benchmarks/runs/20260909-6751-rank21-long-prefix'
target = c/'classification-6773.json'
assert not target.exists() and not (c/'summary-6759.json').exists()
m = json.loads((c/'manifest.json').read_text())
hashes = {}
def bind(p, digest=None):
    actual = sha(p)
    assert digest is None or actual == digest, p
    hashes[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items():
    bind(c/rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root/rel, digest)
records = {}
for arm in ('candidate', 'kissat'):
    p = c/f'results/{arm}.json'
    bind(p)
    r = records[arm] = json.loads(p.read_text())
    assert r['scratch_deleted'] and r['binary_sha256'] == m['hashes']['pins/'+arm]
    for name, digest in r['trace_sha256'].items():
        bind(c/'traces'/name, digest)
r = records['candidate']
assert r['exit_code'] == 124 and r['command'][:2] == ['timeout', '1200']
assert not (c/'traces/candidate.prefix.tsv').exists()
p = c/'traces/candidate.decisions.tsv'
assert hash_prefix(old/'traces/candidate.decisions.tsv', p.stat().st_size)['sha256'] == sha(p)
with p.open('rb') as stream:
    stream.seek(max(0, p.stat().st_size-4096))
    tail = stream.read()
last = tail.split(b'\n')[-2]
last_complete = list(map(int, last.split(b'\t')))
assert len(last_complete) == 5
native = records['kissat']
control = json.loads((old/'results/kissat.json').read_text())
assert native['exit_code'] == 0
assert native['partial_certificate'] == control['partial_certificate']
assert native['prior_certificate_prefix'] == control['prior_certificate_prefix']
for name in ('kissat.decisions.tsv', 'kissat.prefix.tsv'):
    assert native['trace_sha256'][name] == control['trace_sha256'][name]
normalizer = Path(__file__).with_name('validate-square-phase-5486.py')
spec = importlib.util.spec_from_file_location('native_normalizer', normalizer)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.self_test()
rows = lambda p: [line.split('\t') for line in p.read_text().splitlines() if not line.startswith('#')]
extra = module.validate_native_boundaries(rows(old/'traces/kissat.boundaries.tsv'),
    rows(c/'traces/kissat.boundaries.tsv'), tuple(m['search_window']))
content = (c/'traces/kissat.search.tsv').read_text()
try:
    read_search(content, solver='kissat', window=tuple(m['search_window']), cap=m['trace_row_cap'])
except ValueError:
    pass
else:
    raise AssertionError('Expected the frozen capped native detail to be rejected')
lines = content.splitlines()
assert lines[-1] == '# truncated at 250000 records'
detail = [line.split('\t') for line in lines[3:-1]]
assert len(detail) == m['trace_row_cap'] == 250000
assert all(len(row) == 22 and row[0] == 'kissat' and int(row[1]) == i and int(row[3]) == 0
           for i, row in enumerate(detail))
for p in (Path(__file__), normalizer, c/'manifest.json', c/'launch.json',
          root/'tools/bench/certificate_prefix.py', root/'tools/bench/search_window.py'):
    bind(p)
out = dict(candidate=dict(classification='bounded observer solver-timeout, not a final solve loss or proof rejection',
    exit_code=124, scratch_deleted=True, full50k_replay=False,
    available_decision_bytes_exact_prior_prefix=True, available_decision_bytes=(c/'traces/candidate.decisions.tsv').stat().st_size,
    last_complete_decision=last_complete, partial_proof_unvalidated=True),
    native=dict(full50k_decisions_prefix_partial_proof_exact=True, old10k_partial_proof_exact=True,
        extra_analysis_end=extra, search_rows=len(detail), detail_capped=True,
        requested_window_rows=0, scratch_deleted=True),
    next_action='Preserve failed6757 and strict6759 unchanged. Same-pin paired successor uses existing TRAJECTORY_ONLY to exclude preprocessing Sweep noise and analysis snapshots, plus finite EVENT_TRACE_STOP_AFTER_CONFLICT one beyond50K to suppress macro/heap snapshots. Require full own50K decisions/proofs; native selected detail is missing despite its successful solver replay, so a new diagnostic native leg is necessary, not a timing refresh.',
    limitation='Candidate available bytes replay old decisions but lack final prefix/proof. Native solver replay passes, but all250000 detail rows were consumed at preprocessing conflict0: no usable requested-window evidence. Keep both failures; no interpretation of either invalid detail. Cheaper configuration is a diagnostic repair, not a solver defect or timing claim.', hashes=hashes)
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k != 'hashes'}, indent=2))

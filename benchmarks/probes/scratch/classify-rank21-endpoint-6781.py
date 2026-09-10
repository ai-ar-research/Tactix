#!/usr/bin/env python3
"""Classify competing observer stops without accepting a missing prefix record."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from certificate_prefix import hash_prefix
from search_window import read_search
c = root/'benchmarks/runs/20260909-6774-rank21-light-window'
old = root/'benchmarks/runs/20260909-6751-rank21-long-prefix'
target = c/'classification-6781.json'
assert not target.exists() and not (c/'summary-6776.json').exists()
m = json.loads((c/'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    hashes[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items():
    bind(c/rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root/rel, digest)
states = {}
for arm in ('candidate', 'kissat'):
    p = c/f'results/{arm}.json'
    bind(p)
    r = json.loads(p.read_text())
    control = json.loads((old/f'results/{arm}.json').read_text())
    assert r['exit_code'] == (0 if arm == 'candidate' else -15) and r['scratch_deleted']
    assert r['binary_sha256'] == control['binary_sha256']
    assert r['partial_certificate'] == control['partial_certificate']
    assert not (c/f'traces/{arm}.prefix.tsv').exists()
    for name, digest in r['trace_sha256'].items():
        bind(c/'traces'/name, digest)
    p = c/f'traces/{arm}.decisions.tsv'
    n = p.stat().st_size
    assert hash_prefix(old/f'traces/{arm}.decisions.tsv', n)['sha256'] == sha(p)
    if arm == 'candidate':
        assert r['trace_sha256']['candidate.decisions.tsv'] == control['trace_sha256']['candidate.decisions.tsv']
    else:
        assert n < (old/'traces/kissat.decisions.tsv').stat().st_size
        assert r['prior_certificate_prefix'] == control['prior_certificate_prefix']
        log = c/'logs/kissat.out'
        assert 'c raising signal 15 (SIGTERM)' in log.read_text()
        bind(log)
    detail = read_search((c/f'traces/{arm}.search.tsv').read_text(),
        solver='tactix' if arm == 'candidate' else 'kissat', window=(25098,25103), cap=250000)
    assert detail[-1]['event'] == 'analysis_end' and detail[-1]['conflicts'] == 50001
    states[arm] = dict(exit_code=r['exit_code'], scratch_deleted=True, prefix_record_missing=True,
        proof_exact_prior=True, available_decision_bytes_exact_prior=True, available_decision_bytes=n,
        complete_decision_stream=arm == 'candidate', last_observer=detail[-1], detail_rows=len(detail), causal_acceptance=False)
for p in (Path(__file__), c/'manifest.json', c/'launch.json',
          root/'.claude/worktrees/runtime-reduce-watermark-6730/src/diagnostics/event_trace.cpp',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/eventtrace.c'):
    bind(p)
out = dict(classification='Competing diagnostic endpoint preempts cumulative-prefix flush; strict6776 remains failed.',
    states=states, hashes=hashes,
    source_cause='Both prefix writers flush only at conflicts >= requested+1. Event detail tests analysis_end against its own stop first;50001 collides with the deferred50000 prefix checkpoint. Tactix exits0; native raisesSIGTERM, losing the final buffered decision suffix. Both partial-proof byte sequences are exactly the prior50K controls.',
    next_action='Same-pin successor removes SEARCH_ONLY when using TRAJECTORY_ONLY and uses UINT64_MAX-1 solely as the finite snapshot-suppression sentinel. The independent50K prefix remains the sole reachable stop within1200s. Require both complete streams, prefixes, proofs, boundaries and uncapped selected records; no acceptance from6781 alone.',
    test_consideration='Actual terminal records plus source-backed counter ordering, exact available stream-prefix bytes and exact partial proofs classify this failure. No solver soundness or strategy change. Existing strict schema/prefix tests remain; successor full replay is the integration oracle.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k != 'hashes'}, indent=2))

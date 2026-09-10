#!/usr/bin/env python3
"""Prove maintained aggregation exactly reproduces frozen7082 current scopes."""
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples, call_sites, summarize_phases
COMPONENTS = {
    'tactix::vivify_apply(': 'vivify',
    'tactix::factor_apply_seeded(': 'factor',
    'tactix::congruence_apply(': 'congruence',
    'tactix::sweep_apply_with_limits(': 'sweep',
    'tactix::substitute_apply_round(': 'substitute',
    'tactix::strengthen_apply(': 'forward',
    'tactix::(anonymous namespace)::bve_eliminate(': 'bve_attempt',
    'tactix::(anonymous namespace)::reduce_source(': 'transitive_reduce',
    'tactix::failed_literal_apply_chained_binary(': 'backbone',
}
def main():
    target = root / 'benchmarks/reports/2026-09-10-bve-lifecycle-reader-replay-7131.json'
    assert not target.exists()
    c = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost'
    p = c / 'lifecycle-summary-7082.json'; prior = json.loads(p.read_text())
    hashes = {str(p.relative_to(root)): sha(p)}
    changed = {'tools/bench/perf_callstack.py', 'tools/bench/test_perf_callstack.py'}
    for rel, digest in prior['hashes'].items():
        if rel in changed: continue
        assert sha(root / rel) == digest, rel
        hashes[rel] = digest
    entry = 'tactix::DefaultInprocessScheduler::run_phase(tactix::Kernel&, tactix::SlotGroup, int volatile*)'
    caller = 'tactix::DefaultPolicy::solve(tactix::Kernel&, int volatile*)'
    expected = {r['call_pc']: (r['phase'], r['argument_setup']) for r in prior['candidate_call_sites']}
    sites, evidence = call_sites((c / 'solve-disassembly.txt').read_text(), entry, expected)
    assert evidence == prior['candidate_call_sites']
    p = c / 'candidate.samples.txt.xz'; hashes[str(p.relative_to(root))] = sha(p)
    with lzma.open(p, 'rt') as stream:
        summary = summarize_phases(samples(stream),
            str(root / 'benchmarks/runs/20260910-7060-rank35-residual-profile/pins/candidate'),
            {entry: None}, caller, sites, COMPONENTS)
    assert summary == prior['arms']['candidate'], 'maintained reader changed frozen attribution'
    for p in (Path(__file__), *(root / rel for rel in changed)):
        hashes[str(p.relative_to(root))] = sha(p)
    with target.open('x') as stream:
        json.dump(dict(status='PASS', hashes=hashes, exact_frozen_attribution=True,
            samples=summary['samples'], total_period=summary['total_period'],
            phase_periods=summary['phase_periods'],
            scope='Every7082 current count, period, component, self/inclusive map and unresolved caller reproduced exactly by the maintained reader. No solver execution or profile expansion/deletion.'), stream, indent=2)
    print('PASS exact frozen7082 attribution:', summary['samples'], 'samples')
if __name__ == '__main__': main()

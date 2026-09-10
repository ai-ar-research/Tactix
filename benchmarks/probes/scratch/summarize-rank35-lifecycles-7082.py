#!/usr/bin/env python3
"""Conserve all saved sample periods while separating phase call sites."""
from collections import Counter, defaultdict
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples, phase, call_sites
c = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost'
target = c / 'lifecycle-summary-7082.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
p = c / 'extraction-7081.json'; bind(p)
m = json.loads(p.read_text())
for rel, digest in m['hashes'].items(): bind(root / rel, digest)
p = root / 'benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json'; bind(p)
prior = json.loads(p.read_text())
entry = 'tactix::DefaultInprocessScheduler::run_phase(tactix::Kernel&, tactix::SlotGroup, int volatile*)'
caller = 'tactix::DefaultPolicy::solve(tactix::Kernel&, int volatile*)'
sites, call_evidence = call_sites((c / 'solve-disassembly.txt').read_text(), entry, {
    0x507793: ('initial_probe', 'xor %edx,%edx'),
    0x508043: ('probe', 'xor %edx,%edx'),
    0x508be2: ('eliminate', 'mov $0x1,%edx')})
component_prefixes = {
    'candidate': {
        'tactix::vivify_apply(': 'vivify',
        'tactix::factor_apply_seeded(': 'factor',
        'tactix::congruence_apply(': 'congruence',
        'tactix::sweep_apply_with_limits(': 'sweep',
        'tactix::substitute_apply_round(': 'substitute',
        'tactix::strengthen_apply(': 'forward',
        'tactix::(anonymous namespace)::bve_eliminate(': 'bve_attempt',
        'tactix::(anonymous namespace)::reduce_source(': 'transitive_reduce',
        'tactix::failed_literal_apply_chained_binary(': 'backbone'},
    'kissat': {s: name for s,name in (
        ('kissat_vivify','vivify'), ('kissat_factor','factor'),
        ('kissat_congruence','congruence'), ('kissat_sweep','sweep'),
        ('kissat_substitute','substitute'),
        ('kissat_forward_subsume_during_elimination','forward'),
        ('kissat_generate_resolvents','bve_generation'),
        ('kissat_transitive_reduction','transitive_reduce'),
        ('kissat_binary_clauses_backbone','backbone'))}}
arms = {}
for arm, record in m['records'].items():
    binary = str(root / ('benchmarks/runs/20260910-7060-rank35-residual-profile/pins/candidate'
                       if arm == 'candidate' else 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat'))
    entries = {entry:None} if arm == 'candidate' else {
        'kissat_probe_initially':'initial_probe', 'kissat_probe':'probe', 'kissat_eliminate':'eliminate'}
    counts = Counter(); periods = Counter(); callers = Counter()
    components = defaultdict(Counter); own = defaultdict(Counter); inclusive = defaultdict(Counter)
    unresolved = Counter(); total = count = 0
    with lzma.open(root / record['sample_stream'], 'rt') as stream:
        for sample in samples(stream):
            count += 1; total += sample.period
            name, index = phase(sample, binary, entries, caller, sites)
            counts[name] += 1; periods[name] += sample.period
            if index is not None and arm == 'candidate':
                key = hex(sample.frames[index+1].pc) if index+1 < len(sample.frames) else 'missing'
                callers[key] += sample.period
            if name == 'unresolved_phase_caller':
                unresolved[' | '.join(f.symbol for f in sample.frames[index:])] += sample.period
            if index is None:
                continue
            component = 'phase_scaffolding_or_inlined_work'
            # Choose the OUTERMOST recognized rule beneath the phase. Each
            # sample receives one component, never an inclusive sum.
            for frame in reversed(sample.frames[:index]):
                if frame.dso != binary:
                    continue
                matches = [value for prefix,value in component_prefixes[arm].items()
                           if (frame.symbol.startswith(prefix) if arm == 'candidate' else frame.symbol == prefix)]
                assert len(matches) <= 1
                if matches:
                    component = matches[0]; break
            components[name][component] += sample.period
            leaf = sample.frames[0]
            own[name][leaf.symbol + ' [' + Path(leaf.dso).name + ']'] += sample.period
            for symbol in {f.symbol for f in sample.frames[:index+1] if f.dso == binary}:
                inclusive[name][symbol] += sample.period
    print(arm, 'sample count', count, 'total periods', total,
          'flat report', prior['total_cycles'][arm], flush=True)
    assert total == prior['total_cycles'][arm], 'sample-period universe differs from frozen flat report'
    assert sum(periods.values()) == total and sum(counts.values()) == count
    for name in components:
        assert sum(components[name].values()) == periods[name]
        assert sum(own[name].values()) == periods[name]
    arms[arm] = dict(samples=count, total_period=total, phase_samples=dict(counts),
        phase_periods=dict(periods), caller_periods=dict(callers),
        components={k:dict(v) for k,v in components.items()},
        self_by_phase={k:dict(v.most_common()) for k,v in own.items()},
        inclusive_by_phase={k:dict(v.most_common()) for k,v in inclusive.items()},
        unresolved_callers=dict(unresolved))
    print('Phase billion cycles:', {k:round(v/1e9,3) for k,v in periods.items()}, flush=True)
    print('Components billion cycles:', {k:{a:round(b/1e9,3) for a,b in v.items()} for k,v in components.items()}, flush=True)
for p in (Path(__file__), root/'tools/bench/perf_callstack.py', root/'tools/bench/test_perf_callstack.py',
          root/'.claude/worktrees/runtime-collect-retry-7030/src/policies/default_inprocess.hpp'):
    bind(p)
out = dict(hashes=hashes, arms=arms, candidate_call_sites=call_evidence,
    limitations='Saved sampled periods, not invocation timing or common-state per-operation cost. All periods conserved, including no-phase-frame and unknown callers. No-phase-frame may include truncated/unresolved ancestry, not necessarily pure CDCL. Components are mutually exclusive within each phase but implementation wrappers differ; BVE attempt versus generation remain unlike scopes. Inclusive symbol maps overlap and must not be summed. Caller PCs are diagnostic metadata only, never solver policy.',
    temporary_expansions_deleted_bytes=m['temporary_expansions_deleted_bytes'])
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(target)

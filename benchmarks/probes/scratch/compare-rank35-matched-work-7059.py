#!/usr/bin/env python3
"""Bind explicit counter correspondences; do not subtract differently scoped clocks."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import read_timers

def field(lines, label, index):
    rows = [s for s in lines if s.split()[:2] == ['c', label]]
    assert len(rows) == 1, label
    tokens = rows[0].split()
    assert 0 <= index < len(tokens) and tokens[index].isdigit(), (label, index)
    return int(tokens[index]), rows[0]

def fixtures():
    assert field(['c bve 0 rounds, 3 eliminated, 7 resolutions'], 'bve', 6)[0] == 7
    assert field(['c factor-rem 4', 'c factor 5'], 'factor', 2)[0] == 5
    for lines, label, index in (([], 'bve', 2), (['c bve 1','c bve 2'], 'bve', 2),
                                (['c bve -1'], 'bve', 2), (['c bve 1'], 'bve', 4)):
        try: field(lines, label, index)
        except AssertionError: pass
        else: raise AssertionError('malformed correspondence accepted')
    print('counter correspondence: exact label, index, missing/duplicate/negative/short controls PASS')

def main():
    fixtures()
    if sys.argv[1:] == ['--self-test']: return
    target = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime/matched-native-work-7059.json'
    assert not target.exists()
    cp = target.parent / 'provisional/002.json'
    np = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/results/003.json'
    c, n = (json.loads(p.read_text()) for p in (cp, np))
    assert classify(c, False)['status'] == 'pending_validation'
    assert classify(n, True)['checked'] and classify(n, True)['clean']
    assert c['rank'] == n['rank'] == 35 and c['instance'] == n['instance']
    hashes = {str(p.relative_to(root)): sha(p) for p in (cp, np, Path(__file__))}
    # This is an explicit schema projection, with raw source rows retained.
    mappings = [('conflicts',2,'conflicts'), ('decisions',2,'decisions'),
        ('learned',2,'clauses_learned'), ('chrono',2,'chronological'),
        ('restarts',2,'restarts'), ('reductions',2,'reductions'),
        ('rephased',2,'rephased'), ('reordered',2,'reordered'),
        ('warmup',2,'warmups'), ('warmup',4,'warming_decisions'),
        ('warmup',6,'warming_propagations'), ('walk',8,'walk_steps'),
        ('bve',4,'eliminated'), ('bve',6,'eliminate_resolutions'),
        ('factor',2,'factored'), ('factor',8,'factor_ticks'),
        ('transitive',10,'transitive_ticks'), ('substitute',11,'substituted'),
        ('sweep',2,'sweep'), ('sweep',10,'sweep_solved'),
        ('sweep',14,'sweep_equivalences'), ('sweep',16,'sweep_units'),
        ('congruence',2,'closures'), ('congruence',9,'congruent'),
        ('strengthen',6,'forward_steps'), ('strengthen',8,'strengthened'),
        ('strengthen',10,'subsumed')]
    rows = []
    for label, index, native in mappings:
        a, ar = field(c['solver_statistics'], label, index)
        b, br = field(n['solver_statistics'], native + ':', 2)
        assert a == b, (label, index, native, a, b)
        rows.append(dict(native_counter=native, value=a, candidate_row=ar, native_row=br))
    source = root / '.claude/worktrees/runtime-collect-retry-7030'
    for p in (source / 'src/main.cpp', source / 'src/policies/default_policy.cpp',
              source / 'src/rules/registry.cpp', root / 'kissat/src/profile.c',
              root / 'kissat/src/probe.c', root / 'kissat/src/eliminate.c',
              root / 'tools/bench/inprocessing_cost.py'):
        hashes[str(p.relative_to(root))] = sha(p)
    native_profile = [line for line in n['solver_statistics']
                      if len(line.split()) == 5 and line.split()[3] == '%']
    assert any(line.split()[-1] == 'simplify' for line in native_profile)
    out = dict(hashes=hashes, matched_counter_correspondences=rows,
        candidate_timers=read_timers(c['solver_statistics']), native_profile_rows=native_profile,
        decision='27 exact major work correspondences strengthen Rank35 as a residual implementation-cost control. Profile unchanged7030 before selecting a material optimization; reuse checked native7002 samples. Continue Rank31 later adverse-transfer localization independently.',
        timer_scope='Tactix registry uses steady-clock wall nanoseconds; run_phase timer starts before root backtrack/propagation and includes initial Probe. Native profile.c uses process CPU time with overlapping nested profiles; search-time simplify starts after root backtrack/propagation, and initial Probe is separately timed. Do not subtract slot-simplify, BVE-eliminate, or sum nested native profiles to price the gap.',
        limitation='Major counters and1M decision prefix match, not every candidate, physical state, budget or full trajectory. Congruence gate counts and Substitute ticks still differ; propagation and Vivify counts have different scopes. Different timing windows and sampled native run are not a matched-clock unit price. Pending candidate proofs remain unverified.',
        test_consideration='Six fixed correspondence controls plus existing timer tests cover explicit integer-field projection and native rows remain unparsed raw evidence. No solver behavior change.')
    with target.open('x') as stream:
        json.dump(out, stream, indent=2); stream.write('\n')
    print(target, len(rows), 'exact correspondences')

if __name__ == '__main__': main()

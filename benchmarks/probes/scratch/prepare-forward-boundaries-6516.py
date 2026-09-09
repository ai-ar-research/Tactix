#!/usr/bin/env python3
"""Two exact-source pairs observe first-call large input/output of forward cleanup."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6510-rank33-congruence-stages'
m0=json.loads((old/'manifest.json').read_text())
summary=old/'summary.json'; s=json.loads(summary.read_text()); assert s['parent_replay']
assert s['first_call_large_input']['identities_and_values']['equal_multiset']
assert s['binary_comparisons'][2]['equal_multiset']
for rel,digest in m0['hashes'].items(): assert sha(old/rel)==digest,rel
for phase in ('before','after'):
    c=root/f'benchmarks/runs/20260909-6516-forward-{phase}'
    assert not c.exists()
    m=json.loads(json.dumps(m0))
    for name in ('pins','logs','traces','results'): (c/name).mkdir(parents=True)
    for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
    runner=(old/'run.py').read_text().replace('tactix-6510-',f'tactix-6516-{phase}-')
    anchor="env['SAT_EVENT_TRACE_FORMULA_SET_ORDINAL']='0'"; assert runner.count(anchor)==1
    runner=runner.replace(anchor,"env['SAT_EVENT_TRACE_FORMULA_SET_ORDINAL']=str(m['formula_ordinal'])")
    anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
    extra="""env['SAT_EVENT_TRACE_BINARY_SET_PATH']=str(c/'traces'/f'{arm}.forward-binaries.tsv')
env['SAT_EVENT_TRACE_BINARY_SET_MATCH']=m['formula_selectors'][arm]
env['SAT_EVENT_TRACE_BINARY_SET_CONFLICT']=str(m['diagnostic_conflict'])
"""
    runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
    (c/'run.py').write_text(runner)
    batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6510-rank33-stages',f'6516-forward-{phase}')
    assert '--array=0-1' in batch
    (c/'array.sbatch').write_text(batch)
    m.update(phase=phase,formula_ordinal=0 if phase=='before' else 1,
        parent_summary=dict(path=str(summary.relative_to(root)),sha256=sha(summary)),
        parent_runs={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
        formula_selectors=dict(candidate='name=congruence-dense-resume|stage=second',kissat='congruence_dense_second'),
        purpose='Observe first-call227617 immediately before/after forward cleanup using unchanged6510 pins. Four sparse binary snapshots cover before/after of both calls; formula ordinal0/1 selects first-call boundary.',
        validation='Full own6510 ordinary1M/census/partial-proof replay. Compare strict four-frame pre/post binaries and first-call large identity/pose/root-value multisets; compare snapshots across the two campaigns. No broad event trace or early stop.',
        test_consideration='No solver/observer source changes; exact immutable tested pins. Reuse nine formula/stage reader fixtures, add strict sparse binary snapshot positive/negative tests. Require whole own replay for selector-only runner changes.',
        limitation='Bounded diagnostic not final proof or runtime claim. Sparse sites both surround forward pass, but differing closure output may precede cleanup; do not infer matching representatives or units from counts.')
    m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
    (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

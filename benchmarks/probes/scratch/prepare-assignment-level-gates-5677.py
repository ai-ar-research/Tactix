#!/usr/bin/env python3
"""Reuse exact replay, full B28+C5 and independent three-pair unit-cost gates."""
from pathlib import Path
base = Path(__file__).with_name('prepare-congruence-gates-5627.py').read_text()
base = base[:base.index("old=runs/'20260908-5611-current-congruence-output'")]
base = base.replace('20260908-5626-congruence-index-validated','20260908-5672-assignment-level')
base = base.replace("builds = {arm:root/('build/pin-congruence-index-repair-5623-'+arm) for arm in ('release','bc','trace')}",
    "builds={arm:root/f'build/pin-{n}-assignment-level-{arm}' for arm,n in [('release',5672),('trace',5673),('bc',5674)]}")
base = base.replace("replace('tactix-5611-','tactix-5627-')", "replace('tactix-5650-','tactix-5677-')")
exec(compile(base,__file__,'exec'))

old = runs/'20260908-5650-propagation-cursor-rank33-prefix'
c,m = initialize(old,'20260908-5677-assignment-level-rank33-prefix',True)
shutil.copy2(builds['trace']/'tactix',c/'pins/candidate')
m.update(parent_campaign=str(old.relative_to(root)),parent_summary_sha256=sha(old/'summary.json'),
    candidate_source=a['source'],source_patch_sha256=sha(authority/'cumulative.patch'),
    hypothesis='Bounded maximum preserves true levels and every observed decision,boundary,formula/proof byte per arm through1M.',
    validation='Two clean stopped runs and complete parent trace/proof identity;no acceptance timing or complete verdict.')
finish(c,m,True,True)

old = runs/'20260908-5651-propagation-cursor-BC-gate'
c,m = initialize(old,'20260908-5678-assignment-level-BC-gate')
for tier,build in [('B',builds['release']),('C',builds['bc'])]:
    arm='candidate_'+tier
    shutil.copy2(build/'tactix',c/'pins'/arm)
    m['arms'][arm].update(sha256=sha(c/'pins'/arm),source_build=str(build.relative_to(root)),source_patch_sha256=sha(authority/'cumulative.patch'))
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
m.update(purpose='Unchanged cumulative B28+C5 regression gate for true-level maximum early exit.',
    hypothesis='No verdict/work loss under the unchanged true-level semantics.',
    build_identity='Independent matching Release/BC90/90;trace89/89;3288 old/new state cases.',
    status='UNFUZZED isolated source;pending checks allow iteration,not promotion.')
finish(c,m,False,True)

old = runs/'20260908-5652-propagation-cursor-rank39-runtime'
c,m = initialize(old,'20260908-5679-assignment-level-rank39-runtime')
previous = runs/'20260908-5649-propagation-cursor-validated'
for arm,build,auth in [('candidate',builds['release'],authority),
    ('previous',root/'build/pin-propagation-cursor-repair-5647-release',previous)]:
    shutil.copy2(build/'tactix',c/'pins'/arm)
    shutil.copy2(auth/'cumulative.patch',c/'pins'/(arm+'.patch'))
    m['arms'][arm].update(sha256=sha(c/'pins'/arm),source_build=str(build.relative_to(root)),source_patch_sha256=sha(auth/'cumulative.patch'))
annotation=runs/'20260908-5671-propagation-annotation/manifest.json'
m.update(selection_authority=dict(path=str(annotation.relative_to(root)),sha256=sha(annotation)),
    hypothesis='Eliminate redundant antecedent-level reads after the proven current-level upper bound,with all work/ticks and proof bytes identical.',
    candidate_validation=a['test_consideration'],
    decision='Three predeclared independent pairs;advance after B28/prior solver outcomes,not certificates. Require full cross-arm/within-arm statistics/proof identity,all ratios,median,spreads<=2%.Full40 view remains mandatory.',
    status='UNFUZZED isolated successor;no promotion.')
finish(c,m)

#!/usr/bin/env python3
"""Bind unchanged B28+C5, a full causal replay and predeclared Rank39 timing."""
from pathlib import Path
base = Path(__file__).with_name('prepare-congruence-gates-5627.py').read_text()
base = base[:base.index("old=runs/'20260908-5611-current-congruence-output'")]
base = base.replace('20260908-5626-congruence-index-validated', '20260908-5649-propagation-cursor-validated')
base = base.replace('pin-congruence-index-repair-5623-', 'pin-propagation-cursor-repair-5647-')
base = base.replace("replace('tactix-5611-','tactix-5627-')", "replace('tactix-5634-','tactix-5650-')")
exec(compile(base, __file__, 'exec'))

old = runs / '20260908-5634-congruence-index-extended'
c, m = initialize(old, '20260908-5650-propagation-cursor-rank33-prefix', True)
shutil.copy2(builds['trace'] / 'tactix', c / 'pins/candidate')
m.update(parent_campaign=str(old.relative_to(root)), parent_summary_sha256=sha(old / 'summary.json'),
    candidate_source=a['source'], source_patch_sha256=sha(authority / 'cumulative.patch'),
    hypothesis='Local cursor and per-call specialization preserve every observed decision,boundary,clause/proof byte on each arm through1M;no trajectory or budget change is intended.',
    validation='Hash all pins,traces and parent records;require exact complete per-arm replay and both clean stopped proofs. Existing native182093 divergence should remain;no acceptance timing.')
finish(c, m, True, True)

old = runs / '20260908-5628-congruence-index-BC-gate'
c, m = initialize(old, '20260908-5651-propagation-cursor-BC-gate')
for tier, build in [('B', builds['release']), ('C', builds['bc'])]:
    arm = 'candidate_' + tier
    shutil.copy2(build / 'tactix', c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=str(build.relative_to(root)),
        source_patch_sha256=sha(authority / 'cumulative.patch'))
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
m.update(purpose='Unchanged cumulative B28+C5 gate for propagation unit-cost optimization.',
    hypothesis='Preserve all expected verdicts and cumulative regression behavior with unchanged scan semantics.',
    build_identity='Independent Release/BC89/89,matching binary SHA;trace88/88;224 old/new exact-state cases.',
    status='UNFUZZED isolated candidate;pending certificates permit iteration,not promotion.')
finish(c, m, False, True)

old = runs / '20260908-5629-congruence-index-rank33-runtime'
c, m = initialize(old, '20260908-5652-propagation-cursor-rank39-runtime')
previous = runs / '20260908-5626-congruence-index-validated'
for arm, build, auth in [('candidate', builds['release'], authority),
    ('previous', root / 'build/pin-congruence-index-repair-5623-release', previous)]:
    shutil.copy2(build / 'tactix', c / 'pins' / arm)
    shutil.copy2(auth / 'cumulative.patch', c / 'pins' / (arm + '.patch'))
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=str(build.relative_to(root)),
        source_patch_sha256=sha(auth / 'cumulative.patch'))
full = runs / '20260908-5604-factor-support-ranks1-40'
with (full / 'matrix.tsv').open(newline='') as stream:
    target = next(row for row in csv.DictReader(stream, delimiter='\t') if row['rank'] == '39')
with (old / 'matrix.tsv').open(newline='') as stream:
    reader = csv.DictReader(stream, delimiter='\t'); fields = reader.fieldnames; rows = list(reader)
for row in rows:
    for key in ('rank', 'instance', 'expected', 'tier'): row[key] = target[key]
with (c / 'matrix.tsv').open('w', newline='') as stream:
    writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
fm = json.loads((full / 'manifest.json').read_text())
profile = runs / '20260908-5544-small-rank39-profile/summary.json'
m.update(input_sha256={target['instance']: fm['input_sha256'][target['instance']]},
    parent_campaign=str(full.relative_to(root)), selection_authority=dict(path=str(profile.relative_to(root)), sha256=sha(profile)),
    hypothesis='Rank39 profile identifies dominant main propagation cost;remove repeated cursor field traffic and ignore dispatch while preserving exact cross-arm work and proof bytes.',
    decision='Three predeclared independent paired waves,advance after expected solver outcomes not proofs. Require cross-arm and within-arm non-timing statistics/proof identity;all ratios,median,spreads<=2%.Full Rank1-40 remains required.',
    candidate_validation=a['test_consideration'], status='UNFUZZED isolated candidate;no promotion.')
finish(c, m)

#!/usr/bin/env python3
"""Extend the exact-work assignment-level bound to all unchanged40 pairs."""
from pathlib import Path
source=Path(__file__).with_name('prepare-sweep-small-full-5535.py').read_text()
changes={
 '20260908-5485-sweep-cursor-ranks1-40':'20260908-5664-propagation-cursor-ranks1-40',
 '20260908-5522-sweep-small':'20260908-5672-assignment-level',
 '20260908-5529-sweep-small-rank32-trace/repair-localization.json':'20260908-5677-assignment-level-rank33-prefix/summary.json',
 '20260908-5531-sweep-small-rank32-runtime':'20260908-5679-assignment-level-rank39-runtime',
 '20260908-5530-sweep-small-BC-gate':'20260908-5678-assignment-level-BC-gate',
 '20260908-5535-sweep-small-ranks1-40':'20260908-5712-assignment-level-ranks1-40',
 'build/pin-5522-sweep-small-release':'build/pin-5672-assignment-level-release',
 'build/pin-5524-sweep-small-bc':'build/pin-5674-assignment-level-bc',
 'Full unchanged Rank1-40 paired Kissat comparison of retained eligibility and real small-environment Sweep solves on5473.':'Full unchanged Rank1-40 paired Kissat comparison of the exact antecedent-level bound on5649.',
 'General Sweep lifecycle repairs close the observed Rank32 decision divergence; measure net full-suite runtime, work and memory effects, including regressions, without multiplying cross-window ratios.':'Measure the general assignment-level unit-cost optimization across all40 pairs,including regressions. No changed algorithm,trajectory or tuning. One initial exact-work pair does not establish replicated gain.',
 'Release86/86 twice with matching hashes; trace85/85; old one-clause counterexample,12 native/SubSolver lifecycle cases and budget/state/model tests. UNFUZZED.':'Release90/90 twice,trace89/89;3288 actual old/new assignment fixtures agree and5677 full1M parent traces/proofs replay. Current-source fuzz remains required.',
 'Frozen tested Release5522, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5485.':'Frozen tested Release5672,observers disabled;same native Kissat,matrix,runner,checkers and resource contract as5664.',
 'Only one changed-work pair, checks pending; replica waves remain required. This is an investigation trigger, not a replicated acceptance claim.':'Only one exact-work/proof pair,checks pending;three predeclared waves remain required. This full view is an investigation measurement,not promotion.'}
for old,new in changes.items():
    assert old in source,old
    source=source.replace(old,new)
start,end=source.index("assert v['clean_stops']"),source.index('t = [json.loads')
source=source[:start]+"assert v['parent_full_replay']\n"+source[end:]
source=source.replace('records, checked = {}, 0',"""stats=lambda r:[s for s in r['solver_statistics'] if not s.startswith('c timing')]
assert stats(t[0])==stats(t[1]) and t[0]['certificate_sha256']==t[1]['certificate_sha256']
records, checked = {}, 0""")
exec(compile(source,__file__,'exec'))
import subprocess
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted full Rank1-40',job)

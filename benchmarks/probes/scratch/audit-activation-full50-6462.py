#!/usr/bin/env python3
"""Seal all50 activation outcomes, timing launch and adverse-prefix continuation."""
from pathlib import Path
source=Path(__file__).with_name('audit-shared-blocker-6457.py')
code=source.read_text()
for old,new in {
 '2026-09-09-shared-blocker-audit-6457.json':'2026-09-09-activation-full50-audit-6462.json',
 '2026-09-09-activation-closure-audit-6446.json':'2026-09-09-shared-blocker-audit-6457.json',
 '2026-09-09-shared-blocker-experiment.md':'2026-09-09-activation-full50-and-adverse-prefix.md',
 'range(6448,6459)':'range(6459,6467)',
 "'6439-activation-ranks1-50','6451-shared-blocker-BC-gate')":"'6439-activation-ranks1-50','6451-shared-blocker-BC-gate','6459-rank35-later-prefix')",
 '20260909-0845-shared-blocker':'20260909-0900-activation-full50',
 "('6443-activation-three-wave-runtime','6454-shared-blocker-three-wave-runtime')":"('6443-activation-three-wave-runtime','6454-shared-blocker-three-wave-runtime','6464-activation-adverse-three-wave-runtime')",
 'No new solver/proof/model/build failure or excluded measurement.6443 refused before creating output while its full-run witness oracles were pending. Initial PDF had3.23338pt overflow, caught before the audit wrote its checkpoint. Original log retained; paragraph reflowed and two clean compiles required. No resubmission or changed frozen artifact. All source suites and actual old/new fixture controls pass.':'No new solver/proof/model/build failure or excluded measurement.6441 refused without output while only49 solver outcomes existed; no partial subset scored.6461 pending check ran maintained malformed-row controls and created no summary. PDF initially had9.17638/9.5363pt overflow, caught before audit; original log retained and paragraph reflowed before two clean compiles. All previous frozen outputs retained. No source edit this checkpoint.',
 'Complete full6439 measurement and activation timing6443;6453 requires all66 shared-blockerBC expected outcomes and33exactparentwork/proof pairs, then6454/6455 three-wave cost experiment. No new native refresh, source promotion, fuzz inspection, denied transfer or push retry.':'Full6441 snapshot is frozen; continue6443 activation pairedwaves and6459 adverse-prefix validation.6453 requires66shared-blocker outcomes and33exactparentwork/proof pairs, then6454/6455 three cost waves. Preserve every full50 discrepancy, memory and fuzz gates. No native refresh, promotion, fuzz inspection or denied transfer/push retry.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="for p in (root/'benchmarks/reports/2026-09-09-activation-full50-6441').glob('*'): retain(p)"
assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nfull=root/'benchmarks/reports/2026-09-09-activation-full50-6441/summary.json'\ns=json.loads(full.read_text()); assert len(s['rows'])==50 and s['score']['expected_solves']==50\nfor rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel\ngate=root/'benchmarks/runs/20260909-6451-shared-blocker-BC-gate/solver-checkpoint-6453.json'\ng=json.loads(gate.read_text()); assert g['solver_records']==66 and g['exact_parent_candidate_work_and_proof']\nfor group in ('hashes','parent_oracles'):\n    for rel,digest in g[group].items(): assert sha(root/rel)==digest,rel")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Extend the previous immutable audit with6508/6510 observer evidence."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank33-census-6505.py')
wrapper=source.read_text()
end="exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})"
assert wrapper.count(end)==1
namespace={'__file__':str(source),'__name__':'assemble_only'}
exec(compile(wrapper.replace(end,'assembled=code'),str(source),'exec'),namespace)
code=namespace['assembled']
replacements={
 '2026-09-09-rank33-census-audit-6505.json':'2026-09-09-congruence-stages-audit-6513.json',
 '2026-09-09-activation-memory-supplement-6497.json':'2026-09-09-rank33-census-audit-6505.json',
 "owned={'PROGRESS.md','benchmarks/reports/2026-09-09-rank33-split-and-congruence-census.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-congruence-installed-and-input.md','tools/bench/congruence_stages.py','tools/bench/test_congruence_stages.py'}",
 'range(6499,6508)':'range(6508,6516)',
 "'6491-position-key-three-wave-runtime','6501-rank33-congruence-replay')":"'6491-position-key-three-wave-runtime','6501-rank33-congruence-replay','6510-rank33-congruence-stages')",
 '20260909-1045-rank33-census':'20260909-1105-congruence-stages',
 '6488 failed a zero-overlap assumption before writing;6499 corrected with7tests and full prefix/hash/row accounting.6501 manifest inherited array0-2 but hashed batch/runner and exact accounting have intended0-1;6503 sidecar corrects metadata without edits or resubmission.6504 raw post-binary-extraction labels are not equivalent sites;6507 source-order qualification supersedes alignment, proves108890 normalized candidates and planned84pair union exact, not actual installed state. No solver/proof/model/build failure, excluded measurement, source promotion or successful snapshot rerun.':'Initial watcher invocation omitted required --job and failed argument parsing without changing a job. Correct watcher started once. First PDF had1.09502pt overflow; log preserved, paragraph reflowed and two clean compiles required before audit. Initial combined apply_patch context mismatch made no edits. No build, unit-test, solver, proof or model failure; no successful frozen writer rerun, excluded measurement, production promotion or fuzz inspection.',
 '6504/6507 DONE exact own1M replay and qualified census. Next neutral actual post-install/post-gate snapshot with first-call large-formula identity at227617 before solver repair. Keep historical Sweep satisfied-equivalence evidence and Rank35 candidate closure.6493 DONE all18expected, neutralRSS/VmPeak/runtime: no gain or selective repeats for6475; keep6448 rejected. Check finalcertificates asynchronously; no new timingvector or fullreevaluation. No promotion, fuzz inspection or denied transfer/push retry.':'6512 owns actual installed comparisons and first-call incoming large identities/poses/values after full own1M replay. Use its observed earliest difference to select a general repair or next neutral observer, not incompatible stage labels. Keep all50 scored, fixed Kissat vector and neutral/rejected experiments. Check final certificates asynchronously; no promotion, fuzz inspection or denied transfer/push retry.',
}
for old,new in replacements.items():
    assert code.count(old)==1,(old,code.count(old)); code=code.replace(old,new)
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
c=root/'benchmarks/runs/20260909-6508-congruence-stages-build'
b=json.loads((c/'manifest.json').read_text())
assert b['release_exact_parent']
assert b['binaries']['build/pin-6508-congruence-stages-release/tactix']=='5946f59800adbf1bbbb20c64ac4bcbcd00ff8dc980ebfdc04edd844db2cd7eb4'
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
for rel,digest in b['binaries'].items(): assert sha(root/rel)==digest,rel
assert sha(c/'candidate.patch')==b['patch_sha256']
assert sha(c/'parents.json')==b['parents_sha256']
for rel,digest in b['logs'].items(): assert sha(c/rel)==digest,rel
for kind,count in (('release',109),('trace',108)):
    assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
for p in c.iterdir():
    if p.suffix in ('.json','.patch'): retain(p)
for p in (c/'logs').iterdir(): retain(p)
c=root/'benchmarks/runs/20260909-6510-rank33-congruence-stages'
m=json.loads((c/'manifest.json').read_text())
assert m['resource_contract']['array']=='0-1'
assert m['resource_contract']['cpus_per_task']==12 and m['resource_contract']['memory_mib']==48000
for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
s=json.loads((c/'summary.json').read_text()); assert s['parent_replay']
assert len(s['binary_comparisons'])==8 and set(s['first_call_large_input']['counts'])=={'candidate','kissat'}
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
retain(c/'summary.json')
q=c/'gate-pair-localization-6515.json'; z=json.loads(q.read_text())
assert z['parent_summary_sha256']==sha(c/'summary.json')
for rel,digest in z['source_sha256'].items(): assert sha(root/rel)==digest,rel
retain(q)
"""
code=code.replace(anchor,extra+'\n'+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

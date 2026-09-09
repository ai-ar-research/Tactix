#!/usr/bin/env python3
"""Seal observer-neutral selected trace, tested new observer, and active replay."""
from pathlib import Path
source=Path(__file__).with_name('audit-replicas-census-6347.py')
code=source.read_text()
for old,new in (
    ('2026-09-09-replicas-census-audit-6347.json','2026-09-09-transitive-schedule-audit-6354.json'),
    ('2026-09-09-collapse-clean-audit-6337.json','2026-09-09-replicas-census-audit-6347.json'),
    ('2026-09-09-collapse-replicas-and-transitive-census.md','2026-09-09-transitive-source-eligibility.md'),
    ('range(6340,6349)','range(6349,6356)'),
    ('20260909-0530-replicas-and-census','20260909-0547-transitive-eligibility'),
    ("'20260909-6345-rank32-selected-transitive')", "'20260909-6345-rank32-selected-transitive','20260909-6352-rank32-transitive-schedule')")):
    assert old in code,old;code=code.replace(old,new)
marker='commands=[(n,';assert code.count(marker)==1
extra="""owned.update({'tools/bench/transitive_selected.py','tools/bench/test_transitive_selected.py',
    'benchmarks/probes/scratch/transitive-schedule-6350.h','benchmarks/probes/scratch/test-transitive-schedule-6351.c'})
selected=root/'benchmarks/runs/20260909-6345-rank32-selected-transitive/summary.json'
ss=json.loads(selected.read_text());assert ss['parent_replay']
for rel,digest in ss['hashes'].items():assert sha(root/rel)==digest,rel
assert {r['event'] for r in ss['observed']['candidate']['rows']}=={'endpoint'}
assert all(r['source_active']==1 and r['source_value']==0 for r in ss['observed']['candidate']['rows'])
retain(selected)
build=root/'benchmarks/runs/20260909-6350-transitive-schedule-build'
bs=json.loads((build/'manifest.json').read_text());assert bs['release_exact_parent']
assert sha(build/'parents.json')==bs['parents_sha256']
for source,files in bs['files'].items():
    for rel,digest in files.items():assert sha(root/source/rel)==digest,rel
for rel,digest in bs['binaries'].items():assert sha(root/rel)==digest,rel
for rel,digest in bs['patches'].items():assert sha(build/rel)==digest,rel
for rel,digest in bs['logs'].items():assert sha(build/rel)==digest,rel
for q in build.rglob('*'):
    if q.is_file() and q.suffix in ('.json','.patch','.log','.tsv'):retain(q)
assert (build/'writer-c.tsv').read_bytes()==(build/'writer-cpp.tsv').read_bytes()
"""
code=code.replace(marker,extra+marker)
code=code.replace('Completed6340 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and selected6345 scratch untouched.',
    'Completed6340/6345 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and new6352 scratch untouched.')
code=code.replace("failure_classification='6342 passed exact full replay/framing but incorrectly equated conflict-filtered counts to one invocation; preserved6342 and corrected with6344 plus two regression tests. No solver/proof failure.',",
    "failure_classification='No failed test, solver or proof result this increment. Initial selected-test discovery included the imported prior TestCase a second time; switched to module import before counting17 focused/56full benchmark tests. First report draft narrowed its claim to first observed entry; second Tactix invocation eligibility is not independently observed.',")
code=code.replace("next='Validate6345 selected-pair non-transitive6340 replay and partial proof before interpretation; add selected-event framing tests. All6332 solver waves exact; continue checker outcomes, not duplicate timing. Sourcefuzz/memory/newtracker approval/promotion remain open; no denied push/transfer retry or fuzz inspection.'",
    "next='Validate new6352 against full ordinary+selected6345 trace/proof replay, then test/parse every before/after schedule snapshot and require equal multisets. Compare active input membership, actual second-IDX key identities/scorebits/stamps/nativeflag, sorted position and reached prefix. First-entry endpoints valid; second Tactix entry eligibility unobserved. Continue final checkers; no promotion, denied push/transfer retry or fuzz inspection.'")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

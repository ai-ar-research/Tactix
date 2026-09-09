#!/usr/bin/env python3
"""Successor audit; never rewrite the frozen6291 checkpoint."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank50-admission-6291.py')
code=source.read_text()
for old,new in (
    ('2026-09-09-rank50-admission-audit-6291.json','2026-09-09-subsumption-material-audit-6301.json'),
    ('2026-09-09-rank50-admission-gap.md','2026-09-09-subsumption-material-design.md'),
    ('2026-09-09-vivify-binary-checkpoint-6284.json','2026-09-09-vivify-binary-checkpoint-6294.json'),
    ('range(6283, 6293)','range(6293, 6303)'),
    ('20260909-0400-rank50-admission','20260909-0410-subsumption-material'),
    ('validate-rank50-admission-6289.py','validate-subsumption-producer-6300.py'),
    ('Three bounded6286 task scratch directories including partial proofs removed by their owners after hashing','One bounded6298 task scratch directory including its partial proof removed by its owner after hashing'),
    ('Audit canonical forward-added lifecycle and implement separate faithful Eliminate material invitation, preserving SP-SLOT-5 events.',
     'New material tracker needs rule-dev/revamp design approval; request pending. Native6298 fully replayed6286 and identified binary574,-525 at780400. Do not implement new kernel primitive without approval; preserve SP-SLOT-5 events.')):
    assert old in code,old;code=code.replace(old,new)
anchor="'20260909-6286-rank50-admission',"
assert code.count(anchor)==1
code=code.replace(anchor,anchor+" '20260909-6298-rank50-subsumption-producer',")
anchor="commands = [(n, ['python3'"
assert code.count(anchor)==1
extra="""native_build=root/'benchmarks/runs/20260909-6293-subsumption-producer-build'
native=json.loads((native_build/'manifest.json').read_text())
for rel,digest in native['files'].items(): assert sha(root/native['source']/rel)==digest,rel
assert sha(root/native['source']/'build/kissat')==native['binary_sha256']
assert sha(native_build/'native.patch')==native['patch_sha256']
assert 'PASS: actual added-literal flips' in native['fixture']
for p in native_build.rglob('*'):
    if p.is_file() and p.suffix in ('.json','.patch','.log'): retain(p)
"""
code=code.replace(anchor,extra+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

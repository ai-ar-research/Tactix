#!/usr/bin/env python3
"""Full short-case decision/macro trace, with exact own Release replay gate."""
import csv
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7349-root-decision-trace'
gate=root/'benchmarks/runs/20260911-7347-root-storage-BC'
assert not c.exists()
gm=json.loads((gate/'manifest.json').read_text()); deps=dict(gm['dependencies'])
def bind(path,expected=None):
    digest=sha(path); assert expected is None or digest==expected,path
    deps[str(path.relative_to(root))]=digest
for rel,digest in list(deps.items()): bind(root/rel,digest)
for rel,digest in gm['pin_sha256'].items(): bind(gate/rel,digest)
bind(gate/'manifest.json'); bind(gate/'launch.json')
oracles={}
for arm,task in [('parent',26),('candidate',27)]:
    path=gate/f'results/{task:03d}.json'; final=path.exists()
    if not final: path=gate/f'provisional/{task:03d}.json'
    record=json.loads(path.read_text()); assert classify(record,final)['status'] in ('pending_validation','checked_UNSAT')
    bind(path); oracles[arm]=str(path.relative_to(root))
for folder in ('pins','logs','claims','provisional','results','traces'): (c/folder).mkdir(parents=True)
for name in ('drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(gate/'pins'/name,c/'pins'/name)
arms={}
for arm,stem in [('parent','7302-clause-realloc'),('candidate','7340-debug-contracts')]:
    authority=root/('benchmarks/runs/20260911-'+stem+'-build/manifest.json')
    bm=json.loads(authority.read_text()); bind(authority)
    binary=root/('build/pin-'+stem+'-trace/tactix'); rel=str(binary.relative_to(root))
    digest=bm['binaries'][rel] if arm=='parent' else bm['binaries']['trace'][rel]
    bind(binary,digest); shutil.copy2(binary,c/'pins'/arm)
    arms[arm]=dict(path='pins/'+arm,sha256=digest,arguments=[],source_manifest=str(authority.relative_to(root)))
rows=[dict(task=i,rank=14,arm=arm,tier='B',instance=record['instance'],expected='UNSAT') for i,arm in enumerate(('parent','candidate'))]
with (c/'matrix.tsv').open('x',newline='') as out:
    writer=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n'); writer.writeheader(); writer.writerows(rows)
runner=(gate/'pins/runtime_task.py').read_text()
anchor='        solve = measured(command, stdout, stderr,'
assert runner.count(anchor)==1
runner=runner.replace(anchor,'''        trace_files = {name: scratch / (name + '.tsv') for name in ('decisions', 'macro')}
        os.environ['TACTIX_DECISION_STREAM_TRACE'] = str(trace_files['decisions'])
        os.environ['TACTIX_MACRO_BOUNDARY_TRACE'] = str(trace_files['macro'])
'''+anchor)
anchor='        verdict, statistics = solver_summary(stdout)'
assert runner.count(anchor)==1
runner=runner.replace(anchor,'''        import gzip
        trace_artifacts = {}
        for name, path in trace_files.items():
            if not path.is_file(): raise RuntimeError('missing trace: ' + name)
            packed = CAMPAIGN / 'traces' / (f'{task:03d}-' + name + '.tsv.gz')
            with path.open('rb') as inp, packed.open('xb') as raw:
                with gzip.GzipFile(fileobj=raw, mode='wb', mtime=0) as out:
                    shutil.copyfileobj(inp, out)
            trace_artifacts[name] = dict(path=str(packed.relative_to(CAMPAIGN)), sha256=sha256(packed),
                                         uncompressed_sha256=sha256(path), bytes=path.stat().st_size)
        del os.environ['TACTIX_DECISION_STREAM_TRACE']
        del os.environ['TACTIX_MACRO_BOUNDARY_TRACE']
'''+anchor)
anchor='            "schema": "tactix-matched-slurm-memory-regression-result-v2",'
assert runner.count(anchor)==2
runner=runner.replace(anchor,'            "trace_artifacts": trace_artifacts,\n'+anchor,1)
compile(runner,str(c/'pins/runtime_task.py'),'exec')
with (c/'pins/runtime_task.py').open('x') as out: out.write(runner)
batch=(gate/'array.sbatch').read_text().replace(str(gate),str(c)).replace('7347-root-storage-BC','7349-root-decision-trace').replace('--array=0-65','--array=0-1')
with (c/'array.sbatch').open('x') as out: out.write(batch)
for path in [Path(__file__),Path(__file__).with_name('check-submit-root-decision-trace-7349.py')]: bind(path)
manifest=dict(schema='tactix-root-decision-trace-v1',arms=arms,
    input_sha256={record['instance']:gm['input_sha256'][record['instance']]},collect_runtime_context=True,
    resource_contract=dict(gm['resource_contract'],array='0-1'),dependencies=deps,previous_work_oracles=oracles,
    hypothesis='7348 isolates B14 changed work to root folding, not collector connection repair. Locate the earliest changed portable decision and preceding macro boundary on complete independent solves using already-built trace pins.',
    acceptance='Both full proofs/work fingerprints must replay their own uninstrumented BC7347 arm exactly, independent original proofs checked, scratch clean. Compare entire decision streams by permanent external literal and level. Macro trail length and queue stamp reflect representation and are not semantic equivalence oracles; report them raw. No broad EventTrace state digest, no conflicting prefix stop, no runtime performance claim.',
    test_consideration='No solver edit; existing actual trace functions, old runtime measurement/checking contract. New runner only sets two local trace paths and archives them after solver exit. Identity/replay checks and full compressed trace hashes bind the analysis.')
manifest['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out: json.dump(manifest,out,indent=2)
for path in (c/'pins').iterdir(): path.chmod(path.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Pin the unchanged B28+C5 comparison for root storage and collector contracts."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260911-7346-root-storage-BC'
old = root / 'benchmarks/runs/20260911-7305-clause-realloc-BC'
assert not c.exists()
dependencies = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    dependencies[str(path.relative_to(root))] = digest
om = json.loads((old / 'manifest.json').read_text()); bind(old / 'manifest.json')
for rel, digest in om['pin_sha256'].items(): bind(old / rel, digest)
for task in range(66):
    path = old / f'results/{task:03d}.json'
    assert classify(json.loads(path.read_text()), True)['checked'], path
    bind(path)
authority = root / 'benchmarks/runs/20260911-7340-debug-contracts-build/manifest.json'
build = json.loads(authority.read_text()); assert build['status'] == 'PASS'; bind(authority)
for rel, digest in build['files'].items(): bind(root / build['source'] / rel, digest)
for rel, digest in build['hashes'].items(): bind(root / rel, digest)
for stem, expected in [('7341-debug-contracts-regression','PASS_NEGATIVE_CONTROLS'),
                       ('7343-debug-root-proofs','PASS'), ('7345-debug-contract-proofs','PASS')]:
    path = root / ('benchmarks/runs/20260911-' + stem + '/manifest.json')
    gate = json.loads(path.read_text()); assert gate['status'] == expected; bind(path)
    for rel, digest in gate['hashes'].items(): bind(root / rel, digest)
    if stem.startswith('7343'):
        assert gate['checked_UNSAT'] == 64 and gate['checked_SAT'] == 64
        assert gate['negative_controls'] == 2 and gate['exact_fold_disabled_certificates']
    if stem.startswith('7345'): assert gate['checked_UNSAT'] == 112 and gate['checked_SAT'] == 80
for kind, count in [('release',123), ('bc',123), ('trace',122)]:
    path = authority.parent / f'logs/{kind}-ctest.log'
    assert f'100% tests passed out of {count}' in path.read_text(); bind(path)
parent_path = root / 'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
parent = json.loads(parent_path.read_text()); bind(parent_path)
for folder in ('pins','logs','claims','results','provisional'): (c / folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
arms = {}
for tier, kind in [('B','release'),('C','bc')]:
    for label, stem in [('current','7302-clause-realloc'),('candidate','7340-debug-contracts')]:
        binary = root / ('build/pin-' + stem + '-' + kind + '/tactix')
        key = str(binary.relative_to(root))
        digest = parent['binaries'][key] if label == 'current' else build['binaries'][kind][key]
        bind(binary, digest)
        arm = label + '_' + tier
        shutil.copy2(binary, c / 'pins' / arm)
        arms[arm] = dict(path='pins/'+arm, sha256=digest, arguments=[],
            source_commit=parent['source_commit'],
            source_build=str(binary.parent.relative_to(root)),
            source_manifest=str((parent_path if label == 'current' else authority).relative_to(root)),
            source_state='Isolated source; source_commit is base lineage, not complete candidate source.')
for label in ('current','candidate'): assert arms[label+'_B']['sha256'] == arms[label+'_C']['sha256']
batch = (old / 'array.sbatch').read_text().replace(str(old),str(c)).replace('7256-occurrence-BC','7346-root-storage-BC')
with (c / 'array.sbatch').open('x') as out: out.write(batch)
for path in [Path(__file__),Path(__file__).with_name('check-submit-root-storage-BC-7346.py')]: bind(path)
manifest = dict(schema='tactix-root-storage-BC-v1', arms=arms,
    input_sha256=om['input_sha256'], collect_runtime_context=True,
    resource_contract=om['resource_contract'], dependencies=dependencies,
    hypothesis='Archive unreferenced fixed-root facts out of hot per-variable arrays at the existing Reduce compaction boundary; preserve logical root watermarks, public actions, original models and proofs. Correct collection connection-state invariant independently of optional occurrence index.',
    test_consideration=build['test_consideration']+' Plus 320 independent original-CNF proof/model checks, bad-proof controls, actual-parent collector failure and strict optimized diagnostic-symbol negative controls.',
    acceptance='All66 expected outcomes and independent checked/clean original models/proofs, no loss. This is a correctness/regression gate against actual7302, not production competitiveness. Preserve and enumerate every complete non-timing counter/certificate difference; any difference blocks automatic exact-work acceptance until separately classified from source. No arbitrary fingerprint exclusions or changed policy thresholds. Pending checks permit provisional independent development only.',
    next='After all66 solver records and classification of any drift, three independent paired Rank25/31 waves on actual7302 versus7340. Measure whole proof-on runtime and peak RSS, median and full ratio/spread ranges. Memory improvement must not be bought by material runtime/solve regression. Full50 only after the targeted premise survives; no Kissat denominator refresh.',
    pending='Full Debug7342 final demo and current-source fuzz remain separate unresolved gates. Production unchanged.')
manifest['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c / 'manifest.json').open('x') as out: json.dump(manifest,out,indent=2)
for path in (c / 'pins').iterdir(): path.chmod(path.stat().st_mode & ~0o222)
print(c)

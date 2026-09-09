#!/usr/bin/env python3
"""Short current-vs-native prefixes for the priority memory/work and UNSAT cases."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
old = root / 'benchmarks/runs/20260908-6092-rank37-macro-heaps'
om = json.loads((old / 'manifest.json').read_text())
for rel, digest in om['hashes'].items():
    assert sha(old / rel) == digest, rel
build = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json'
bm = json.loads(build.read_text())
candidate = root / 'build/pin-6127-publication-trace/tactix'
assert sha(candidate) == bm['binaries']['build/pin-6127-publication-trace']
selection = root / 'benchmarks/baselines/rank1-50-selection-v1'
sm = json.loads((selection / 'manifest.json').read_text())
assert sha(selection / 'matrix.tsv') == sm['matrix_sha256']
with (selection / 'matrix.tsv').open(newline='') as stream:
    inputs = {int(r['rank']): r for r in csv.DictReader(stream, delimiter='\t')}
for number, rank, stop in ((6139, 21, 10000), (6140, 4, 100000)):
    c = root / f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'
    assert not c.exists()
    for name in ('pins', 'logs', 'results', 'traces'):
        (c / name).mkdir(parents=True)
    shutil.copy2(candidate, c / 'pins/candidate')
    shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
    code = (old / 'run.py').read_text()
    begin = code.index('env.update(SAT_DECISION_HEAP_SNAPSHOT=')
    end = code.index('assert not any(key.endswith', begin)
    code = code[:begin] + code[end:]
    code = code.replace('tactix-6092-', f'tactix-{number}-')
    needle = "m['stream_from'] if arm == 'candidate' else 100000"
    assert code.count(needle) == 1
    code = code.replace(needle, "m['stream_from']")
    assert 'SAT_CHOICE_STATE_' not in code and 'SAT_MACRO_HEAP_PATH' not in code
    (c / 'run.py').write_text(code)
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
    batch = batch.replace('6092-rank37-macro-heaps', f'{number}-rank{rank}-prefix')
    (c / 'array.sbatch').write_text(batch)
    compressed = root / 'benchmarks/cache' / inputs[rank]['instance']
    assert sha(compressed) == sm['input_sha256'][inputs[rank]['instance']]
    m = dict(rank=rank, input=str(compressed.relative_to(root)),
        input_sha256_compressed=sha(compressed), stop_conflict=stop,
        stream_from=0, common_decisions=1,
        resource_contract=om['resource_contract'],
        candidate_authority=dict(path=str(build.relative_to(root)), sha256=sha(build)),
        native_authority=dict(path=str((old / 'manifest.json').relative_to(root)), sha256=sha(old / 'manifest.json')),
        selection_sha256=sha(selection / 'manifest.json'),
        purpose='Locate first decision or portable-boundary mismatch oncurrent6107 versusnative;prefix only,not runtime,finalproof ormodel acceptance.',
        hypothesis=('Rank21 excess work afterReduce rootpublication needsits next coupled boundary,not reverting the validated general repair.'
                    if rank == 21 else 'Rank4bothfreshUNSATcertificates pass;useashortcurrentprefix todistinguish changedwork fromunitcost beforeoptimizing.'),
        validation='Both exactrequested stops,completecontiguous decisionstreams from0,hash-bound partialproofs/traces andclean scratch. Report firstmismatch withoutinferringcause fromlatercounts. Initialone-decision hash isnot afullprefix claim.',
        test_consideration='No newsolverorobserver source;trace98/98 andjournal99/99 buildauthority. Reused boundedrunner with allrank37-specificselectors removed;allfirst-prefix decisions retained forbotharms. Existing decision-prefix/comparator tests required.',
        limitation='Nativeinstrumenteddiagnostic rerun allowed;frozen timingvectorunchanged. Observer-neutrality mustbevalidated beforecausalinterpretation. Allpriorcampaign scriptsandpins untouched.')
    m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
                   [c / 'pins/candidate', c / 'pins/kissat', c / 'run.py', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir():
        p.chmod(p.stat().st_mode & ~0o222)
    print(c)

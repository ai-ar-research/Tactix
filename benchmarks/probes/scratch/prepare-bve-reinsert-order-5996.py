#!/usr/bin/env python3
"""Observe adjacent reinsertion subjects and the existing retry producer."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]
PARENT = ROOT / 'benchmarks/runs/20260908-5992-bve-subtree-ancestry'
C = ROOT / 'benchmarks/runs/20260908-5996-bve-reinsert-order'
SUBJECTS = '5409,5415,5419,5232,148944,331390,225614,60705'

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def main():
    assert not C.exists()
    m = json.loads((PARENT / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items():
        assert sha(PARENT / rel) == digest, rel
    validation = PARENT / 'heap-validation.json'
    v = json.loads(validation.read_text())
    assert v['full_ordinary_trace_and_partial_proof_replay']
    assert v['complete_bve_candidate_replay']
    runner = (PARENT / 'run.py').read_text().replace('tactix-5992-', 'tactix-5996-')
    old = "env['SAT_BVE_HEAP_SUBJECTS'] = '225614,60705,477747,244396,5409,3343,3531,242204'"
    assert runner.count(old) == 1
    runner = runner.replace(old, "env['SAT_BVE_HEAP_SUBJECTS'] = '" + SUBJECTS + "'\n"
        "env[prefix + '_RETRY_PRODUCER_TRACE'] = str(c / 'traces' / (arm + '.retry-producer.tsv'))\n"
        "env['SAT_RETRY_PRODUCER_FROM'] = '271079'\n"
        "env['SAT_RETRY_PRODUCER_TO'] = '271079'\n"
        "env['SAT_RETRY_PRODUCER_EXTERNAL'] = '5409'")
    compile(runner, str(C / 'run.py'), 'exec')
    assert len(set(SUBJECTS.split(','))) == 8
    C.mkdir()
    for name in ('pins', 'logs', 'results', 'traces'):
        (C / name).mkdir()
    for p in (PARENT / 'pins').iterdir():
        shutil.copy2(p, C / 'pins' / p.name)
    (C / 'run.py').write_text(runner)
    batch = (PARENT / 'array.sbatch').read_text().replace(str(PARENT), str(C))
    batch = batch.replace('5992-bve-subtree-ancestry', '5996-bve-reinsert-order')
    (C / 'array.sbatch').write_text(batch)
    m.update(parent_campaign=str(PARENT.relative_to(ROOT)), subjects=SUBJECTS.split(','),
        prior_heap_evidence=dict(path=str(validation.relative_to(ROOT)), sha256=sha(validation)),
        hypothesis='5409 and5415 are reinserted in opposite order while eliminating148944, caused by physical literal pose of the seven-literal incoming clause. Test actual Tactix deletion pose and both post-push orders; native retry observer has no clause payload, so native deletion pose remains indirect.',
        native_rerun_reason='Missing5415 adjacent push and5409 producer events only; fixed12CPU runtime denominator unchanged.',
        validation='Require full ordinary300K traces, partial proof bytes and entire BVE candidate journals to replay5992 before interpreting producer and heap rows.',
        test_consideration='No source or observer edits. Reuse validated5949 pins and parser controls; add producer-schema/window and post-push ordering controls in new validator.',
        created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
    m['hashes'] = {str(p.relative_to(C)): sha(p) for p in
                   [C / 'run.py', C / 'array.sbatch', *(C / 'pins').iterdir()]}
    (C / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    print(C)

if __name__ == '__main__':
    main()

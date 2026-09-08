#!/usr/bin/env python3
"""Capture complete entry/exit formula images of the second forward pass."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5917-reduce-root-next-choice'
current = root / 'benchmarks/runs/20260908-5925-reduce-preclean-rank32-prefix'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
pm = json.loads((parent / 'manifest.json').read_text())
for rel,digest in pm['hashes'].items(): assert sha(parent / rel) == digest
cm = json.loads((current / 'manifest.json').read_text())
for rel,digest in cm['hashes'].items(): assert sha(current / rel) == digest
for number,stage in ((5931,'begin'),(5932,'end')):
    c = root / f'benchmarks/runs/20260908-{number}-second-forward-{stage}-formula'
    c.mkdir(exist_ok=False)
    for folder in ('pins','logs','results','traces'): (c / folder).mkdir()
    shutil.copy2(current / 'pins/candidate',c / 'pins/candidate')
    shutil.copy2(parent / 'pins/kissat',c / 'pins/kissat')
    runner = (parent / 'run.py').read_text().replace('tactix-5917-',f'tactix-{number}-')
    start = runner.index("env.update(SAT_DECISION_HEAP_SNAPSHOT=")
    end = runner.index("assert not any(key.endswith('_EVENT_TRACE')", start)
    runner = runner[:start] + f'''env.update(SAT_EVENT_TRACE_FORMULA_SET_PATH=str(c / 'traces' / (arm + '.formula.tsv')),
           SAT_EVENT_TRACE_FORMULA_SET_CONFLICT='271079',
           SAT_EVENT_TRACE_FORMULA_SET_MATCH='name=Strengthen|stage={stage}',
           SAT_EVENT_TRACE_FORMULA_SET_ORDINAL='1', SAT_EVENT_TRACE_FORMULA_SET_POSE='1',
           SAT_EVENT_TRACE_FORMULA_SET_META='1', SAT_EVENT_TRACE_FORMULA_SET_VALUES='1')
''' + runner[end:]
    assert 'SAT_EVENT_TRACE_SET_STOP' not in runner and '_CHOICE_STATE_TRACE' not in runner
    (c / 'run.py').write_text(runner)
    batch = (parent / 'array.sbatch').read_text().replace(str(parent),str(c))
    batch = '\n'.join(f'#SBATCH --job-name={number}-forward-{stage}' if line.startswith('#SBATCH --job-name=') else line for line in batch.split('\n'))
    (c / 'array.sbatch').write_text(batch)
    m = dict(pm)
    m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(current.relative_to(root)),
        candidate_authority=cm['candidate_authority'],candidate_source=cm['candidate_source'],
        source_patch_sha256=cm['source_patch_sha256'],
        hypothesis='Second forward271079 first changes large-clause counts;compare full incoming andoutgoing multisets,physical order,pose androot values to identify exact residual before choosing candidate/partner diagnostics.',
        validation='Require complete ordinary300K trace hashes andpartial proof bytes to replay cached5925 candidate and5771 native. Full formula snapshot required with boundary-matched irredundant/redundant counts. Slot payload selector andzero-based ordinal1 select second call;no early-stop observer.',
        native_rerun_reason='Cached native traces lack complete entry/exit formula images atsecond forward271079. Missing causal fields only,no timing denominator refresh.',
        formula_stage=stage,formula_ordinal=1,formula_conflict=271079,
        test_consideration='Read-only observer configuration on unchanged tested pins. Existing full snapshot parser malformed/duplicate/pose/value fixtures andactual native payload-selector controls retained;validate expected counts andwhole replay before interpretation. No implementation change or new solver test.',
        contract='Two separate proof-on diagnostic tasks,12CPU/48000MiB,1200s solver/25min allocation. Full width,no node constraint or throttle. Diagnostic snapshots are not final proof or runtime acceptance.')
    m['cached_controls'] = dict(pm['cached_controls'])
    record = current / 'results/candidate.json'
    m['cached_controls']['candidate'] = dict(record=str(record.relative_to(root)),sha256=sha(record))
    m['hashes'] = {str(p.relative_to(c)):sha(p) for p in (c / 'run.py',c / 'array.sbatch',c / 'pins/candidate',c / 'pins/kissat')}
    (c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

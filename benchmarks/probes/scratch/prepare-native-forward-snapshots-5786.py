#!/usr/bin/env python3
"""Replace missing native snapshots only, after actual-code selector controls."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
native = runs / '20260908-5760-forward-exit-observer/native'
authority = json.loads((native.parent / 'fixed-control.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(native) == authority['hashes'][str(native.relative_to(root))]
for number, old_number, stage in ((5786, 5782, 'begin'), (5787, 5783, 'end')):
    old = runs / f'20260908-{old_number}-rank32-forward-{stage}-formula'
    c = runs / f'20260908-{number}-rank32-native-forward-{stage}-formula'
    m = json.loads((old / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
    prior = json.loads((old / 'results/kissat.json').read_text())
    assert prior['exit_code'] == 0 and prior['scratch_deleted']
    assert 'kissat.formula.tsv' not in prior['trace_sha256']
    c.mkdir(exist_ok=False)
    for folder in ('pins', 'logs', 'results', 'traces', 'selector-fixtures'): (c / folder).mkdir()
    controls = []
    for selected, expected in ((f'strengthen_{stage}', False), (f'name=Strengthen|stage={stage}', True)):
        label = 'corrected' if expected else 'old-missing'
        snapshot = c / 'selector-fixtures' / (label + '.formula.tsv')
        env = {k: v for k, v in os.environ.items() if not k.startswith(('TACTIX_', 'KISSAT_', 'SAT_'))}
        env.update(SAT_EVENT_TRACE_FORMULA_SET_PATH=str(snapshot),
                   SAT_EVENT_TRACE_FORMULA_SET_MATCH=selected, SAT_EVENT_TRACE_FORMULA_SET_CONFLICT='0')
        result = subprocess.run([native], env=env, capture_output=True, text=True)
        (c / 'selector-fixtures' / (label + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0 and snapshot.exists() == expected
        if expected: assert snapshot.read_text().startswith('# kissat-formula-set\t2\n')
        controls.append(dict(selector=selected, expected_snapshot=expected, exit_code=result.returncode,
                             snapshot_sha256=sha(snapshot) if expected else None))
    shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
    runner = (old / 'run.py').read_text().replace(f'tactix-{old_number}-', f'tactix-{number}-')
    before = f"'strengthen_{stage}' if arm == 'kissat' else 'name=Strengthen|stage={stage}'"
    assert runner.count(before) == 1
    (c / 'run.py').write_text(runner.replace(before, f"'name=Strengthen|stage={stage}'"))
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
    assert '#SBATCH --array=0-1' in batch
    (c / 'array.sbatch').write_text(batch.replace('#SBATCH --array=0-1', '#SBATCH --array=1-1'))
    m.update(candidate_snapshot_campaign=str(old.relative_to(root)),
             native_rerun_reason='Prior native clean stop has no snapshot because selector used ordinary-journal name rather than standalone slot payload. Actual native fixed controls prove corrected selector. Missing native snapshot only;no candidate or timing-baseline repeat.',
             selector_controls=controls, selector_control_binary=dict(path=str(native.relative_to(root)), sha256=sha(native)),
             validation='Native ordinary300K traces and partial proof must exactly replay5771;selected snapshot required. Reuse Tactix snapshot from original companion without another solve.',
             resource_contract=dict(m['resource_contract'], array='1-1'))
    m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/kissat')}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    print(c, 'actual-native positive/negative selector controls PASS', flush=True)

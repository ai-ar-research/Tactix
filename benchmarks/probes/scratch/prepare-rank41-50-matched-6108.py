#!/usr/bin/env python3
"""Freeze the approved fifty and prepare ten matched pairs; never edit old runs."""
import copy
import csv
import datetime as dt
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def validate(rows, original):
    assert len(rows) == 50
    assert [int(r['rank']) for r in rows] == list(range(1, 51))
    assert len({r['instance'] for r in rows}) == 50
    assert all(r['expected'] in ('SAT', 'UNSAT') for r in rows)
    assert rows[:40] == original
    assert sum(r['expected'] == 'UNSAT' for r in rows[40:]) == 7

def matrix(rows):
    return [dict(task=2*i+j, rank=r['rank'], arm=arm, tier='D',
                 instance=r['instance'], expected=r['expected'])
            for i, r in enumerate(rows[40:]) for j, arm in enumerate(('tactix', 'kissat'))]

def tests():
    rows = [dict(rank=str(i), instance=str(i), expected='UNSAT' if i <= 47 else 'SAT')
            for i in range(1, 51)]
    validate(rows, rows[:40])
    tasks = matrix(rows)
    assert len(tasks) == 20 and [r['task'] for r in tasks] == list(range(20))
    for i in range(10):
        assert tasks[2*i]['rank'] == tasks[2*i+1]['rank'] == str(41+i)
        assert tasks[2*i]['arm'] == 'tactix' and tasks[2*i+1]['arm'] == 'kissat'
    bads = [rows[:-1], rows[::-1]]
    for index, key, value in ((40, 'instance', '1'), (0, 'expected', 'SAT'),
                               (40, 'rank', '40'), (40, 'expected', 'UNKNOWN')):
        bad = copy.deepcopy(rows); bad[index][key] = value; bads.append(bad)
    for bad in bads:
        try:
            validate(bad, rows[:40])
        except AssertionError:
            continue
        raise AssertionError('invalid membership accepted')
    print('PASS: exact prefix, coverage, uniqueness, verdict/mix and paired task mapping; six rejects')

def prepare():
    frozen = ROOT / 'benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json'
    ref = json.loads(frozen.read_text())
    origin = ROOT / ref['source_campaign']
    old = json.loads((origin / 'manifest.json').read_text())
    candidate = ROOT / 'benchmarks/runs/20260908-6061-root-lock-ranks1-40'
    cm = json.loads((candidate / 'manifest.json').read_text())
    proposal = ROOT / 'benchmarks/reports/2026-09-08-cpuq-expansion/proposed-rank1-50.tsv'
    rows = list(csv.DictReader(io.StringIO(proposal.read_text()), delimiter='\t'))
    original = [dict(rank=str(r['rank']), instance=r['instance'], expected=r['expected'])
                for r in sorted(ref['records'], key=lambda r: r['rank'])]
    validate(rows, original)
    for campaign, manifest in ((origin, old), (candidate, cm)):
        for rel, digest in manifest['pin_sha256'].items():
            assert sha(campaign / rel) == digest, rel
    assert old['arms']['kissat']['sha256'] == ref['solver']['sha256']
    selection = ROOT / 'benchmarks/baselines/rank1-50-selection-v1'
    c = ROOT / 'benchmarks/runs/20260908-6108-rank41-50-matched'
    assert not selection.exists() and not c.exists()
    inputs = {r['instance']: sha(ROOT / 'benchmarks/cache' / r['instance']) for r in rows}
    assert all(inputs[r['instance']] == r['input_sha256'] for r in ref['records'])
    cpuq = ROOT / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
    protected = [cpuq/'manifest.json', cpuq/'array.sbatch', frozen, origin/'manifest.json']
    protected += [p for p in (cpuq/'pins').iterdir() if p.is_file() and p.suffix == '.py']
    protected_hashes = {str(p.relative_to(ROOT)): sha(p) for p in protected}
    selection.mkdir()
    shutil.copy2(proposal, selection / 'matrix.tsv')
    sm = dict(schema='tactix-approved-rank1-50-selection-v1',
              created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
              approval='User: merge the gap on Rank 1-50 instead of Rank 1-40; rerun ranks 41-50 on the same hardware.',
              instances=50, additions=dict(UNSAT=7, SAT=3),
              matrix_sha256=sha(selection/'matrix.tsv'), input_sha256=inputs,
              original_reference=dict(path=str(frozen.relative_to(ROOT)), sha256=sha(frozen)),
              selection_source=dict(path=str(proposal.relative_to(ROOT)), sha256=sha(proposal)),
              runtime_target_ratio=1.01, regression_gates=['B28', 'C5'],
              baseline_policy='Retain original forty timings; append the first complete valid matched twelve-CPU Kissat measurements for ranks41-50. No best-of or cpu-q denominator.')
    (selection/'manifest.json').write_text(json.dumps(sm, indent=2)+'\n')
    for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
        (c/name).mkdir(parents=True)
    for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py', 'kissat'):
        shutil.copy2(origin/'pins'/name, c/'pins'/name)
    for name in ('tactix', 'candidate-source.patch'):
        shutil.copy2(candidate/'pins'/name, c/'pins'/name)
    tasks = matrix(rows)
    with (c/'matrix.tsv').open('x', newline='') as stream:
        w = csv.DictWriter(stream, fieldnames=tasks[0], delimiter='\t', lineterminator='\n')
        w.writeheader(); w.writerows(tasks)
    batch = (origin/'array.sbatch').read_text().replace(str(origin), str(c))
    assert batch.count('#SBATCH --array=0-79') == 1
    batch = batch.replace('#SBATCH --array=0-79', '#SBATCH --array=0-19')
    batch = batch.replace('--job-name=runtime-checked', '--job-name=6108-ranks41-50')
    (c/'array.sbatch').write_text(batch)
    m = dict(schema='tactix-matched-rank41-50-extension-v1',
             created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
             arms=dict(tactix=cm['arms']['tactix'], kissat=old['arms']['kissat']),
             input_sha256={r['instance']: inputs[r['instance']] for r in rows[40:]},
             resource_contract=dict(old['resource_contract'], array='0-19', scheduler_wall_seconds=18000),
             collect_runtime_context=True,
             candidate_authority=cm['candidate_authority'],
             selection=dict(path=str((selection/'manifest.json').relative_to(ROOT)), sha256=sha(selection/'manifest.json')),
             original_reference=sm['original_reference'],
             purpose='Current6049 and fixed Kissat paired on approved ranks41-50 using the original Rank1-40 hardware/allocation contract.',
             native_rerun_reason='User-approved extension: missing matching twelve-CPU reference measurements, not a refresh of ranks1-40.',
             validation='Check all ten models/proofs per arm and cleanup. Solver-only1200s; checker16000s separately. Continue development while checking. Record node/L3 placement; no node constraint.',
             test_consideration='Unchanged hash-identical solver/checker runner; six membership rejection controls and exact20-task paired mapping. Existing benchmark tests cover runtime/checker handling.',
             protected_campaign_hashes=protected_hashes)
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
                       [*(c/'pins').iterdir(), c/'matrix.tsv', c/'array.sbatch']}
    (c/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
    for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    for rel, digest in protected_hashes.items(): assert sha(ROOT/rel) == digest, rel
    print(c)

if __name__ == '__main__':
    tests()
    if '--test' not in sys.argv: prepare()

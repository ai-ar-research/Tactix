#!/usr/bin/env python3
"""Prepare a separate user-requested paired Tier-D experiment; never submit."""
import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
reference = root / 'benchmarks/baselines/sc24-26-32g-5000s'
candidate = root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40'
native = root / 'benchmarks/runs/20260908-5712-assignment-level-ranks1-40'
campaign = root / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): digest.update(block)
    return digest.hexdigest()

def matrix(instances):
    assert len({i['rel'] for i in instances}) == len(instances)
    rows = []
    for index, instance in enumerate(instances, 1):
        assert instance['expected'] in ('SAT', 'UNSAT', 'UNKNOWN')
        for arm in ('tactix', 'kissat'):
            rows.append(dict(task=len(rows), rank=index, arm=arm, tier='D',
                             instance=instance['rel'], expected=instance['expected']))
    return rows

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--job-wall-seconds', type=int)
    args = parser.parse_args()
    ref = json.loads((reference / 'manifest.json').read_text())
    assert sha(reference / 'manifest.json') == json.loads((reference / 'launch.json').read_text())['manifest_sha256']
    assert sha(reference / 'cache.sha256') == ref['cache_manifest_sha256']
    inputs = {}
    for line in (reference / 'cache.sha256').read_text().splitlines():
        digest, path = line.split()
        assert path not in inputs
        inputs[path] = digest
    rows = matrix(ref['instances'])
    assert len(ref['instances']) == 1166 and len(rows) == 2332
    assert set(inputs) == {i['rel'] for i in ref['instances']}
    assert all((root / 'benchmarks/cache' / i).is_file() for i in inputs)
    if args.job_wall_seconds is None:
        print('READY:1166 unique instances,2332 instance-major paired tasks;4CPU,32768MiB,cpu-q,1200s solve. Awaiting solver-only versus hard-job time choice; no campaign or submission created.')
        return
    assert args.job_wall_seconds >= 1200 + 16000 + 600, 'existing atomic checker protocol needs independent checker/cleanup time'
    current = json.loads((candidate / 'manifest.json').read_text())
    nm = json.loads((native / 'manifest.json').read_text())
    for parent, manifest in ((candidate, current), (native, nm)):
        for rel, digest in manifest['pin_sha256'].items(): assert sha(parent / rel) == digest
    campaign.mkdir(exist_ok=False)
    for folder in ('pins', 'logs', 'claims', 'provisional', 'results'): (campaign / folder).mkdir()
    for name in ('tactix', 'candidate-source.patch', 'checkmodel', 'modelcheck.py', 'drat-trim'):
        shutil.copy2(candidate / 'pins' / name, campaign / 'pins' / name)
    shutil.copy2(native / 'pins/kissat', campaign / 'pins/kissat')
    shutil.copy2(root / 'tools/bench/runtime_task.py', campaign / 'pins/runtime_task.py')
    with (campaign / 'matrix.tsv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=['task', 'rank', 'arm', 'tier', 'instance', 'expected'], delimiter='\t')
        writer.writeheader()
        writer.writerows(rows)
    hours, remainder = divmod(args.job_wall_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    (campaign / 'array.sbatch').write_text(f'''#!/usr/bin/env bash
#SBATCH --job-name=experimental-tierd-pair
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time={hours:02}:{minutes:02}:{seconds:02}
#SBATCH --array=0-2331
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
set -euo pipefail
exec python3 {campaign}/pins/runtime_task.py {campaign} "$SLURM_ARRAY_TASK_ID"
''')
    arms = dict(tactix=current['arms']['tactix'], kissat=nm['arms']['kissat'])
    m = dict(schema='tactix-experimental-paired-tierd-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
             arms=arms, input_sha256=inputs, candidate_authority=current['candidate_authority'],
             dataset_authority=dict(path=str((reference / 'manifest.json').relative_to(root)), sha256=sha(reference / 'manifest.json'),
                                    dataset_sha256=ref['dataset_sha256'], cache_manifest_sha256=ref['cache_manifest_sha256']),
             resource_contract=dict(partition='cpu-q', cpus_per_task=4, memory_mib=32768, array='0-2331',
                 throttle=None, node_constraint=None, proof_mode='DRAT on both arms', one_solver_per_task=True,
                 solve_timeout=1200, checker_timeout=16000, scheduler_wall_seconds=args.job_wall_seconds),
             collect_runtime_context=True,
             isolation='Independent campaign copies;cpu-q node001-016 is disjoint from ongoing cpu-dense-hwulab-q node017-018. Shared filesystem and user/account quotas remain shared.',
             purpose='User-requested fresh apple-to-apple breadth experiment across all1166 frozen unique TierD instances,not the canonical5000s gate or replacement12CPU Rank1-40 denominator.',
             rank_column='Frozen dataset ordinal only,not the runtime-priority Rank1-40 ordering.',
             validation='SAT original-CNF model and UNSAT DRAT checks remain mandatory.108 UNKNOWN expected labels admit either verdict only after independent checking;timeouts,wrong exits and checker failures are never valid.',
             continuation='Background experiment;do not wait for proof checks to continue development. Pending is unverified,not promotion. Separate all infrastructure/crash/OOM/time-limit/checker outcomes.',
             test_consideration='Three runtime acceptance unit tests cover known/unknown labels and exhaustive verdict/exit/timeout/checker combinations. Prior pinned runner copies remain byte-identical;new runner is independently frozen.')
    m['pin_sha256'] = {str(p.relative_to(campaign)):sha(p) for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv', campaign / 'array.sbatch']}
    (campaign / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (campaign / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(campaign)
    print('Prepared only; no job submitted. Manifest SHA256:', sha(campaign / 'manifest.json'))

if __name__ == '__main__': main()

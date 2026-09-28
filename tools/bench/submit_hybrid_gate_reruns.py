#!/usr/bin/env python3
"""Submit the four AM549 gate arrays behind a shared 120-task capacity barrier."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CONTROL = ROOT / 'benchmarks/runs/20260928-am549-hybrid-gates-control'
PREFIX = '20260928-am549-no-mab-hybrid-'
SETS = ('rank1-50', 'b28c5', 'wv100', 'wv2-89')
WINNER = '1733043'
SATSUMA = '1737781'


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def submit(args):
    return subprocess.check_output(['sbatch', '--parsable', *args], text=True).strip().split(';')[0]


def save(path, body):
    with path.open('x') as target:
        json.dump(body, target, indent=2)
        target.write('\n')


def main():
    CONTROL.mkdir(exist_ok=True)
    # Existing arrays have user-directed live scheduling overrides. Re-running
    # the historical launcher must not restore its original throttles or gate.
    existing = {name: ROOT / 'benchmarks/runs' / (PREFIX + name) / 'launch.json'
                for name in SETS}
    if all(path.exists() for path in existing.values()):
        print(json.dumps({'already_submitted': True,
                          'arrays': {name: json.loads(path.read_text())['job_id']
                                     for name, path in existing.items()}}, indent=2))
        return
    barrier = CONTROL / 'capacity-barrier.sbatch'
    if not barrier.exists():
        barrier.write_text(f'''#!/usr/bin/env bash
#SBATCH --job-name=am549-capacity-barrier
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=512M
#SBATCH --time=24:00:00
#SBATCH --no-requeue
#SBATCH --output={CONTROL}/capacity-barrier-%j.log
set -euo pipefail
while true; do
  if active=$(squeue -h -r -j {WINNER} -t R -o '%i' | wc -l); then
    printf '%s winner_active=%s\\n' "$(date -u +%FT%TZ)" "$active"
    if (( active <= 79 )); then exit 0; fi
  else
    printf '%s winner occupancy unavailable; retrying\\n' "$(date -u +%FT%TZ)" >&2
  fi
  sleep 600
done
''')
        barrier.chmod(0o444)
    barrier_launch = CONTROL / 'barrier-launch.json'
    if barrier_launch.exists():
        barrier_job = json.loads(barrier_launch.read_text())['job_id']
    else:
        barrier_job = submit([str(barrier)])
        save(barrier_launch, dict(job_id=barrier_job, script_sha256=sha(barrier),
                                  submitted_at=dt.datetime.now(dt.timezone.utc).isoformat()))
    arrays = []
    for name in SETS:
        campaign = ROOT / 'benchmarks/runs' / (PREFIX + name)
        manifest = json.loads((campaign / 'manifest.json').read_text())
        for rel, digest in manifest['pin_sha256'].items():
            if sha(campaign / rel) != digest:
                raise ValueError(f'pin mismatch: {campaign / rel}')
        if manifest['arms'][next(iter(manifest['arms']))]['sha256'] != '2fc7421f4d66ed013756e176bd2d6cd0f0bda4876270830704ea2029ed0eb970':
            raise ValueError('no-MAB binary pin mismatch')
        validation_launch = campaign / 'validation-launch.json'
        if validation_launch.exists():
            validation_job = json.loads(validation_launch.read_text())['job_id']
        else:
            validation_job = submit([f'--dependency=afterok:{barrier_job}',
                                     str(campaign / 'validate.sbatch')])
            save(validation_launch, dict(job_id=validation_job,
                                          dependency_afterok=barrier_job,
                                          manifest_sha256=sha(campaign / 'manifest.json')))
        array_launch = campaign / 'launch.json'
        if array_launch.exists():
            array_job = json.loads(array_launch.read_text())['job_id']
        else:
            array_job = submit([f'--dependency=afterok:{validation_job}',
                                str(campaign / 'array.sbatch')])
            save(array_launch, dict(job_id=array_job, validation_job=validation_job,
                                    dependency_afterok=validation_job,
                                    manifest_sha256=sha(campaign / 'manifest.json'),
                                    submitted_at=dt.datetime.now(dt.timezone.utc).isoformat()))
        arrays.append(array_job)
    scorer_launch = CONTROL / 'score-launch.json'
    if scorer_launch.exists():
        scorer_job = json.loads(scorer_launch.read_text())['job_id']
        dependency = f'afterany:{WINNER},afterok:{scorer_job}'
    else:
        dependency = 'afterany:' + ':'.join([WINNER, *arrays])
    subprocess.run(['scontrol', 'update', f'JobId={SATSUMA}',
                    f'Dependency={dependency}'], check=True)
    (CONTROL / 'schedule.json').write_text(json.dumps(dict(
        barrier_job=barrier_job, gate_arrays=dict(zip(SETS, arrays)),
        satsuma_dependency=dependency,
        winner_throttle=79, gate_throttles={name: 10 for name in SETS},
        max_active_benchmark_tasks=119), indent=2) + '\n')
    print(json.dumps(dict(barrier=barrier_job, arrays=dict(zip(SETS, arrays)),
                          satsuma_dependency=dependency), indent=2))


if __name__ == '__main__':
    main()

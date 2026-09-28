#!/usr/bin/env python3
"""Clone the four frozen no-MAB gate lists with the current hybrid checker."""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
SETS = ('rank1-50', 'b28c5', 'wv100', 'wv2-89')
OLD = '20260927-am519-no-mab-'


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def prepare(name, prefix, array_throttle):
    old = ROOT / 'benchmarks/runs' / (OLD + name)
    new = ROOT / 'benchmarks/runs' / (prefix + name)
    if new.exists():
        raise ValueError(f'campaign already exists: {new}')
    manifest = json.loads((old / 'manifest.json').read_text())
    for rel, digest in manifest['pin_sha256'].items():
        if sha(old / rel) != digest:
            raise ValueError(f'old campaign pin changed: {old / rel}')
    new.mkdir()
    for folder in ('pins', 'logs', 'results', 'provisional', 'claims'):
        (new / folder).mkdir()
    for source in (old / 'pins').iterdir():
        if not source.is_file():
            continue
        target = new / 'pins' / source.name
        if source.name in ('runtime_task.py', 'proof_oracle.py'):
            shutil.copy2(ROOT / 'tools/bench' / source.name, target)
        else:
            shutil.copy2(source, target)
    for name_ in ('test_anonymous_scratch.py', 'validate_anonymous_runtime.py'):
        shutil.copy2(ROOT / 'tools/bench' / name_, new / 'pins' / name_)
    shutil.copy2(old / 'matrix.tsv', new / 'matrix.tsv')
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(new))
    array = manifest['resource_contract']['array']
    if array_throttle:
        batch = batch.replace(f'#SBATCH --array={array}',
                              f'#SBATCH --array={array}%{array_throttle}')
        manifest['resource_contract']['array'] = f'{array}%{array_throttle}'
    manifest['resource_contract']['throttle'] = array_throttle or None
    batch = batch.replace('#SBATCH --job-name=', '#SBATCH --job-name=hybrid-')
    (new / 'array.sbatch').write_text(batch)
    validation = f'''#!/usr/bin/env bash
#SBATCH --job-name=hybrid-gate-validation
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time=00:05:00
#SBATCH --no-requeue
#SBATCH --output={new}/logs/validation-%j.log
set -euo pipefail
ulimit -c 0
python3 {new}/pins/test_anonymous_scratch.py
exec python3 {new}/pins/validate_anonymous_runtime.py {new}
'''
    (new / 'validate.sbatch').write_text(validation)
    manifest['created_at'] = dt.datetime.now(dt.timezone.utc).isoformat()
    manifest['predecessor'] = {'path': str((old / 'manifest.json').relative_to(ROOT)),
                                'sha256': sha(old / 'manifest.json')}
    manifest['purpose'] = 'Same no-MAB solver and frozen instance list, rerun with user-selected hybrid certificate route.'
    manifest['validation'] = ('Original-CNF SAT model; startup PBP through VeriPB/CakePB; '
                              'witness-free native DSR through validated DRAT/drat-trim/CakeLPR; '
                              'witnessed DSR through no-option dsr-trim/Trestle.')
    manifest['checker_contract'] = 'hybrid-native-dsr-v1'
    manifest['pin_sha256'] = {str(p.relative_to(new)): sha(p)
                               for p in [*(new / 'pins').iterdir(), new / 'matrix.tsv',
                                         new / 'array.sbatch', new / 'validate.sbatch']}
    (new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for p in [*(new / 'pins').iterdir(), new / 'matrix.tsv', new / 'array.sbatch',
              new / 'validate.sbatch', new / 'manifest.json']:
        p.chmod(p.stat().st_mode & ~0o222)
    print(name, new, sha(new / 'manifest.json'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', default='20260928-am549-no-mab-hybrid-')
    parser.add_argument('--array-throttle', type=int, default=0,
                        help='optional explicit cap; 0 runs at full partition width')
    args = parser.parse_args()
    if args.array_throttle < 0:
        parser.error('--array-throttle cannot be negative')
    for item in SETS:
        prepare(item, args.prefix, args.array_throttle)

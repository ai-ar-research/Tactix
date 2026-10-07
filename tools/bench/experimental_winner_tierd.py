#!/usr/bin/env python3
"""Prepare and launch four archived winner arms with anonymous Tier D scratch."""

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
from proof_oracle import pin_dsr_transport
import shutil
import subprocess
import sys

import portfolio_rerun as prior


ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s'
ARMS = (
    ('kissat-sc2024', 'pins/kissat-sc2024', 'positional', 'drat-cake-lpr'),
    ('lymphosat', 'pins/lymphosat/solver/solver', 'lymphosat-direct',
     'pbp-veripb-cakepb'),
    ('ae-kissat-mab', 'pins/ae-kissat-mab', 'positional', 'drat-cake-lpr'),
    ('satsuma-kissat', 'pins/satsuma-kissat/solver/kissat/build/kissat',
     'satsuma-anonymous', 'dsr-hybrid'),
)
CHECKERS = ('drat-trim', 'cake-lpr', 'veripb', 'cake-pb-cnf',
            'dsr-trim', 'dsr-compress', 'srcheck', 'checkmodel', 'modelcheck.py')
PARTITION = 'cpu-q'
CPUS = 4
MEMORY_GIB = 32
SOLVE_SECONDS = 5000
CHECK_SECONDS = 14400
THROTTLE = 120


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    with Path(path).open('x') as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write('\n')


def prepare(campaign, selected_arms=None, dsr_transport=None):
    campaign = campaign.resolve()
    if campaign.exists():
        raise SystemExit(f'campaign already exists: {campaign}')
    original = json.loads((REFERENCE / 'manifest.json').read_text())
    original_sha = sha(REFERENCE / 'manifest.json')
    if original_sha != json.loads((REFERENCE / 'launch.json').read_text())['manifest_sha256']:
        raise SystemExit('archived baseline manifest differs from its launch authority')
    if len(original['instances']) != 1166:
        raise SystemExit('archived Tier D union is not 1,166 rows')
    original_arms = {arm['name']: arm for arm in original['solvers']}
    chosen = [arm for arm in ARMS if selected_arms is None or arm[0] in selected_arms]
    if not chosen or (selected_arms is not None and
                      set(selected_arms) != {arm[0] for arm in chosen}):
        raise SystemExit('unknown or empty arm selection')
    campaign.mkdir(parents=True)
    for name in ('pins', 'claims', 'results', 'logs'):
        (campaign / name).mkdir()
    source_pin_names = {arm[0] for arm in chosen} | set(CHECKERS)
    for name in sorted(source_pin_names):
        prior.copy_pin(REFERENCE / 'pins' / name, campaign / 'pins' / name)
    for name in source_pin_names - {'modelcheck.py'}:
        record = original['provenance'].get(name)
        if record and prior.source_digest(campaign / record['path']) != record['sha256']:
            raise SystemExit(f'archived pin differs: {name}')
    for name in ('anonymous_scratch.py', 'portfolio_rerun.py',
                 'experimental_winner_task.py', 'procfd_realpath.c',
                 'proof_oracle.py'):
        shutil.copy2(ROOT / 'tools/bench' / name, campaign / 'pins' / name)
    pin_dsr_transport(dsr_transport, campaign / 'pins')
    shim = campaign / 'pins/procfd_realpath.so'
    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-o', str(shim), str(campaign / 'pins/procfd_realpath.c'), '-ldl'],
                   check=True)
    arms = {}
    for name, path, kind, route in chosen:
        archived = original_arms[name]
        if not (campaign / path).is_file():
            raise SystemExit(f'missing winner entrypoint: {path}')
        execution = {'positional': 'positional',
                     'lymphosat-direct': 'lymphosat-anonymous',
                     'satsuma-anonymous': 'satsuma-anonymous'}[kind]
        arm_path = f'pins/{name}' if execution != 'positional' else path
        arms[name] = {'name': name, 'path': arm_path,
                      'execution': execution, 'certificate': archived['certificate'],
                      'route': route, 'arguments': [],
                      'archive_checker_pipeline': archived['checker_pipeline'],
                      'source_digest': original['provenance'][name]['sha256']}
        if name == 'satsuma-kissat':
            arms[name]['transport_shim'] = 'pins/procfd_realpath.so'
    instances = original['instances']
    matrix = [{'task': len(chosen) * rank + arm_index, 'rank': rank + 1,
               'arm': arm[0], 'instance': row['rel'], 'expected': row['expected']}
              for rank, row in enumerate(instances) for arm_index, arm in enumerate(chosen)]
    inputs = dict(line.split() for line in (REFERENCE / 'cache.sha256').read_text().splitlines())
    input_hashes = {rel: digest for digest, rel in inputs.items()}
    if set(input_hashes) != {row['rel'] for row in instances}:
        raise SystemExit('archived input hash inventory differs from Tier D matrix')
    cache_root = ROOT / 'benchmarks/cache/tierD24-26'
    if any(not (cache_root / Path(rel).name).is_file() for rel in input_hashes):
        raise SystemExit('Tier D compressed cache is incomplete')
    pins = {str(path.relative_to(campaign)): sha(path)
            for path in (campaign / 'pins').rglob('*') if path.is_file()}
    manifest = {
        'schema': 'experimental-winner-tierd-v2',
        'created_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
        'baseline_authority': {'path': str(REFERENCE / 'manifest.json'),
                               'sha256': original_sha},
        'dataset_sha256': original['dataset_sha256'],
        'source_pins': {name: original['provenance'][name]
                        for name in (arm[0] for arm in chosen)},
        'cache_root': str(cache_root), 'input_sha256': input_hashes,
        'arms': arms, 'matrix': matrix,
        'pin_sha256': pins,
        'pin_digests': {rel: sha(campaign / rel) if (campaign / rel).is_file()
                        else prior.source_digest(campaign / rel)
                        for rel in [*{arm['path'] for arm in arms.values()},
                                    *(f'pins/{name}' for name in CHECKERS),
                                    'pins/procfd_realpath.so',
                                    'pins/portfolio_rerun.py']},
        'runner': 'pins/portfolio_rerun.py',
        'checkers': {name: f'pins/{name}' for name in CHECKERS},
        'solver_environment': {**prior.clean_environment(),
                               'LD_PRELOAD': str(shim)},
        'runner_sha256': sha(campaign / 'pins/experimental_winner_task.py'),
        'scratch_policy': 'anonymous-local-files-v1',
        'proof_contract': {name: contract for name, contract in {
            'kissat-sc2024': 'DRAT→drat-trim LRAT→CakeLPR',
            'ae-kissat-mab': 'DRAT→drat-trim LRAT→CakeLPR',
            'lymphosat': 'PBP→VeriPB kernel PBP→CakePB',
            'satsuma-kissat': 'Witness-free DSR→DRAT→drat-trim→CakeLPR; witnessed DSR→no-option dsr-trim→Trestle',
        }.items() if name in arms},
        'resource_contract': {'partition': PARTITION, 'cpus_per_task': CPUS,
                              'memory_mib': MEMORY_GIB * 1024,
                              'solve_timeout': SOLVE_SECONDS,
                              'checker_timeout': CHECK_SECONDS,
                              'scheduler_wall_seconds': 20700,
                              'array': f'0-{len(matrix)-1}%{THROTTLE}',
                              'array_throttle': THROTTLE,
                              'node_constraint': None},
        'cleanup_contract': 'O_TMPFILE on /tmp for CNF, reduced CNF, proof, model and checker files; all storage released by the kernel on last close, including SIGKILL/OOM.',
    }
    write_json(campaign / 'manifest.json', manifest)
    common = (f'#!/usr/bin/env bash\n#SBATCH --partition={PARTITION}\n'
              f'#SBATCH --ntasks=1\n#SBATCH --cpus-per-task={CPUS}\n'
              f'#SBATCH --mem={MEMORY_GIB}G\n#SBATCH --no-requeue\n')
    setup = f'export TACTIX_REPO_ROOT={ROOT}\nulimit -c 0\n'
    runner = campaign / 'pins/experimental_winner_task.py'
    (campaign / 'validate.sbatch').write_text(
        common + f'#SBATCH --job-name=tierd-winner-validate\n'
        f'#SBATCH --time=00:30:00\n#SBATCH --output={campaign}/logs/validate-%j.log\n'
        + setup +
        f'exec {sys.executable} {runner} {campaign} --validate\n')
    (campaign / 'array.sbatch').write_text(
        common + f'#SBATCH --job-name=tierd-four-winners\n'
        f'#SBATCH --time=05:45:00\n#SBATCH --array=0-{len(matrix)-1}%{THROTTLE}\n'
        f'#SBATCH --output={campaign}/logs/task-%A_%a.log\n'
        + setup +
        f'exec {sys.executable} {runner} {campaign} --task "$SLURM_ARRAY_TASK_ID"\n')
    for path in (campaign / 'pins').rglob('*'):
        if path.is_file():
            path.chmod(path.stat().st_mode & ~0o222)
    for name in ('manifest.json', 'validate.sbatch', 'array.sbatch'):
        path = campaign / name
        path.chmod(path.stat().st_mode & ~0o222)
    print(json.dumps({'campaign': str(campaign), 'tasks': len(matrix),
                      'manifest_sha256': sha(campaign / 'manifest.json')}, indent=2))


def submit(campaign, validate, afterany=None):
    campaign = campaign.resolve()
    manifest = json.loads((campaign / 'manifest.json').read_text())
    expected_array = f"0-{len(manifest['matrix'])-1}%{THROTTLE}"
    if manifest['resource_contract']['array'] != expected_array:
        raise SystemExit('winner array contract changed')
    if manifest['resource_contract']['checker_timeout'] != CHECK_SECONDS:
        raise SystemExit('checker limit changed')
    for rel, digest in manifest['pin_sha256'].items():
        if sha(campaign / rel) != digest:
            raise SystemExit('winner pin hash changed: ' + rel)
    marker = campaign / ('validation-launch.json' if validate else 'launch.json')
    if marker.exists():
        raise SystemExit(f'already launched: {marker}')
    if not validate:
        validation = json.loads((campaign / 'validation.json').read_text())
        if not validation['passed'] or validation['manifest_sha256'] != sha(campaign / 'manifest.json'):
            raise SystemExit('four-arm compute-node validation is missing or stale')
    command = ['sbatch', '--parsable']
    if afterany is not None and not validate:
        command.append(f'--dependency=afterany:{afterany}')
    job = subprocess.check_output(
        [*command, str(campaign / ('validate.sbatch' if validate else 'array.sbatch'))],
        text=True).strip().split(';')[0]
    write_json(marker, {'job_id': job, 'manifest_sha256': sha(campaign / 'manifest.json'),
                        'submitted_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
                        'dependency_afterany': afterany})
    print(job)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', required=True, type=Path)
    parser.add_argument('--dsr-transport', type=Path)
    parser.add_argument('--arm', action='append', choices=[arm[0] for arm in ARMS],
                        help='limit a new campaign to the named arm')
    parser.add_argument('--afterany', type=int,
                        help='start the submitted array only after this job ends')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--prepare', action='store_true')
    group.add_argument('--validate', action='store_true')
    group.add_argument('--submit', action='store_true')
    args = parser.parse_args()
    if args.afterany is not None and (args.afterany < 1 or args.prepare or args.validate):
        parser.error('--afterany requires a positive job id and --submit')
    if args.prepare:
        prepare(args.campaign, args.arm, args.dsr_transport)
    else:
        submit(args.campaign, args.validate, args.afterany)


if __name__ == '__main__':
    main()

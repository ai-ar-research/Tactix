#!/usr/bin/env python3
"""Prepare the same-window graph-labeling recognition A/B (runtime_task.py).

Mirrors tools/bench/controller_regression.py prepare: canonical checkers are
copied from the c1 WV2-89 regression campaign, both Release binaries are
pinned, and every task runs through pins/runtime_task.py with anonymous scratch.
"""
import datetime as dt, hashlib, json, shutil, sys
from pathlib import Path

ROOT = Path('/home/hwu/SAT-solving')
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import CANONICAL_CHECKERS

CAMPAIGN = ROOT / 'benchmarks/runs/20261007-glrec-ab'
CHECKERS = ROOT / 'benchmarks/runs/20261007-c1-regression-wv2-89/pins'
ARGS = ['--auto-proof', '--default-graph-labeling=on']
ARMS = {'c1': ROOT / 'build/pin-candidate-c1-rel',
        'glrec': ROOT / 'build/pin-glrec-rel-8e953218f'}
ROWS = [  # rank, tier, instance, expected
    (991, 'fixed44', 'tierD24-26/d8225a58e2ca7c2fb93c533f406c02d5-abw-K-dwt__234.mtx-w50.cnf.xz', 'SAT'),
    (192, 'WV2-89', 'tierD24-26/6ebe901234af9615bb85b688a26ef2c4-17.normalised.cnf.xz', 'SAT'),
    (5, 'Rank1-50', 'tierD24-26/1c21a43aa1ad437e4a7e4eb5b9cd45a1-9.normalised.cnf.xz', 'SAT'),
    (8, 'Rank1-50', 'tierD24-26/210fe1c0ffefde5bcd03d59b1efa6984-32_325.sanitized.cnf.xz', 'SAT'),
    (10, 'Rank1-50', 'tierD24-26/093fa3ff9f7bc9979c43f9d2310ac21d-128_125.sanitized.cnf.xz', 'SAT'),
    (13, 'Rank1-50', 'tierD24-26/84c6d7e4a18aacf166105aaa3cd6e3de-128_100.sanitized.cnf.xz', 'SAT'),
    (14, 'Rank1-50', 'tierD24-26/a14d9ee17051ec08a4334ba43089502c-64_200.sanitized.cnf.xz', 'SAT'),
    (12, 'Rank1-50', 'tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz', 'UNSAT'),
    (8, 'B28', 'tierB/hwmcc17miters-xits-iso-oski15a08b00s.sanitized.cnf.xz', 'UNSAT'),
    (31, 'C5', 'tierC/circuit_32in32out_with_500gates_6in6out_dist64_seed1.sanitized.cnf.xz', 'SAT'),
    (0, 'near-family', 'tierD24-26/2d6d26e1c5b4f2763c03697a6d00d3cf-fixedbandwidth-eq-31_shuffled.cnf.xz', 'UNSAT'),
]


def sha(path):
    d = hashlib.sha256()
    with open(path, 'rb') as s:
        for b in iter(lambda: s.read(8 << 20), b''):
            d.update(b)
    return d.hexdigest()


def main():
    assert not CAMPAIGN.exists(), CAMPAIGN
    CAMPAIGN.mkdir()
    for d in ('pins', 'logs', 'results', 'provisional', 'claims'):
        (CAMPAIGN / d).mkdir()
    for f in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
        shutil.copy2(ROOT / 'tools/bench' / f, CAMPAIGN / 'pins' / f)
    for f in ('checkmodel', 'modelcheck.py', *CANONICAL_CHECKERS):
        if f in CANONICAL_CHECKERS:
            assert sha(CHECKERS / f) == CANONICAL_CHECKERS[f], f
        shutil.copy2(CHECKERS / f, CAMPAIGN / 'pins' / f)
    arms = {}
    for name, build in ARMS.items():
        pin = json.loads((build / 'PIN.json').read_text())
        assert pin['build_type'] == 'Release'
        assert sha(build / 'tactix') == pin['binaries']['tactix']
        shutil.copy2(build / 'tactix', CAMPAIGN / 'pins' / f'tactix-{name}')
        shutil.copy2(build / 'PIN.json', CAMPAIGN / 'pins' / f'{name}-pin.json')
        arms[name] = dict(path=f'pins/tactix-{name}', sha256=pin['binaries']['tactix'],
                          source_commit=pin['source_commit'],
                          source_build=str(build.relative_to(ROOT)),
                          arguments=ARGS, certificate='auto')
    lines = ['task\trank\tarm\ttier\tinstance\texpected']
    inputs = {}
    task = 0
    for rank, tier, instance, expected in ROWS:
        inputs[instance] = sha(ROOT / 'benchmarks/cache' / instance)
        for arm in arms:
            lines.append(f'{task}\t{rank}\t{arm}\t{tier}\t{instance}\t{expected}')
            task += 1
    (CAMPAIGN / 'matrix.tsv').write_text('\n'.join(lines) + '\n')
    contract = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12, memory_mib=32768,
                    solve_timeout=1200, checker_timeout=1800, checker_timeout_overrides={},
                    array=f'0-{task - 1}', throttle=None, node_constraint=None,
                    one_solver_per_task=True,
                    proof_mode='automatic startup PBP; solver-declared native DSR final route after no-option dsr-trim')
    batch = f'''#!/bin/bash
#SBATCH --job-name=glrec-ab
#SBATCH --partition={contract['partition']}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={contract['cpus_per_task']}
#SBATCH --mem={contract['memory_mib']}M
#SBATCH --time=01:10:00
#SBATCH --array={contract['array']}
#SBATCH --no-requeue
#SBATCH --output={CAMPAIGN}/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
exec python3 {CAMPAIGN}/pins/runtime_task.py {CAMPAIGN} "$SLURM_ARRAY_TASK_ID"
'''
    (CAMPAIGN / 'array.sbatch').write_text(batch)
    manifest = dict(schema='tactix-glrec-same-window-ab-v1',
                    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
                    preparer='benchmarks/runs/20261007-glrec-ab-prep/prepare.py',
                    arms=arms, resource_contract=contract, input_sha256=inputs,
                    collect_runtime_context=True,
                    scratch_policy='anonymous-local-files-v1',
                    limitation='Same-window paired A/B on selected rows; one draw per arm.')
    manifest['pin_sha256'] = {str(p.relative_to(CAMPAIGN)): sha(p)
        for p in [*(CAMPAIGN / 'pins').iterdir(), CAMPAIGN / 'matrix.tsv', CAMPAIGN / 'array.sbatch']}
    (CAMPAIGN / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(CAMPAIGN, task)


if __name__ == '__main__':
    main()

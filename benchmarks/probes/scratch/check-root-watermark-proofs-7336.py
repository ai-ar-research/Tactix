#!/usr/bin/env python3
"""Fixed original-CNF DRAT/LRAT and model checks on the repaired root fold."""
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status

campaign = root / 'benchmarks/runs/20260911-7336-root-watermark-proofs'
authority = root / 'benchmarks/runs/20260911-7335-root-watermark-build/manifest.json'
build = json.loads(authority.read_text())
assert build['status'] == 'PASS'
for rel, digest in build['files'].items():
    assert sha(root / build['source'] / rel) == digest, rel
for binaries in build['binaries'].values():
    for rel, digest in binaries.items(): assert sha(root / rel) == digest, rel
assert not campaign.exists(), 'preserve every successful or failed attempt'
(campaign / 'logs').mkdir(parents=True)
hashes = {str(authority.relative_to(root)): sha(authority)}
records = []
removed = 0

def run(name, command):
    command = list(map(str, command))
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run(command, cwd=campaign, env=env,
                            capture_output=True, text=True)
    log = campaign / 'logs' / (name + '.json')
    with log.open('x') as out:
        json.dump(dict(command=command, returncode=result.returncode,
                       stdout=result.stdout, stderr=result.stderr), out, indent=2)
    hashes[str(log.relative_to(root))] = sha(log)
    return result

def consume(path):
    global removed
    assert path.parent == campaign and path.suffix in ('.cnf', '.proof', '.out')
    removed += path.stat().st_size
    path.unlink()

for mode in ('drat', 'lrat'):
    cnf = campaign / ('negative-' + mode + '.cnf')
    proof = cnf.with_suffix('.proof')
    with cnf.open('x') as out: out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out: out.write('2 0 1 0\n' if mode == 'lrat' else '0\n')
    checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode == 'lrat' else 'drat-trim')
    result = run('negative-' + mode, [checker, cnf, proof])
    assert proof_check_status(checker.name, result.returncode, result.stdout, result.stderr) == 'rejected'
    hashes[str(checker.relative_to(root))] = sha(checker)
    consume(cnf); consume(proof)

for kind in ('release', 'sanitizers'):
    exe = root / ('build/pin-7335-root-watermark-' + kind + '/tests/test_root_fold')
    hashes[str(exe.relative_to(root))] = sha(exe)
    for polarity in range(8):
        roots = [-(i+1) if polarity & (1 << i) else i+1 for i in range(3)]
        if polarity & 1: roots.reverse()
        x, y = (7, 9) if polarity & 2 else (4, 5)
        for unsat in (False, True):
            clauses = [[lit] for lit in roots] + [[x,y], [-x,y], [x,-y]]
            if unsat: clauses.append([-x,-y])
            original = f'p cnf {max(x,y)} {len(clauses)}\n' + ''.join(
                ' '.join(map(str, row)) + ' 0\n' for row in clauses)
            for mode in ('drat', 'lrat'):
                pair = []
                for arm in ('control', 'fold'):
                    name = f'{kind}-{polarity}-{int(unsat)}-{mode}-{arm}'
                    cnf, proof, model = [campaign / (name + suffix)
                                         for suffix in ('.cnf', '.proof', '.out')]
                    with cnf.open('x') as out: out.write(original)
                    result = run(name + '-fixture', [exe, cnf, proof, mode, arm])
                    assert result.returncode == (20 if unsat else 10), (name, result.stdout, result.stderr)
                    with model.open('x') as out: out.write(result.stdout)
                    if unsat:
                        checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode == 'lrat' else 'drat-trim')
                        checked = run(name + '-checker', [checker, cnf, proof])
                        assert proof_check_status(checker.name, checked.returncode, checked.stdout, checked.stderr) == 'verified', (name, checked.stdout, checked.stderr)
                    else:
                        checker = root / 'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel'
                        checked = run(name + '-checker', [checker, cnf, model])
                        assert checked.returncode == 0 and checked.stdout.startswith('checkmodel: OK'), (name, checked.stdout, checked.stderr)
                    record = dict(name=name, unsat=unsat, input_sha256=sha(cnf),
                                  proof_sha256=sha(proof), proof_bytes=proof.stat().st_size,
                                  model_lines=[line for line in result.stdout.splitlines() if line.startswith('v ')])
                    records.append(record); pair.append(record)
                    consume(cnf); consume(proof); consume(model)
                assert pair[0]['proof_sha256'] == pair[1]['proof_sha256'], (name, 'proof differs')
                assert pair[0]['model_lines'] == pair[1]['model_lines'], (name, 'model differs')
    print(kind, 'fixed original proofs/models PASS', flush=True)
for path in [Path(__file__), root / 'tools/bench/proof_oracle.py',
             root / 'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel',
             root / 'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/modelcheck.py']:
    hashes[str(path.relative_to(root))] = sha(path)
assert len(records) == 128 and not any(campaign.glob('*.cnf')) and not any(campaign.glob('*.proof'))
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, records=records, checked_UNSAT=64,
                   checked_SAT=64, negative_controls=2, removed_bytes=removed,
                   exact_fold_disabled_certificates=True,
                   scope='Fixed original-CNF proofs/models with both signs, reverse root order, external holes; Release and unsuppressed ASan/UBSan/LSan.',
                   limitation='Not current-source fuzz or B28+C5/Rank1-50 runtime acceptance.'), out, indent=2)
print(campaign / 'manifest.json')

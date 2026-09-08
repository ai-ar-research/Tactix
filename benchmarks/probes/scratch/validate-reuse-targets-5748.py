#!/usr/bin/env python3
"""Actual predecessor target controls,policy parity and independent micro-proofs."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5741-reuse-target'
d = c / 'target-validation'
d.mkdir(exist_ok=False)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
commands = []
def run(label, cmd):
    commands.append([str(x) for x in cmd])
    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    (d / (label + '.log')).write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (label, r.returncode, r.stdout[-2000:], r.stderr)
    return r.stdout
old = root / 'build/pin-5718-root-conflict-reuse-release'
parent = root / '.claude/worktrees/runtime-root-conflict-reuse-5718'
fixture = root / '.claude/worktrees/runtime-reuse-target-5741/tests/test_conflict_reuse_targets.cpp'
link = shlex.split((old / 'tests/CMakeFiles/test_root_conflict_reuse.dir/link.txt').read_text())
objects = [(old / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
assert objects and all(p.is_file() for p in objects)
run('compile-previous', [link[0], '-O3', '-DNDEBUG', '-std=c++20', '-fno-exceptions', '-fno-rtti',
    '-I' + str(parent / 'src'), '-I' + str(parent / 'include'), fixture, *objects, '-o', d / 'previous'])
previous_dir, current_dir = d / 'previous-proofs', d / 'current-proofs'
previous_dir.mkdir(); current_dir.mkdir()
previous = run('previous', [d / 'previous', '--previous', previous_dir])
binary = root / 'build/pin-5741-reuse-target-release/tests/test_conflict_reuse_targets'
current = run('current', [binary, '--current', current_dir])
rows = lambda text: [line for line in text.splitlines() if line.startswith('POLICY ')]
assert len(rows(current)) == len(rows(previous)) == 240
assert rows(current) == rows(previous)
assert 'PASS: 2400 reuse target fixtures; previous ignored public targets' in previous
assert 'PASS: 2400 reuse target fixtures; honored public targets' in current
records = []
for arm, directory in [('previous', previous_dir), ('current', current_dir)]:
    for cnf in sorted(directory.glob('*.cnf')):
        proof = cnf.with_suffix('.proof')
        kind = 'lrat' if cnf.stem.endswith('-lrat') else 'drat'
        checker = root / ('tools/checkers/bin/lrat-check' if kind == 'lrat' else 'tools/checkers/bin/drat-trim')
        r = subprocess.run([checker, cnf, proof], cwd=root, capture_output=True, text=True)
        log = r.stdout + r.stderr
        log_path = d / (arm + '-' + cnf.stem + '-check.log')
        log_path.write_text(log)
        assert ('c VERIFIED' in log and r.returncode == 0) if kind == 'lrat' else 's VERIFIED' in log
        records.append(dict(arm=arm, case=cnf.stem, kind=kind, cnf_sha256=sha(cnf),
            proof_sha256=sha(proof), checker_sha256=sha(checker), exit_code=r.returncode,
            log_sha256=sha(log_path)))
        proof.unlink(); cnf.unlink()
    assert not list(directory.iterdir())
    print(arm, '2400 independent proof checks PASS', flush=True)
assert len(records) == 4800
paths = [fixture, *objects, d / 'previous', binary]
(d / 'summary.json').write_text(json.dumps(dict(predecessor_cases=2400, current_cases=2400,
    policy_transition_rows=240, policy_transition_rows_equal=True, micro_proofs=records,
    commands=commands, hashes={str(p.relative_to(root)):sha(p) for p in paths},
    limitation='Actual-code fixed target/transition/proof checks,not current-source fuzz or a full DefaultPolicy trajectory comparison. Exact caller targets differ intentionally; original policy transition observations agree.'), indent=2) + '\n')
print('PASS:2400 predecessor controls,2400 current targets,240 policy rows,4800 checked proofs', flush=True)

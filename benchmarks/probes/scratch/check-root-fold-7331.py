#!/usr/bin/env python3
"""Full debug/Release suites plus fixed original-CNF root-fold proof/model gates."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root/'benchmarks/runs/20260911-7331-root-fold-preflight'
assert not c.exists(), 'preserve successful or failed evidence'
(c/'logs').mkdir(parents=True)
source = root/'.claude/worktrees/runtime-root-fold-7330'
parent_path = root/'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
parent = json.loads(parent_path.read_text())
files = {str(p.relative_to(source)):sha(p) for folder in ('src', 'include', 'tests')
         for p in (source/folder).rglob('*') if p.is_file()}
files['CMakeLists.txt'] = sha(source/'CMakeLists.txt')
changed = sorted(rel for rel, digest in files.items() if parent['files'].get(rel) != digest)
patch = ''
for rel in changed:
    old = root/parent['source']/rel
    patch += ''.join(difflib.unified_diff(old.read_text().splitlines(True) if old.exists() else [],
        (source/rel).read_text().splitlines(True), fromfile='a/'+rel if old.exists() else '/dev/null', tofile='b/'+rel))
with (c/'incremental.patch').open('x') as out: out.write(patch)
commands, hashes, records, removed = {}, {}, [], 0
def run(name, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=c, capture_output=True, text=True)
    with (c/'logs'/f'{name}.json').open('x') as out:
        json.dump(dict(command=command, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr), out, indent=2)
    commands[name] = dict(command=command, exit_code=p.returncode)
    return p
def build(kind):
    pin = root/f'build/pin-7330-root-fold-{kind.lower()}'
    args = ['cmake', '-S', source, '-B', pin, '-DCMAKE_BUILD_TYPE='+kind,
        '-DTACTIX_ENABLE_EVENT_TRACE=OFF', '-DTACTIX_ENABLE_RULE_JOURNAL=OFF',
        '-DTACTIX_ENABLE_PROOF_SHADOW=OFF', '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
        '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
        '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']
    for name, command in (('configure', args), ('build', ['cmake','--build',pin,'-j2']),
                          ('ctest', ['ctest','--test-dir',pin,'--output-on-failure','-j1'])):
        p = run(kind+'-'+name, command)
        assert p.returncode == 0, (kind, name, p.stdout[-4000:], p.stderr[-4000:])
        print(kind, name, 'PASS', flush=True)
    return kind, dict(path=str((pin/'tactix').relative_to(root)), sha256=sha(pin/'tactix'))
with ThreadPoolExecutor(max_workers=2) as pool:
    binaries = dict(pool.map(build, ('Debug', 'Release')))

def consume(p):
    global removed
    assert p.parent == c and p.suffix in ('.cnf', '.proof', '.out')
    removed += p.stat().st_size
    p.unlink()
for mode in ('drat', 'lrat'):
    cnf = c/f'negative-{mode}.cnf'; proof = cnf.with_suffix('.proof')
    with cnf.open('x') as out: out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out: out.write('2 0 1 0\n' if mode == 'lrat' else '0\n')
    checker = root/'tools/checkers/bin'/('cake_lpr' if mode == 'lrat' else 'drat-trim')
    p = run('negative-'+mode, [checker, cnf, proof])
    assert proof_check_status(checker.name, p.returncode, p.stdout, p.stderr) == 'rejected'
    hashes[str(checker.relative_to(root))] = sha(checker)
    consume(cnf); consume(proof)
for kind in ('Debug', 'Release'):
    exe = root/f'build/pin-7330-root-fold-{kind.lower()}/tests/test_root_fold'
    hashes[str(exe.relative_to(root))] = sha(exe)
    for polarity in range(8):
        roots = [-(i+1) if polarity & (1 << i) else i+1 for i in range(3)]
        # Both root signs, root order and gaps in the original external space.
        if polarity & 1: roots.reverse()
        x, y = (7, 9) if polarity & 2 else (4, 5)
        for unsat in (False, True):
            clauses = [[lit] for lit in roots] + [[x,y],[-x,y],[x,-y]]
            if unsat: clauses.append([-x,-y])
            original = f'p cnf {max(x,y)} {len(clauses)}\n' + ''.join(' '.join(map(str,row))+' 0\n' for row in clauses)
            for mode in ('drat', 'lrat'):
                results = []
                for arm in ('control', 'fold'):
                    name = f'{kind}-{polarity}-{int(unsat)}-{mode}-{arm}'
                    cnf, proof, model = (c/(name+suffix) for suffix in ('.cnf','.proof','.out'))
                    with cnf.open('x') as out: out.write(original)
                    p = run(name+'-fixture', [exe, cnf, proof, mode, arm])
                    assert p.returncode == (20 if unsat else 10), (name,p.stdout,p.stderr)
                    with model.open('x') as out: out.write(p.stdout)
                    if unsat:
                        checker = root/'tools/checkers/bin'/('cake_lpr' if mode=='lrat' else 'drat-trim')
                        q = run(name+'-checker', [checker, cnf, proof])
                        assert proof_check_status(checker.name,q.returncode,q.stdout,q.stderr)=='verified',(name,q.stdout,q.stderr)
                    else:
                        checker = root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel'
                        q = run(name+'-checker', [checker, cnf, model])
                        assert q.returncode == 0 and q.stdout.startswith('checkmodel: OK'), (name,q.stdout,q.stderr)
                    record = dict(name=name, unsat=unsat, arm=arm, input_sha256=sha(cnf),
                                  proof_sha256=sha(proof), proof_bytes=proof.stat().st_size,
                                  model_lines=[line for line in p.stdout.splitlines() if line.startswith('v ')])
                    records.append(record); results.append(record)
                    consume(cnf); consume(proof); consume(model)
                assert results[0]['proof_sha256'] == results[1]['proof_sha256'], (name,'proof differs')
                assert results[0]['model_lines'] == results[1]['model_lines'], (name,'model differs')
    print(kind, 'fixed original proofs/models PASS', flush=True)
for rel, digest in files.items():
    assert sha(source/rel) == digest, 'source changed during validation'
    hashes[str((source/rel).relative_to(root))] = digest
for p in c.rglob('*'):
    if p.is_file(): hashes[str(p.relative_to(root))] = sha(p)
for p in (Path(__file__), parent_path, Path(__file__).with_name('prepare-root-fold-7330.py'),
          root/'tools/bench/proof_oracle.py', root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel',
          root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/modelcheck.py'):
    hashes[str(p.relative_to(root))] = sha(p)
assert len(records) == 128 and not any(c.glob('*.cnf')) and not any(c.glob('*.proof'))
with (c/'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', source=str(source.relative_to(root)), files=files, changed=changed,
        hashes=hashes, commands=commands, binaries=binaries, records=records,
        checked_UNSAT=64, checked_SAT=64, negative_controls=2, removed_bytes=removed,
        scope='Fixed kernel compaction, external units/value/model, repeated/extension/reference/reason and Decide surface fixtures; full debug/Release suites; original-CNF paired DRAT/LRAT proofs and SAT models byte-exact to fold-disabled control.',
        limitation='Isolated unfuzzed representation prototype, not runtime acceptance. No cadence/guard changes; existing DefaultPolicy compaction calls enable folding. Real checked B/C and targeted repeated/memory gates remain.'), out, indent=2)
print(c/'manifest.json')

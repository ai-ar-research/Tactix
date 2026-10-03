import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path('/home/hwu/SAT-solving')
RUN = Path(__file__).resolve().parent
STAGE = ROOT / 'build/shared-policy-terminal-substitute-f6f01982-staging'
ENV = {k: v for k, v in os.environ.items() if not k.startswith(('TACTIX_', 'SAT_'))}

def verify():
    manifest = json.loads((RUN / 'manifest.json').read_text())
    for name, expected in manifest['sha256'].items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected, name
    subprocess.run([sys.executable, str(ROOT / 'benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/verify.py')], check=True)

def call(name, command):
    done = subprocess.run(command, capture_output=True, text=True, timeout=60, env=ENV)
    (RUN / (name + '.stdout')).write_text(done.stdout)
    (RUN / (name + '.stderr')).write_text(done.stderr)
    return {'name': name, 'command': command, 'exit': done.returncode,
            'stdout': done.stdout, 'stderr': done.stderr}

def main():
    verify()
    results = {'scope': 'Staged causal execution/public-feedback repair; not final-pin or whole-goal acceptance.',
               'root_solver_modified': False, 'execution': [], 'feedback': [], 'proofs': []}
    good = True
    for arm in ('original', 'patched'):
        for batch in (0, 1):
            for borrowed in (0, 1):
                name = f'{arm}-execution-{batch}-{borrowed}'
                row = call(name, [str(STAGE / (arm + '-terminal_execution')), str(batch), str(borrowed)])
                row['expected_exit'] = 3 if arm == 'original' and not borrowed else 0
                row['matched'] = row['exit'] == row['expected_exit']
                good &= row['matched']
                results['execution'].append(row)
        for retention in (0, 1):
            for enabled in (0, 1):
                name = f'{arm}-feedback-{retention}-{enabled}'
                proof = RUN / (name + '.dsr')
                row = call(name, [str(STAGE / (arm + '-terminal_feedback')), str(RUN / '009.cnf'), str(proof), str(retention), str(enabled)])
                row['expected_exit'] = 3 if arm == 'original' else 0
                row['matched'] = row['exit'] == row['expected_exit'] and 'FINAL verdict=20' in row['stdout']
                if enabled:
                    row['matched'] &= 'refutations=1 ended=1' in row['stdout']
                    row['matched'] &= ('UNRESTORED Substitute' in row['stdout']) == (arm == 'original')
                    row['matched'] &= ('UNRESTORED session_end' in row['stdout']) == (arm == 'original')
                good &= row['matched']
                row['proof_sha256'] = hashlib.sha256(proof.read_bytes()).hexdigest()
                results['feedback'].append(row)
                command = [sys.executable, str(RUN / 'proof_oracle.py'), 'check', str(RUN / '009.cnf'), str(proof), '--workdir', str(RUN / (name + '-oracle'))]
                for option in ('dsr-trim', 'dsr-compress', 'srcheck', 'drat-trim', 'cake-lpr'):
                    command += ['--' + option, str((ROOT / 'tools/checkers/bin' / ('cake_lpr' if option == 'cake-lpr' else option)).resolve())]
                (RUN / (name + '-oracle')).mkdir(exist_ok=True)
                checked = call(name + '-check', command)
                checked['verified'] = checked['exit'] == 0 and 's VERIFIED UNSAT' in checked['stdout']
                good &= checked['verified']
                results['proofs'].append(checked)
    results['all_certificate_hashes_equal'] = len({r['proof_sha256'] for r in results['feedback']}) == 1
    good &= results['all_certificate_hashes_equal']
    verify()
    results['preflight_postflight_verified'] = True
    results['accepted_within_scope'] = bool(good)
    (RUN / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps({k: v for k, v in results.items() if k not in ('execution', 'feedback', 'proofs')}, indent=2))
    return 0 if good else 1

if __name__ == '__main__':
    raise SystemExit(main())

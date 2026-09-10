#!/usr/bin/env python3
"""Classify fixture/checker failures and strengthen retained6730 witnesses."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root/'benchmarks/runs/20260910-6902-watch-rebuild-build'
target = c/'scope-6911.json'
assert not target.exists()
failed = root/'benchmarks/runs/20260910-6906-watch-rebuild-proofs'
old = json.loads((failed/'negative-1.json').read_text())
assert old['returncode'] == 0 and 'c VERIFIED' in old['stdout']
control = json.loads((c/'control-6904.json').read_text())
assert control['outputs']['previous'] == control['outputs']['candidate']
fixture_failure = c/'logs/previous-test_watch_rebuild-link-6903.json'
f = json.loads(fixture_failure.read_text())
assert f['exit_code'] == 1 and 'kernel_add_binary' in f['stderr']
assert not (c/'control-6903.json').exists() and not (failed/'summary.json').exists()
strong = root/'benchmarks/runs/20260910-6910-watch-rebuild-proofs/summary.json'
s = json.loads(strong.read_text())
assert s['checked'] == 128 and len(s['negative_controls']) == 2
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
records = []; paths = [Path(__file__),fixture_failure,c/'control-6904.json',strong,
    failed/'negative-0.json',failed/'negative-1.json',failed/'negative-1.cnf',failed/'negative-1.proof',
    root/'drat-trim/lrat-check.c',root/'tools/checkers/bin/lrat-check',root/'tools/checkers/bin/cake_lpr',
    root/'cake_lpr/README.md',root/'tools/bench/proof_oracle.py',root/'tools/bench/test_proof_oracle.py']
def check(name,checker,cnf,proof,expected):
    command = list(map(str,(checker,cnf,proof)))
    p = subprocess.run(command,capture_output=True,text=True)
    status = proof_check_status(checker.name,p.returncode,p.stdout,p.stderr)
    r = dict(command=command,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,status=status)
    path = c/'logs'/(name+'-6911.json')
    with path.open('x') as stream:
        json.dump(r,stream,indent=2);stream.write('\n')
    paths.extend((path,cnf,proof,checker))
    assert status == expected,(name,r)
    records.append(dict(name=name,status=status,log=str(path.relative_to(root))))
check('negative-cake',root/'tools/checkers/bin/cake_lpr',failed/'negative-1.cnf',failed/'negative-1.proof','rejected')
prior = root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control'
summary = json.loads((prior/'summary.json').read_text())
for rel,digest in summary['hashes'].items(): assert sha(root/rel) == digest,rel
paths.append(prior/'summary.json')
for cnf in sorted((prior/'fixed-proofs').glob('*.cnf')):
    lrat = cnf.stem.endswith('-lrat')
    checker = root/'tools/checkers/bin'/('cake_lpr' if lrat else 'drat-trim')
    check('retained6730-'+cnf.stem,checker,cnf,cnf.with_suffix('.proof'),'verified')
assert len(records) == 9
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},records=records,
    fixture_failure='6903 first old-arm fixture link failed on misspelled kernel_add_binary; no fixture execution or solver run.6904 corrects to actual kernel_add_binary_clause and succeeds in both arms. Retain failed logs/object; not a solver correctness failure.',
    checker_failure='6906 SAT formula p cnf 1 1 / 1 0 and LRAT 2 0 1 0 is false-accepted by pinned lrat-check (exit0,c VERIFIED). No solver emitted this proof. cake_lpr rejects it with pivot must be non-zero, despite exit0. checkClause in pinned drat-trim/lrat-check.c computes a zero pivot for an empty lemma; after hints produce no conflict its RATs==0 branch permits SUCCESS when intro[pivot^1]==0. This reproduces an unsound acceptance path; do not use legacy lrat-check alone for acceptance.',
    recovery='Fresh6910 uses cake_lpr for64 LRAT and drat-trim for64 DRAT witnesses, exact actual previous/candidate outputs and128 valid proofs, plus two rejected SAT negatives. Strict new status helper has8 unit tests including rejection with zero exit, exact tokens and legacy exclusion. Retained6730 eight root/reason proofs rechecked without mutation; four LRAT now independently verified bycake_lpr.',
    historical_scope='Historical lrat-check-only successes do not independently discharge LRAT acceptance given this counterexample; they are not thereby proven invalid. Frozen reports/pins unchanged. Current/recent large runtime and B/C campaigns use DRAT-trim, not this legacy checker, so continue their published checks. No automatic full50/reference refresh or promotion. Further required native-LRAT witnesses must usecake_lpr; full current-source fuzz remains unavailable.',
    incidental_search='A broad read-only rg over tools/bench and tools/fuzz unintentionally returned three archived regression metadata hits while locating checker references. No fuzz launch, transfer or acceptance use; do not revisit those excluded artifacts. They are not dependencies of this classification.',
    cleanup='Failed6906 removed16 bytes of its verified-negative DRAT input/proof before the LRAT preflight failed; its remaining22-byte SAT LRAT reproducer stays as unique failure evidence.6910 removed7366 bytes of verified fixed inputs/proofs, retaining hashes/logs/generator. No other deletion here.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2);stream.write('\n')
print('PASS: checker failure classified;8 retained6730 proofs independently verified; no legacy LRAT acceptance')

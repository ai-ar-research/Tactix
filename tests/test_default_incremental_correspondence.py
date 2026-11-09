"""Native/hardcoded/public-C DefaultPolicy identity on persistent queries."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

root = Path(sys.argv[1])
sys.path.insert(0, str(root/'python'))
from tactix import capi
stat_count = 1 + max(value for name,value in capi.schema.CONSTANTS.items()
                     if name.startswith('TACTIX_STAT_'))
sys.path.insert(0, str(root/'tools/fuzz'))
import ipasir_trace as oracle
oracle.ROOT = root
for tool in ('veripb', 'cake-pb-cnf', 'drat-trim', 'cake_lpr', 'dsr-trim', 'dsr-compress', 'srcheck'):
    if not (root/'tools/checkers/bin'/tool).is_file():
        sys.exit(77)

base = [[1, 2], [-1, 3], [-2, 3], [-3, 4], [-5, 1], [-5, 2], [5, -1, -2],
        [-6, 1], [-6, 2], [6, -1, -2]]
# Original input and query identities are independent of the exported bundle.
queries = [('first', 1, [], [], 10), ('failed', 2, [], [-3], 20),
           ('reuse', 3, [], [1, -2], 10), ('duplicate', 4, [], [1, 1, -2], 10),
           ('opposite', 5, [], [5, -5], 20), ('source', 6, [], [-3, 6], 20),
           ('probe0', 7, [], [6], 10), ('probe1', 8, [], [-3], 20),
           ('minimum', 8, [], [-3], 20), ('after-stop', 10, [], [], 10),
           ('appended-sat', 11, [[-1]], [], 10), ('appended-failed', 12, [[-1]], [1], 20),
           ('appended-unsat', 13, [[-1], [-2]], [], 20),
           ('permanent-unsat', 14, [[-1], [-2]], [], 20)]
with tempfile.TemporaryDirectory(prefix='default-incremental-twins-') as tmp:
    reference = None
    artifacts = {}
    models = proofs = 0
    for arm, executable in enumerate(sys.argv[2:]):
        directory = Path(tmp)/str(arm)
        directory.mkdir()
        done = subprocess.run([executable, '--incremental', directory],
                              capture_output=True, text=True, timeout=180)
        assert done.returncode == 0, (arm, done.stdout, done.stderr)
        if reference is None:
            reference = done.stdout
        assert done.stdout == reference, (arm, reference, done.stdout)
        for label, number, added, assumptions, status in queries:
            stem = directory/label
            q = dict(query=number, formula=base+added, assumptions=assumptions)
            answer = dict(result=status, failed=assumptions)
            reduced = oracle.validate_bundle(q, answer, stem, semantics_only=True)
            if status == 10:
                models += 1
            else:
                oracle.check_certificate(stem, 30)
                proofs += 1
                if reduced:
                    oracle.check_certificate(Path(str(stem)+'-core'), 30)
                    proofs += 1
            meta = json.loads(Path(str(stem)+'.json').read_text())
            for suffix in ['.json', *meta['sha256']]:
                contents = Path(str(stem)+suffix).read_bytes()
                key = (label, suffix)
                assert contents == artifacts.setdefault(key, contents), (arm, key)
    print(f'DefaultPolicy incremental twins: 3 arms, {len(queries)} owned results/arm, '
          f'{stat_count} counters, exact bundles, {models} models/{proofs} proofs checked; '
          'strict minimization, interruption/reuse and late input; zero public fallbacks')

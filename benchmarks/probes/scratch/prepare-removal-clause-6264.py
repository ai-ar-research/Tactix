#!/usr/bin/env python3
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6256-rank32-remaining-retry'
build = root / 'benchmarks/runs/20260909-6262-removal-observer-build/manifest.json'
b = json.loads(build.read_text()); source = root / b['source']
for rel, digest in b['files'].items(): assert sha(source / rel) == digest
assert sha(source / 'build/kissat') == b['binary_sha256']
s = json.loads((old / 'summary.json').read_text()); assert s['parent_replay']
c = root / 'benchmarks/runs/20260909-6264-rank32-removal-clause'
assert not c.exists()
for folder in ('pins', 'logs', 'traces', 'results'): (c / folder).mkdir(parents=True)
shutil.copy2(source / 'build/kissat', c / 'pins/kissat')
(c / 'pins/kissat').chmod((c / 'pins/kissat').stat().st_mode & ~0o222)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
runner = (old / 'run.py').read_text().replace('tactix-6256-', 'tactix-6264-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "assert arm == 'kissat'\nenv['KISSAT_REMOVED_CLAUSE_TRACE'] = str(c / 'traces/kissat.removed-clause.tsv')\nenv['SAT_REMOVED_CLAUSE_CONFLICT'] = '372473'\nenv['SAT_REMOVED_CLAUSE_EXTERNAL'] = '399915'\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6256-rank32', '6264-rank32')
assert batch.count('#SBATCH --array=0-1\n') == 1
(c / 'array.sbatch').write_text(batch.replace('#SBATCH --array=0-1\n', '#SBATCH --array=1\n'))
m['resource_contract']['array'] = '1'
m.update(observer_build=dict(path=str(build.relative_to(root)), sha256=sha(build)),
    parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    purpose='Identify source and original clause for native-only399915 removal at372473.',
    validation='Require every6256 native trace and partial-proof hash/size unchanged; only removed-clause.tsv is new. Validate complete nontruncated lists and selected conflict/literal before source interpretation.',
    test_consideration='Four actual native fixture cases check deletion and pure observation plus disabled/conflict/variable filters. Generic strict observer parser with negative controls required. No Tactix semantic change or timing claim.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
print(c)

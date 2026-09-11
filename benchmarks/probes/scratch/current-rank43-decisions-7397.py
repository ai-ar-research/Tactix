#!/usr/bin/env python3
"""Fresh current/native full Rank43 decisions, using unchanged pinned observers."""
import csv
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha, reference
from checked_comparison import classify
from native_work import primary_statistics

C = ROOT / 'benchmarks/runs/20260911-7397-current-rank43-decisions'
CURRENT = ROOT / 'benchmarks/runs/20260911-7385-corrected-root-ranks1-50'
BUILD = ROOT / 'benchmarks/runs/20260911-7373-lucky-reattach-build'
NATIVE = ROOT / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
TRACE = ROOT / 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
WRAPPER = ROOT / 'benchmarks/runs/20260910-7064-rank31-full-boundaries'


def read(p): return json.loads(p.read_text())


def prepare():
    assert not C.exists(), 'never overwrite a campaign'
    assert not any(k.startswith(('SAT_', 'TACTIX_', 'KISSAT_')) for k in os.environ)
    cm, bm, nm, tm, wm = [read(p/'manifest.json') for p in (CURRENT, BUILD, NATIVE, TRACE, WRAPPER)]
    assert bm['status'] == 'PASS'
    deps = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, str(p)
        deps[str(p.relative_to(ROOT))] = digest
    for p in (CURRENT, BUILD, NATIVE, TRACE, WRAPPER): bind(p/'manifest.json')
    for rel, digest in cm['pin_sha256'].items(): bind(CURRENT/rel, digest)
    for rel, digest in wm['pin_sha256'].items(): bind(WRAPPER/rel, digest)
    for rel, digest in tm['hashes'].items(): bind(TRACE/rel, digest)
    for rel, digest in bm['files'].items(): bind(ROOT/bm['source']/rel, digest)
    trace_items = list(bm['binaries']['trace'].items()); assert len(trace_items) == 1
    trace_binary, trace_sha = trace_items[0]; bind(ROOT/trace_binary, trace_sha)
    for name in ('fixed_runtime', 'checked_comparison', 'native_work'):
        bind(ROOT/f'tools/bench/{name}.py')
    bind(Path(__file__))
    ref = reference(); rref = ref['records'][42]; assert rref['rank'] == 43
    bind(ROOT/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
    bind(ROOT/'benchmarks/cache'/rref['instance'], rref['input_sha256'])
    cp = CURRENT/'results/042.json'
    if not cp.exists(): cp = CURRENT/'provisional/042.json'
    np = NATIVE/'results/085.json'
    cr, nr = read(cp), read(np)
    assert classify(cr, cp.parent.name == 'results')['status'] in ('pending_validation', 'checked_UNSAT')
    assert classify(nr, True)['checked'] and classify(nr, True)['clean']
    assert cr['instance'] == nr['instance'] == rref['instance']
    assert primary_statistics(nr['solver_statistics'])['eliminated'] == 0
    for p in (cp, np): bind(p)
    protected = read(CURRENT/'launch.json')['protected_campaign_hashes']
    for rel, digest in protected.items(): bind(ROOT/rel, digest)
    for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
        (C/name).mkdir(parents=True)
    for name in ('runtime_task.py', 'checkmodel', 'modelcheck.py', 'drat-trim', 'candidate-source.patch'):
        shutil.copy2(CURRENT/'pins'/name, C/'pins'/name)
    for name in ('run.py', 'solver_environment.py'):
        shutil.copy2(WRAPPER/'pins'/name, C/'pins'/name)
    shutil.copy2(ROOT/trace_binary, C/'pins/candidate')
    shutil.copy2(TRACE/'pins/kissat', C/'pins/kissat')
    arms = {}; rows = []; oracles = {}
    for task, (arm, config, path) in enumerate((('candidate', cm['arms']['tactix'], cp), ('kissat', nm['arms']['kissat'], np))):
        prefix = 'TACTIX' if arm == 'candidate' else 'KISSAT'
        binary = C/'pins'/arm
        env = {}
        for key, suffix in (('_DECISION_STREAM_TRACE', 'decisions.tsv'), ('_INPROCESS_BOUNDARY_TRACE', 'boundaries.tsv')):
            assert (prefix+key).encode()+b'\0' in binary.read_bytes()
            env[prefix+key] = str(C/'traces'/f'{arm}.{suffix}')
        if arm == 'candidate': env['TACTIX_MACRO_BOUNDARY_TRACE'] = str(C/'traces/candidate.macro.tsv')
        arms[arm] = dict(config, path='pins/'+arm, sha256=sha(binary), environment=env,
                         source_build=str(Path(trace_binary).parent) if arm == 'candidate' else str(TRACE.relative_to(ROOT)))
        oracles['43/'+arm] = dict(path=str(path.relative_to(ROOT)), sha256=sha(path), final=path.parent.name == 'results')
        rows.append(dict(task=task, rank=43, arm=arm, tier='D', instance=rref['instance'], expected='UNSAT'))
    with (C/'matrix.tsv').open('x', newline='') as stream:
        w = csv.DictWriter(stream, rows[0], delimiter='\t', lineterminator='\n'); w.writeheader(); w.writerows(rows)
    batch = (WRAPPER/'array.sbatch').read_text().replace(str(WRAPPER), str(C))
    assert batch.count('7064-rank31-boundaries') == 1
    with (C/'array.sbatch').open('x') as stream: stream.write(batch.replace('7064-rank31-boundaries','7397-current-rank43-decisions'))
    m = dict(schema='current-native-full-rank43-decisions-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=arms, dependencies=deps, protected_campaign_hashes=protected,
        input_sha256={rref['instance']:rref['input_sha256']}, collect_runtime_context=True,
        resource_contract=dict(cm['resource_contract'], array='0-1'), unprofiled_records=oracles,
        hypothesis='Rank43 native completes before BVE/Factor while current executes both. Localize the earliest full decision and coarse inprocessing split, before pricing different work as unit cost or changing late scheduling.',
        validation='Two complete expected UNSAT solves; exact own full certificate bytes/hash and candidate every non-timing statistic/native full primary statistics. Require declared observer environment and complete five-column decision framing; validate all boundary rows. Independently checked-clean outcomes remain asynchronous acceptance gates.',
        limitation='Fresh missing-information native diagnostic, not a reference refresh, timing acceptance or promotion. No stop, global event tracing, new observer, policy change or fuzz waiver. Preserve old unsubmitted7077, all submitted campaigns and established closures.',
        test_consideration='Unchanged current solver/observers, pinned runtime and solver-only environment wrapper. Existing native-work, decision-prefix, boundary and environment tests plus new full-stream framing/ordering negative controls. No new solver unit test or duplicate fuzz campaign for this diagnostic.')
    m['pin_sha256'] = {str(p.relative_to(C)):sha(p) for p in [*(C/'pins').iterdir(),C/'matrix.tsv',C/'array.sbatch']}
    with (C/'manifest.json').open('x') as stream: json.dump(m, stream, indent=2)
    for p in (C/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(C)


def bindings(m):
    for rel, digest in m['pin_sha256'].items(): assert sha(C/rel) == digest, rel
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items(): assert sha(ROOT/rel) == digest, rel


def submit():
    assert not (C/'launch.json').exists(), 'already submitted'
    m = read(C/'manifest.json'); bindings(m)
    p = CURRENT/'results/042.json'
    if p.exists(): assert classify(read(p), True)['status'] == 'checked_UNSAT'
    since = dt.datetime.now().replace(microsecond=0).isoformat()
    command = ['sbatch', '--parsable', str(C/'array.sbatch')]
    job = subprocess.check_output(command, text=True).strip().split(';')[0]
    assert job.isdigit()
    with (C/'launch.json').open('x') as stream:
        json.dump(dict(job_id=job, accounting_since=since, command=command,
                       manifest_sha256=sha(C/'manifest.json'), submit_helper_sha256=sha(Path(__file__))), stream, indent=2)
    print(job, since)


if __name__ == '__main__':
    assert sys.argv[1:] in (['--prepare'], ['--submit'])
    if sys.argv[1:] == ['--prepare']: prepare()
    else: submit()

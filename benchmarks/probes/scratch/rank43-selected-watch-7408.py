#!/usr/bin/env python3
"""Same full solver replay, with bounded pre-conflict -1038 watch snapshots."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha
from bounded_trace_config import validate_trace_keys, UNREACHABLE_FINITE_STOP
from checked_comparison import classify

OLD = ROOT / 'benchmarks/runs/20260911-7401-rank43-analysis-window'
C = ROOT / 'benchmarks/runs/20260911-7408-rank43-selected-watch'
EVIDENCE = ROOT / 'benchmarks/reports/2026-09-11-rank43-conflict-semantics-7407.json'
WINDOW = (1, 82)
CAP = 250000


def read(p):
    return json.loads(p.read_text())


def environment(prefix, original):
    env = {k: v.replace(str(OLD), str(C)) for k, v in original.items()}
    del env[prefix + '_EVENT_TRACE_SKIP_WATCH']
    env.update({prefix + '_' + k: v for k, v in dict(
        EVENT_TRACE_CONFLICT_TO='82', EVENT_TRACE_LIMIT=str(CAP),
        EVENT_TRACE_WATCH_SCAN_LITERAL='-1038', EVENT_TRACE_WATCH_POSE='1',
        EVENT_TRACE_WATCH_VALUES='1').items()})
    return env


def fixtures():
    for prefix in ('TACTIX', 'KISSAT'):
        env = environment(prefix, {prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
                                  prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT': str(UNREACHABLE_FINITE_STOP)})
        assert prefix + '_EVENT_TRACE_SKIP_WATCH' not in env
        assert env[prefix + '_EVENT_TRACE_WATCH_SCAN_LITERAL'] == '-1038'
        assert env[prefix + '_EVENT_TRACE_CONFLICT_TO'] == '82'
        assert env[prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT'] == str(UNREACHABLE_FINITE_STOP)
        source = '\n'.join('getenv("' + k + '")' for k in env)
        assert len(validate_trace_keys(env, prefix, source)) == len(env)
        for k in env:
            try:
                validate_trace_keys(env, prefix, source.replace('getenv("' + k + '")', ''))
            except ValueError:
                pass
            else:
                raise AssertionError('unsupported selected-watch option accepted')
    print('Selected-watch config: two prefixes and twelve missing-key controls PASS')


def prepare():
    assert not C.exists()
    m = read(OLD / 'manifest.json')
    evidence = read(EVIDENCE)
    assert evidence['first_conflict_differences']['literals']['conflict'] == 82
    deps = dict(m['dependencies'])
    def bind(p, expected=None):
        d = sha(p)
        assert expected is None or d == expected, str(p)
        deps[str(p.relative_to(ROOT))] = d
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items():
            bind(ROOT / rel, digest)
    for rel, digest in m['pin_sha256'].items():
        bind(OLD / rel, digest)
    for rel, digest in evidence['hashes'].items():
        bind(ROOT / rel, digest)
    for p in (EVIDENCE, Path(__file__), OLD / 'manifest.json', OLD / 'launch.json',
              ROOT / 'tools/bench/watch_observer.py', ROOT / 'tools/bench/test_watch_observer.py',
              ROOT / '.claude/worktrees/runtime-lucky-reattach-7371/src/kernel/kernel.cpp',
              ROOT / 'tools/solvers/variants/kissat-imply-fast-5414/src/proplit.h'):
        bind(p)
    sources = dict(candidate=ROOT / '.claude/worktrees/runtime-lucky-reattach-7371/src/diagnostics/event_trace.cpp',
                   kissat=ROOT / 'tools/solvers/variants/kissat-imply-fast-5414/src/eventtrace.c')
    for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
        (C / name).mkdir(parents=True)
    for p in (OLD / 'pins').iterdir():
        if p.is_file():
            shutil.copy2(p, C / 'pins' / p.name)
    for arm, config in m['arms'].items():
        prefix = 'TACTIX' if arm == 'candidate' else 'KISSAT'
        config['environment'] = environment(prefix, config['environment'])
        trace_env = {k: v for k, v in config['environment'].items() if k.startswith(prefix + '_EVENT_TRACE')}
        source = sources[arm]
        bind(source)
        validate_trace_keys(trace_env, prefix, source.read_text())
        blob = (C / config['path']).read_bytes()
        assert all(k.encode() + b'\0' in blob for k in trace_env), arm
    for name in ('matrix.tsv', 'array.sbatch'):
        body = (OLD / name).read_text().replace(str(OLD), str(C)).replace('7401-rank43-analysis-window', '7408-rank43-selected-watch')
        with (C / name).open('x') as stream:
            stream.write(body)
    m.update(schema='rank43-selected-watch-full-replay-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
             dependencies=deps, window=list(WINDOW), row_cap=CAP, selected_scan=-1038,
             hypothesis='C82 first different incoming conflict clause precedes different binary learned clauses and backjump. Compare ordered watch membership/pose at propagated -1038 before C82; no change to solver scheduling or clause order.',
             validation='Exact full own7397 non-timing work, certificate, decisions and original boundaries/macro with explicit observer additions. Strict uncapped1..82 search; every completed-core7406 row must replay except sequence with only selected watch rows added in that core. Missing/changed rows, malformed payloads or cap invalidate diagnosis. Require selected scans before C82 and retain physical order and duplicates. Full independent proofs and cleanup remain mandatory, asynchronous.',
             test_consideration='No solver/observer/runner changes. Selected-watch config positive/negative controls, maintained watch_observer and search_window tests, plus whole-work/proof/decision validators. No duplicate unchanged-source fuzz required.',
             limitation='Diagnostic-only, no runtime acceptance or fixed-native refresh. Native large-clause formatter is bounded; reject malformed watch payloads rather than silently normalizing. Binary watch payloads do not include values, so membership/order alone does not prove both candidate conflicts were simultaneously false.')
    m['pin_sha256'] = {str(p.relative_to(C)): sha(p) for p in [*(C / 'pins').iterdir(), C / 'matrix.tsv', C / 'array.sbatch']}
    with (C / 'manifest.json').open('x') as stream:
        json.dump(m, stream, indent=2)
    for p in (C / 'pins').iterdir():
        p.chmod(p.stat().st_mode & ~0o222)
    print(C)


def submit():
    assert not (C / 'launch.json').exists()
    m = read(C / 'manifest.json')
    for rel, digest in m['pin_sha256'].items():
        assert sha(C / rel) == digest, rel
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items():
            assert sha(ROOT / rel) == digest, rel
    for task in range(2):
        p = OLD / f'results/{task:03d}.json'
        if p.exists():
            assert classify(read(p), True)['status'] == 'checked_UNSAT'
    since = dt.datetime.now().replace(microsecond=0).isoformat()
    command = ['sbatch', '--parsable', str(C / 'array.sbatch')]
    job = subprocess.check_output(command, text=True).strip().split(';')[0]
    assert job.isdigit()
    with (C / 'launch.json').open('x') as stream:
        json.dump(dict(job_id=job, accounting_since=since, command=command,
                       manifest_sha256=sha(C / 'manifest.json'), submit_helper_sha256=sha(Path(__file__))), stream, indent=2)
    print(job, since)


if __name__ == '__main__':
    assert sys.argv[1:] in (['--prepare'], ['--submit'], ['--self-test'])
    fixtures()
    if sys.argv[1:] == ['--prepare']:
        prepare()
    elif sys.argv[1:] == ['--submit']:
        submit()

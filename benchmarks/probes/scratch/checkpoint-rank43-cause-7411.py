#!/usr/bin/env python3
"""Read-only audit by default; seal publication and immutable local evidence once."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

TARGET = ROOT / 'benchmarks/reports/2026-09-11-rank43-cause-checkpoint-7411.json'


def audit(seal=False):
    if seal:
        assert not TARGET.exists()
    hashes = {}
    owned = set()
    def bind(p, expected=None, own=False):
        d = sha(p)
        assert expected is None or d == expected, str(p)
        rel = str(p.relative_to(ROOT))
        hashes[rel] = d
        if own:
            owned.add(rel)
    def read(p, own=False):
        bind(p, own=own)
        return json.loads(p.read_text())
    campaigns = {}
    jobs = []
    for stem, count, new in (
        ('7385-corrected-root-ranks1-50', 50, False), ('7382-corrected-root-runtime', 12, False),
        ('7389-current-gap-profiles', 4, False), ('7397-current-rank43-decisions', 2, False),
        ('7401-rank43-analysis-window', 2, True), ('7403-rank43-native-analysis-complete', 1, True),
        ('7408-rank43-selected-watch', 2, True),
    ):
        c = ROOT / ('benchmarks/runs/20260911-' + stem)
        m = read(c / 'manifest.json', new)
        for rel, d in m['pin_sha256'].items():
            p = c / rel
            bind(p, d, new and p.suffix in ('.py', '.patch', '.tsv', '.sbatch'))
        if new:
            for group in ('dependencies', 'protected_campaign_hashes'):
                for rel, d in m[group].items():
                    bind(ROOT / rel, d)
        for p in c.glob('launch*.json'):
            launch = read(p, new)
            jobs.append((str(launch['job_id']), launch['accounting_since']))
        states = {}
        arms = set()
        for folder in ('provisional', 'results'):
            for p in (c / folder).glob('*.json'):
                r = read(p, True)
                state = classify(r, folder == 'results')
                assert state['status'] in ('pending_validation', 'checked_UNSAT', 'checked_SAT'), (str(p), state)
                states[r['task']] = state
                arms.add(r['arm'])
        if new:
            for p in (c / 'traces').glob('*.tsv'):
                assert p.name.split('.')[0] in arms, 'do not hash growing trace without published solver record'
                bind(p, own=True)
            for p in c.glob('*summary*.json'):
                read(p, True)
        campaigns[stem] = dict(expected=count, published=len(states), checked=sum(s['checked'] for s in states.values()), states=states)
    for filename in ('rank43-analysis-window-7401.py', 'validate-rank43-analysis-7402.py',
                     'rank43-native-trace-successor-7403.py', 'validate-rank43-transactions-7404.py',
                     'rank43-complete-core-7406.py', 'rank43-conflict-semantics-7407.py',
                     'rank43-selected-watch-7408.py', 'validate-rank43-watch-7409.py', Path(__file__).name):
        bind(Path(__file__).with_name(filename), own=True)
    for rel in ('benchmarks/runs/20260911-7401-rank43-analysis-window/native-truncation-7403.json',
                'benchmarks/reports/2026-09-11-rank43-conflict-semantics-7407.json',
                'benchmarks/reports/2026-09-11-rank43-conflict-selection-7410.md'):
        bind(ROOT / rel, own=True)
    accounting = {}
    for job, since in sorted(set(jobs)):
        r = subprocess.run(['sacct', '-n', '-X', '-j', job, '-S', since, '-o', 'JobID,State%30,ExitCode,Elapsed,AllocCPUS', '-P'], text=True, capture_output=True, check=True)
        assert r.stdout.strip(), 'missing accounting is not terminal'
        accounting[job] = dict(since=since, stdout=r.stdout)
    q = subprocess.run(['squeue', '-h', '-r', '-j', ','.join(sorted({j for j, s in jobs})), '-o', '%i|%T|%M|%R'], text=True, capture_output=True, check=True)
    d = subprocess.run(['ps', '-p', '3043191', '-o', 'pid,ppid,etime,time,pcpu,stat,comm'], text=True, capture_output=True)
    out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes, owned_paths=sorted(owned),
               campaigns=campaigns, accounting=accounting, queue=q.stdout,
               debug7342=dict(command=d.args, exit=d.returncode, stdout=d.stdout,
                              complete_manifest_present=(ROOT / 'benchmarks/runs/20260911-7342-debug-contracts-debug/manifest.json').exists()),
               manual_removed_bytes=0,
               limitations='Published solver/final records only; traces are hashed only after solver publication. Live checker residue belongs to active wrappers and is not deleted or accepted. Source7371/pins7373/productionc2a95a6/fixed7239.010s denominator unchanged. Original capped traces and failed validators preserved. No new solver change or duplicate fuzz, no denied push/transfer retry. No performance promotion; full convergence and synchronization remain open.')
    if seal:
        with TARGET.open('x') as stream:
            json.dump(out, stream, indent=2)
    print(json.dumps({k: (v['published'], v['checked'], v['expected']) for k, v in campaigns.items()}, indent=2))
    print(q.stdout, d.stdout)


if __name__ == '__main__':
    assert sys.argv[1:] in ([], ['--seal'])
    audit(sys.argv[1:] == ['--seal'])

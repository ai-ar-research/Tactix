#!/usr/bin/env python3
"""Per-instance Tier D results of the five milestone versions (proof-free) beside the current
version's proof-checked run, and the summary tables quoted in README.md.

A SAT answer counts when its model checks; UNSAT answers have no certificate in the proof-free
runs, so a version's UNSAT count is unchecked. PAR-2 counts every unsolved instance as 10,000 s.
The current-version column uses its own scorer (proof-verified, 64 GB recheck substituted).

Writes results.csv (one row per instance and version) and wide.csv (one row per instance).
"""
import collections
import csv
import glob
import json
import subprocess
from pathlib import Path

ROOT = Path('/home/hwu/SAT-solving')
OUT = Path(__file__).resolve().parent
RUNS = ROOT / 'benchmarks/runs'
PENALTY = 10000
LADDER = [
    ('tierA-dcc3ecbd6', '20261011-tierd-noproof-1-tierA-dcc3ecbd6'),
    ('tierB14-546298924', '20261011-tierd-noproof-2-tierB14-546298924'),
    ('tierC-6f44f23d6', '20261011-tierd-noproof-3-tierC-6f44f23d6'),
    ('rank1-50-f50e46c0c', '20261011-tierd-noproof-4-rank1-50-f50e46c0c'),
    ('wv100-0a9063387', '20261011-tierd-noproof-5-wv100-0a9063387'),
]
CURRENT = 'current-0f7c10174'
OLD_B = 'ff32e6f4b^:benchmarks/tiers/B.list'


def records(name):
    return {d['instance']: d for d in
            (json.load(open(f)) for f in glob.glob(str(RUNS / name / 'results/*.json')))}


def solved(e):
    return e.get('verdict') in ('SAT', 'UNSAT') and e.get('expected_match', True)


def counted(e, proof_free):
    if not solved(e):
        return False
    if e['verdict'] == 'UNSAT' and proof_free:
        return True
    return bool(e.get('valid'))


def status(e, proof_free):
    solve = e.get('solve') or {}
    if solved(e):
        if e['verdict'] == 'UNSAT' and proof_free:
            return 'unsat-unchecked'
        return 'verified' if e.get('valid') else 'unverified'
    if e.get('verdict') in ('SAT', 'UNSAT'):
        return 'wrong-answer'
    if solve.get('timed_out'):
        return 'timeout'
    return 'solver-memout' if solve.get('exit_code') == 137 else f"no-answer (exit {solve.get('exit_code')})"


def main():
    arms = {k: (records(n), 'runs/' + n, True) for k, n in LADDER}
    cur = records('20261010-tierd-fastel')
    for k, v in records('20261011-tierd-fastel-recheck-64g').items():
        if v.get('valid'):
            cur[k] = v
    arms[CURRENT] = (cur, 'runs/20261010-tierd-fastel (+64 GB recheck)', False)
    names = [k for k, _ in LADDER] + [CURRENT]

    years = {}
    with open(ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/year-membership.tsv') as f:
        for y in csv.DictReader(f, delimiter='\t'):
            years[y['hash']] = '+'.join(c for c in ('SC2024', 'SC2025', 'SC2026') if y[c] == '1')
    old_b = {line.split()[0].split('/')[-1] for line in subprocess.check_output(
        ['git', '-C', str(ROOT), 'show', OLD_B], text=True).splitlines()
        if line.strip() and not line.startswith('#')}

    instances = sorted(cur)
    assert all(set(a) == set(instances) for a, _, _ in arms.values())
    rows = []
    for inst in instances:
        h, name = inst.split('/')[-1].split('-', 1)
        for k in names:
            recs, campaign, proof_free = arms[k]
            e = recs[inst]
            solve = e.get('solve') or {}
            ok = counted(e, proof_free)
            mc = e.get('model_check') or {}
            pc = e.get('proof_check') or {}
            rows.append(dict(
                instance=inst, name=name, hash=h, competition=years.get(h), old_tier_b=name in old_b,
                expected=e.get('expected'), version=k, proof_mode='none' if proof_free else 'auto-proof',
                source_campaign=campaign, status=status(e, proof_free), verdict=e.get('verdict'),
                counted=ok, solve_wall_s=solve.get('wall_seconds'),
                solve_peak_rss_mb=round(solve['peak_rss_kb'] / 1024) if solve.get('peak_rss_kb') else None,
                solve_exit_code=solve.get('exit_code'), solve_timed_out=solve.get('timed_out'),
                par2_s=round(solve['wall_seconds'], 3) if ok else PENALTY,
                model_check_result=mc.get('result'), proof_check_result=pc.get('result'),
                proof_check_wall_s=pc.get('wall_seconds'), node=e.get('node')))

    with open(OUT / 'results.csv', 'w', newline='') as f:
        w = csv.DictWriter(f, list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    with open(OUT / 'wide.csv', 'w', newline='') as f:
        w = csv.writer(f)
        cols = ['status', 'solve_wall_s', 'par2_s']
        w.writerow(['name', 'competition', 'old_tier_b', 'expected'] + [f'{k}:{c}' for k in names for c in cols])
        for i in range(0, len(rows), len(names)):
            g = rows[i:i + len(names)]
            w.writerow([g[0]['name'], g[0]['competition'], g[0]['old_tier_b'], g[0]['expected']] +
                       [r[c] for r in g for c in cols])

    by = collections.defaultdict(dict)
    for r in rows:
        by[r['version']][r['instance']] = r
    print('| Version | Solved | SAT | UNSAT | Unsolved | Memout | PAR-2 sum |')
    for k in names:
        a = by[k].values()
        print(f"| {k} | {sum(r['counted'] for r in a)} | {sum(r['counted'] and r['verdict'] == 'SAT' for r in a)}"
              f" | {sum(r['counted'] and r['verdict'] == 'UNSAT' for r in a)}"
              f" | {sum(not r['counted'] for r in a)} | {sum(r['status'] == 'solver-memout' for r in a)}"
              f" | {sum(r['par2_s'] for r in a):,.0f} |")
    print('\nconsecutive gained/lost')
    for p, q in zip(names, names[1:]):
        g = [i for i in instances if by[q][i]['counted'] and not by[p][i]['counted']]
        l = [i for i in instances if by[p][i]['counted'] and not by[q][i]['counted']]
        print(f'{p} -> {q}: +{len(g)} -{len(l)}')
    print('\nby competition (solved)')
    comps = sorted({r['competition'] for r in rows})
    for k in names:
        print(k, {c: sum(r['counted'] for r in by[k].values() if r['competition'] == c) for c in comps})
    print('total', {c: sum(r['competition'] == c for r in by[CURRENT].values()) for c in comps})
    print('\nold Tier B rows in Tier D')
    for i in instances:
        if by[CURRENT][i]['old_tier_b']:
            print('|', by[CURRENT][i]['name'][:44], '|', by[CURRENT][i]['expected'], '|',
                  ' | '.join(f"{by[k][i]['verdict'][0]} {by[k][i]['solve_wall_s']:.0f}" if by[k][i]['counted']
                             else ('MO' if by[k][i]['status'] == 'solver-memout' else 'TO') for k in names), '|')


if __name__ == '__main__':
    main()

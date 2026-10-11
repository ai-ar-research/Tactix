#!/usr/bin/env python3
"""Per-instance Tier D results for every arm of the 2026-10-11 final comparison.

Merge rule (same as ../2026-10-11-tierd-fastel-final/README.md):
- Tactix 0f7c10174: 20261010-tierd-fastel, with a 64 GB recheck record substituted when it verifies.
- Tactix no-MAB d170b4a5: 20260928-am546 (4 h proof-check window).
- Satsuma+Kissat: the full fixed64 8 h rerun, with a 64 GB recheck record substituted when it verifies.
- kissat-sc2024, kissat-4.0.4, AE-Kissat-MAB, LymphoSAT: the frozen 32 GB campaign, with a 64 GB / 8 h
  recheck record substituted when it verifies.
A SAT answer counts only if the model checks and (reference solvers) the output follows the
competition protocol. Unverified rows count 10,000 s in PAR-2.

Writes results.csv (one row per instance and solver), wide.csv (one row per instance) and results.json.
"""
import csv
import glob
import json
from pathlib import Path

ROOT = Path('/home/hwu/SAT-solving')
OUT = Path(__file__).resolve().parent
RUNS, BASE = ROOT / 'benchmarks/runs', ROOT / 'benchmarks/baselines'
TIMEOUT, PENALTY = 5000, 10000
SOLVERS = ['tactix-0f7c10174', 'tactix-no-mab', 'satsuma-kissat', 'kissat-sc2024',
           'kissat-4.0.4', 'ae-kissat-mab', 'lymphosat']


def run_records(name):
    return {d['instance']: d for d in
            (json.load(open(f)) for f in glob.glob(str(RUNS / name / 'results/*.json')))}


def baseline_records(name):
    out = {}
    for e in json.load(open(BASE / name / 'results.json'))['results']:
        out.setdefault(e['solver'], {})[e['instance']] = e
    return out


def solved(e):
    return e.get('verdict') in ('SAT', 'UNSAT') and e.get('expected_match', True)


def verified(e):
    if not solved(e):
        return False
    if 'model_ok' in e and e['verdict'] == 'SAT':
        return e['model_ok'] is True
    return bool(e.get('valid'))


def mb(kb):
    return round(kb / 1024) if kb else None


def proof_detail(e):
    pc = e.get('proof_check') or {}
    stages = pc.get('stages') or []
    peak = max([s.get('peak_rss_kb') or 0 for s in stages] + [pc.get('peak_rss_kb') or 0])
    cause = None
    if pc and pc.get('result') != 'verified':
        bad = next((s for s in stages if s.get('result') != 'verified'), stages[-1] if stages else {})
        if pc.get('timed_out') or bad.get('timed_out'):
            cause = 'check-timeout'
        elif bad.get('exit_code') == 137:
            cause = 'check-memout'
        else:
            cause = f"check-failed ({bad.get('name')} exit {bad.get('exit_code')})"
    return dict(proof_pipeline=pc.get('pipeline'),
                proof_check_result=pc.get('result'),
                proof_check_wall_s=pc.get('wall_seconds'),
                proof_check_peak_rss_mb=mb(peak),
                proof_check_stages=' > '.join(
                    f"{s.get('name')}:{s.get('result')}:{s.get('wall_seconds')}s" for s in stages) or None,
                proof_check_failure=cause)


def row(solver, campaign, e, recheck):
    solve = e.get('solve') or {}
    mc = e.get('model_check') or {}
    ok = verified(e)
    if not e:
        status = 'missing'
    elif e.get('verdict') in ('SAT', 'UNSAT') and not e.get('expected_match', True):
        status = 'wrong-answer'
    elif ok:
        status = 'verified'
    elif e.get('verdict') == 'SAT':
        status = 'model-ok-nonprotocol' if mc.get('exit_code') == 0 else 'model-rejected'
    elif e.get('verdict') == 'UNSAT':
        status = 'unverified'
    elif solve.get('timed_out'):
        status = 'timeout'
    elif solve.get('exit_code') == 137:
        status = 'solver-memout'
    else:
        status = f"no-answer (exit {solve.get('exit_code')})"
    wall = solve.get('wall_seconds')
    r = dict(solver=solver, source_campaign=campaign, status=status, verdict=e.get('verdict'),
             verified=ok, solve_wall_s=wall, solve_peak_rss_mb=mb(solve.get('peak_rss_kb')),
             solve_exit_code=solve.get('exit_code'), solve_timed_out=solve.get('timed_out'),
             par2_s=round(wall, 3) if ok else PENALTY,
             model_check_result=mc.get('result'), model_check_wall_s=mc.get('wall_seconds'),
             model_protocol_ok=e.get('model_protocol_ok'),
             certificate_bytes=e.get('certificate_bytes'), node=e.get('node'))
    r.update(proof_detail(e))
    if recheck:
        rc, re = recheck
        r['recheck_campaign'] = rc
        r['recheck_status'] = ('verified' if verified(re) else
                               (proof_detail(re)['proof_check_failure'] or re.get('verdict')))
    return r


def main():
    arms = {}
    tac = run_records('20261010-tierd-fastel')
    rec = run_records('20261011-tierd-fastel-recheck-64g')
    arms['tactix-0f7c10174'] = (tac, 'runs/20261010-tierd-fastel',
                                {k: ('runs/20261011-tierd-fastel-recheck-64g', v) for k, v in rec.items()})
    arms['tactix-no-mab'] = (run_records('20260928-am546-no-mab-tactix-hybrid-tierd-cpuq'),
                             'runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq', {})
    frozen = baseline_records('sc24-26-32g-5000s')
    sat_full = baseline_records('sc24-26-satsuma-fixed64-32g-8h')['satsuma-kissat']
    rechecks = {}
    for c in ('sc24-26-recheck-64g-8h', 'sc24-26-satsuma-recheck-64g-8h'):
        for s, d in baseline_records(c).items():
            for k, v in d.items():
                rechecks.setdefault(s, {})[k] = ('baselines/' + c, v)
    arms['satsuma-kissat'] = (sat_full, 'baselines/sc24-26-satsuma-fixed64-32g-8h',
                              rechecks.get('satsuma-kissat', {}))
    for s in ('kissat-sc2024', 'kissat-4.0.4', 'ae-kissat-mab', 'lymphosat'):
        arms[s] = (frozen[s], 'baselines/sc24-26-32g-5000s', rechecks.get(s, {}))

    years = {}
    with open(BASE / 'sc24-26-32g-5000s/year-membership.tsv') as f:
        for y in csv.DictReader(f, delimiter='\t'):
            years[y['hash']] = '+'.join(c for c in ('SC2024', 'SC2025', 'SC2026') if y[c] == '1')

    instances = sorted(tac)
    assert all(set(d) == set(instances) for d, _, _ in arms.values())
    rows = []
    for inst in instances:
        h, name = inst.split('/')[-1].split('-', 1)
        meta = dict(instance=inst, name=name, hash=h, competition=years.get(h),
                    expected=tac[inst].get('expected'))
        for solver in SOLVERS:
            records, campaign, rc = arms[solver]
            e = records[inst]
            recheck = rc.get(inst)
            if recheck and verified(recheck[1]):
                e, campaign = recheck[1], recheck[0]
            rows.append({**meta, **row(solver, campaign, e, recheck)})

    fields = list(dict.fromkeys(k for r in rows for k in r))
    with open(OUT / 'results.csv', 'w', newline='') as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        w.writerows(rows)
    (OUT / 'results.json').write_text('[\n' + ',\n'.join(json.dumps(r) for r in rows) + '\n]\n')

    wide_cols = ['status', 'solve_wall_s', 'proof_check_wall_s', 'solve_peak_rss_mb', 'par2_s']
    with open(OUT / 'wide.csv', 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['name', 'competition', 'expected'] + [f'{s}:{c}' for s in SOLVERS for c in wide_cols])
        for i in range(0, len(rows), len(SOLVERS)):
            grp = rows[i:i + len(SOLVERS)]
            w.writerow([grp[0]['name'], grp[0]['competition'], grp[0]['expected']] +
                       [r[c] for r in grp for c in wide_cols])

    print(f'{len(instances)} instances x {len(SOLVERS)} solvers')
    for solver in SOLVERS:
        rs = [r for r in rows if r['solver'] == solver]
        print(f"{solver:17s} solved {sum(r['verdict'] in ('SAT', 'UNSAT') and r['status'] != 'wrong-answer' for r in rs):5d}"
              f" verified {sum(r['verified'] for r in rs):5d} PAR-2 {sum(r['par2_s'] for r in rs):12,.0f}")


if __name__ == '__main__':
    main()

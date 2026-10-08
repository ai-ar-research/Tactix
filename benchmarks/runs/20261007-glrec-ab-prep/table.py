#!/usr/bin/env python3
"""Pair the glrec A/B results (final if present, else provisional) per row."""
import json, re, sys
from pathlib import Path
C = Path('/home/hwu/SAT-solving/benchmarks/runs/20261007-glrec-ab')
rows = {}
for line in (C / 'matrix.tsv').read_text().splitlines()[1:]:
    task, rank, arm, tier, inst, exp = line.split('\t')
    f = C / 'results' / f'{int(task):03d}.json'
    p = C / 'provisional' / f'{int(task):03d}.json'
    r = json.loads(f.read_text()) if f.exists() else json.loads(p.read_text()) if p.exists() else None
    if r is None:
        continue
    kv, solve_ms = {}, None
    for l in r.get('solver_statistics') or []:
        m = re.match(r'c graph-schedule (\w+)=(\d+)', l)
        if m: kv[m.group(1)] = int(m.group(2))
        m = re.match(r'c timing\s+(\d+) ms solve', l)
        if m: solve_ms = int(m.group(1))
    ins = cyc = None
    for l in ((r.get('runtime_context') or {}).get('perf_statistics') or []):
        f2 = l.split('\t')
        if len(f2) > 2 and f2[2] == 'instructions': ins = int(f2[0])
        if len(f2) > 2 and f2[2] == 'cycles': cyc = int(f2[0])
    chk = (r.get('model_check') or r.get('proof_check') or {}).get('result') if f.exists() else 'pending'
    rows.setdefault((tier, rank, inst.split('/')[-1][:44], exp), {})[arm] = dict(
        verdict=r.get('verdict'), wall=r.get('solve', {}).get('wall_seconds'), solve_ms=solve_ms,
        auth=kv.get('authentication_work'), exh=kv.get('authentication_exhausted'),
        rec=kv.get('recognized'), vert=kv.get('vertices'), sat=kv.get('sat'),
        hints=kv.get('useful_hints'), work=kv.get('work'), check=chk, valid=r.get('valid'),
        node=r.get('node'), ins=ins, cyc=cyc, cert=r.get('certificate_sha256'))
print('| Row | Expected | c1 verdict / wall s / solve ms | glrec verdict / wall s / solve ms | Recognition work c1 -> glrec | exhausted c1/glrec | recognized (n) c1/glrec | graph SAT c1/glrec | total GL work c1 -> glrec | check c1/glrec |')
print('|---|---|---|---|---|---|---|---|---|---|')
for (tier, rank, name, exp), a in rows.items():
    c, g = a.get('c1', {}), a.get('glrec', {})
    f = lambda d, k: d.get(k, '-')
    print(f"| {tier} {rank} {name} | {exp} | {f(c,'verdict')} / {f(c,'wall')} / {f(c,'solve_ms')} | {f(g,'verdict')} / {f(g,'wall')} / {f(g,'solve_ms')} | {f(c,'auth')} -> {f(g,'auth')} | {f(c,'exh')}/{f(g,'exh')} | {f(c,'rec')} ({f(c,'vert')}) / {f(g,'rec')} ({f(g,'vert')}) | {f(c,'sat')}/{f(g,'sat')} | {f(c,'work')} -> {f(g,'work')} | {f(c,'check')}/{f(g,'check')} |")
print()
print('| Row | node c1/glrec | instructions c1 -> glrec (delta) | IPC c1/glrec | certificate identical |')
print('|---|---|---|---|---|')
for (tier, rank, name, exp), a in rows.items():
    c, g = a.get('c1', {}), a.get('glrec', {})
    if not (c.get('ins') and g.get('ins')): continue
    d = g['ins'] - c['ins']
    print(f"| {tier} {rank} {name} | {c['node']}/{g['node']} | {c['ins']/1e9:.3f}e9 -> {g['ins']/1e9:.3f}e9 ({d/1e9:+.3f}e9, {100*d/c['ins']:+.2f}%) | {c['ins']/c['cyc']:.2f}/{g['ins']/g['cyc']:.2f} | {'yes' if c['cert']==g['cert'] else 'NO'} |")
if '--json' in sys.argv:
    print(json.dumps({' '.join(map(str, k)): v for k, v in rows.items()}, indent=1))

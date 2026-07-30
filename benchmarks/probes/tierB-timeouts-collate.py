#!/usr/bin/env python3
"""Collate the three tierB-timeouts probe arrays into one committed evidence
file: a one-line row per arm, then the full stat dump of the arms the report
cites.  Raw per-task outputs stay in $TMPDIR-style scratch dirs (tbto*-raw/),
which are NOT committed (192 files)."""
import re, os
ROOT = '/home/hwu/SAT-solving/benchmarks/probes'
WAVES = [('tierB-timeouts', 'tbto-raw', 1574764),
         ('tierB-timeouts2', 'tbto2-raw', 1574882),
         ('tierB-timeouts3', 'tbto3-raw', 1574949),
         ('tierB-timeouts4', 'tbto4-raw', 1575173)]
CITED = {('tbto-raw', n) for n in (1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 81, 85, 117)} | \
        {('tbto2-raw', n) for n in (2, 8, 24, 35)} | \
        {('tbto3-raw', n) for n in (5, 11)} | \
        {('tbto4-raw', n) for n in (1, 5)}
out = []
out.append("Tier B timeout diagnosis — probe arrays 1574764 / 1574882 / 1574949 "
           "(2026-07-30)\nshape: cpu-dense-hwulab-q, 1 cpu / 8 GB, %36/%20/%16; "
           "solver build/pin-final (git 77fc578)\nreport: "
           "benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md\n")
for mf, raw, jid in WAVES:
    out.append(f"\n{'='*78}\n=== array {jid} — {mf}.mf\n{'='*78}")
    for i, line in enumerate(open(f'{ROOT}/{mf}.mf'), 1):
        lab = line.split('|')[0]
        p = f'{ROOT}/{raw}/{i}.out'
        if not os.path.exists(p):
            out.append(f"{i:>4} {lab:<32} NOFILE"); continue
        t = open(p, errors='replace').read()
        g = lambda r: (re.search(r, t, re.M).group(1) if re.search(r, t, re.M) else None)
        w = g(r'^wall ([0-9.]+) s') or '?'
        v = (g(r'^s (SATISFIABLE|UNSATISFIABLE|UNKNOWN)') or 'TO')[:5]
        c = g(r'^c conflicts\s+(\d+)') or g(r'^c conflicts:\s+(\d+)') or ''
        s = g(r'^c timing\s+\d+ ms \(([0-9.]+)%\) inprocessing slot') or ''
        out.append(f"{i:>4} {lab:<32} {w:>9} s  {v:<6} conflicts={c:<12} slot={s}")
out.append(f"\n\n{'#'*78}\n# FULL OUTPUT of the arms the report cites\n{'#'*78}")
for mf, raw, jid in WAVES:
    for i, line in enumerate(open(f'{ROOT}/{mf}.mf'), 1):
        if (raw, i) not in CITED: continue
        p = f'{ROOT}/{raw}/{i}.out'
        if os.path.exists(p):
            out.append(f"\n{'-'*78}\n--- {raw}/{i}.out\n{'-'*78}")
            out.append(open(p, errors='replace').read().rstrip())
open(f'{ROOT}/tierB-timeouts-1574764.out', 'w').write("\n".join(out) + "\n")
print("wrote", f'{ROOT}/tierB-timeouts-1574764.out')

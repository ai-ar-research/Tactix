#!/usr/bin/env python3
"""Summarize the SCHED-H3 `c redscan` probe outputs.

One row per instance: arena words Reduce's enumeration walk covered, and the
leading run of them a kissat `first_reducible` watermark (reduce.c:161) would
have skipped.
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True,
                      check=True).stdout.strip()
D = os.path.join(ROOT, "benchmarks", "probes", "scratch")
PAT = re.compile(r"^c redscan\s+(\d+) scans, (\d+) arena words walked, "
                 r"(\d+) skippable")
# `c mode ... (N ticks, ...)` — the kernel's propagation memory-traffic proxy,
# counted in CACHE LINES, which is the unit Reduce's walk has to be priced in.
MODE = re.compile(r"^c mode\s+\S+ after \d+ switches \((\d+) ticks")
WORDS_PER_LINE = 16  # 4-byte arena words in a 64-byte line

rows = []
for name in sorted(os.listdir(D)):
    if not (name.startswith("h3-") and name.endswith(".out")):
        continue
    inst, verdict, scans, walked, skip, ticks = "?", "?", 0, 0, 0, 0
    for line in open(os.path.join(D, name)):
        if line.startswith("== "):
            inst = line.split()[-1].split("/")[-1]
        elif line.startswith("s "):
            verdict = line.split()[1]
        elif PAT.match(line):
            scans, walked, skip = (int(g) for g in PAT.match(line).groups())
        elif MODE.match(line):
            ticks = int(MODE.match(line).group(1))
    rows.append((inst, verdict, scans, walked, skip, ticks))

rows.sort(key=lambda r: -r[3])
tw = ts = tt = 0
print(f"{'instance':46s} {'verd':5s} {'scans':>6s} {'Mwords':>9s} "
      f"{'Mskip':>9s} {'skip%':>6s} {'Mticks':>9s} {'walk/tick%':>10s}")
for inst, verdict, scans, walked, skip, ticks in rows:
    tw += walked
    ts += skip
    tt += ticks
    pct = 100.0 * skip / walked if walked else 0.0
    share = (100.0 * (walked / WORDS_PER_LINE) / ticks) if ticks else 0.0
    print(f"{inst[:46]:46s} {verdict[:5]:5s} {scans:6d} {walked/1e6:9.1f} "
          f"{skip/1e6:9.1f} {pct:6.1f} {ticks/1e6:9.1f} {share:10.2f}")
print(f"{'TOTAL':46s} {'':5s} {'':6s} {tw/1e6:9.1f} {ts/1e6:9.1f} "
      f"{(100.0 * ts / tw if tw else 0):6.1f} {tt/1e6:9.1f} "
      f"{(100.0 * (tw / WORDS_PER_LINE) / tt if tt else 0):10.2f}")
solved = [r for r in rows if r[1].startswith(("SAT", "UNSAT"))]
print(f"\ninstances: {len(rows)} ({len(solved)} answered), "
      f"reductions: {sum(r[2] for r in rows)}")
shares = sorted(100.0 * (r[3] / WORDS_PER_LINE) / r[5] for r in rows if r[5])
skips = sorted(100.0 * r[4] / r[3] for r in rows if r[3])
if shares:
    print(f"walk as a share of propagation cache-line traffic: median "
          f"{shares[len(shares) // 2]:.2f}%, max {shares[-1]:.2f}% "
          f"(n={len(shares)}); the aging walk doubles it")
if skips:
    print(f"per-instance skip%: min {skips[0]:.1f}, median "
          f"{skips[len(skips) // 2]:.1f}, max {skips[-1]:.1f}; "
          f"{sum(1 for s in skips if s < 1.0)} instances under 1%")
print(f"enumeration walk: {tw * 4 / 1e9:.1f} GB read; a first_reducible "
      f"watermark would skip {ts * 4 / 1e9:.1f} GB of it.")
print("(reduce_apply_refs walks the same arena twice more -- the aging tick "
      "and the SCHED-F6 survivor envelope -- so Reduce's true arena traffic "
      "is ~3x these figures.)")
sys.exit(0)

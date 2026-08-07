#!/usr/bin/env python3
"""Summarize the block-1 D3/E3 counters probe over a Tier B array.

D3 retires iff `blocked` is 0 on BOTH slot clocks on every instance; E3's
`clamped` is read beside `global ticks`, which is the denominator that decides
whether a zero means anything. Both counters are driven by the cumulative clock,
so both read zero on every quick instance — the verdict lines below therefore
report WHICH instances moved, never just a count, and the reader is expected to
check that the instances which run to the cap are present at all.
"""
import glob
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
D = os.path.join(ROOT, "benchmarks/probes/scratch/block1-d3")

rows, missing = [], []
for f in sorted(glob.glob(os.path.join(D, "*_*.out"))):
    t = open(f, errors="replace").read()
    m = re.search(r"stem=(\S+)", t)
    if not m:
        missing.append((os.path.basename(f), "no header"))
        continue
    stem = m.group(1)
    clock = re.search(r"^c slot-clock\s+probe phases (\d+) next (\d+) blocked (\d+) \| "
                      r"eliminate phases (\d+) next (\d+) blocked (\d+)", t, re.M)
    if not clock:
        missing.append((stem, "no slot-clock line"))
        continue
    ticks = re.search(r"global ticks (\d+)", t)
    verdict = re.search(r"^s (\S+)", t, re.M)
    rows.append(dict(stem=stem, verdict=verdict.group(1) if verdict else "NO-VERDICT",
                     pph=int(clock.group(1)), pblk=int(clock.group(3)),
                     eph=int(clock.group(4)), eblk=int(clock.group(6)),
                     ticks=int(ticks.group(1)) if ticks else 0,
                     clamped=sum(int(c) for c in
                                 re.findall(r"^\S+ maxeff \d+ clamped (\d+)", t, re.M)),
                     ceil=[(e, int(mx)) for e, mx in
                           re.findall(r"^(\S+) maxeff (\d+)", t, re.M) if int(mx)],
                     wrapped=sum(int(w) for w in re.findall(r"wrapped (\d+)", t))))

print(f"{'instance':<56} {'verdict':<14} {'pph':>4} {'pblk':>5} {'eph':>4} {'eblk':>5} "
      f"{'clamped':>8} {'globalticks':>13} {'wrap':>5}")
for r in sorted(rows, key=lambda r: -r["ticks"]):
    print(f"{r['stem'][:56]:<56} {r['verdict']:<14} {r['pph']:>4} {r['pblk']:>5} "
          f"{r['eph']:>4} {r['eblk']:>5} {r['clamped']:>8} {r['ticks']:>13,} {r['wrapped']:>5}")

blk = [(r["stem"], r["pblk"], r["eblk"]) for r in rows if r["pblk"] or r["eblk"]]
clp = [(r["stem"], r["clamped"]) for r in rows if r["clamped"]]
wrp = [r["stem"] for r in rows if r["wrapped"]]
tk = [r["ticks"] for r in rows] or [0]
print(f"\nread {len(rows)} instances; missing {len(missing)}: {missing}")
print(f"D3  blocked (probe, eliminate) != 0 on: {blk or 'NONE'}")
print(f"E3  clamped != 0 on: {clp or 'NONE'}")
print(f"    ledger wrapped != 0 on: {wrp or 'NONE'}")
print(f"    global ticks: min {min(tk):,} max {max(tk):,}")
print(f"    entries carrying a ceiling: {sorted({e for r in rows for e, _ in r['ceil']})}")
print(f"    no slot phase ran at all on: "
      f"{[r['stem'] for r in rows if not r['pph'] and not r['eph']] or 'NONE'}")
sys.exit(1 if missing else 0)

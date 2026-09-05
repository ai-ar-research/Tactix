#!/usr/bin/env python3
"""Paired base/cand summary of a two-arm campaign, from provisional records.

Provisional records are published before the independent proof check, so a
timing read is available without waiting for certification; validity still
comes from `results/`.  Prints per-instance pairs, the arm totals over the
pairs that are complete on both arms, and whether the two arms produced
byte-identical certificates (the behaviour-identity oracle).
"""

import argparse
import glob
import json
from pathlib import Path


def load(campaign):
    rows = {}
    for sub in ("provisional", "results"):
        for path in glob.glob(str(Path(campaign) / sub / "*.json")):
            row = json.loads(Path(path).read_text())
            key = (row.get("rank"), row["instance"], row["arm"])
            if key in rows and sub == "provisional":
                continue
            rows[key] = row
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("campaign")
    ap.add_argument("--label", nargs=2, default=["base", "cand"])
    args = ap.parse_args()
    base_arm, cand_arm = args.label
    rows = load(args.campaign)
    keys = sorted({(r, i) for r, i, _ in rows},
                  key=lambda k: (k[0] is None, k[0], k[1]))
    print("%-6s %10s %10s %9s  %s" %
          ("id", base_arm, cand_arm, "delta", "verdict/identity"))
    tb = tc = 0.0
    pairs = mismatch = identical = 0
    for rank, instance in keys:
        b = rows.get((rank, instance, base_arm))
        c = rows.get((rank, instance, cand_arm))
        name = str(rank) if rank is not None else Path(instance).name[:28]
        if not b or not c:
            have = base_arm if b else (cand_arm if c else "-")
            print("%-6s %10s %10s %9s  pending (%s only)" %
                  (name, "-", "-", "-", have))
            continue
        bs = b["solve"]["wall_seconds"]
        cs = c["solve"]["wall_seconds"]
        tb += bs
        tc += cs
        pairs += 1
        note = "%s/%s" % (b["verdict"], c["verdict"])
        if b["verdict"] != c["verdict"]:
            note += " VERDICT-MISMATCH"
            mismatch += 1
        elif b.get("certificate_sha256") and c.get("certificate_sha256"):
            same = b["certificate_sha256"] == c["certificate_sha256"]
            identical += same
            note += " proof-identical" if same else " PROOF-DIFFERS"
        print("%-6s %10.1f %10.1f %+9.1f  %s" % (name, bs, cs, cs - bs, note))
    print("\npaired instances: %d" % pairs)
    print("%-6s %10.1f %10.1f %+9.1f  (%.4fx)" %
          ("TOTAL", tb, tc, tc - tb, tc / tb if tb else 0))
    print("byte-identical certificates: %d ; verdict mismatches: %d"
          % (identical, mismatch))


if __name__ == "__main__":
    main()

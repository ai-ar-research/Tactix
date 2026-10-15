#!/usr/bin/env python3
"""Fix the Chvatal (n=7) evaluation set for the paper.

Population: every distinct subproblem that symbreak's SAT partitioner, in its
published n=7 configuration (chvatal-sat 7 -t 3 -D 20 -p 16), submitted to its
Kissat oracle (corpus/chvatal/population.json, content-deduplicated).

Difficulty is measured only by the neutral reference solver Kissat 4.0.4,
symbreak's own oracle and not one of the compared strategies.  A node is hard
when Kissat needs at least HARD_SECONDS (or does not finish within the screen
limit).  Every instance on which the twin-merge strategy was developed or tuned
is excluded by clause-content hash, so the evaluation is held out.  If more
than TARGET hard nodes remain, a uniform sample is drawn with a fixed seed.
"""

import argparse
import glob
import hashlib
import json
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEVELOPMENT_LISTS = ["dev5.txt", "dev11.txt", "mid6.txt", "hard9.txt",
                     "chvatal30.txt"]


def content_hash(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for line in source:
            if not line.startswith(b"c"):
                h.update(line)
    return h.hexdigest()


def load_screen(run):
    manifest = json.load(open(os.path.join(run, "manifest.json")))
    out = {}
    for path in glob.glob(os.path.join(run, "results", "*.json")):
        task = manifest["tasks"][int(os.path.basename(path)[:5])]
        out[task["rel"]] = json.load(open(path))
    return manifest, out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--screen", required=True,
                        help="Kissat 4.0.4 population screen run directory")
    parser.add_argument("--hard-seconds", type=float, default=300.0)
    parser.add_argument("--target", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20261015)
    parser.add_argument("--out", default="corpus/chvatal/paper-hard.txt")
    args = parser.parse_args()

    population = json.load(open(os.path.join(ROOT, "corpus/chvatal/population.json")))
    manifest, screen = load_screen(os.path.join(ROOT, args.screen))
    limit = float(manifest["timeout"])
    missing = [n["file"] for n in population["nodes"] if n["file"] not in screen]
    if missing:
        raise SystemExit(f"screen incomplete: {len(missing)} population nodes "
                         "have no result")

    excluded = set()
    for name in DEVELOPMENT_LISTS:
        for rel in open(os.path.join(ROOT, "corpus/chvatal", name)):
            if rel.strip():
                excluded.add(content_hash(os.path.join(ROOT, rel.strip())))

    hard, dev_hard = [], []
    for node in population["nodes"]:
        result = screen[node["file"]]
        seconds = result["time"] if result["solved"] else None
        if seconds is not None and seconds < args.hard_seconds:
            continue
        row = {"file": node["file"], "node": node["node"],
               "content_sha256": node["content_sha256"],
               "kissat_seconds": seconds,
               "kissat_verdict": result["verdict"]}
        (dev_hard if node["content_sha256"] in excluded else hard).append(row)

    hard.sort(key=lambda r: r["file"])
    chosen = hard
    if len(hard) > args.target:
        chosen = sorted(random.Random(args.seed).sample(hard, args.target),
                        key=lambda r: r["file"])
    with open(os.path.join(ROOT, args.out), "w") as target:
        target.writelines(r["file"] + "\n" for r in chosen)
    record = {
        "rule": (f"Kissat 4.0.4 time >= {args.hard_seconds:g}s or unsolved at "
                 f"{limit:g}s; development instances excluded by content hash; "
                 f"uniform sample of {args.target} with seed {args.seed} when "
                 "more remain"),
        "screen": args.screen, "screen_limit_seconds": limit,
        "population": len(population["nodes"]),
        "hard_total": len(hard) + len(dev_hard),
        "hard_excluded_development": len(dev_hard),
        "hard_eligible": len(hard), "selected": chosen,
        "development_lists": DEVELOPMENT_LISTS,
    }
    with open(os.path.join(ROOT, args.out.replace(".txt", ".json")), "w") as target:
        json.dump(record, target, indent=1)
        target.write("\n")
    print(f"population {len(population['nodes'])}; hard {len(hard) + len(dev_hard)}"
          f" (development {len(dev_hard)}, eligible {len(hard)}); selected "
          f"{len(chosen)}")
    for r in chosen:
        print(f"  {r['kissat_seconds'] if r['kissat_seconds'] else 'TO':>8} "
              f"{os.path.basename(r['file'])}")


if __name__ == "__main__":
    main()

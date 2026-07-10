#!/usr/bin/env python3
"""tools/bench/fetch.py — populate the benchmark instance cache.

Usage: fetch.py --tier B [--sources path]

Downloads the instances listed in `benchmarks/tiers/<tier>.sources`
(lines: `<gbd-hash> <filename>`) from the Global Benchmark Database
(benchmark-database.de) into `benchmarks/cache/tier<tier>/<filename>`.
The cache is untracked; the .sources files are frozen in git, so any
machine can rebuild the cache bit-for-bit (GBD is content-addressed).
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GBD_URL = "https://benchmark-database.de/file/{}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tier", required=True, choices="ABCD")
    ap.add_argument("--sources", default=None)
    args = ap.parse_args()
    sources = args.sources or os.path.join(
        ROOT, "benchmarks", "tiers", f"{args.tier}.sources")
    if not os.path.exists(sources):
        sys.exit(f"error: {sources} not found")
    cache = os.path.join(ROOT, "benchmarks", "cache", f"tier{args.tier}")
    os.makedirs(cache, exist_ok=True)

    entries = []
    with open(sources) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                h, fn = line.split()
                entries.append((h, fn))

    fetched = skipped = 0
    for h, fn in entries:
        dst = os.path.join(cache, fn)
        if os.path.exists(dst) and os.path.getsize(dst) > 0:
            skipped += 1
            continue
        print(f"fetching {fn} ({h}) ...", flush=True)
        # curl instead of urllib: system CA bundle just works everywhere
        subprocess.run(["curl", "-sSfL", "-o", dst, GBD_URL.format(h)],
                       check=True)
        fetched += 1
    print(f"fetch: {fetched} downloaded, {skipped} already cached -> {cache}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

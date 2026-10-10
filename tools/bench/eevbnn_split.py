#!/usr/bin/env python3
"""Write the fixed EEVBNN policy-development split.

Ranks whose remainder modulo five is 2 or 3 form the held-out set.  This
selects 20 of 50 entries across every family and both verdicts without using
solver runtime beyond the already-fixed corpus rank.
"""

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MANIFEST = os.path.join(ROOT, "corpus", "eevbnn", "manifest.json")
OUTPUT = os.path.join(ROOT, "corpus", "eevbnn", "split.json")


def main():
    with open(MANIFEST, encoding="utf-8") as source:
        instances = json.load(source)["instances"]
    validation = [row["candidate_id"] for row in instances
                  if row["rank"] % 5 in (2, 3)]
    train = [row["candidate_id"] for row in instances
             if row["rank"] % 5 not in (2, 3)]
    result = {
        "method": "manifest rank modulo 5; validation remainders are 2 and 3",
        "train_count": len(train),
        "validation_count": len(validation),
        "train": train,
        "validation": validation,
    }
    if len(train) != 30 or len(validation) != 20:
        raise SystemExit("expected a 30/20 split")
    with open(OUTPUT, "w", encoding="utf-8") as target:
        json.dump(result, target, indent=2, sort_keys=True)
        target.write("\n")
    print(OUTPUT)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Stage selected EEVBNN gzip instances as checker-compatible plain DIMACS."""

import argparse
import gzip
import json
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORPUS = os.path.join(ROOT, "corpus", "eevbnn")
SCREEN_RANKS = (4, 6, 9, 19, 29, 34)
LUCKY_RANKS = (29, 34, 44)


def selected(stage, rows, split):
    if stage == "screen":
        return [row for row in rows if row["rank"] in SCREEN_RANKS]
    if stage == "proof":
        return [row for row in rows if row["rank"] == 48]
    if stage in ("lucky", "input", "lock"):
        return [row for row in rows if row["rank"] in LUCKY_RANKS]
    if stage == "train":
        ids = set(split["train"])
        return [row for row in rows if row["candidate_id"] in ids]
    if stage == "validate":
        ids = set(split["validation"])
        return [row for row in rows if row["candidate_id"] in ids]
    if stage in ("validate-d", "validate-final"):
        ids = set(split["validation"])
        return [row for row in rows
                if row["candidate_id"] in ids and row["family"] == "d"]
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "proof", "lucky", "input", "lock",
                                          "train", "validate", "validate-d",
                                          "validate-final", "all"))
    args = parser.parse_args()
    with open(os.path.join(CORPUS, "manifest.json"), encoding="utf-8") as source:
        rows = json.load(source)["instances"]
    with open(os.path.join(CORPUS, "split.json"), encoding="utf-8") as source:
        split = json.load(source)
    rows = selected(args.stage, rows, split)
    output_dir = os.path.join(CORPUS, "plain")
    os.makedirs(output_dir, exist_ok=True)
    for index, row in enumerate(rows, 1):
        source_path = os.path.join(CORPUS, row["file"])
        output_path = os.path.join(output_dir,
                                   os.path.basename(row["file"])[:-3])
        if os.path.isfile(output_path) and os.path.getsize(output_path) == row["raw_bytes"]:
            print(f"[{index}/{len(rows)}] present {os.path.basename(output_path)}", flush=True)
            continue
        partial = output_path + f".partial.{os.getpid()}"
        print(f"[{index}/{len(rows)}] staging {os.path.basename(output_path)}", flush=True)
        try:
            with gzip.open(source_path, "rb") as source, open(partial, "wb") as target:
                shutil.copyfileobj(source, target, 8 * 1024 * 1024)
            if os.path.getsize(partial) != row["raw_bytes"]:
                raise RuntimeError(f"wrong staged size for {output_path}")
            os.replace(partial, output_path)
        finally:
            if os.path.exists(partial):
                os.unlink(partial)
    print(f"staged {len(rows)} instance(s) in {output_dir}")


if __name__ == "__main__":
    main()

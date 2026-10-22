#!/usr/bin/env python3
"""Map a deterministic global fuzz range onto one array shard."""

import argparse
from typing import Tuple


def shard_range(total: int, shards: int, shard: int) -> Tuple[int, int]:
    if total < 1:
        raise ValueError("total must be positive")
    if shards < 1 or shards > total:
        raise ValueError("shards must be between 1 and total")
    if shard < 0 or shard >= shards:
        raise ValueError("shard index out of range")
    start = total * shard // shards
    end = total * (shard + 1) // shards
    return start, end - start


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("total", type=int)
    parser.add_argument("shards", type=int)
    parser.add_argument("shard", type=int)
    args = parser.parse_args()
    start, count = shard_range(args.total, args.shards, args.shard)
    print(start, count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run the three expensive Lucky lifecycle solves concurrently."""

from concurrent.futures import ThreadPoolExecutor
import re
import subprocess
import sys
from typing import Dict, Tuple


MODES = ("eager", "deferred", "disabled")
RESULT = re.compile(
    r"^LATE_LUCKY_RESULT mode=(\w+) early=(\d+) late=(\d+) "
    r"decisions=(\d+) conflicts=(\d+) propagations=(\d+)$",
    re.MULTILINE,
)


def run_mode(executable: str, mode: str) -> Tuple[str, Dict[str, int], str]:
    process = subprocess.run(
        [executable, "--late-mode", mode],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if process.returncode:
        raise RuntimeError(
            f"Lucky {mode} subprocess exited {process.returncode}:\n{process.stdout}"
        )
    match = RESULT.search(process.stdout)
    if not match or match.group(1) != mode:
        raise RuntimeError(f"Lucky {mode} emitted no valid result:\n{process.stdout}")
    keys = ("early", "late", "decisions", "conflicts", "propagations")
    values = {key: int(value) for key, value in zip(keys, match.groups()[1:])}
    return mode, values, process.stdout


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} TEST_LUCKY")

    with ThreadPoolExecutor(max_workers=len(MODES)) as pool:
        completed = list(pool.map(lambda mode: run_mode(sys.argv[1], mode), MODES))

    results = {}
    for mode, values, output in completed:
        results[mode] = values
        print(output, end="" if output.endswith("\n") else "\n")

    trajectory = ("early", "late", "decisions", "conflicts", "propagations")
    for key in trajectory:
        if results["eager"][key] != results["deferred"][key]:
            raise RuntimeError(
                f"eager/deferred Lucky mismatch for {key}: "
                f"{results['eager'][key]} != {results['deferred'][key]}"
            )
    if results["disabled"]["early"] != 1 or results["disabled"]["late"] != 0:
        raise RuntimeError(f"invalid disabled Lucky counters: {results['disabled']}")

    print("test_lucky_parallel OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

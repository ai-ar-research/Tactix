#!/usr/bin/env python3
"""Grade deterministic fuzz shards as one monolithic campaign."""

import argparse
import json
import os
import sys

from shard import shard_range


def add_counts(total, values):
    for key, value in values.items():
        if not isinstance(value, int):
            raise ValueError(f"non-integer count {key}={value!r}")
        total[key] = total.get(key, 0) + value


def fail(message):
    print(f"fuzz aggregate: FAIL: {message}")
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--shards", required=True, type=int)
    parser.add_argument("--total", required=True, type=int)
    args = parser.parse_args()

    summaries = []
    for shard in range(args.shards):
        path = os.path.join(args.root, f"{args.prefix}{shard}",
                            "summary.json")
        try:
            with open(path, encoding="utf-8") as source:
                summary = json.load(source)
        except (OSError, ValueError) as error:
            return fail(f"cannot read shard {shard} summary: {error}")
        expected_offset, expected_count = shard_range(
            args.total, args.shards, shard)
        if summary.get("schema") != 1:
            return fail(f"shard {shard} has unsupported summary schema")
        if summary.get("range") != {"offset": expected_offset,
                                    "count": expected_count}:
            return fail(f"shard {shard} does not cover its assigned range")
        summaries.append(summary)

    runner = summaries[0].get("runner")
    generator = summaries[0].get("generator")
    requirements = summaries[0].get("requirements")
    if any(item.get("runner") != runner for item in summaries):
        return fail("shards used different fuzz runners")
    if any(item.get("generator") != generator for item in summaries):
        return fail("shards used different generator seeds")
    if any(item.get("requirements") != requirements for item in summaries):
        return fail("shards used different qualification requirements")

    counts = {}
    for item in summaries:
        try:
            add_counts(counts, item["counts"])
        except (KeyError, ValueError) as error:
            return fail(str(error))
    if counts.get("cases_run") != args.total and runner == "seeded":
        return fail(f"ran {counts.get('cases_run', 0)}/{args.total} cases")
    if counts.get("done") != args.total and runner != "seeded":
        return fail(f"ran {counts.get('done', 0)}/{args.total} cases")
    if counts.get("failures", 0):
        return fail(f"{counts['failures']} correctness failures")

    errors = []
    if runner == "seeded":
        reach_rules = {}
        reach_spec_seen = {}
        controller_fields = {}
        for item in summaries:
            add_counts(reach_rules, item.get("reach_rules", {}))
            add_counts(reach_spec_seen, item.get("reach_spec_seen", {}))
            for name, row in item.get("controller", {}).get("rule", {}).items():
                fields = controller_fields.setdefault(name, {})
                add_counts(fields, row.get("sum", row))
        if counts.get("stats_cases", 0):
            missing = [name for name, seen in reach_spec_seen.items()
                       if not seen]
            if missing:
                errors.append(f"reach-spec counters absent: {missing}")
        missing = [name for name in requirements["require_reached"]
                   if not reach_rules.get(name, 0)]
        if missing:
            errors.append(f"required rules never fired: {missing}")
        for field, key in (("applications", "require_controller_applied"),
                           ("effective", "require_controller_effective")):
            missing = [name for name in requirements[key]
                       if not controller_fields.get(name, {}).get(field, 0)]
            if missing:
                errors.append(f"controller {field} missing: {missing}")
        if counts.get("stress_target_fires", 0) < \
                requirements["minimum_target_fires"]:
            errors.append("stress target minimum not met")
        maximum = requirements["max_nonterminating_frac"]
        if maximum is not None and args.total and \
                counts.get("nonterminating", 0) / args.total > maximum:
            errors.append("non-terminating fraction exceeds maximum")
        if counts.get("effective", 0) < requirements["minimum_verdicts"]:
            errors.append("verdict minimum not met")
    elif runner == "controller-v5":
        if not counts.get("gf3_effective") or not counts.get("direct_effective"):
            errors.append("required typed action family never effective")
        if counts.get("SAT", 0) + counts.get("UNSAT", 0) < \
                requirements["minimum_verdicts"]:
            errors.append("verdict minimum not met")
    elif runner == "forced-ttf":
        if counts.get("forced_reached") != counts.get("forced_expected"):
            errors.append("forced-TTF reach incomplete")
        if not counts.get("negative_controls") or not counts.get("tight_cases"):
            errors.append("required control class missing")
        if counts.get("SAT", 0) + counts.get("UNSAT", 0) < \
                requirements["minimum_verdicts"]:
            errors.append("verdict minimum not met")
    else:
        return fail(f"unknown runner {runner!r}")

    if errors:
        return fail("; ".join(errors))
    print(f"fuzz aggregate: PASS: {runner}, {args.total} deterministic cases, "
          f"{args.shards} shards")
    return 0


if __name__ == "__main__":
    sys.exit(main())

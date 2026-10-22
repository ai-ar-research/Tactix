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
    parser.add_argument("--minimum-verdicts", type=int,
                        help="explicit regrade override; preserves saved summaries")
    parser.add_argument("--max-nonterminating-frac", type=float,
                        help="explicit regrade override; preserves saved summaries")
    parser.add_argument("--regrade-reason",
                        help="required explanation when overriding saved thresholds")
    args = parser.parse_args()
    overrides = {key: value for key, value in {
        "minimum_verdicts": args.minimum_verdicts,
        "max_nonterminating_frac": args.max_nonterminating_frac,
    }.items() if value is not None}
    if overrides and not (args.regrade_reason or "").strip():
        parser.error("threshold overrides require --regrade-reason")
    if args.minimum_verdicts is not None and args.minimum_verdicts < 0:
        parser.error("--minimum-verdicts must be nonnegative")
    if args.max_nonterminating_frac is not None and not (
            0 <= args.max_nonterminating_frac <= 1):
        parser.error("--max-nonterminating-frac must be between 0 and 1")

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
    if overrides:
        print("fuzz aggregate: REGRADE: " + json.dumps({
            "reason": args.regrade_reason,
            "original": {key: requirements[key] for key in overrides},
            "revised": overrides,
        }, sort_keys=True))
        requirements = {**requirements, **overrides}

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
        policy_fields = {}
        policy_counters = {}
        for item in summaries:
            add_counts(reach_rules, item.get("reach_rules", {}))
            add_counts(reach_spec_seen, item.get("reach_spec_seen", {}))
            for name, row in item.get("policy", {}).get("rule", {}).items():
                fields = policy_fields.setdefault(name, {})
                add_counts(fields, row.get("sum", row))
            add_counts(policy_counters,
                       item.get("policy", {}).get("loop", {}).get("sum", {}))
        if counts.get("stats_cases", 0):
            missing = [name for name, seen in reach_spec_seen.items()
                       if not seen]
            if missing:
                errors.append(f"reach-spec counters absent: {missing}")
        missing = [name for name in requirements["require_reached"]
                   if not reach_rules.get(name, 0)]
        if missing:
            errors.append(f"required rules never fired: {missing}")
        for field, key in (("applications", "require_policy_applied"),
                           ("effective", "require_policy_effective")):
            missing = [name for name in requirements[key]
                       if not policy_fields.get(name, {}).get(field, 0)]
            if missing:
                errors.append(f"random policy {field} missing: {missing}")
        missing = [name for name in requirements.get(
                       "require_policy_counter", [])
                   if not policy_counters.get(name, 0)]
        if missing:
            errors.append(f"random policy counters missing: {missing}")
        maximum = requirements["max_nonterminating_frac"]
        if maximum is not None and args.total and \
                counts.get("nonterminating", 0) / args.total > maximum:
            errors.append("non-terminating fraction exceeds maximum")
        if counts.get("effective", 0) < requirements["minimum_verdicts"]:
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

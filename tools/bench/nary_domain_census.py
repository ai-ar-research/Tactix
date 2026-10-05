#!/usr/bin/env python3
"""Summarize native NaryCspPrefix recognition and application on a campaign."""

import argparse
import csv
import json
from pathlib import Path
import re


NARY = re.compile(
    r"^c nary-csp\s+(\d+) attempts, (\d+) applied, (\d+) no-structure, "
    r"(\d+) no-consequence, (\d+) already-published, (\d+) invalid, "
    r"(\d+) inapplicable, (\d+) exhausted, (\d+) interrupted, (\d+) stale, "
    r"(\d+) unsupported-proof$")
FIELDS = ("attempts", "applied", "no_structure", "no_consequence",
          "already_published", "invalid", "inapplicable", "exhausted",
          "interrupted", "stale", "unsupported_proof")


def key_values(lines, prefix):
    selected = [line for line in lines if line.startswith(prefix)]
    if not selected:
        return None
    if len(selected) != 1:
        raise ValueError(f"duplicate statistics line: {prefix}")
    fields = {}
    for token in selected[0][len(prefix):].split():
        if "=" not in token:
            raise ValueError(f"malformed statistics token: {token}")
        name, value = token.split("=", 1)
        if not value.isdigit():
            raise ValueError(f"non-integer statistics token: {token}")
        fields[name] = int(value)
    return fields


def parse_statistics(lines):
    matches = [NARY.fullmatch(line) for line in lines]
    matches = [match for match in matches if match]
    if len(matches) > 1:
        raise ValueError("duplicate nary-csp result line")
    status = (dict(zip(FIELDS, map(int, matches[0].groups())))
              if matches else None)
    producer = key_values(lines, "c controller-producer NaryCspPrefix ")
    rule = key_values(lines, "c controller-rule NaryCspPrefix ")
    return {"status": status, "producer": producer, "rule": rule}


def disposition(parsed):
    producer, status = parsed["producer"], parsed["status"]
    if producer is None:
        return "producer-statistics-missing"
    if producer.get("calls", 0) == 0:
        return "producer-not-called"
    if producer.get("offers", 0) == 0:
        return "no-offer"
    if status is None or status["attempts"] == 0:
        return "offered-not-applied"
    for name in ("invalid", "inapplicable", "exhausted", "interrupted",
                 "stale", "unsupported_proof", "no_structure",
                 "no_consequence", "already_published"):
        if status[name]:
            return name.replace("_", "-")
    if status["applied"]:
        return "applied"
    return "unclassified"


def score(campaign):
    campaign = campaign.resolve()
    manifest = json.loads((campaign / "manifest.json").read_text())
    arm = manifest["arms"]["tactix"]
    arguments = arm["arguments"]
    required = {"--controller-native-producers=nary-csp",
                "--controller-families=NaryCspPrefix"}
    policies = [value for value in arguments
                if value.startswith("--controller-policy=")]
    if not required.issubset(arguments) or len(policies) != 1:
        raise ValueError("campaign is not the exact native n-ary census profile")
    with (campaign / "matrix.tsv").open(newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    output = []
    totals = {}
    for row in rows:
        task = int(row["task"])
        result_path = campaign / "results" / f"{task:03d}.json"
        if not result_path.is_file():
            output.append({"task": task, "rank": int(row["rank"]),
                           "instance": row["instance"], "disposition": "missing-result"})
            totals["missing-result"] = totals.get("missing-result", 0) + 1
            continue
        result = json.loads(result_path.read_text())
        parsed = parse_statistics(result.get("solver_statistics") or [])
        state = disposition(parsed)
        totals[state] = totals.get(state, 0) + 1
        output.append({"task": task, "rank": int(row["rank"]),
                       "instance": row["instance"], "verdict": result.get("verdict"),
                       "solve": result.get("solve"), "disposition": state, **parsed})
    return {"schema": "tactix-nary-domain-census-v1",
            "campaign": str(campaign), "rows": len(rows),
            "controller_policy": policies[0].split("=", 1)[1],
            "totals": dict(sorted(totals.items())), "results": output}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = score(args.campaign)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Validate checked, effective public-theory target diagnostics."""

import argparse
import csv
import json
from pathlib import Path
import re

import experimental_tierd as tierd


ROOT = Path(__file__).resolve().parents[2]
TARGET_ROOT = ROOT / "benchmarks/probes/public-theory-targets-v1"
PROFILES = {
    "gf3-wv100": {
        "family": "Gf3GaussianConsequence", "producer": "gf3-gaussian",
        "timeout": 1200,
    },
    "tseitin-wv100": {
        "family": "TseitinCycleBreaking", "producer": "tseitin-cycle",
        "timeout": 1200,
    },
    "tseitin-wv2": {
        "family": "TseitinCycleBreaking", "producer": "tseitin-cycle",
        "timeout": 600,
    },
    "direct-php-wv2": {
        "family": "DirectPhpRefutation", "producer": None,
        "timeout": 600,
    },
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def table_rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def counter_line(statistics, kind, family):
    prefix = f"c controller-{kind} {family} "
    lines = [line for line in statistics if line.startswith(prefix)]
    require(len(lines) == 1, f"expected one {kind} counter for {family}")
    fields = {}
    for item in lines[0][len(prefix):].split():
        require(re.fullmatch(r"[^=]+=[0-9]+", item) is not None,
                f"malformed controller counter: {item}")
        key, value = item.split("=", 1)
        fields[key] = int(value)
    return fields


def validate_result(profile, row, result):
    task = int(row["task"])
    prefix = f"task {task}: "
    require(result.get("task") == task, prefix + "task mismatch")
    require(result.get("instance") == row["instance"], prefix + "instance mismatch")
    require(result.get("rank") == int(row["rank"]), prefix + "rank mismatch")
    solve = result.get("solve") or {}
    require(not solve.get("timed_out"), prefix + "solver timeout")
    require(result.get("verdict") == row["expected"], prefix + "verdict mismatch")
    require(result.get("valid") is True, prefix + "runtime marked result invalid")
    if row["expected"] == "SAT":
        require((result.get("model_check") or {}).get("result") == "verified",
                prefix + "model was not independently verified")
    else:
        require((result.get("proof_check") or {}).get("result") == "verified",
                prefix + "proof was not independently verified")
    statistics = result.get("solver_statistics") or []
    rule = counter_line(statistics, "rule", profile["family"])
    require(rule.get("applications", 0) > 0, prefix + "action was not applied")
    require(rule.get("effective", 0) > 0, prefix + "action had no effective transition")
    producer = None
    if profile["producer"]:
        producer = counter_line(statistics, "producer", profile["family"])
        require(producer.get("calls", 0) > 0, prefix + "producer was not called")
        require(producer.get("offers", 0) > 0, prefix + "producer made no offer")
    return {"rule": rule, "producer": producer}


def validate_campaign(campaign):
    campaign = campaign.resolve()
    manifest = tierd.verify(campaign)
    authority = manifest.get("selection_authority") or {}
    authority_path = ROOT / authority.get("path", "")
    require(authority_path.parent == TARGET_ROOT, "selection is not a public-theory target table")
    key = authority_path.stem
    require(key in PROFILES, "unknown public-theory target profile")
    profile = PROFILES[key]
    require(authority_path.is_file(), "selection authority is missing")
    require(tierd.sha(authority_path) == authority.get("sha256"),
            "selection authority hash mismatch")
    resources = manifest.get("resource_contract") or {}
    require(resources.get("solve_timeout") == profile["timeout"], "solve timeout mismatch")
    require(resources.get("checker_timeout") == 1800, "checker timeout is not 1,800 seconds")
    require(resources.get("throttle") is None, "array throttle is forbidden")
    require(resources.get("node_constraint") is None, "node constraint is forbidden")
    arm = (manifest.get("arms") or {}).get("tactix") or {}
    arguments = arm.get("arguments") or []
    require(arm.get("certificate") == "dsr", "target diagnostic must use DSR")
    require(arguments and arguments[0] == "--dsr", "DSR argument is missing")
    require(any(value == "--controller-policy" or
                value.startswith("--controller-policy=") for value in arguments),
            "controller policy argument is missing")
    require(f"--controller-families={profile['family']}" in arguments,
            "exact controller family restriction is missing")
    require(not any(value.startswith("--controller-actions=") for value in arguments),
            "static controller payload suppresses native discovery")
    native = [value for value in arguments
              if value.startswith("--controller-native-producers=")]
    if profile["producer"]:
        require(native == [f"--controller-native-producers={profile['producer']}"],
                "exact native producer selection is missing")
    else:
        require(not native or native == ["--controller-native-producers=none"],
                "Direct PHP diagnostic enables an unrelated native producer")

    with (campaign / "matrix.tsv").open(newline="") as stream:
        matrix = list(csv.DictReader(stream, delimiter="\t"))
    selected = table_rows(authority_path)
    require(len(matrix) == len(selected), "matrix/table cardinality mismatch")
    selected_by_instance = {row["instance"]: row for row in selected}
    results = []
    totals = {"applications": 0, "effective": 0, "producer_calls": 0,
              "producer_offers": 0}
    for row in matrix:
        selected_row = selected_by_instance.get(row["instance"])
        require(selected_row is not None, "matrix contains an unselected instance")
        require(row["rank"] == selected_row["coverage_rank"], "coverage rank mismatch")
        require(row["expected"] == selected_row["verdict"], "expected verdict mismatch")
        path = campaign / "results" / f"{int(row['task']):03d}.json"
        require(path.is_file(), f"missing result: {path.name}")
        result = json.loads(path.read_text())
        coverage = validate_result(profile, row, result)
        totals["applications"] += coverage["rule"]["applications"]
        totals["effective"] += coverage["rule"]["effective"]
        if coverage["producer"]:
            totals["producer_calls"] += coverage["producer"]["calls"]
            totals["producer_offers"] += coverage["producer"]["offers"]
        results.append({"task": int(row["task"]), "instance": row["instance"],
                        "verdict": result["verdict"], **coverage})
    return {"profile": key, "campaign": str(campaign), "rows": len(results),
            "totals": totals, "results": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", nargs="+")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        summary = {"schema": "tactix-public-theory-target-score-v1",
                   "campaigns": [validate_campaign(Path(path))
                                 for path in args.campaign]}
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        parser.error(str(error))
    encoded = json.dumps(summary, indent=2) + "\n"
    if args.output:
        with args.output.open("x") as stream:
            stream.write(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()

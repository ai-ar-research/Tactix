#!/usr/bin/env python3
"""Validate and summarize policy-feedback v1 JSONL without calculating a reward."""
import argparse
from collections import Counter, defaultdict
import json
import math
import pathlib
import sys

STOP_NAMES = (
    "completed", "fixpoint", "no-change", "inapplicable", "budget",
    "interrupted", "conflict", "sat", "unsat", "invalid", "stale",
    "resource", "accounting", "restart", "error", "detached",
    "root-preparation", "strategy-change",
)
KIND_NAMES = ("rule", "resume-search", "search-service", "support")
WORK_NAMES = ("applications", "bve-ticks", "propagations", "sr-discovery-ticks")
UINT64_MAX = (1 << 64) - 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, label, maximum=UINT64_MAX):
    require(type(value) is int and 0 <= value <= maximum, f"invalid {label}")
    return value


def keys(value, expected, label, optional=""):
    required, allowed = set(expected.split()), set(optional.split())
    require(type(value) is dict and required <= set(value) <= required | allowed,
            f"invalid {label} fields")


def state(value):
    keys(value, "arena touch strategy", "state")
    for key, item in value.items():
        integer(item, key)


def native(value):
    if type(value) is list:
        for item in value:
            native(item)
    else:
        integer(value, "native measurement")


def outcome(value):
    keys(value, "status result budget_status unit work applications step instance "
         "progress discovery_progress discovery_entry_required discovery_exhausted "
         "selection_complete revision before after composition_overshoot resumed_search",
         "outcome")
    for key in ("before", "after"):
        state(value[key])
    for key in ("progress", "discovery_entry_required", "discovery_exhausted",
                "selection_complete", "resumed_search"):
        require(type(value[key]) is bool, f"invalid {key}")
    for key in ("status", "result", "budget_status", "unit", "work", "applications",
                "step", "instance", "discovery_progress", "revision", "composition_overshoot"):
        integer(value[key], key)
    integer(value["status"], "invocation status", 12)
    integer(value["result"], "rule result", 4)
    integer(value["budget_status"], "budget status", 5)
    integer(value["unit"], "work unit", 3)


def observation(value):
    keys(value, "version state sequence variables unassigned active_unassigned level trail "
         "root_population irredundant_clauses inconsistent conflict fixpoint searching "
         "have_configuration configuration rotation_intervals next_restart_conflicts "
         "reluctant fast_glue slow_glue last_action native", "observation",
         optional="native_policy_state semantic_variable_candidates")
    if "native_policy_state" in value:
        require(type(value["native_policy_state"]) is str, "invalid native policy state")
    require(integer(value["version"], "observation version") == 1,
            "unsupported observation version")
    if "semantic_variable_candidates" in value:
        candidates = value["semantic_variable_candidates"]
        require(type(candidates) is list, "invalid semantic variable candidates")
        ids = set()
        for candidate in candidates:
            keys(candidate, "id action_id arena_generation touch_epoch strategy_epoch "
                 "producer existing_variables introduced_variables",
                 "semantic variable candidate")
            for key in ("id", "action_id", "arena_generation", "touch_epoch",
                        "strategy_epoch", "producer", "existing_variables",
                        "introduced_variables"):
                integer(candidate[key], "semantic variable candidate " + key)
            identity = candidate["id"]
            require(identity >> 8 and identity not in ids,
                    "invalid/repeated semantic variable candidate identity")
            ids.add(identity)
            integer(candidate["producer"], "semantic variable producer", 3)
            require(candidate["existing_variables"] +
                    candidate["introduced_variables"] > 0,
                    "empty semantic variable candidate")
    state(value["state"])
    for key in ("sequence", "variables", "unassigned", "active_unassigned", "level", "trail",
                "root_population", "irredundant_clauses", "rotation_intervals",
                "next_restart_conflicts"):
        integer(value[key], key)
    for key in ("inconsistent", "conflict", "fixpoint", "searching", "have_configuration"):
        require(type(value[key]) is bool, f"invalid {key}")
    require(type(value["configuration"]) is str, "invalid configuration")
    require(type(value["reluctant"]) is list and len(value["reluctant"]) == 3,
            "invalid reluctant clock")
    for item in value["reluctant"]:
        integer(item, "reluctant clock")
    for key in ("fast_glue", "slow_glue"):
        require(type(value[key]) in (int, float) and math.isfinite(value[key])
                and value[key] >= 0, f"invalid {key}")
    if value["last_action"] is not None:
        last = value["last_action"]
        keys(last, "sequence type before after propagation_ticks root_assignments outcome",
             "last action")
        integer(last["sequence"], "last sequence")
        integer(last["type"], "action type", 1)
        state(last["before"])
        state(last["after"])
        integer(last["propagation_ticks"], "propagation ticks")
        integer(last["root_assignments"], "root assignments")
        outcome(last["outcome"])
    require(type(value["native"]) is dict and
            {"decisions", "conflicts", "root_assignments"} <= value["native"].keys(),
            "missing native measurements")
    for key, item in value["native"].items():
        require(not key.endswith("_nanos"), "native timing in deterministic snapshot")
        native(item)


def object_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON field {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"non-finite JSON constant {value}")


def inspect(path):
    actions = defaultdict(lambda: {"records": 0, "applications": 0, "progress": 0,
                                  "work_by_native_unit": Counter(), "stop": Counter(),
                                  "overshoot_by_native_unit": Counter(), "kinds": Counter(),
                                  "duration_nanos": 0})
    intervals = Counter()
    action_sequence = interval_sequence = 0
    pending = None
    begun = ended = False
    header = terminal = None
    interval_nanos = 0
    lines = 0
    with pathlib.Path(path).open() as source:
        for lines, line in enumerate(source, 1):
            try:
                row = json.loads(line, object_pairs_hook=object_pairs,
                                 parse_constant=reject_constant)
                require(type(row) is dict, "record is not an object")
                kind = row.get("type")
                require(not ended, "record after session end")
                if pending is not None:
                    keys(row, "type value", "elapsed record")
                    require(kind == "elapsed_nanos", "missing elapsed record")
                    elapsed = integer(row["value"], "elapsed nanos")
                    if pending[0] == "action":
                        actions[pending[1]]["duration_nanos"] += elapsed
                    else:
                        interval_nanos += elapsed
                    pending = None
                    continue
                if header is None:
                    keys(row, "type version metadata", "header")
                    require(kind == "header" and integer(row["version"], "header version") == 1,
                            "unsupported header")
                    require(type(row["metadata"]) is str, "invalid provenance metadata")
                    header = row
                    continue
                if not begun:
                    keys(row, "type observation", "session begin")
                    require(kind == "session_begin", "missing session begin")
                    observation(row["observation"])
                    begun = True
                    continue
                if kind == "session_end":
                    keys(row, "type verdict stop observation", "session end")
                    require(type(row["verdict"]) is int and row["verdict"] in (-1, 0, 10, 20),
                            "invalid verdict")
                    integer(row["stop"], "stop reason", len(STOP_NAMES) - 1)
                    observation(row["observation"])
                    terminal = {"verdict": row["verdict"], "stop": STOP_NAMES[row["stop"]]}
                    ended = True
                    continue
                require(kind in ("action", "interval"), "unexpected record type")
                require(integer(row.get("version"), "record version") == 1,
                        "unsupported record version")
                observation(row["before"])
                observation(row["after"])
                integer(row["stop"], "stop reason", len(STOP_NAMES) - 1)
                if kind == "interval":
                    keys(row, "type version sequence preceding start stop before after", "interval")
                    interval_sequence += 1
                    require(integer(row["sequence"], "interval sequence") == interval_sequence,
                            "interval sequence gap")
                    integer(row["start"], "interval start", 2)
                    integer(row["preceding"], "preceding interval")
                    require(row["preceding"] == (interval_sequence - 1 if row["start"] else 0)
                            and (not row["start"] or interval_sequence > 1),
                            "invalid continuation link")
                    intervals[("resume-search", "partial-continuation",
                               "native-continuation")[row["start"]]] += 1
                    pending = (kind, None)
                    continue
                keys(row, "type version sequence kind identity instance request allowance "
                     "before after outcome stop", "action", optional="native_request")
                action_sequence += 1
                require(integer(row["sequence"], "action sequence") == action_sequence,
                        "action sequence gap")
                integer(row["kind"], "action kind", len(KIND_NAMES) - 1)
                integer(row["instance"], "instance")
                require(type(row["identity"]) is str and row["identity"], "missing action identity")
                if row["request"] is not None:
                    req = row["request"]
                    keys(req, "version sequence type expected search search_components invocation", "request")
                    # A v1 record must preserve malformed requests that the
                    # native controller rejected, including unknown versions.
                    integer(req["version"], "request version", (1 << 32) - 1)
                    integer(req["sequence"], "request sequence")
                    integer(req["type"], "request type", 255)
                    state(req["expected"])
                    require(type(req["search"]) is str and type(req["invocation"]) is str,
                            "invalid request payload")
                    require(type(req["search_components"]) is list and
                            len(req["search_components"]) == 3, "invalid search components")
                    for component in req["search_components"]:
                        integer(component, "raw search component", 255)
                if row["allowance"] is not None:
                    keys(row["allowance"], "unit amount native_mode native_amount", "allowance")
                    for key, item in row["allowance"].items():
                        integer(item, key)
                    integer(row["allowance"]["unit"], "raw allowance unit", 255)
                    integer(row["allowance"]["native_mode"], "raw native budget mode", 255)
                outcome(row["outcome"])
                result = row["outcome"]
                require(result["before"] == row["before"]["state"] and
                        result["after"] == row["after"]["state"], "outcome state mismatch")
                entry = actions[row["identity"]]
                entry["records"] += 1
                entry["applications"] += result["applications"]
                entry["progress"] += result["progress"]
                unit = WORK_NAMES[result["unit"]]
                work, overshoot = result["work"], result["composition_overshoot"]
                if "native_request" in row:
                    desc = row["native_request"]
                    keys(desc, "version description invocation currency unlimited amount work "
                         "overshoot accounting_overflow", "native request")
                    require(integer(desc["version"], "native request version") == 1,
                            "unsupported native request version")
                    for key in ("description", "currency"):
                        require(type(desc[key]) is str, f"invalid native {key}")
                    require(desc["currency"], "missing native currency")
                    require(desc["invocation"] is None or type(desc["invocation"]) is str,
                            "invalid native invocation")
                    for key in ("unlimited", "accounting_overflow"):
                        require(type(desc[key]) is bool, f"invalid native {key}")
                    for key in ("amount", "work", "overshoot"):
                        integer(desc[key], f"native {key}")
                    require(not desc["unlimited"] or desc["amount"] == 0,
                            "invalid unlimited native allowance")
                    require(desc["overshoot"] == (0 if desc["unlimited"] else
                            max(desc["work"] - desc["amount"], 0)), "native overshoot mismatch")
                    require(not desc["accounting_overflow"] or desc["work"] == 0,
                            "invalid overflow measurement")
                    unit, work, overshoot = desc["currency"], desc["work"], desc["overshoot"]
                entry["work_by_native_unit"][unit] += work
                entry["overshoot_by_native_unit"][unit] += overshoot
                entry["kinds"][KIND_NAMES[row["kind"]]] += 1
                entry["stop"][STOP_NAMES[row["stop"]]] += 1
                pending = (kind, row["identity"])
            except (ValueError, KeyError, TypeError, OverflowError) as error:
                raise ValueError(f"line {lines}: {error}") from error
    require(ended and pending is None, "incomplete feedback session")
    return {"version": 1, "metadata": header["metadata"], "lines": lines,
            "actions": dict(sorted(actions.items())), "intervals": dict(intervals),
            "interval_duration_nanos": interval_nanos, "terminal": terminal}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("records", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        summary = inspect(args.records)
    except (OSError, ValueError) as error:
        print(f"feedback inspection failed: {error}", file=sys.stderr)
        return 1
    report = json.dumps(summary, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(report)
    else:
        print(report, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())

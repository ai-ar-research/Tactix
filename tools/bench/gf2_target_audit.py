#!/usr/bin/env python3
"""Fail-closed supplemental diagnosis for GF(2) target campaigns.

This tool does not score or accept a target.  It keeps the frozen
``gf2_target_score.py`` classification intact, then joins that classification
with the runtime/checker disposition and independently cross-checks the
feedback terminal and X9 transaction counts.  A damaged or incomplete
telemetry chain produces an explicitly untrusted diagnostic rather than a
zero-yield claim.
"""

import argparse
import json
from pathlib import Path

from gf2_target_score import (
    _cross_check,
    _verify_descriptor,
    bounded_jsonl,
    classify,
    independent_checked,
    matrix_rows,
    object_pairs,
    parse_feedback,
    parse_provenance,
    raw_yield,
    reject_constant,
    validate_result,
)


SCHEMA = "tactix-experimental-gf2-target-audit-v1"

PRODUCER_STATUS_NAMES = {
    0: "complete",
    1: "no-structure",
    2: "inapplicable",
    3: "resource-exhausted",
    4: "interrupted",
    5: "stale",
    6: "invalid",
}

INVOCATION_STATUS_NAMES = {
    0: "completed",
    1: "scoped-fixpoint",
    2: "no-change",
    3: "inapplicable",
    4: "budget-exhausted",
    5: "interrupted",
    6: "conflict",
    7: "sat",
    8: "unsat",
    9: "invalid-request",
    10: "stale-request",
    11: "resource-exhausted",
    12: "accounting-error",
}

STOP_REASON_NAMES = {
    0: "completed",
    1: "fixpoint",
    2: "no-change",
    3: "inapplicable",
    4: "budget",
    5: "interrupted",
    6: "conflict",
    7: "sat",
    8: "unsat",
    9: "invalid",
    10: "stale",
    11: "resource",
    12: "accounting",
    13: "restart",
    14: "error",
    15: "detached",
    16: "root-preparation",
    17: "strategy-change",
}

RUNTIME_VERDICTS = {"SAT": 10, "UNSAT": 20, "UNKNOWN": 0}


def strict_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=object_pairs,
                      parse_constant=reject_constant)


def issue(code, message):
    return {"code": code, "message": str(message)}


def named_histogram(histogram, names):
    if type(histogram) is not dict:
        raise ValueError("status histogram is not an object")
    result = {}
    for raw, amount in histogram.items():
        if type(raw) is not str or not raw.isdigit() or type(amount) is not int or amount < 0:
            raise ValueError("invalid status histogram")
        number = int(raw)
        result[names.get(number, f"unknown-{number}")] = amount
    return dict(sorted(result.items()))


def solve_diagnosis(result):
    solve = result.get("solve")
    verdict = result.get("verdict")
    expected = result.get("expected")
    if type(solve) is not dict:
        return {
            "disposition": "solve-record-missing",
            "verdict": verdict,
            "expected": expected,
        }
    timed_out = solve.get("timed_out") is True
    exit_code = solve.get("exit_code")
    expected_exit = 10 if verdict == "SAT" else 20 if verdict == "UNSAT" else None
    if timed_out:
        disposition = "solver-timeout"
    elif verdict not in ("SAT", "UNSAT"):
        disposition = "solver-unknown"
    elif exit_code != expected_exit:
        disposition = "solver-exit-mismatch"
    elif result.get("expected_match") is False or expected not in (verdict, "UNKNOWN"):
        disposition = "dataset-verdict-mismatch"
    else:
        disposition = "solver-sat" if verdict == "SAT" else "solver-unsat"
    return {
        "disposition": disposition,
        "verdict": verdict,
        "expected": expected,
        "exit_code": exit_code,
        "expected_exit_code": expected_exit,
        "timed_out": timed_out,
        "wall_seconds": solve.get("wall_seconds"),
    }


def check_diagnosis(result, solve):
    verdict = result.get("verdict")
    kind = "model" if verdict == "SAT" else "proof" if verdict == "UNSAT" else None
    check = result.get("model_check" if kind == "model" else "proof_check") if kind else None
    if solve["disposition"] == "solver-timeout":
        disposition = "not-run-after-solver-timeout"
    elif kind is None:
        disposition = "not-run-without-verdict"
    elif type(check) is not dict:
        disposition = "checker-record-missing"
    elif check.get("timed_out") is True or check.get("deadline_expired") is True:
        disposition = "checker-timeout"
    elif check.get("result") != "verified":
        disposition = "checker-failure"
    elif result.get("valid") is not True:
        disposition = "verified-but-runtime-invalid"
    else:
        disposition = "checked-sat" if verdict == "SAT" else "checked-unsat"

    stages = []
    if type(check) is dict:
        for stage in check.get("stages", []):
            if type(stage) is dict and (stage.get("result") != "verified" or
                                        stage.get("timed_out") is True):
                stages.append({name: stage.get(name) for name in
                               ("name", "result", "exit_code", "timed_out")})
    return {
        "kind": kind,
        "disposition": disposition,
        "result": check.get("result") if type(check) is dict else None,
        "timed_out": check.get("timed_out") if type(check) is dict else None,
        "deadline_expired": (check.get("deadline_expired")
                             if type(check) is dict else None),
        "pipeline": check.get("pipeline") if type(check) is dict else None,
        "failed_stages": stages,
    }


def telemetry_surface_disposition(result, campaign):
    telemetry = result.get("gf2_telemetry")
    if type(telemetry) is not dict:
        return "missing-telemetry"
    integrity = telemetry.get("integrity_error")
    if integrity:
        if "byte cap exceeded" in str(integrity):
            return "cap-loss"
        if "No such file" in str(integrity) or not telemetry.get("sidecars"):
            return "missing-sidecars"
        return "runtime-integrity-error"
    sidecars = telemetry.get("sidecars")
    if type(sidecars) is not dict or not {
            "feedback", "structural_provenance"} <= set(sidecars):
        return "missing-sidecars"
    for descriptor in sidecars.values():
        if type(descriptor) is not dict or type(descriptor.get("path")) is not str:
            return "missing-sidecars"
        path = (Path(campaign) / descriptor["path"]).resolve()
        if not path.is_file():
            return "missing-sidecars"
    feedback = telemetry.get("feedback")
    provenance = telemetry.get("structural_provenance")
    if (type(feedback) is dict and feedback.get("complete") is False) or (
            type(provenance) is dict and provenance.get("complete") is False):
        return "incomplete-telemetry"
    return "present"


def installed_empty_events(records):
    events = []
    for row in records[1:]:
        if (row.get("kind") in ("affine-basis-proof-rup",
                                "affine-basis-proof-rat") and
                row.get("install") is True and row.get("clause") == []):
            events.append({name: row.get(name) for name in
                           ("action", "semantic", "owner", "kind", "id")})
    return events


def joined_diagnosis(result, feedback, provenance, raw, provenance_records):
    problems = []
    runtime_verdict = RUNTIME_VERDICTS.get(result.get("verdict"))
    terminal = feedback.get("terminal")
    if type(terminal) is not dict:
        problems.append(issue("feedback-terminal-missing",
                              "complete feedback has no terminal record"))
    elif runtime_verdict is None or terminal.get("verdict") != runtime_verdict:
        problems.append(issue(
            "feedback-result-verdict-mismatch",
            f"feedback verdict {terminal.get('verdict')} != runtime verdict "
            f"{result.get('verdict')!r}"))

    native = feedback["action"]["native"]
    applied = native["affine_consequence_applied"]
    if provenance["actions"] != applied:
        problems.append(issue(
            "provenance-action-count-mismatch",
            f"provenance actions {provenance['actions']} != native applied {applied}"))

    empty_events = installed_empty_events(provenance_records)
    if len(empty_events) > provenance["actions"]:
        problems.append(issue("empty-event-count-mismatch",
                              "more installed empty events than X9 transactions"))

    producer_statuses = named_histogram(
        feedback["producer"]["statuses"], PRODUCER_STATUS_NAMES)
    action_statuses = named_histogram(
        feedback["action"]["statuses"], INVOCATION_STATUS_NAMES)
    native_statuses = {
        name.removeprefix("affine_consequence_"): amount
        for name, amount in native.items()
        if name in {
            "affine_consequence_attempts", "affine_consequence_applied",
            "affine_consequence_no_structure", "affine_consequence_invalid",
            "affine_consequence_inapplicable",
            "affine_consequence_resource_exhausted",
            "affine_consequence_interrupted",
            "affine_consequence_unsupported_proof", "affine_consequence_stale",
            "affine_consequence_already_published",
        } and amount
    }

    solve = solve_diagnosis(result)
    check = check_diagnosis(result, solve)
    terminal_refutation = bool(
        not problems and result.get("verdict") == "UNSAT" and
        solve["disposition"] == "solver-unsat" and
        type(terminal) is dict and terminal.get("verdict") == 20 and
        terminal.get("stop") == 8 and applied > 0 and
        provenance["actions"] > 0 and empty_events and
        native["affine_consequence_published"] >= len(empty_events))

    frozen = classify(raw, independent_checked(result))
    if terminal_refutation:
        supplemental = ("checked-terminal-x9-refutation"
                        if check["disposition"] == "checked-unsat"
                        else "unchecked-terminal-x9-refutation")
    elif raw["action"]["effective"] > 0:
        supplemental = ("checked-after-x9-publication"
                        if check["disposition"] in ("checked-sat", "checked-unsat")
                        else "unchecked-after-x9-publication")
    else:
        supplemental = frozen

    return {
        "issues": problems,
        "solve": solve,
        "check": check,
        "producer_action": {
            "frozen_classification": frozen,
            "supplemental_classification": supplemental,
            "producer_statuses": producer_statuses,
            "action_statuses": action_statuses,
            "native_statuses": dict(sorted(native_statuses.items())),
            "raw_yield": raw,
        },
        "feedback_terminal": {
            "verdict": terminal.get("verdict") if type(terminal) is dict else None,
            "stop": terminal.get("stop") if type(terminal) is dict else None,
            "stop_name": (STOP_REASON_NAMES.get(terminal.get("stop"),
                                                f"unknown-{terminal.get('stop')}")
                          if type(terminal) is dict else None),
        },
        "provenance": {
            "actions": provenance["actions"],
            "native_applied": applied,
            "installed_empty_events": empty_events,
        },
        "terminal_x9_refutation": {
            "recognized": terminal_refutation,
            "independently_checked": (terminal_refutation and
                                      check["disposition"] == "checked-unsat"),
        },
    }


def audit_result(campaign, result_path):
    campaign = Path(campaign).resolve()
    result_path = Path(result_path).resolve()
    report = {
        "schema": SCHEMA,
        "campaign": str(campaign),
        "result": str(result_path),
        "diagnostic_only": True,
        "evidence_complete": False,
        "issues": [],
    }
    try:
        result = strict_json(result_path)
    except (OSError, TypeError, ValueError) as error:
        report["issues"].append(issue("result-unreadable", error))
        report["telemetry_disposition"] = "unavailable"
        return report

    report.update(task=result.get("task"), instance=result.get("instance"),
                  solve=solve_diagnosis(result))
    report["check"] = check_diagnosis(result, report["solve"])
    report["telemetry_disposition"] = telemetry_surface_disposition(result, campaign)

    try:
        frozen = validate_result(campaign, result_path)
    except (OSError, KeyError, TypeError, ValueError) as error:
        report["issues"].append(issue("frozen-validation-failed", error))
        return report

    try:
        manifest = strict_json(campaign / "manifest.json")
        contract = manifest["gf2_telemetry"]
        telemetry = result["gf2_telemetry"]
        feedback_path = _verify_descriptor(
            campaign, telemetry["sidecars"]["feedback"],
            contract["feedback_max_bytes"])
        provenance_path = _verify_descriptor(
            campaign, telemetry["sidecars"]["structural_provenance"],
            contract["structural_provenance_max_bytes"])
        feedback = parse_feedback(feedback_path, contract["feedback_max_bytes"])
        provenance = parse_provenance(
            provenance_path, contract["structural_provenance_max_bytes"])
        _, provenance_records, terminated = bounded_jsonl(
            provenance_path, contract["structural_provenance_max_bytes"])
        if not terminated:
            raise ValueError("structural provenance is not newline terminated")
        raw = raw_yield(feedback, provenance, result.get("solver_statistics"))
        _cross_check(feedback, provenance, raw)
        joined = joined_diagnosis(
            result, feedback, provenance, raw, provenance_records)
        report.update(joined)
        report["frozen_score_row"] = frozen
        if joined["issues"]:
            report["issues"] = joined["issues"]
            report["telemetry_disposition"] = "supplemental-validation-failed"
        else:
            report["evidence_complete"] = True
            report["telemetry_disposition"] = "complete"
    except (OSError, KeyError, TypeError, ValueError) as error:
        report["issues"].append(issue("supplemental-validation-failed", error))
    return report


def audit_campaign(campaign):
    campaign = Path(campaign).resolve()
    report = {
        "schema": SCHEMA,
        "campaign": str(campaign),
        "diagnostic_only": True,
        "evidence_complete": False,
        "issues": [],
        "rows": [],
    }
    try:
        rows = matrix_rows(campaign)
    except (OSError, KeyError, TypeError, ValueError) as error:
        report["issues"].append(issue("matrix-unreadable", error))
        return report
    expected = {f"{task:03d}.json" for task in rows}
    results = campaign / "results"
    present = {path.name for path in results.glob("*.json")}
    for task in rows:
        path = results / f"{task:03d}.json"
        if path.is_file():
            report["rows"].append(audit_result(campaign, path))
        else:
            report["rows"].append({
                "schema": SCHEMA,
                "task": task,
                "instance": rows[task]["instance"],
                "diagnostic_only": True,
                "evidence_complete": False,
                "telemetry_disposition": "result-missing",
                "issues": [issue("result-missing", path)],
            })
    extras = sorted(present - expected)
    if extras:
        report["issues"].append(issue("extra-results", extras))
    report["evidence_complete"] = (
        not report["issues"] and report["rows"] and
        all(row["evidence_complete"] for row in report["rows"]))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path)
    parser.add_argument("--result", type=Path,
                        help="audit one result instead of the whole matrix")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = (audit_result(args.campaign, args.result) if args.result else
              audit_campaign(args.campaign))
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    return 0 if report["evidence_complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

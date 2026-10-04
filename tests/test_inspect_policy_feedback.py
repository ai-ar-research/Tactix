#!/usr/bin/env python3
"""Inspect native-generated feedback and reject malformed/incomplete evidence."""
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "feedback_inspector", ROOT / "tools/bench/inspect_policy_feedback.py")
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


def main():
    with tempfile.TemporaryDirectory(prefix="tactix-feedback-inspector-") as directory:
        path = pathlib.Path(directory) / "records.jsonl"
        subprocess.run([sys.argv[1], str(path)], check=True, timeout=30,
                       stdout=subprocess.DEVNULL)
        original = path.read_text()
        rows = [json.loads(line) for line in original.splitlines()]
        summary = inspector.inspect(path)
        assert summary["terminal"] == {"verdict": 20, "stop": "unsat"}
        assert summary["actions"]["ConflictAnalysis"]["records"] > 0
        assert summary["actions"]["Decide"]["applications"] > 0
        assert summary["intervals"]["resume-search"] > 0
        assert "fixture\"\\\n\t" in summary["metadata"]
        output = pathlib.Path(directory) / "summary.json"
        subprocess.run([sys.executable, str(ROOT / "tools/bench/inspect_policy_feedback.py"),
                        str(path), "--output", str(output)], check=True)
        assert json.loads(output.read_text()) == summary

        def rejected(text, expected):
            path.write_text(text)
            try:
                inspector.inspect(path)
            except ValueError as error:
                assert expected in str(error), (expected, str(error))
            else:
                raise AssertionError(f"accepted corrupt evidence: {expected}")

        def encode(changed):
            return "".join(json.dumps(row) + "\n" for row in changed)

        rejected(encode(rows[:-1]), "incomplete")
        rejected(original + "{}\n", "after session end")
        action = next(i for i, row in enumerate(rows) if row["type"] == "action")
        changed = json.loads(json.dumps(rows))
        changed[action]["sequence"] += 1
        rejected(encode(changed), "sequence gap")
        changed = json.loads(json.dumps(rows))
        changed[action]["outcome"]["after"]["strategy"] += 1
        rejected(encode(changed), "state mismatch")
        changed = json.loads(json.dumps(rows))
        changed[action + 1]["value"] = -1
        rejected(encode(changed), "elapsed nanos")
        changed = json.loads(json.dumps(rows))
        del changed[action]["before"]["local_search_candidates"]
        rejected(encode(changed), "missing candidate observation")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["local_search_candidates"] = [{
            "id": 0, "formula_touch_epoch": 0, "formula_root_assignments": 0,
            "external_variables": 2, "objective": 0, "objective_exact": True,
            "effort": 1, "effort_unit": 0}]
        rejected(encode(changed), "candidate identity")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["semantic_variable_candidates"] = [{
            "id": 0, "action_id": 1, "arena_generation": 0,
            "touch_epoch": 0, "strategy_epoch": 0, "producer": 0,
            "existing_variables": 1, "introduced_variables": 0}]
        rejected(encode(changed), "semantic variable candidate identity")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["native"]["solve_nanos"] = 1
        rejected(encode(changed), "native timing")
        rejected(original.replace('"type":"header"', '"type":"header","type":"header"', 1),
                 "duplicate JSON field")
        changed = rows[:action + 1] + rows[action + 2:]
        rejected(encode(changed), "elapsed record")
        changed = json.loads(json.dumps(rows))
        changed[action]["outcome"]["status"] = 99
        rejected(encode(changed), "invocation status")
        changed = json.loads(json.dumps(rows))
        changed[0]["version"] = 2
        rejected(encode(changed), "unsupported header")
        changed[0]["version"] = True
        rejected(encode(changed), "header version")
        changed = json.loads(json.dumps(rows))
        changed[action]["sequence"] = True
        rejected(encode(changed), "action sequence")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["fast_glue"] = float("inf")
        rejected(encode(changed).replace("Infinity", "1e999"), "fast_glue")
        subprocess.run([sys.argv[1], str(path), "invalid"], check=True, timeout=30,
                       stdout=subprocess.DEVNULL)
        rejected_summary = inspector.inspect(path)
        assert rejected_summary["actions"]["ResumeSearch"]["stop"] == {"invalid": 1}
        rejected_rows = [json.loads(line) for line in path.read_text().splitlines()]
        bad_request = rejected_rows[2]["request"]
        assert bad_request["version"] == 7 and bad_request["search_components"][0] == 255

        subprocess.run([sys.argv[2], directory, "feedback"], check=True, timeout=30,
                       stdout=subprocess.DEVNULL)
        default_records = sorted(pathlib.Path(directory).glob("profile-*.feedback.jsonl"))
        expected_records = {
            f"profile-{profile}-pigeons-{pigeons}.feedback.jsonl"
            for profile in range(7) for pigeons in (4, 6)
        }
        assert {record.name for record in default_records} == expected_records
        for default_path in default_records:
            default_summary = inspector.inspect(default_path)
            assert default_summary["terminal"]["verdict"] in (10, 20)
        default_rows = [json.loads(line) for line in default_records[-1].read_text().splitlines()]
        native_action = next(i for i, row in enumerate(default_rows)
                             if row["type"] == "action" and "native_request" in row)
        changed = json.loads(json.dumps(default_rows))
        changed[native_action]["native_request"]["overshoot"] += 1
        rejected(encode(changed), "native overshoot mismatch")
        changed = json.loads(json.dumps(default_rows))
        changed[native_action]["native_request"]["unlimited"] = 1
        rejected(encode(changed), "native unlimited")
        changed = json.loads(json.dumps(default_rows))
        changed[native_action]["native_request"]["version"] = 2
        rejected(encode(changed), "unsupported native request version")
        changed = json.loads(json.dumps(default_rows))
        changed[native_action]["before"]["native_policy_state"] = 7
        rejected(encode(changed), "native policy state")
    print("native feedback inspection and corruption controls passed")


if __name__ == "__main__":
    main()

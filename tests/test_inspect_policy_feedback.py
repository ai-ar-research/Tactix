#!/usr/bin/env python3
"""Inspect native-generated feedback and reject malformed/incomplete evidence."""
import importlib.util
import json
import pathlib
import re
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
        v2_original = path.read_text()
        v2_rows = [json.loads(line) for line in v2_original.splitlines()]
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

        def legacy_rows(encoded_rows):
            """Expand canonical v2 deltas into an equivalent v1 inspector fixture."""
            expanded = json.loads(json.dumps(encoded_rows))
            assert expanded[0]["version"] == 2
            expanded[0]["version"] = 1
            current = expanded[1]["observation"]
            for row in expanded[2:]:
                if row["type"] in ("action", "interval"):
                    before = inspector.observation_delta(current, row.pop("before_delta"))
                    after = inspector.observation_delta(before, row.pop("after_delta"))
                    row["before"], row["after"] = before, after
                    row["version"] = 1
                    current = after
                elif row["type"] == "session_end":
                    current = inspector.observation_delta(
                        current, row.pop("observation_delta"))
                    row["observation"] = current
            return expanded

        rows = legacy_rows(v2_rows)
        original = encode(rows)
        path.write_text(original)

        v2_action = next(i for i, row in enumerate(v2_rows)
                         if row["type"] == "action")
        changed = json.loads(json.dumps(v2_rows))
        changed[v2_action]["after_delta"]["set"]["unknown"] = 1
        rejected(encode(changed), "unknown observation delta replacement")
        changed = json.loads(json.dumps(v2_rows))
        changed[v2_action]["after_delta"]["clear"] = ["last_action"]
        rejected(encode(changed), "invalid observation delta clear list")
        arrays = changed = json.loads(json.dumps(v2_rows))
        array_action = next(i for i, row in enumerate(arrays)
                            if row.get("after_delta", {}).get("native_arrays"))
        field = next(iter(changed[array_action]["after_delta"]["native_arrays"]))
        changed[array_action]["after_delta"]["native_arrays"][field][0][0] = 1 << 30
        rejected(encode(changed), "native array index order")

        compressed = pathlib.Path(directory) / "records.jsonl.gz"
        subprocess.run([sys.argv[1], str(compressed)], check=True, timeout=30,
                       stdout=subprocess.DEVNULL)
        assert compressed.read_bytes().startswith(b"\x1f\x8b")
        assert inspector.inspect(compressed)["version"] == 2

        # Action-space v6 changes the payload encoded in request.invocation;
        # its observation object deliberately retains the v5 field shape.
        changed = json.loads(json.dumps(rows))
        for row in changed:
            if row["type"] in ("session_begin", "session_end"):
                row["observation"]["version"] = 6
            elif row["type"] in ("action", "interval"):
                row["before"]["version"] = 6
                row["after"]["version"] = 6
        path.write_text(encode(changed))
        inspector.inspect(path)

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
        rejected(v2_original.replace('"type":"header"',
                                     '"type":"header","type":"header"', 1),
                 "duplicate JSON field")
        changed = rows[:action + 1] + rows[action + 2:]
        rejected(encode(changed), "elapsed record")
        changed = json.loads(json.dumps(rows))
        changed[action]["outcome"]["status"] = 99
        rejected(encode(changed), "invocation status")
        changed = json.loads(json.dumps(rows))
        changed[0]["version"] = 3
        rejected(encode(changed), "unsupported header")
        changed[0]["version"] = True
        rejected(encode(changed), "header version")
        changed = json.loads(json.dumps(rows))
        changed[action]["sequence"] = True
        rejected(encode(changed), "action sequence")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["fast_glue"] = float("inf")
        rejected(encode(changed).replace("Infinity", "1e999"), "fast_glue")
        changed = json.loads(json.dumps(rows))
        changed[action]["before"]["version"] = 7
        rejected(encode(changed), "unsupported observation version")
        producer_rows = json.loads(json.dumps(rows))
        producer_action = next(
            i for i, row in enumerate(producer_rows) if row.get("request"))
        producer = {
            "family": "Gf3AffineConsequence", "status": 6,
            "complete": False, "preparation_work": 11,
            "enumeration_work": 13, "total_work": 24,
            "candidates": 3, "offers": 1}
        producer_rows[producer_action]["request"]["producer_effort"] = [producer]
        path.write_text(encode(producer_rows))
        inspector.inspect(path)
        changed = json.loads(json.dumps(producer_rows))
        del changed[producer_action]["request"]["producer_effort"][0]["offers"]
        rejected(encode(changed), "producer effort fields")
        changed = json.loads(json.dumps(producer_rows))
        changed[producer_action]["request"]["producer_effort"][0]["family"] = "unknown"
        rejected(encode(changed), "producer family")
        changed = json.loads(json.dumps(producer_rows))
        changed[producer_action]["request"]["producer_effort"][0]["status"] = 7
        rejected(encode(changed), "producer status")
        changed = json.loads(json.dumps(producer_rows))
        changed[producer_action]["request"]["producer_effort"][0]["complete"] = 1
        rejected(encode(changed), "producer completeness")
        changed = json.loads(json.dumps(producer_rows))
        changed[producer_action]["request"]["producer_effort"][0]["total_work"] += 1
        rejected(encode(changed), "producer total work mismatch")
        changed = json.loads(json.dumps(producer_rows))
        changed[producer_action]["request"]["producer_effort"][0]["offers"] = 4
        rejected(encode(changed), "producer offers exceed candidates")
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
        guided_path = pathlib.Path(directory) / "profile-6-pigeons-6.feedback.jsonl"
        guided_rows = legacy_rows(
            [json.loads(line) for line in inspector.open_records(guided_path)])
        guided_actions = [row for row in guided_rows if row["type"] == "action"]
        imports = 0
        for action in guided_actions:
            if action["identity"] != "Walk":
                continue
            before = action["before"]["native"]["walk_seed_imports"]
            after = action["after"]["native"]["walk_seed_imports"]
            if after == before:
                continue
            assert after == before + 1
            imports += 1
            invocation = action["native_request"]["invocation"]
            match = re.search(r"walk2\([^,]+,[^,]+,(\d+),", invocation)
            assert match
            assert int(match.group(1)) > 0
        assert imports > 0
        assert not any(action["identity"] == "LocalSearchGuidance"
                       for action in guided_actions)
        with inspector.open_records(default_records[-1]) as source:
            default_rows = legacy_rows([json.loads(line) for line in source])
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

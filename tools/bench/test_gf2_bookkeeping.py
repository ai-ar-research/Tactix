"""Focused tests for the GF(2) single-result bookkeeping extractor."""

import gzip
import json
from pathlib import Path
import tempfile
import unittest

from gf2_bookkeeping import (ADVICE_COUNTERS, feedback_actions, followups,
                             join_transactions,
                             parse_affine_invocation,
                             provenance_transactions)
from gf2_target_score import AFFINE_COUNTERS


def native(**updates):
    result = {
        "decisions": 0, "conflicts": 0, "propagations": 0, "ticks": 0,
        "root_assignments": 0,
        **{name: 0 for name in AFFINE_COUNTERS},
        **{name: 0 for name in ADVICE_COUNTERS},
    }
    result.update(updates)
    return result


def observation(values, strategy=0):
    return {
        "version": 1, "state": {"arena": 1, "touch": 1, "strategy": strategy},
        "sequence": strategy, "variables": 4, "unassigned": 4,
        "active_unassigned": 4, "level": 0, "trail": 0,
        "root_population": 0, "irredundant_clauses": 4,
        "inconsistent": False, "conflict": False, "fixpoint": True,
        "searching": False, "have_configuration": False,
        "configuration": "vmtf/adaptive-glue/saved", "rotation_intervals": 0,
        "next_restart_conflicts": 1, "reluctant": [1, 1, 0],
        "fast_glue": 0, "slow_glue": 0, "last_action": None,
        "native": values,
    }


def outcome(before, after):
    return {
        "status": 0, "result": 0, "budget_status": 0, "unit": 0,
        "work": 1, "applications": 1, "step": 1, "instance": 1,
        "progress": True, "discovery_progress": 0,
        "discovery_entry_required": False, "discovery_exhausted": False,
        "selection_complete": True, "revision": 1,
        "before": before["state"], "after": after["state"],
        "composition_overshoot": 0, "resumed_search": False,
    }


class Gf2BookkeepingTests(unittest.TestCase):
    PAYLOAD = (
        "sequence-repeat=once:1 step=1:AffineConsequence:3:once:1:0:1:"
        "affine-basis-row1[9,1,2,2,4,7,1,2,0,3,2,3,1,2,3,0,4,"
        "31,3,1,2,3,32,3,-1,-2,3,33,3,-1,2,-3,34,3,1,-2,-3,"
        "2,4,7,1,2,41,2,4,7,42,2,-4,-7]"
    )

    def test_exact_affine_payload(self):
        rows = parse_affine_invocation(self.PAYLOAD)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["owner_version"], row["kind"], row["width"]),
                         (9, "direct-binary", 2))
        self.assertEqual(row["source_equation_ordinals"], [0, 3])
        self.assertEqual(row["source_equations"][0]["premises"][2],
                         {"clause_id": 33, "clause": [-1, 2, -3]})
        with self.assertRaises(ValueError):
            parse_affine_invocation(self.PAYLOAD.replace(",2,41,2", ",3,41,2"))

    def test_v2_feedback_reconstruction(self):
        before = observation(native())
        after_native = native(affine_consequence_attempts=1,
                              affine_consequence_applied=1,
                              affine_consequence_additions=1,
                              affine_consequence_published=1)
        after = observation(after_native, 1)
        delta = lambda left, right: {
            "set": {key: value for key, value in right.items()
                    if key != "native" and left.get(key) != value},
            "native": {key: value for key, value in right["native"].items()
                       if left["native"].get(key) != value},
        }
        rows = [
            {"type": "header", "version": 2, "metadata": "x"},
            {"type": "session_begin", "observation": before},
            {"type": "action", "version": 2, "sequence": 1, "kind": 0,
             "identity": "AffineConsequence", "instance": 1,
             "request": {"invocation": self.PAYLOAD}, "allowance": None,
             "before_delta": {"set": {}}, "after_delta": delta(before, after),
             "outcome": outcome(before, after), "stop": 0},
            {"type": "elapsed_nanos", "value": 1},
            {"type": "session_end", "verdict": 0, "stop": 0,
             "observation_delta": {"set": {}}},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "feedback.jsonl.gz"
            logical = "".join(json.dumps(row) + "\n" for row in rows).encode()
            path.write_bytes(gzip.compress(logical, mtime=0))
            version, actions = feedback_actions(path)
        self.assertEqual(version, 2)
        self.assertEqual(actions[0]["after"]["native"]
                         ["affine_consequence_applied"], 1)

    def test_provenance_join_checks_exact_sources(self):
        selection = parse_affine_invocation(self.PAYLOAD)[0]
        common = {"action": 12, "semantic": 12, "owner": 9}
        records = [
            {"kind": "session", "version": 1, "input_file_sha256": "1" * 64},
            {**common, "kind": "affine-basis-begin", "equations": 4,
             "rows": 1, "definitions": 0, "propagation": "deferred-controller"},
        ]
        occurrence = 0
        for equation in selection["source_equations"]:
            for premise in equation["premises"]:
                occurrence += 1
                records.append({**common, "kind": "affine-basis-source",
                                "occurrence": occurrence,
                                "source_id": premise["clause_id"],
                                "clause": premise["clause"]})
        records.extend([
            {**common, "kind": "affine-basis-row", "row": 0, "rhs": True,
             "variables": [4, 7], "sources": [0, 3],
             "premises": list(range(1, occurrence + 1))},
            {**common, "kind": "affine-basis-proof-rup", "id": 50,
             "install": True, "clause": [4, 7]},
            {**common, "kind": "affine-basis-complete", "additions": 1,
             "deletions": 0, "published": 1, "extent_growth": 0},
        ])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "provenance.jsonl"
            path.write_text("".join(json.dumps(row) + "\n" for row in records))
            _, transactions = provenance_transactions(path)
        before = observation(native())
        after_values = native(**{name: 0 for name in AFFINE_COUNTERS})
        after_values.update(affine_consequence_attempts=1,
                            affine_consequence_applied=1,
                            affine_consequence_additions=1,
                            affine_consequence_published=1)
        after = observation(after_values, 1)
        action = {"sequence": 1, "identity": "AffineConsequence",
                  "request": {"invocation": self.PAYLOAD},
                  "before": before, "after": after,
                  "outcome": outcome(before, after)}
        joined = join_transactions([action], transactions)
        self.assertEqual(joined[0]["committed"]["action_id"], 12)
        self.assertEqual(joined[0]["committed"]["published"], 1)

    def test_propagation_and_advice_followups(self):
        initial = observation(native())
        propagated_native = native(root_assignments=1, propagations=2, ticks=3)
        propagated = observation(propagated_native, 1)
        propagated["root_population"] = 1
        candidate = {"id": 99, "action_id": 12, "arena_generation": 1,
                     "touch_epoch": 1, "strategy_epoch": 1, "producer": 0,
                     "existing_variables": 2, "introduced_variables": 0}
        advice_before = dict(propagated)
        advice_before["semantic_variable_candidates"] = [candidate]
        advised_native = dict(propagated_native)
        advised_native.update(semantic_variable_advice_attempts=1,
                              semantic_variable_advice_applied=1,
                              semantic_variable_advice_variables=2,
                              semantic_variable_advice_priority=6)
        advice_after = dict(propagated)
        advice_after["native"] = advised_native
        actions = [
            {"sequence": 1, "identity": "AffineConsequence"},
            {"sequence": 2, "identity": "Propagate", "before": initial,
             "after": propagated, "outcome": {}},
            {"sequence": 3, "identity": "SemanticVariableAdvice",
             "instance": 99, "before": advice_before, "after": advice_after,
             "request": {"invocation":
                         "x:semantic-variable-advice1(10,100,2,5)"},
             "outcome": {}},
            {"sequence": 4, "identity": "Decide", "instance": 4},
        ]
        transactions = [{"feedback_sequence": 1,
                         "committed": {"action_id": 12}}]
        effects, decisions = followups(actions, transactions)
        self.assertEqual(effects["propagate"][0]["delta_root_population"], 1)
        self.assertEqual(effects["propagate"][0]["preceding_affine_action_id"], 12)
        self.assertTrue(effects["advice"][0]["source_is_committed_affine"])
        self.assertEqual(effects["advice"][0]["configured_parameters"]
                         ["existing_theory_priority"], 2)
        self.assertEqual(decisions, [{"sequence": 4, "dimacs_literal": 4}])


if __name__ == "__main__":
    unittest.main()

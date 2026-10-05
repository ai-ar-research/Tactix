import csv
import json
from pathlib import Path
import tempfile
import unittest

from nary_domain_census import (disposition, parse_statistics, score,
                                validate_controller_counters)


class NaryDomainCensusTests(unittest.TestCase):
    def test_precise_status_and_controller_counters(self):
        parsed = parse_statistics([
            "c controller-rule NaryCspPrefix requests=1 applications=1 effective=0 exhausted=1 stale=0 interrupted=0 work=99 overshoot=0",
            "c controller-producer NaryCspPrefix calls=1 work=88 candidates=3 offers=2",
            "c nary-csp   1 attempts, 0 applied, 0 no-structure, 0 no-consequence, 0 already-published, 0 invalid, 0 inapplicable, 1 exhausted, 0 interrupted, 0 stale, 0 unsupported-proof",
        ])
        self.assertEqual(parsed["producer"]["offers"], 2)
        self.assertEqual(parsed["status"]["exhausted"], 1)
        self.assertEqual(disposition(parsed), "exhausted")
        validate_controller_counters(parsed)

    def test_applied_takes_precedence_over_declines(self):
        parsed = parse_statistics([
            "c controller-rule NaryCspPrefix requests=2 applications=2 effective=1 exhausted=1 stale=0 interrupted=0 work=99 overshoot=0",
            "c controller-producer NaryCspPrefix calls=1 work=88 candidates=3 offers=2",
            "c nary-csp   2 attempts, 1 applied, 0 no-structure, 0 no-consequence, 0 already-published, 0 invalid, 0 inapplicable, 1 exhausted, 0 interrupted, 0 stale, 0 unsupported-proof",
        ])
        validate_controller_counters(parsed)
        self.assertEqual(disposition(parsed), "applied")

    def test_controller_counter_disagreement_is_rejected(self):
        parsed = parse_statistics([
            "c controller-rule NaryCspPrefix requests=1 applications=0 effective=0 exhausted=0 stale=0 interrupted=0 work=99 overshoot=0",
            "c controller-producer NaryCspPrefix calls=1 work=88 candidates=3 offers=2",
            "c nary-csp   1 attempts, 1 applied, 0 no-structure, 0 no-consequence, 0 already-published, 0 invalid, 0 inapplicable, 0 exhausted, 0 interrupted, 0 stale, 0 unsupported-proof",
        ])
        with self.assertRaisesRegex(ValueError, "applications disagree"):
            validate_controller_counters(parsed)

    def test_no_offer_and_missing_statistics_are_distinct(self):
        no_offer = parse_statistics([
            "c controller-producer NaryCspPrefix calls=1 work=10 candidates=0 offers=0",
            "c controller-rule NaryCspPrefix requests=0 applications=0 effective=0 exhausted=0 stale=0 interrupted=0 work=0 overshoot=0",
        ])
        self.assertEqual(disposition(no_offer), "no-offer")
        self.assertEqual(disposition(parse_statistics([])),
                         "producer-statistics-missing")

    def test_matrix_identity_and_require_final(self):
        with tempfile.TemporaryDirectory() as temporary:
            campaign = Path(temporary)
            (campaign / "results").mkdir()
            (campaign / "manifest.json").write_text(json.dumps({
                "arms": {"tactix": {"arguments": [
                    "--controller-policy=2",
                    "--controller-native-producers=nary-csp",
                    "--controller-families=NaryCspPrefix"]}}}))
            with (campaign / "matrix.tsv").open("w", newline="") as target:
                writer = csv.DictWriter(target, delimiter="\t",
                                        fieldnames=("task", "rank", "arm", "instance"))
                writer.writeheader()
                writer.writerow({"task": 0, "rank": 190, "arm": "tactix",
                                 "instance": "tierD/example.cnf.xz"})
            with self.assertRaisesRegex(ValueError, "missing final result"):
                score(campaign, require_final=True)
            result = {"task": 0, "rank": 191, "arm": "tactix",
                      "instance": "tierD/example.cnf.xz", "solver_statistics": []}
            (campaign / "results/000.json").write_text(json.dumps(result))
            with self.assertRaisesRegex(ValueError, "rank does not match"):
                score(campaign, require_final=True)


if __name__ == "__main__":
    unittest.main()

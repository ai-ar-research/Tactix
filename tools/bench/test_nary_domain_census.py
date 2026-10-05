import unittest

from nary_domain_census import disposition, parse_statistics


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

    def test_no_offer_and_missing_statistics_are_distinct(self):
        no_offer = parse_statistics([
            "c controller-producer NaryCspPrefix calls=1 work=10 candidates=0 offers=0",
            "c controller-rule NaryCspPrefix requests=0 applications=0 effective=0 exhausted=0 stale=0 interrupted=0 work=0 overshoot=0",
        ])
        self.assertEqual(disposition(no_offer), "no-offer")
        self.assertEqual(disposition(parse_statistics([])),
                         "producer-statistics-missing")


if __name__ == "__main__":
    unittest.main()

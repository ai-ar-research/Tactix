#!/usr/bin/env python3
"""Exhaustive small-formula controls for algebraic bundle composition."""
import itertools
from pathlib import Path
import random
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/bench"))

from check_algebraic_proof_bundle import selector_challenge  # noqa: E402


def satisfies_clause(bits, clause):
    return any(bits[abs(literal) - 1] == (literal > 0) for literal in clause)


def satisfies(bits, clauses):
    return all(satisfies_clause(bits, clause) for clause in clauses)


def models(variables, clauses):
    return [bits for bits in itertools.product((False, True), repeat=variables)
            if satisfies(bits, clauses)]


def random_clause(rng, variables):
    chosen = rng.sample(range(1, variables + 1), rng.randint(1, variables))
    return [variable if rng.randrange(2) else -variable for variable in chosen]


class AlgebraicBundleSemantics(unittest.TestCase):
    def test_selector_challenge_matches_entailment(self):
        rng = random.Random(0xA16EB4A)
        for case in range(5000):
            variables = rng.randint(1, 5)
            source = [random_clause(rng, variables)
                      for _ in range(rng.randint(0, 8))]
            selected = [random_clause(rng, variables)
                        for _ in range(rng.randint(1, 5))]
            source_models = models(variables, source)
            expected_unsat = all(
                all(satisfies_clause(bits, lemma) for bits in source_models)
                for lemma in selected)

            selectors, challenge = selector_challenge(
                source, selected, variables)
            self.assertEqual(selectors,
                             list(range(variables + 1,
                                        variables + len(selected) + 1)))
            actual_unsat = not models(variables + len(selected), challenge)
            self.assertEqual(actual_unsat, expected_unsat,
                             f"selector mismatch in random case {case}")

    def test_composition_implication(self):
        rng = random.Random(0xC0A905E)
        for case in range(5000):
            variables = rng.randint(1, 5)
            original = [random_clause(rng, variables)
                        for _ in range(rng.randint(0, 9))]
            source = [clause for clause in original if rng.randrange(2)]
            lemmas = [random_clause(rng, variables)
                      for _ in range(rng.randint(1, 5))]

            _, challenge = selector_challenge(source, lemmas, variables)
            batches_accept = not models(variables + len(lemmas), challenge)
            consumer_accepts = not models(variables, original + lemmas)
            if batches_accept and consumer_accepts:
                self.assertFalse(models(variables, original),
                                 f"composition failed in random case {case}")


if __name__ == "__main__":
    unittest.main()

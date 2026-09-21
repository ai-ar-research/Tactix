#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/signed_cardinality_proof.hpp"
#include "strategy/rule_parameters.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static Kernel fixture(bool omit_last = false) {
  std::ostringstream dimacs;
  constexpr std::array<int, 5> positive{1, 2, 3, 4, 5};
  constexpr std::array<int, 5> negative{-1, -2, -3, -4, -5};
  const int clauses = omit_last ? 19 : 20;
  dimacs << "p cnf 5 " << clauses << '\n';
  int emitted = 0;
  for (const auto &group : {positive, negative})
    for (int i = 0; i < 5; ++i)
      for (int j = i + 1; j < 5; ++j)
        for (int k = j + 1; k < 5; ++k) {
          if (emitted++ == clauses)
            continue;
          dimacs << group[i] << ' ' << group[j] << ' ' << group[k] << " 0\n";
        }

  FILE *input = std::tmpfile();
  assert(input);
  const std::string text = dimacs.str();
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

int main() {
  {
    Kernel kernel = fixture();
    assert(signed_cardinality_proof_guard(kernel));
    assert(signed_cardinality_proof_apply(kernel) == ApplyResult::Unsat);
    assert(kernel.inconsistent);
    assert(kernel.stats.signed_cardinality_proof_calls == 1);
    assert(kernel.stats.signed_cardinality_proof_recognized == 1);
    assert(kernel.stats.signed_cardinality_proof_groups == 2);
    assert(kernel.stats.signed_cardinality_proof_bdd_nodes > 0);
    assert(kernel.stats.signed_cardinality_proof_additions > 0);
    assert(kernel.stats.signed_cardinality_proof_unsat == 1);
  }
  {
    Kernel kernel = fixture(true);
    assert(signed_cardinality_proof_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.inconsistent);
    assert(!kernel.stats.signed_cardinality_proof_recognized);
  }
  {
    Kernel kernel = fixture();
    strategy::SignedCardinalityProofParameters parameters;
    parameters.maximum_proof_additions = 1;
    strategy::ScopedSignedCardinalityProofParameters posed(kernel, parameters);
    assert(signed_cardinality_proof_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.inconsistent);
    assert(kernel.stats.signed_cardinality_proof_budget_exhausted == 1);
  }
  {
    Kernel kernel = fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(lucky.apply_instance(kernel, 16) == ApplyResult::Unsat);
    assert(kernel.inconsistent);
    assert(kernel.stats.signed_cardinality_proof_unsat == 1);
  }
  return 0;
}

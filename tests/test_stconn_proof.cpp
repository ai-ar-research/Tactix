#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/opportunities.hpp"


// Shared with micro_proofs.sh; the unit executable embeds the literal.
static const char stconn_3x3[] =
#include "fixtures/stconn-3x3.cnf.inc"
;

static Kernel fixture(bool corrupt = false) {
  std::string text = stconn_3x3;
  if (corrupt)
    text.insert(text.rfind("0\n"), "1 ");
  return parse_dimacs_text(text);
}

int main() {
  // The refutation through Lucky 15; tests/micro_proofs.sh checks its proof.
  {
    Kernel k = fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(apply_rule_instance(lucky, k, 15) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.stconn_proof_recognized == 1 &&
           k.stats.stconn_proof_width == 3 && k.stats.stconn_proof_columns == 3 &&
           k.stats.stconn_proof_unsat == 1);
  }
  {
    Kernel k = fixture(true);
    assert(stconn_proof_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.stconn_proof_recognized);
  }
  {
    Kernel k = fixture();
    strategy::StconnProofParameters parameters;
    parameters.maximum_rup_clauses = 1;
    strategy::ScopedRuleParameters<strategy::StconnProofParameters> posed(k, parameters);
    assert(stconn_proof_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && k.stats.stconn_proof_budget_exhausted == 1);
  }
  return 0;
}

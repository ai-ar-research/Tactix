#include "unit_test.hpp"
#include "query_test_support.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "rules/condition/condition.hpp"
#include "strategy/bve_invocation.hpp"
#include "proof/proof.hpp"
#include "session/query_projection.hpp"

namespace tactix {
SubstitutionRedundancyApplicationResult substitution_redundancy_apply_orbitopal_prefix(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters &, volatile sig_atomic_t *);
}

using namespace tactix::session;
using query_test::clause;
using query_test::drive;

static void sparse_assumption_membership() {
  for (const std::vector<int32_t> cube : {
           std::vector<int32_t>{}, {1, 1, -7, INT32_MAX}, {-INT32_MAX}, {7, -7, -7}}) {
    QueryHistory history;
    for (int32_t literal : cube) assert(history.assume(literal) == InputStatus::Ok);
    assert(history.begin_query() == InputStatus::Ok);
    QueryProjection projection(history, 1);
    assert(std::equal(cube.begin(), cube.end(), projection.constraints().begin(),
                      projection.constraints().end()));
    for (int32_t literal : {0, 1, -1, 7, -7, INT32_MAX, -INT32_MAX})
      assert(projection.is_assumption(literal) ==
             (std::find(cube.begin(), cube.end(), literal) != cube.end()));
    assert(projection.constrains(INT32_MAX - 1u) ==
           (projection.is_assumption(INT32_MAX) || projection.is_assumption(-INT32_MAX)));
    assert(projection.constrains(6) ==
           (projection.is_assumption(7) || projection.is_assumption(-7)));
    assert(!projection.constrains(INT32_MAX) && !projection.constrains(UINT32_MAX));
    assert(projection.contradictory() == std::any_of(cube.begin(), cube.end(),
        [&](int32_t literal) { return std::find(cube.begin(), cube.end(), -literal) != cube.end(); }));
  }
}

static ApplyResult eliminate(Kernel &k, int32_t external) {
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(rule, k));
  return rule.apply_instance(k, lit_instance(k, kernel_internalize(k, external)));
}

static void compatible_elimination_and_cube(bool dynamic) {
  QueryHistory h;
  clause(h, {1, 2});
  assert(h.assume(1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel s;
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  strategy::BveParameters parameters;
  parameters.candidate_scope = strategy::BveCandidateScope::QueryPureClosure;
  std::vector<uint64_t> selected;
  const auto &bve = tactix_rules()[tactix_rule_id("BVE")];
  strategy::bve_complete_instances(k, bve, parameters, selected);
  assert(selected.size() == 1);
  Lit selected_literal;
  assert(lit_of_instance(k, selected[0], &selected_literal));
  assert(kernel_externalize(k, selected_literal) == 2);
  assert(eliminate(k, 1) == ApplyResult::Ok);
  assert(s.projection_state(t).reconstruction_literals && !s.projection_state(t).restore);
  kernel_compact_variables(k);
  assert(drive(s, h, dynamic) == 10);
  assert(k.model_values[internalize(1)] > 0);
  assert(h.finish_query(1, QueryResult::Sat) == InputStatus::Ok);

  QueryHistory cube;
  clause(cube, {1, 2});
  clause(cube, {3, -3});
  assert(cube.assume(1) == InputStatus::Ok);
  assert(cube.begin_query() == InputStatus::Ok);
  QueryKernel c;
  assert(c.replay(cube) == ReplayStatus::Ok);
  const auto ct = c.ticket();
  Kernel &ck = *c.kernel(ct);
  ConditionInstance instance;
  instance.clause = {internalize(1), internalize(2)};
  instance.witness = {internalize(1), internalize(-3)};
  assert(condition_apply_explicit(ck, instance));
  assert(c.projection_state(ct).reconstruction_literals == 2);
  assert(!c.projection_state(ct).restore);
  assert(drive(c, cube, dynamic) == 10);
}

static void contradictory_reconstruction(bool dynamic) {
  QueryHistory h;
  clause(h, {1, 2});
  assert(h.assume(1) == InputStatus::Ok && h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel s;
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  assert(eliminate(*s.kernel(t), 1) == ApplyResult::Ok);
  assert(s.projection_state(t).reconstruction_literals);
  assert(!s.projection_state(t).restore);
  assert(drive(s, h, dynamic) == 20);
}

static void equivalence_translation(bool negative, bool dynamic) {
  QueryHistory h;
  clause(h, {-1, negative ? -2 : 2});
  clause(h, {1, negative ? 2 : -2});
  assert(h.assume(2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel s;
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  assert(s.next_assumption(t).status==AssumptionStatus::Decide);
  assert(substitute_apply(k) == ApplyResult::Ok);
  const int32_t expected = negative ? -1 : 1;
  assert(s.effective_assumptions(t).size() == 1);
  assert(s.effective_assumptions(t)[0] == expected);
  assert(k.query_projection->constrains(0));
  assert(!k.query_projection->constrains(1));
  assert(k.query_projection->preserves_constant(internalize(expected)));
  assert(!k.query_projection->preserves_constant(internalize(-expected)));
  assert(k.query_projection->preserves_constant(internalize(-2)));
  assert(s.projection_state(t).translated_assumptions == 1);
  assert(!s.projection_state(t).restore);
  assert(k.value(internalize(expected)) == 0); // proof-only query premise
  kernel_compact_variables(k);
  assert(drive(s, h, dynamic) == 10);
  assert(k.model_values[internalize(2)] > 0);
}

static void compatible_restriction(bool dynamic) {
  QueryHistory h;
  clause(h, {1, 2});
  assert(h.assume(-2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel s;
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(1)};
  sr.witness = sr.clause;
  assert(substitution_redundancy_apply_detailed(k, sr).rule_result == ApplyResult::Ok);
  assert(s.projection_state(t).preserved_restrictions == 1);
  assert(!s.projection_state(t).restore);
  assert(drive(s, h, dynamic) == 10);
}

static void incompatible_restriction() {
  QueryHistory h;
  clause(h, {1, 2});
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  QueryKernel s;
  assert(s.replay(h) == ReplayStatus::Ok);
  Kernel &k = *s.kernel(s.ticket());
  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(1)};
  sr.witness = sr.clause;
  // The selected generated domain must reject before native/proof mutation.
  // Explicit X7 with the same parameters retains its original semantics.
  SubstitutionRedundancyParameters parameters;
  parameters.graph_symmetry.pointwise_root_stabilizer = true;
  const auto id = k.next_clause_id;
  const auto applied = k.stats.sr_applications;
  assert(substitution_redundancy_apply_orbitopal_prefix(k, sr, parameters, nullptr).status ==
         SubstitutionRedundancyApplicationStatus::Inapplicable);
  assert(k.next_clause_id == id && k.stats.sr_applications == applied &&
         !s.projection_state(s.ticket()).restore && !k.value(internalize(1)));
  assert(substitution_redundancy_apply_detailed(k, sr, parameters).rule_result == ApplyResult::Ok);
  assert(s.next_assumption(s.ticket()).status == AssumptionStatus::Restore);
  assert(s.check_model(s.ticket()).status == ModelStatus::InvalidQuery);
}

static void hard_false(QueryHistory &h, int32_t guard, int32_t a, int32_t b) {
  clause(h, {-guard, a, b});
  clause(h, {-guard, a, -b});
  clause(h, {-guard, -a, b});
  clause(h, {-guard, -a, -b});
}

static void proof_fixture(const std::string &prefix, const std::string &mode,
                           const std::string &scenario, bool dynamic) {
  QueryHistory h;
  if (scenario == "opposite") {
    clause(h, {1, 2});
    assert(h.assume(1) == InputStatus::Ok && h.assume(-1) == InputStatus::Ok);
  } else if (scenario == "bve") {
    clause(h, {1, 2}); clause(h, {-1, 2});
    assert(h.assume(-2) == InputStatus::Ok);
  } else if (scenario == "substitute") {
    clause(h, {-1, 2}); clause(h, {1, -2});
    hard_false(h, 1, 3, 4);
    assert(h.assume(2) == InputStatus::Ok);
  } else {
    assert(scenario == "sr" || scenario == "ulc");
    if (scenario == "ulc") clause(h, {1, 2, 6});
    else clause(h, {1, 2});
    hard_false(h, 3, 4, 5);
    assert(h.assume(3) == InputStatus::Ok);
    if (scenario == "ulc") assert(h.assume(1) == InputStatus::Ok);
  }
  assert(h.begin_query() == InputStatus::Ok);
  query_test::export_query(h, prefix);
  auto writer = query_test::writer(prefix, mode);
  QueryKernel s;
  assert(s.replay(h, std::move(writer)) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  if (scenario == "bve" || scenario == "opposite") assert(eliminate(k, 1) == ApplyResult::Ok);
  else if (scenario == "substitute") {
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(s.projection_state(t).translated_assumptions == 1);
    assert(!k.value(kernel_internalize(k, 1)));
  } else if (scenario == "ulc") {
    SubstitutionRedundancyInstance sr;
    sr.clause = {internalize(-2), internalize(-1)};
    sr.unique_literal_amo_certificate = true;
    assert(substitution_redundancy_apply_orbitopal_prefix(k, sr, {}, nullptr).rule_result == ApplyResult::Ok);
    assert(s.projection_state(t).preserved_restrictions == 1);
  } else {
    SubstitutionRedundancyInstance sr;
    sr.clause = {internalize(1)};
    sr.witness = sr.clause;
    assert(substitution_redundancy_apply_detailed(k, sr).rule_result == ApplyResult::Ok);
    assert(s.projection_state(t).preserved_restrictions == 1);
  }
  assert(!s.projection_state(t).restore);
  assert(drive(s, h, dynamic) == 20);
  assert(s.flush_proof(t));
  std::printf("conflicts=%llu decisions=%llu aliases=%llu witnesses=%llu sr=%llu\n",
      (unsigned long long)k.stats.conflicts, (unsigned long long)k.stats.decisions,
      (unsigned long long)s.projection_state(t).translated_assumptions,
      (unsigned long long)s.projection_state(t).reconstruction_literals,
      (unsigned long long)s.projection_state(t).preserved_restrictions);
}

int main(int argc, char **argv) {
  if (argc == 5) {
    proof_fixture(argv[1], argv[2], argv[3], std::string(argv[4]) == "registry");
    return 0;
  }
  assert(argc == 1);
  sparse_assumption_membership();
  for (bool dynamic : {false, true}) {
    compatible_elimination_and_cube(dynamic);
    equivalence_translation(false, dynamic);
    equivalence_translation(true, dynamic);
    compatible_restriction(dynamic);
    contradictory_reconstruction(dynamic);
  }
  incompatible_restriction();
}

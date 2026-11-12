#undef NDEBUG
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "query_test_support.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "session/query_kernel.hpp"
#include "strategy/representation_execution.hpp"

#include <cassert>

using namespace tactix;
using namespace tactix::session;

static NativeVariable restore(Kernel &k, int32_t lit) {
  return execution::execute_action(k, execution::ReactivateExternalVariableRequest{lit});
}

static const auto input = input_clauses;

using query_test::eliminate;

static void retained_coordinate_and_guidance() {
  Kernel k;
  ScopedHeuristics guidance(k);
  input(k, {1, 2, 0});
  k.h->stable = true;
  heuristics_update_scores(k);
  k.h->saved[0] = -1;
  heuristics_score_set(k, 0, 0.75);
  eliminate(k, 1);
  assert(kernel_var_retired(k, 0) && !heuristics_var_active(k, 0));
  const auto rho = k.reconstruction;
  const auto *owner = k.h;
  const auto other_score = k.h->scores.score[1];
  const auto external_extent = k.external_to_internal.size();
  const auto result = restore(k, -1);
  assert(result.status == NativeVariableStatus::Reactivated && result.literal == internalize(-1));
  assert(k.h == owner && k.h->saved[0] == -1 && k.h->scores.score[0] == 0.75);
  assert(k.h->scores.score[1] == other_score && heuristics_var_active(k, 0));
  assert(heuristics_heap_contains(k, 0));
  assert(k.projection_changed && k.reconstruction == rho);
  assert(k.external_to_internal.size() == external_extent);
  const auto epoch = k.strategy_epoch;
  assert(restore(k, 1).status == NativeVariableStatus::Existing);
  assert(k.strategy_epoch == epoch && k.stats.reactivations == 1);
  // Exercise the reactivated variable in ordinary watched propagation. The
  // test reintroduces the known original clause; this is not a proof bridge.
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  assert(decide_apply(k, result.literal, false) == ApplyResult::Ok);
  assert(propagate_service(k) == ApplyResult::Ok && k.value(internalize(2)) > 0);
  kernel_check_invariants(k);
}

static void compacted_identity_and_helpers(bool guided) {
  Kernel k;
  Heuristics h;
  if (guided) { k.h = &h; k.saved_phases = &h.saved; }
  input(k, {1, 2, 0, 3, 4, 0});
  const Var helper = kernel_new_extension_vars(k, 1);
  assert(kernel_externalize(k, make_lit(helper, false)) == 5);
  Var proof_only;
  assert(kernel_reserve_proof_variables(k, 2, proof_only));
  eliminate(k, 1);
  assert(kernel_compact_variables(k));
  assert(kernel_internalize(k, 1) == INVALID_LIT);
  const auto extent = k.external_to_internal.size();
  const auto old_nvars = k.nvars;
  const auto ranks = k.external_import_rank;
  const auto saved = guided ? h.saved : std::vector<int8_t>{};
  const auto scores = guided ? h.scores.score : std::vector<double>{};
  const auto result = restore(k, -1);
  assert(result.status == NativeVariableStatus::Reactivated);
  assert(var_of(result.literal) == old_nvars && negated(result.literal));
  assert(k.nvars == old_nvars+1 && k.external_to_internal.size() == extent);
  assert(k.external_import_rank == ranks);
  assert(k.internal_import_rank[old_nvars] == ranks[0]);
  assert(kernel_externalize(k, result.literal) == -1);
  assert(!kernel_is_solver_extension(k, old_nvars));
  const Var native_helper = var_of(kernel_internalize(k, 5));
  assert(kernel_is_solver_extension(k, native_helper));
  if (guided) {
    assert(std::equal(saved.begin(), saved.end(), h.saved.begin()));
    assert(std::equal(scores.begin(), scores.end(), h.scores.score.begin()));
    assert(heuristics_var_active(k, old_nvars));
  } else {
    ScopedHeuristics late(k);
    assert(heuristics_var_active(k, old_nvars));
    kernel_check_invariants(k);
  }
  assert(restore(k, int32_t(proof_only+1)).status == NativeVariableStatus::UnknownIdentity);
  assert(k.nvars == old_nvars+1 && k.external_to_internal.size() == extent);
  std::vector<Var> order;
  kernel_vars_in_import_order(k, order);
  assert(order.front() == old_nvars);
  assert(decide_apply(k, result.literal, false) == ApplyResult::Ok);
  assert(k.stats.decisions_extension == 0);
  restart_apply_to(k, 0);
  assert(decide_apply(k, make_lit(native_helper, false), false) == ApplyResult::Ok);
  assert(k.stats.decisions_extension == 1);
  kernel_check_invariants(k);
  k.h = nullptr;
  k.saved_phases = nullptr;
}

static void fixed_and_rejected_boundaries() {
  Kernel k;
  input(k, {1, 0, 2, 3, 0});
  assert(kernel_compact_variables(k, true));
  const auto nvars = k.nvars;
  const auto root_count = kernel_logical_root_fixed(k);
  const auto fixed_id = kernel_external_unit_id(k, 0);
  assert(restore(k, 1).status == NativeVariableStatus::Fixed);
  const auto negative = restore(k, -1);
  assert(negative.status == NativeVariableStatus::Fixed && negative.value == -1);
  assert(k.nvars == nvars && kernel_logical_root_fixed(k) == root_count);
  assert(kernel_external_unit_id(k, 0) == fixed_id);
  assert(restore(k, 0).status == NativeVariableStatus::InvalidLiteral);
  assert(restore(k, INT32_MIN).status == NativeVariableStatus::InvalidLiteral);
  assert(restore(k, 100).status == NativeVariableStatus::UnknownIdentity);
  const auto epoch = k.strategy_epoch;
  k.inprocessing = true;
  assert(restore(k, 2).status == NativeVariableStatus::Busy);
  k.inprocessing = false;
  k.warming = true;
  assert(restore(k, 2).status == NativeVariableStatus::Busy);
  k.warming = false;
  k.pending.valid = true;
  assert(restore(k, 2).status == NativeVariableStatus::Busy);
  k.pending.valid = false;
  assert(k.nvars == nvars && k.strategy_epoch == epoch);
  const Lit two = kernel_internalize(k, 2);
  assert(decide_apply(k, two, false) == ApplyResult::Ok);
  assert(restore(k, 3).status == NativeVariableStatus::Busy);
  restart_apply_to(k, 0);
  kernel_check_invariants(k);
}

static void compacted_helper_keeps_extension_boundary() {
  Kernel k;
  ScopedHeuristics guidance(k);
  input(k, {1, 2, 0});
  const Var helper = kernel_new_extension_vars(k, 1);
  const int32_t external = kernel_externalize(k, make_lit(helper, false));
  kernel_add_binary_clause(k, make_lit(helper, false), internalize(2), ++k.next_clause_id);
  eliminate(k, external);
  assert(kernel_compact_variables(k));
  assert(kernel_internalize(k, external) == INVALID_LIT);
  const auto result = restore(k, external);
  assert(result.status == NativeVariableStatus::Reactivated);
  assert(kernel_is_solver_extension(k, var_of(result.literal)));
  assert(decide_apply(k, result.literal, false) == ApplyResult::Ok);
  assert(k.stats.decisions_extension == 1);
  kernel_check_invariants(k);
}

static void reactivation_keeps_projection_obligation() {
  QueryHistory h;
  for (int32_t lit : {1, 2, 0}) assert(h.add(lit) == InputStatus::Ok);
  assert(h.assume(-1) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
  QueryKernel query;
  assert(query.replay(h) == ReplayStatus::Ok);
  const auto ticket = query.ticket();
  Kernel &k = *query.kernel(ticket);
  eliminate(k, 1);
  assert(query.next_assumption(ticket).status == AssumptionStatus::Restore);
  const auto *proof = k.proof;
  const auto proof_id = k.next_clause_id;
  assert(restore(k, -1).status == NativeVariableStatus::Reactivated);
  assert(k.proof == proof && k.next_clause_id == proof_id);
  assert(query.next_assumption(ticket).status == AssumptionStatus::Restore);
  assert(query.check_model(ticket).status == ModelStatus::InvalidQuery);
}

int main() {
  retained_coordinate_and_guidance();
  compacted_identity_and_helpers(false);
  compacted_identity_and_helpers(true);
  fixed_and_rejected_boundaries();
  compacted_helper_keeps_extension_boundary();
  reactivation_keeps_projection_obligation();
}

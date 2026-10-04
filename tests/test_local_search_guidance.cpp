#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/local_search/local_search.hpp"
#include "strategy/controller_observation.hpp"
#include "rules/registry.hpp"

#include <cassert>

using namespace tactix;

static void publication_and_selection() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 3);
  k.h->saved = {1, 1, -1};
  k.h->best = {-1, 1, 1};
  const auto saved = k.h->saved, best = k.h->best;
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp,
      {1, 0, 1}, 2, true, {19, strategy::LocalSearchEffortUnit::CspWork});
  assert(k.h->saved == saved && k.h->best == best);
  assert(k.stats.local_search_hints_offered == 1);
  const auto observed = strategy::controller::observe_kernel(k);
  assert(observed.local_search_candidates.size() == 1);
  const auto info = observed.local_search_candidates.front();
  assert(info.id == 256 && info.effort.amount == 19);
  assert(info.formula_touch_epoch == k.touch_epoch && info.external_variables == 3);
  const RuleVTable &op = tactix_rules()[tactix_rule_id("LocalSearchGuidance")];
  assert(op.kind == ActionKind::PolicyOp && op.guard(k));
  uint64_t instances[4]{};
  assert(op.instances(k, instances, 4) == 1 && instances[0] == info.id);
  assert(op.apply_instance(k, info.id) == ApplyResult::Ok);
  assert((k.h->target == std::vector<int8_t>{1, -1, 1}));
  assert(k.h->saved == saved && k.h->best == best);
  assert(k.stats.local_search_hints_applied == 1);
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp,
      {0, 2, 0}, 1, true);
  assert(k.stats.local_search_hints_offered == 1);
  assert(kernel_local_search_hint(k, info.id));

  // Exact means the score is known, not that another invocation has the same
  // objective. The producer owns its best; replacement receives a new identity.
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::DirectCsp,
      {0, 1, 0}, 3, true);
  assert(!kernel_local_search_hint(k, info.id));
  const auto target = k.h->target;
  assert(op.apply_instance(k, info.id) == ApplyResult::NoOp);
  assert(k.h->target == target && k.stats.local_search_hints_stale == 1);
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Affine, {0, 1, 0});
  assert(k.stats.local_search_hints_offered == 3);
  uint64_t cursor = 0;
  const std::vector<strategy::LocalSearchKind> rotation{
      strategy::LocalSearchKind::Parity, strategy::LocalSearchKind::Affine,
      strategy::LocalSearchKind::DirectCsp};
  const auto selected = local_search_guidance_select_next(k, rotation, cursor);
  assert(cursor == 2 && k.h->target == target); // selection does not import
  assert(op.apply_instance(k, selected) == ApplyResult::Ok);
  assert((k.h->target == std::vector<int8_t>{-1, 1, -1}));
  assert(k.h->saved == saved && k.h->best == best);
  assert(op.apply_instance(k, 99) == ApplyResult::NoOp);
  // Historical kind-only payload still has an explicitly current meaning.
  assert(op.apply_instance(k, 0) == ApplyResult::NoOp); // identical target
}

static void identity_and_phase_lifecycle() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 4);
  kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Parity, {0, 0, 0, 0});
  const uint64_t candidate = k.local_search_hints[2].id;
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, 1);
  --k.unassigned_active;
  assert(kernel_compact_variables(k));
  kernel_reserve_vars(k, 4); // new external identity, no candidate polarity
  k.h->saved.assign(4, 1);
  k.h->best.assign(4, 1);
  k.h->target.assign(4, 1);
  assert(local_search_guidance_apply_candidate(k, candidate) == ApplyResult::Ok);
  assert((k.h->target == std::vector<int8_t>{1, -1, -1, 1}));
  assert((k.h->saved == std::vector<int8_t>(4, 1)) && k.h->saved == k.h->best);
  assert(k.h->target_assigned == 0);
  k.h->stable = false;
  assert(!negated(heuristics_decide_lit(k, 1))); // focused saved phase preserved
  k.h->stable = true;
  assert(negated(heuristics_decide_lit(k, 1))); // target-consuming configuration
  kernel_assign_decision(k, make_lit(1, false));
  assert(!local_search_guidance_guard(k));
  kernel_update_target_phases(k);
  assert(k.h->target[1] == 1); // native capture supersedes the advice
  kernel_backtrack(k, 0);
  kernel_check_invariants(k);
}

static void partial_publication_and_identity_lifetime() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  const auto kind = strategy::LocalSearchKind::Affine;
  kernel_offer_local_search_partial_hint(k, kind, {1, 0, -1}, 4, true);
  const auto first = k.local_search_hints[1].id;
  k.h->saved.assign(3, -1);
  k.h->best.assign(3, 1);
  k.h->target = {-1, -1, 1};
  assert(local_search_guidance_apply_candidate(k, first) == ApplyResult::Ok);
  assert((k.h->target == std::vector<int8_t>{1, -1, -1}));
  assert((k.h->saved == std::vector<int8_t>(3, -1)));
  assert((k.h->best == std::vector<int8_t>(3, 1)));
  for (const std::vector<int8_t> invalid :
       {std::vector<int8_t>{0, 0, 0}, {1, 2, 0}, {-2, 0, 1}, {1}}) {
    kernel_offer_local_search_partial_hint(k, kind, invalid);
    assert(k.local_search_hints[1].id == first);
    assert(k.local_search_publication == 1);
  }
  // Resetting instrumentation cannot make an old replay identity current again.
  k.stats.local_search_hints_offered = 0;
  kernel_offer_local_search_partial_hint(k, kind, {0, 1, 0});
  assert(k.local_search_hints[1].id != first);
  assert(!kernel_local_search_hint(k, first));
  const auto second = k.local_search_hints[1].id;
  k.local_search_publication = strategy::LOCAL_SEARCH_GENERATION_MAX;
  kernel_offer_local_search_partial_hint(k, kind, {-1, 0, 0});
  kernel_offer_local_search_hint(k, kind, {1, 1, 1});
  assert(k.local_search_hints[1].id == second);
  assert(k.stats.local_search_hints_offered == 1);
}

int main() {
  publication_and_selection();
  identity_and_phase_lifecycle();
  partial_publication_and_identity_lifetime();
}

#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/local_search/local_search.hpp"
#include "rules/registry.hpp"

#include <cassert>

using namespace tactix;

int main() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 3);

  kernel_offer_local_search_hint(
      k, strategy::LocalSearchKind::DirectCsp, {1, 0, 1}, 2, true);
  assert(k.stats.local_search_hints_offered == 1);
  assert(local_search_guidance_guard(k));

  const int id = tactix_rule_id("LocalSearchGuidance");
  assert(id >= 0);
  const RuleVTable &op = tactix_rules()[static_cast<size_t>(id)];
  assert(op.kind == ActionKind::PolicyOp && op.guard(k));
  uint64_t instances[4]{};
  assert(op.instances(k, instances, 4) == 1);
  assert(instances[0] ==
         static_cast<uint64_t>(strategy::LocalSearchKind::DirectCsp));
  assert(op.apply_instance(k, instances[0]) == ApplyResult::Ok);
  assert((k.h->saved == std::vector<int8_t>{1, -1, 1}));
  assert(k.h->target == k.h->saved);
  assert(k.stats.local_search_hints_applied == 1);

  // Invalid candidate data is rejected atomically and cannot partially
  // replace a previously retained assignment.
  kernel_offer_local_search_hint(
      k, strategy::LocalSearchKind::DirectCsp, {0, 2, 0}, 1, true);
  assert(k.stats.local_search_hints_offered == 1);
  const auto &direct = k.local_search_hints[static_cast<size_t>(
      strategy::LocalSearchKind::DirectCsp)];
  assert((direct.external_phases == std::vector<int8_t>{1, -1, 1}));
  assert(direct.objective == 2 && direct.objective_exact);

  // A worse exact candidate from the same strategy does not replace its best
  // result. A different strategy remains independently selectable.
  kernel_offer_local_search_hint(
      k, strategy::LocalSearchKind::DirectCsp, {0, 1, 0}, 3, true);
  kernel_offer_local_search_hint(
      k, strategy::LocalSearchKind::Affine, {0, 1, 0});
  assert(k.stats.local_search_hints_offered == 2);
  uint64_t cursor = 0;
  const std::vector<strategy::LocalSearchKind> rotation{
      strategy::LocalSearchKind::Parity,
      strategy::LocalSearchKind::Affine,
      strategy::LocalSearchKind::DirectCsp};
  assert(local_search_guidance_apply_next(k, rotation, cursor) ==
         ApplyResult::Ok);
  assert((k.h->saved == std::vector<int8_t>{-1, 1, -1}));
  assert(cursor == 2); // skipped unavailable Parity, consumed Affine

  assert(op.apply_instance(k, 99) == ApplyResult::NoOp);
}

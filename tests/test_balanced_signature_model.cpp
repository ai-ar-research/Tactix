#undef NDEBUG
#include "rules/lucky/balanced_signature_model.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;

static Kernel balance_fixture(bool complete_shape = true) {
  // N=3, W=2: P=(3^2-1)/2=4, pair block=18, signature boundary=42.
  Kernel k;
  // Variable 44 is deliberately outside the recognized shape and remains
  // unconstrained when the partial hint is published.
  kernel_reserve_vars(k, 44);
  for (const int unit : {-1, -5, -9, -10, -14, -18})
    kernel_assign_unit(k, internalize(unit));
  if (complete_shape)
    kernel_assign_unit(k, internalize(43));
  assert(propagate_service(k) == ApplyResult::Ok);
  return k;
}

int main() {
  {
    Kernel k = balance_fixture();
    assert(balanced_signature_model_guard(k));
    const ApplyResult result = balanced_signature_model_apply(k);
    if (result != ApplyResult::Sat)
      std::fprintf(stderr,
                   "balance result=%u calls=%llu recognized=%llu candidates=%llu "
                   "restarts=%llu steps=%llu completion=%llu hints=%llu\n",
                   unsigned(result),
                   (unsigned long long)k.stats.balanced_signature_calls,
                   (unsigned long long)k.stats.balanced_signature_recognized,
                   (unsigned long long)k.stats.balanced_signature_candidates,
                   (unsigned long long)k.stats.balanced_signature_restarts,
                   (unsigned long long)k.stats.balanced_signature_steps,
                   (unsigned long long)k.stats.balanced_signature_completion_rounds,
                   (unsigned long long)k.stats.local_search_hints_offered);
    assert(result == ApplyResult::Sat);
    assert(k.stats.balanced_signature_calls == 1);
    assert(k.stats.balanced_signature_recognized == 1);
    assert(k.stats.balanced_signature_candidates == 1);
    assert(k.stats.balanced_signature_sat == 1);
    assert(k.unassigned_active == 0);
    assert(k.local_search_hints[static_cast<size_t>(
               strategy::LocalSearchKind::BalancedSignature)]
               .available());
    assert(k.local_search_hints[static_cast<size_t>(
               strategy::LocalSearchKind::BalancedSignature)]
               .external_phases[43] == 0);
  }
  {
    Kernel k = balance_fixture(false);
    const size_t trail = k.trail.size();
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.balanced_signature_recognized == 0);
    assert(k.trail.size() == trail);
    assert(!k.local_search_hints[static_cast<size_t>(
                strategy::LocalSearchKind::BalancedSignature)]
                .available());
  }
  {
    Kernel k = balance_fixture();
    k.level = 1;
    assert(!balanced_signature_model_guard(k));
    k.level = 0;
    k.large_watches_connected = false;
    assert(!balanced_signature_model_guard(k));
  }
  {
    Kernel k = balance_fixture();
    strategy::BalancedSignatureModelParameters parameters;
    parameters.steps_per_restart = 0;
    strategy::ScopedBalancedSignatureModelParameters posed(k, parameters);
    assert(balanced_signature_model_apply(k) == ApplyResult::NoOp);
    assert(k.trail.size() == 7);
  }
  {
    Kernel k = balance_fixture();
    const int lucky_id = tactix_rule_id("Lucky");
    assert(lucky_id >= 0);
    const RuleVTable &lucky = tactix_rules()[lucky_id];
    uint64_t instances[LUCKY_ACTIVE_INSTANCE_COUNT] = {};
    const uint32_t count = lucky.instances(
        k, instances, LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(count == LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(std::find(instances, instances + count, uint64_t{10}) !=
           instances + count);
    assert(lucky.apply_instance(k, 10) == ApplyResult::Sat);
  }
  return 0;
}

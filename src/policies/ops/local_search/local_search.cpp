#include "policies/ops/local_search/local_search.hpp"

#include "policies/heuristics.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility local_search_guidance_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("heuristics-attached", k.h != nullptr);
}

ApplyResult local_search_guidance_apply_candidate(Kernel &k, uint64_t candidate) {
  assert(local_search_guidance_guard(k));
  const auto *selected = kernel_local_search_hint(k, candidate);
  if (!selected) {
    ++k.stats.local_search_hints_stale;
    return ApplyResult::NoOp;
  }
  const auto &hint = *selected;
  ++k.stats.local_search_target_install_attempts;
  uint64_t eligible = 0, changed = 0, matching = 0, replaced = 0;
  bool provenance_changed = false;
  for (Var external = 0; external < hint.external_phases.size(); ++external) {
    const int8_t phase = hint.external_phases[external];
    if (!phase || external >= k.external_to_internal.size())
      continue;
    const Var internal = k.external_to_internal[external];
    if (internal == INVALID_VAR || internal >= k.nvars ||
        kernel_var_retired(k, internal) ||
        k.value(make_lit(internal, false)))
      continue;
    ++eligible;
    if (k.h->target[internal] == phase)
      ++matching;
    else
      ++changed;
    replaced += k.h->target_candidate[internal] &&
                k.h->target_candidate[internal] != hint.id;
    provenance_changed |= k.h->target_candidate[internal] != hint.id;
    k.h->target[internal] = phase;
    k.h->target_candidate[internal] = hint.id;
  }
  k.stats.local_search_target_eligible_phases += eligible;
  k.stats.local_search_target_changed_phases += changed;
  k.stats.local_search_target_matching_phases += matching;
  k.stats.local_search_target_replaced_phases += replaced;
  if (!eligible || (!changed && !provenance_changed))
    return ApplyResult::NoOp;
  k.h->target_assigned = 0;
  ++k.stats.local_search_hints_applied;
  ++k.stats.local_search_target_installs;
  return ApplyResult::Ok;
}

ApplyResult local_search_guidance_apply_kind(
    Kernel &k, strategy::LocalSearchKind kind) {
  assert(local_search_guidance_guard(k));
  if (!strategy::valid_local_search_kind(kind))
    return ApplyResult::NoOp;
  const auto &hint = k.local_search_hints[static_cast<size_t>(kind)];
  return hint.available() ? local_search_guidance_apply_candidate(k, hint.id)
                          : ApplyResult::NoOp;
}

uint64_t local_search_guidance_select_next(
    const Kernel &k, const std::vector<strategy::LocalSearchKind> &rotation,
    uint64_t &cursor) {
  for (size_t tried = 0; tried < rotation.size(); ++tried) {
    const strategy::LocalSearchKind kind =
        rotation[cursor++ % rotation.size()];
    if (!strategy::valid_local_search_kind(kind))
      continue;
    const auto &hint = k.local_search_hints[static_cast<size_t>(kind)];
    if (hint.available())
      return hint.id;
  }
  return 0;
}

ApplyResult local_search_guidance_apply_next(
    Kernel &k, const std::vector<strategy::LocalSearchKind> &rotation,
    uint64_t &cursor) {
  assert(local_search_guidance_guard(k));
  const uint64_t candidate = local_search_guidance_select_next(k, rotation, cursor);
  return candidate ? local_search_guidance_apply_candidate(k, candidate)
                   : ApplyResult::NoOp;
}

ApplyResult local_search_guidance_apply(Kernel &k) {
  assert(local_search_guidance_guard(k));
  for (size_t i = 0; i < strategy::LOCAL_SEARCH_KIND_COUNT; ++i)
    if (strategy::valid_local_search_kind(
            static_cast<strategy::LocalSearchKind>(i)) &&
        k.local_search_hints[i].available())
      return local_search_guidance_apply_kind(
          k, static_cast<strategy::LocalSearchKind>(i));
  return ApplyResult::NoOp;
}

static uint32_t local_search_guidance_instances(Kernel &k, uint64_t *out,
                                                uint32_t cap) {
  uint32_t n = 0;
  for (size_t i = 0; i < strategy::LOCAL_SEARCH_KIND_COUNT && n < cap; ++i)
    if (strategy::valid_local_search_kind(
            static_cast<strategy::LocalSearchKind>(i)) &&
        k.local_search_hints[i].available())
      out[n++] = k.local_search_hints[i].id;
  return n;
}

static ApplyResult local_search_guidance_apply_instance(Kernel &k,
                                                        uint64_t instance) {
  if (instance == INSTANCE_CANONICAL)
    return local_search_guidance_apply(k);
  if (instance >= strategy::LOCAL_SEARCH_KIND_COUNT)
    return local_search_guidance_apply_candidate(k, instance);
  return local_search_guidance_apply_kind(
      k, static_cast<strategy::LocalSearchKind>(instance));
}

TACTIX_REGISTER_POLICY_OP(LocalSearchGuidance, RuleTier::Macro,
                          local_search_guidance_guard,
                          local_search_guidance_apply,
                          local_search_guidance_instances,
                          local_search_guidance_apply_instance);

} // namespace tactix

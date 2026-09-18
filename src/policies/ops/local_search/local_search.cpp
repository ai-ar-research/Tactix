#include "policies/ops/local_search/local_search.hpp"

#include "policies/heuristics.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility local_search_guidance_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0)
      .fact("propagated", k.propagate_head == k.trail.size())
      .fact("heuristics-attached", k.h != nullptr);
}

ApplyResult local_search_guidance_apply_kind(
    Kernel &k, strategy::LocalSearchKind kind) {
  assert(local_search_guidance_guard(k));
  if (!strategy::valid_local_search_kind(kind))
    return ApplyResult::NoOp;
  const auto &hint = k.local_search_hints[static_cast<size_t>(kind)];
  if (!hint.available())
    return ApplyResult::NoOp;
  bool changed = false;
  for (Var external = 0; external < hint.external_phases.size(); ++external) {
    const int8_t phase = hint.external_phases[external];
    if (!phase || external >= k.external_to_internal.size())
      continue;
    const Var internal = k.external_to_internal[external];
    if (internal == INVALID_VAR || internal >= k.nvars ||
        kernel_var_retired(k, internal) ||
        k.value(make_lit(internal, false)))
      continue;
    changed |= k.h->saved[internal] != phase;
    k.h->saved[internal] = phase;
  }
  if (!changed)
    return ApplyResult::NoOp;
  std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
  k.h->target_assigned = 0;
  ++k.stats.local_search_hints_applied;
  return ApplyResult::Ok;
}

ApplyResult local_search_guidance_apply_next(
    Kernel &k, const std::vector<strategy::LocalSearchKind> &rotation,
    uint64_t &cursor) {
  assert(local_search_guidance_guard(k));
  for (size_t tried = 0; tried < rotation.size(); ++tried) {
    const strategy::LocalSearchKind kind =
        rotation[cursor++ % rotation.size()];
    if (!strategy::valid_local_search_kind(kind))
      continue;
    const auto &hint = k.local_search_hints[static_cast<size_t>(kind)];
    if (hint.available())
      return local_search_guidance_apply_kind(k, kind);
  }
  return ApplyResult::NoOp;
}

ApplyResult local_search_guidance_apply(Kernel &k) {
  assert(local_search_guidance_guard(k));
  for (size_t i = 0; i < strategy::LOCAL_SEARCH_KIND_COUNT; ++i)
    if (k.local_search_hints[i].available())
      return local_search_guidance_apply_kind(
          k, static_cast<strategy::LocalSearchKind>(i));
  return ApplyResult::NoOp;
}

static uint32_t local_search_guidance_instances(Kernel &k, uint64_t *out,
                                                uint32_t cap) {
  uint32_t n = 0;
  for (size_t i = 0; i < strategy::LOCAL_SEARCH_KIND_COUNT && n < cap; ++i)
    if (k.local_search_hints[i].available())
      out[n++] = i;
  return n;
}

static ApplyResult local_search_guidance_apply_instance(Kernel &k,
                                                        uint64_t instance) {
  if (instance == INSTANCE_CANONICAL)
    return local_search_guidance_apply(k);
  if (instance >= strategy::LOCAL_SEARCH_KIND_COUNT)
    return ApplyResult::NoOp;
  return local_search_guidance_apply_kind(
      k, static_cast<strategy::LocalSearchKind>(instance));
}

TACTIX_REGISTER_POLICY_OP(LocalSearchGuidance, RuleTier::Macro,
                          local_search_guidance_guard,
                          local_search_guidance_apply,
                          local_search_guidance_instances,
                          local_search_guidance_apply_instance);

} // namespace tactix

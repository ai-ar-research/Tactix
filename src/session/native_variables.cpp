#include "session/native_variables.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace tactix::session {
namespace {

void activate_guidance(Kernel &k, Var v) {
  k.retired_events.erase(std::remove(k.retired_events.begin(), k.retired_events.end(), v),
                         k.retired_events.end());
  k.substituted_events.erase(std::remove(k.substituted_events.begin(), k.substituted_events.end(), v),
                             k.substituted_events.end());
  if (!k.compact_import_order.empty() && k.compact_import_position[v] == INVALID_VAR) {
    k.compact_import_position[v] = static_cast<Var>(k.compact_import_order.size());
    k.compact_import_order.push_back(v);
  }
  if (!k.h) {
    if (std::find(k.input_activation_order.begin(), k.input_activation_order.end(), v) ==
        k.input_activation_order.end()) k.input_activation_order.push_back(v);
    return;
  }
  // Keep surviving scores/phases and relative queue order. A retired native
  // coordinate rejoins at the recent end; a compacted coordinate receives the
  // ordinary fresh-variable initialization through kernel_reserve_vars.
  if (!heuristics_queue_contains(k, v)) {
    auto &h = *k.h;
    auto &link = h.links[v];
    link.prev = h.queue_last;
    link.next = INVALID_VAR;
    if (h.queue_last == INVALID_VAR) h.queue_first = v;
    else h.links[h.queue_last].next = v;
    h.queue_last = v;
    if (h.queue_stamp == UINT32_MAX) {
      uint32_t stamp = 0;
      for (Var at = h.queue_first; at != INVALID_VAR; at = h.links[at].next)
        h.links[at].stamp = ++stamp;
      h.queue_stamp = stamp;
    } else link.stamp = ++h.queue_stamp;
    ++k.stats.reactivations;
  }
  if (k.h->stable && !heuristics_heap_contains(k, v)) heuristics_heap_push(k, v);
  heuristics_reset_queue_search(k);
}

// An invalid literal, or a kernel not at a quiet root boundary.
std::optional<NativeVariable> unavailable(const Kernel &k, int32_t external) {
  if (!external || external == INT32_MIN)
    return NativeVariable{NativeVariableStatus::InvalidLiteral};
  if (k.level || k.inprocessing || k.warming || k.phase_saving_suppressed || k.probing.recording ||
      !k.large_watches_connected || k.dense || k.arena_ordering || k.pending.valid ||
      k.conflict_valid || (k.affine_matrix_search_session &&
                          k.affine_matrix_search_session->has_deferred_reason()))
    return NativeVariable{NativeVariableStatus::Busy};
  return std::nullopt;
}

} // namespace

void retract_query_roots(Kernel &k) {
  assert(!k.inprocessing && !k.warming && !k.phase_saving_suppressed &&
         !k.probing.recording && k.large_watches_connected && !k.dense &&
         !k.arena_ordering && !k.pending.valid);
  assert(!k.affine_matrix_search_session ||
         !k.affine_matrix_search_session->has_deferred_reason());
  std::vector<Var> roots;
  for (Lit lit : k.trail)
    if (!k.assigned[var_of(lit)].level) roots.push_back(var_of(lit));
  kernel_unwind_speculation(k, 0);
  k.unflushed_root = 0;
  k.binary_only_head = UINT32_MAX;
  k.trail_tainted = false;
  std::fill(k.fixed_external_values.begin(), k.fixed_external_values.end(), 0);
  std::fill(k.fixed_external_units.begin(), k.fixed_external_units.end(), 0);
  k.fixed_external_count = 0;
  for (Var v : roots) if (!kernel_var_retired(k, v)) activate_guidance(k, v);
  k.model_extended = false;
  k.model_values.clear();
}

NativeVariable admit_external_variable(Kernel &k, int32_t external) {
  if (const auto rejected = unavailable(k, external)) return *rejected;
  if (k.original_external_vars != UINT32_MAX &&
      k.external_to_internal.size() > k.original_external_vars)
    return {NativeVariableStatus::UnsupportedNamespace};
  const Var ev = var_of(internalize(external));
  const bool missing = ev >= k.external_to_internal.size() ||
                       k.external_to_internal[ev] == INVALID_VAR;
  const bool fixed = ev < k.fixed_external_values.size() && k.fixed_external_values[ev];
  if (missing && !fixed && k.nvars == static_cast<uint32_t>(INT32_MAX))
    return {NativeVariableStatus::Exhausted};
  if (ev >= k.external_to_internal.size()) {
    // Publish the client boundary before any native growth event. Gaps have
    // no internal variable or import rank until the client actually uses them.
    if (k.original_external_vars != UINT32_MAX) k.original_external_vars = ev+1;
    k.external_to_internal.resize(size_t(ev)+1, INVALID_VAR);
    k.external_import_rank.resize(size_t(ev)+1, UINT32_MAX);
  }
  if (fixed) return reactivate_external_variable(k, external);
  Var iv = k.external_to_internal[ev];
  if (iv != INVALID_VAR) {
    kernel_record_import(k, iv);
    return reactivate_external_variable(k, external);
  }
  if (k.external_import_rank[ev] != UINT32_MAX)
    return reactivate_external_variable(k, external);
  kernel_materialize_cold_state(k);
  iv = k.nvars;
  kernel_reconnect_external_variable(k, ev);
  kernel_record_import(k, iv);
  if (k.first_extension_var != UINT32_MAX) k.first_extension_var = k.nvars;
  if (!k.h) k.compact_import_position.resize(k.nvars, INVALID_VAR);
  activate_guidance(k, iv);
  k.model_extended = false;
  k.model_values.clear();
  ++k.strategy_epoch;
  return {NativeVariableStatus::Created, make_lit(iv, external < 0)};
}

NativeVariable reactivate_external_variable(Kernel &k, int32_t external) {
  if (const auto rejected = unavailable(k, external)) return *rejected;
  const Lit raw = internalize(external);
  const Var ev = var_of(raw);
  if (ev >= k.external_to_internal.size()) return {NativeVariableStatus::UnknownIdentity};
  Var iv = k.external_to_internal[ev];
  const int8_t fixed = ev < k.fixed_external_values.size() ? k.fixed_external_values[ev] : 0;
  if (fixed) return {NativeVariableStatus::Fixed, INVALID_LIT, int8_t(negated(raw) ? -fixed : fixed)};
  if (iv != INVALID_VAR) {
    const Lit literal = make_lit(iv, negated(raw));
    if (k.value(literal)) return {NativeVariableStatus::Fixed, literal, k.value(literal)};
    if (!kernel_var_retired(k, iv)) return {NativeVariableStatus::Existing, literal};
  } else {
    // A registered import rank distinguishes a former native identity from a
    // proof-only helper hole. Compaction retains that rank for old coordinates.
    if (ev >= k.external_import_rank.size() || k.external_import_rank[ev] == UINT32_MAX)
      return {NativeVariableStatus::UnknownIdentity};
    if (k.nvars == static_cast<uint32_t>(INT32_MAX))
      return {NativeVariableStatus::Exhausted};
  }
  kernel_materialize_cold_state(k);
  if (iv == INVALID_VAR) {
    iv = k.nvars;
    // Share normal growth, installing the old external identity before any
    // per-variable event or heuristic initialization can observe the mapping.
    kernel_reconnect_external_variable(k, ev);
    if (k.first_extension_var == iv && !kernel_is_solver_extension(k, iv))
      ++k.first_extension_var;
    if (!k.h) k.compact_import_position.resize(k.nvars, INVALID_VAR);
  } else {
    kernel_affine_note_namespace_change(k);
    k.var_status[iv] = VarStatus::Active;
    ++k.unassigned_active;
    kernel_mark_bve_retry(k, iv);
    kernel_touch_var(k, iv);
    kernel_mark_subsume_material(k, iv);
    kernel_mark_factor_literal(k, make_lit(iv, false), "reactivate");
  }
  activate_guidance(k, iv);
  k.model_extended = false;
  k.model_values.clear();
  ++k.strategy_epoch;
  return {NativeVariableStatus::Reactivated, make_lit(iv, negated(raw))};
}

} // namespace tactix::session

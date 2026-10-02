#undef NDEBUG
#include "policies/restart_controller.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/feedback_trace.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace tactix;
using namespace tactix::execution;
using namespace tactix::strategy::controller;

// The retained comparison compiles this same fixture with literal preserved
// constructor/destructor bodies under distinct class names, avoiding inline
// symbol interposition. Ordinary CTest also checks the independent properties.
#ifdef TACTIX_PRESERVED_HEURISTIC_LIFECYCLE
using ReferenceScope = PreservedScopedHeuristics;
using ReferenceController = PreservedRestartController;
#else
using ReferenceScope = ScopedHeuristics;
using ReferenceController = RestartController;
#endif

static Kernel fixture(bool seeded, bool fixed, bool retired) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  for (Var v : {5u, 2u, 7u, 1u, 3u, 0u, 6u, 4u}) kernel_record_import(k, v);
  kernel_add_binary_clause(k, internalize(2), internalize(6));
  if (seeded) {
    k.input_activation_order.reserve(64);
    k.input_activation_order = {5, 2, 7, 1, 3, 0, 6, 4};
  }
  if (fixed) {
    kernel_assign_unit(k, internalize(-2));
    kernel_assign_unit(k, internalize(4));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  if (retired) {
    kernel_add_binary_clause(k, internalize(7), internalize(8));
    kernel_add_binary_clause(k, internalize(-7), internalize(8));
    const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
    assert(apply_rule_instance(rule, k, lit_instance(k, internalize(7))) == ApplyResult::Ok);
    assert(kernel_var_retired(k, 6));
  }
  return k;
}

static std::string image(const Kernel &k) {
  std::ostringstream out;
  out << std::setprecision(17) << serialize_observation(observe_kernel(k));
  out << " seed-capacity=" << k.input_activation_order.capacity();
  for (Var v : k.input_activation_order) out << ":a" << v;
  for (Var v : k.compact_import_order) out << ":i" << v;
  for (Var v : k.compact_import_position) out << ":p" << v;
  out << " have-saved=" << bool(k.saved_phases);
  if (k.h) {
    const auto &h = *k.h;
    out << " H=" << h.nvars << ':' << h.activation_count << ':' << h.fixed_representative
        << ':' << h.queue_first << ':' << h.queue_last << ':' << h.queue_stamp
        << ':' << h.search_idx << ':' << h.search_stamp << ':' << h.scinc
        << ':' << h.stable << ':' << h.target_assigned << ':' << h.best_assigned;
    for (auto phase : h.saved) out << ":s" << int(phase);
    for (auto phase : h.target) out << ":t" << int(phase);
    for (auto phase : h.best) out << ":b" << int(phase);
    for (const auto &link : h.links) out << ":l" << link.prev << ':' << link.next << ':' << link.stamp;
    for (auto value : h.scores.stack) out << ":q" << value;
    for (auto value : h.scores.pos) out << ":o" << value;
    for (auto value : h.scores.score) out << ":e" << value;
  }
  return out.str();
}

static void change_owned_state(Kernel &k) {
  kernel_reserve_vars(k, 10);
  k.h->saved[9] = -1;
  k.h->target[8] = 1;
  k.h->best[9] = -1;
  k.h->stable = true;
  heuristics_update_scores(k);
  heuristics_score_bump(k, 0);
  heuristics_queue_bump(k, 4);
}

static void fresh_lifecycle(bool seeded, bool fixed, bool retired, bool controller) {
  Kernel native = fixture(seeded, fixed, retired), shared = fixture(seeded, fixed, retired);
  const bool pending_seed = !shared.input_activation_order.empty();
  const auto seed_capacity = shared.input_activation_order.capacity();
  Heuristics storage;
  execute_action(shared, HeuristicAttachmentRequest{storage, nullptr,
      controller ? ActivationStorage::Retain : ActivationStorage::Shrink});
  const auto check = [&] {
    assert(image(native) == image(shared));
    assert(shared.h == &storage && shared.saved_phases == &storage.saved);
    assert(storage.nvars == shared.nvars && shared.input_activation_order.empty());
    if (pending_seed) {
      if (controller) assert(shared.input_activation_order.capacity() == seed_capacity);
      // The native shrink request is nonbinding (and is a no-op under the
      // shipped libstdc++ -fno-exceptions build). Full native equality above
      // checks its actual capacity rather than requiring it to release.
      else assert(shared.input_activation_order.capacity() <= seed_capacity);
    }
    for (Var v = 0; v < shared.nvars; ++v)
      if (kernel_root_value(shared, make_lit(v, false)) || kernel_var_retired(shared, v))
        assert(!heuristics_var_active(shared, v));
    kernel_check_invariants(shared);
  };
  if (controller) {
    ReferenceController owner(native);
    check(); change_owned_state(native); change_owned_state(shared); check();
  } else {
    ReferenceScope owner(native);
    check(); change_owned_state(native); change_owned_state(shared); check();
  }
  execute_action(shared, HeuristicDetachmentRequest{storage});
  assert(!native.h && !shared.h && !native.saved_phases && !shared.saved_phases);
  assert(image(native) == image(shared));
}

static void transferred_lifecycle(bool saved_wire) {
  Kernel native = fixture(true, true, true), shared = fixture(true, true, true);
  ScopedHeuristics previous_native(native), previous_shared(shared);
  for (Kernel *k : {&native, &shared}) {
    k->h->saved[0] = -1; k->h->target[2] = 1; k->h->best[4] = -1;
    if (!saved_wire) k->saved_phases = nullptr;
    // An existing owner must not consume a pending unrelated parse seed.
    k->input_activation_order = {3, 1};
  }
  auto *previous = shared.h;
  auto *saved = shared.saved_phases;
  Heuristics storage;
  execute_action(shared, HeuristicAttachmentRequest{storage, previous, ActivationStorage::Retain});
  {
    ReferenceController owner(native);
    assert(image(native) == image(shared));
    assert((shared.input_activation_order == std::vector<Var>{3, 1}));
    change_owned_state(native); change_owned_state(shared);
    assert(image(native) == image(shared));
  }
  execute_action(shared, HeuristicDetachmentRequest{storage, previous, saved});
  assert(shared.h == previous && shared.saved_phases == saved);
  assert(previous->nvars == 10 && previous->saved[9] == -1 && previous->target[8] == 1);
  assert(image(native) == image(shared));
  // An intentionally absent borrowed wire tests exact restoration, but is
  // not a valid attached-H kernel for the global invariant checker. Restore
  // the embedding's wire only after checking that both owners returned null.
  if (!saved_wire) {
    assert(!native.saved_phases && !shared.saved_phases);
    native.saved_phases = &native.h->saved;
    shared.saved_phases = &shared.h->saved;
  }
  kernel_check_invariants(native);
  kernel_check_invariants(shared);
}

static void residual_guidance() {
  Kernel native = fixture(true, true, true), shared = fixture(true, true, true);
  const auto expected = heuristics_prepare_residual_activation_order(native);
  const auto actual = execute_action(shared, ResidualActivationRequest{});
  assert(actual == expected && image(native) == image(shared));
  const auto order = shared.input_activation_order;
  ScopedHeuristics native_h(native), shared_h(shared);
  heuristics_seed_residual_import_order(native, order);
  execute_action(shared, ResidualImportRequest{order});
  assert(image(native) == image(shared) && shared.compact_import_order == order);
  kernel_check_invariants(shared);
}

int main() {
  unsigned cases = 0;
  for (bool seeded : {false, true}) for (bool fixed : {false, true})
    for (bool retired : {false, true}) for (bool controller : {false, true}) {
      fresh_lifecycle(seeded, fixed, retired, controller); ++cases;
    }
  for (bool saved : {false, true}) { transferred_lifecycle(saved); ++cases; }
  residual_guidance(); ++cases;
  std::cout << "Heuristic lifecycle: " << cases << " controls preserve complete state and growth.\n";
}

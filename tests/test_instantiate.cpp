// X4 Instantiate: the success shape (a rare literal removed after the
// assume-and-propagate probe conflicts), cadical's three candidate filters,
// model preservation, and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "policies/default_policy.hpp"
#include "rules/instantiate/instantiate.hpp"
#include "rules/propagate/propagate.hpp"
#include "proof/proof.hpp"
#define tactix tactix_handle // the C handle typedef collides with the namespace
#include "tactix.h"
#undef tactix

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;

static void addn(Kernel &k, std::vector<int> ds) {
  std::vector<Lit> l;
  for (const int d : ds)
    l.push_back(internalize(d));
  if (l.size() == 2)
    kernel_add_binary_clause(k, l[0], l[1]);
  else
    kernel_add_large_clause(k, l.data(), static_cast<uint32_t>(l.size()),
                            /*redundant=*/false, 0);
}

static const RuleVTable &instantiate_rule() {
  const int id = tactix_rule_id("Instantiate");
  assert(id >= 0 && "the rule must be registry-visible (N1)");
  return tactix_rules()[id];
}

// The candidate plus (-1 v 4), (-1 v -4), under which 1 fails.
static void failing_one(Kernel &k, uint32_t vars, std::vector<int> candidate) {
  kernel_reserve_vars(k, vars);
  addn(k, candidate);
  addn(k, {-1, 4});
  addn(k, {-1, -4});
}

static Ref find_ref(const Kernel &k, uint32_t size) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == size)
      return ref;
    ref += clause_actual_words(c);
  }
  return INVALID_REF;
}

static const Clause *find_clause(const Kernel &k, uint32_t size) {
  const Ref ref = find_ref(k, size);
  return ref == INVALID_REF ? nullptr : clause_at(k.arena, ref);
}

static Kernel two_choice_fixture() {
  Kernel k;
  failing_one(k, 6, {1, 2, 3, 6});
  addn(k, {-2, 5});
  addn(k, {-2, -5});
  return k;
}

// The shape the rule exists for: literal 1 is rare and {1} propagates to a
// conflict, so F ⊨ ¬1 ∨ (2 ∨ 3 ∨ 5) resolves against C to drop 1 — and ¬1 is
// not root-derivable by propagation, so instantiation is doing the work.
static void test_removes_rare_literal() {
  Kernel k;
  failing_one(k, 5, {1, 2, 3, 5});
  assert(instantiate_guard(k));
  assert(instantiate_apply(k) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
  assert(k.stats.instantiate_rounds == 1);
  const Clause *c = find_clause(k, 3);
  assert(c && "the quaternary must have become a ternary");
  for (const Lit l : *c)
    assert(l != internalize(1)); // the rare literal is the one removed
  assert(find_clause(k, 4) == nullptr);
  assert(default_solve(k) == 10); // still SAT: the rewrite is model-preserving
}

// At the size boundary a ternary becomes a binary, which lives outside the
// arena (`kernel_replace_clause`'s Binary outcome).
static void test_ternary_becomes_binary() {
  Kernel k;
  failing_one(k, 4, {1, 2, 3});
  assert(instantiate_apply(k) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
  assert(find_clause(k, 3) == nullptr);         // candidate gone from the arena
  assert(k.binary_ids.size() >= 1);             // ... and living as a binary
  assert(default_solve(k) == 10);
}

// Occurrence limit (cadical `instantiateocclim` = 1): nothing is rare enough.
static void test_occurrence_limit_rejects() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  addn(k, {1, 2, 3});
  addn(k, {1, 5, 6});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  assert(instantiate_apply(k) == ApplyResult::NoOp);
  assert(k.stats.instantiated == 0);
  assert(k.stats.instantiate_checked > 0); // the rare literals were probed
}

// Avoid learning units (cadical's `unassigned < 3`): only two literals free.
static void test_unit_avoidance() {
  Kernel k;
  failing_one(k, 4, {1, 2, 3});
  kernel_assign_unit(k, internalize(-3));
  assert(!kernel_propagate(k));
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// Minimum clause size: binaries are never candidates (INST_CLSLIM = 3).
static void test_binary_not_a_candidate() {
  Kernel k;
  failing_one(k, 4, {1, 2});
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// No conflict, no rewrite: the probe fails and the formula is untouched.
static void test_probe_failure_is_a_noop() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  addn(k, {1, 2, 3});
  addn(k, {-1, 4});
  addn(k, {4, 5});
  assert(instantiate_apply(k) == ApplyResult::NoOp);
  assert(k.stats.instantiated == 0);
  assert(find_clause(k, 3) != nullptr); // the candidate survived intact
}

// Redundant clauses are out of scope (deviation 1): C3 Vivify owns them.
static void test_redundant_out_of_scope() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3}, true, 2);
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// The v1 clause payload retains its historical first-success behavior, while
// the additive semantic surface makes either successful literal selectable.
static void test_semantic_pair_surface() {
  const RuleVTable *rule = &instantiate_rule();
  assert(rule->semantic_instances && rule->apply_semantic_instance);

  Kernel legacy = two_choice_fixture();
  uint64_t legacy_instances[2];
  assert(rule->instances(legacy, legacy_instances, 2) == 1);
  const Ref original = static_cast<Ref>(legacy_instances[0]);
  assert(rule->apply_instance(legacy, original) == ApplyResult::Ok);
  const Clause *legacy_clause = find_clause(legacy, 3);
  assert(legacy_clause);
  assert(std::find(legacy_clause->begin(), legacy_clause->end(),
                   internalize(1)) == legacy_clause->end());
  assert(std::find(legacy_clause->begin(), legacy_clause->end(),
                   internalize(2)) != legacy_clause->end());

  for (int selected : {1, 2}) {
    Kernel k = two_choice_fixture();
    // Canonical discovery can be disabled without shrinking explicit N1.
    k.instantiate_parameters = {UINT32_MAX, 0, UINT32_MAX, false};
    assert(rule->instances(k, legacy_instances, 2) == 0);
    uint64_t semantic[8];
    const uint32_t count = rule->semantic_instances(k, semantic, 8);
    assert(count == 4);
    uint64_t chosen = INSTANCE_CANONICAL;
    for (uint32_t i = 0; i < count; ++i) {
      Ref ref;
      Lit literal;
      clause_literal_of_instance(semantic[i], &ref, &literal);
      if (kernel_externalize(k, literal) == selected) {
        assert(ref == find_ref(k, 4));
        chosen = semantic[i];
      }
    }
    assert(chosen != INSTANCE_CANONICAL);
    assert(rule->apply_semantic_instance(k, chosen) == ApplyResult::Ok);
    const Clause *rewritten = find_clause(k, 3);
    assert(rewritten);
    assert(std::find(rewritten->begin(), rewritten->end(),
                     internalize(selected)) == rewritten->end());
    assert(std::find(rewritten->begin(), rewritten->end(),
                     internalize(selected == 1 ? 2 : 1)) != rewritten->end());
    kernel_check_invariants(k);
    assert(default_solve(k) == 10);
  }
}

// N1: the touched-epoch filter is policy-only. With the watermark advanced the
// round declines, but `instances()` and `apply_instance` stay complete.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  failing_one(k, 4, {1, 2, 3});
  k.instantiate_touch_mark = k.touch_epoch;
  assert(instantiate_apply(k) == ApplyResult::NoOp);
  assert(k.stats.instantiate_mark_skipped == 1);
  const RuleVTable &v = instantiate_rule();
  assert(v.guard(k));
  uint64_t out[8];
  assert(v.instances(k, out, 8) == 1); // not narrowed by the mark
  assert(v.apply_instance(k, out[0]) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
  assert(default_solve(k) == 10);
}

// Both interrupt sites. The literal-loop site sits past a failed probe's unwind
// and scope close, so the kernel must be left at a quiescent root, still SAT.
static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  { // the round loop over candidate clauses
    Kernel k;
    failing_one(k, 5, {1, 2, 3, 5});
    KernelStopPublisher pub(k, &stop);
    assert(instantiate_apply(k) == ApplyResult::NoOp);
    assert(k.stats.instantiated == 0);
    assert(k.stats.instantiate_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
    assert(k.level == 0 && !k.inconsistent);
  }
  { // the per-candidate literal loop, through the single-instance entry
    Kernel k;
    failing_one(k, 5, {1, 2, 3, 5});
    const Ref cref = find_ref(k, 4);
    KernelStopPublisher pub(k, &stop);
    assert(instantiate_try(k, cref) == ApplyResult::NoOp);
    assert(k.stats.instantiated == 0);
    assert(k.stats.instantiate_interrupted == 1);
    assert(k.level == 0 && !k.inconsistent);
    assert(find_clause(k, 4) != nullptr); // the candidate is untouched
  }
  {
    Kernel k;
    failing_one(k, 5, {1, 2, 3, 5});
    const Ref cref = find_ref(k, 4);
    assert(instantiate_try(k, cref) == ApplyResult::Ok);
    assert(k.stats.instantiated == 1 && k.stats.instantiate_interrupted == 0);
    assert(default_solve(k) == 10);
  }
}

// The public parameters, with CaDiCaL's defaults (instantiateclslim 3,
// instantiateocclim 1, at least 3 unassigned), and the semantic C API.
static void test_public_parameters() {
  tactix_handle *solver = tactix_init();
  const auto config = [&](int key) { return tactix_config(solver, key); };
  assert(config(TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE) == 3);
  assert(config(TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT) == 1);
  assert(config(TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED) == 3);
  assert(config(TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT) == 0);
  assert(config(TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 1);
  tactix_set_instantiate_limits(solver, 17, 19, 23, 1);
  assert(config(TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE) == 17);
  assert(config(TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT) == 19);
  assert(config(TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED) == 23);
  assert(config(TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT) == 1);
  tactix_set_instantiate_material_threshold(solver, 29);
  assert(config(TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 29);
  // The four-field setter retains the additive setting.
  tactix_set_instantiate_limits(solver, 31, 37, 41, 0);
  assert(config(TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 29);

  tactix_set_instantiate_limits(solver, UINT32_MAX, 0, UINT32_MAX, 0);
  for (int literal : {1, 2, 3, 0, -1, 4, 0, -1, -4, 0}) tactix_add(solver, literal);
  const int instantiate = tactix_rule_id("Instantiate");
  assert(instantiate >= 0 && std::string(tactix_rule_name(instantiate)) == "Instantiate");
  uint64_t legacy[4], semantic[4];
  assert(tactix_rule_instances(solver, instantiate, legacy, 4) == 0);
  assert(tactix_rule_semantic_instances(solver, instantiate, semantic, 4) == 3);
  assert(tactix_apply_semantic_instance(solver, instantiate, semantic[0]) == 0);
  tactix_release(solver);
}

int main() {
  test_public_parameters();
  test_interrupt();
  test_removes_rare_literal();
  test_touch_filter_is_policy_only();
  test_ternary_becomes_binary();
  test_occurrence_limit_rejects();
  test_unit_avoidance();
  test_binary_not_a_candidate();
  test_probe_failure_is_a_noop();
  test_redundant_out_of_scope();
  test_semantic_pair_surface();
  printf("test_instantiate: all passed\n");
  return 0;
}

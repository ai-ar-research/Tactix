// Forward subsumption (C1) and strengthening (C2): the public limit surface,
// scoped parameters, canonical admission versus explicit actions, mid-round
// units, the post-forward closure before the next scheduler slot, and proofs.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rule_test_utils.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "strategy/forward_parameters.hpp"
#define tactix tactix_handle // the C handle typedef collides with the namespace
#include "tactix.h"
#undef tactix
#include "dimacs_text.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

using namespace tactix;

// Input clauses with fresh ids (units at the root), checked against the model.
struct Fixture {
  Kernel k;
  OccIndex occ;
  std::vector<std::vector<int>> inputs;
  uint32_t entry = 0;
  explicit Fixture(Proof *proof = nullptr) {
    kernel_reserve_vars(k, 16);
    k.proof = proof;
    if (proof) proof->bind_variable_map(&k.internal_to_external);
  }
  void add(std::initializer_list<int> clause) {
    inputs.emplace_back(clause);
    const uint64_t id = ++k.next_clause_id;
    if (clause.size() == 1) {
      const Lit unit = internalize(*clause.begin());
      k.unit_ids[var_of(unit)] = id;
      kernel_assign_unit(k, unit);
    } else {
      add_external_clause(k, clause, false, 0, id);
    }
  }
  void seed() { add({1, 2, 3}); add({-2, 3}); add({-1}); }
  void enter(bool dense) {
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) == 0);
    entry = k.propagate_head;
    if (dense) kernel_flush_large_watches(k);
  }
  // A dense C2 round stopped by its budget after one candidate.
  void partial() {
    enter(true);
    kernel_dense_connect(k, occ, true);
    k.midround_budget = true;
    k.work_budget = 1;
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_checked == 1 && !k.strengthen_complete);
    assert(k.value(internalize(3)) > 0 && k.value(internalize(5)) == 0);
    k.midround_budget = false;
    k.work_budget = 0;
  }
  int finish(bool rebuild) {
    if (rebuild) {
      kernel_rebuild_large_watches(k);
      if (default_resume_sparse_propagation(k, entry, true) == ApplyResult::Unsat)
        return 20;
    }
    const int result = default_solve(k);
    if (result == 10) {
      kernel_extend_model(k);
      for (const auto &clause : inputs) {
        bool satisfied = false;
        for (int lit : clause) satisfied |= k.value(internalize(lit)) > 0;
        assert(satisfied);
      }
    }
    return result;
  }
  void model() {
    if (k.dense) kernel_dense_disconnect(k);
    assert(finish(!k.large_watches_connected) == 10);
  }
};

// A DSR proof at stem.proof, saved with stem.cnf, or at a temporary path.
struct ProofSink {
  std::string stem;
  TemporaryFile temporary;
  Proof proof;
  explicit ProofSink(const std::string &stem) : stem(stem) {
    const std::string path = stem.empty() ? temporary.path : stem + ".proof";
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  }
  void save(const Fixture &f) {
    proof.flush();
    if (!stem.empty())
      write_file((stem + ".cnf").c_str(), dimacs_text(16, f.inputs));
  }
};

static int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(tactix_rule_name(i), name)) return i;
  assert(false);
  return -1;
}

static void add(tactix_handle *s, std::initializer_list<int> lits) {
  for (int lit : lits) tactix_add(s, lit);
  tactix_add(s, 0);
}

static bool applicable(tactix_handle *s, int id) {
  std::vector<int> ids(tactix_rule_count());
  const int count = tactix_rules_applicable(s, ids.data(), ids.size());
  return std::find(ids.begin(), ids.begin() + count, id) != ids.begin() + count;
}

static void readback() {
  tactix_handle *s = tactix_init();
  const uint64_t defaults[] = {1000, 100, 10000, 1000, 1000};
  for (int i = 0; i < 5; ++i)
    assert(tactix_config(s, TACTIX_CFG_SUBSUME_CLAUSE_SIZE + i) == defaults[i]);
  add(s, {1, 2, 3});
  for (bool nonroot : {false, true}) {
    if (nonroot) assert(tactix_decide(s, 1) == 0);
    for (uint32_t n : {0u, 1u, 999u, 1000u, 1001u, UINT32_MAX}) {
      tactix_set_subsume_limits(s, n, n, n);
      tactix_set_strengthen_limits(s, n, n);
      for (int i = 0; i < 5; ++i)
        assert(tactix_config(s, TACTIX_CFG_SUBSUME_CLAUSE_SIZE + i) == n);
      assert(applicable(s, rule("SubsumeForward")) == !nonroot);
      assert(applicable(s, rule("Strengthen")) == !nonroot);
    }
  }
  tactix_release(s);
}

static void scoped_parameters() {
  Kernel k;
  const strategy::SubsumeForwardParameters c1 = k.subsume_forward_parameters;
  const strategy::StrengthenParameters c2 = k.strengthen_parameters;
  {
    strategy::ScopedSubsumeForwardParameters pose_c1(k, {7, 11, 13});
    assert(k.subsume_forward_parameters.clause_size_limit == 7);
    assert(k.subsume_forward_parameters.large_occurrence_limit == 11);
    assert(k.subsume_forward_parameters.binary_occurrence_limit == 13);
    {
      strategy::ScopedStrengthenParameters pose_c2(k, {17, 19});
      assert(k.strengthen_parameters.clause_size_limit == 17);
      assert(k.strengthen_parameters.occurrence_limit == 19);
    }
    assert(k.strengthen_parameters.clause_size_limit == c2.clause_size_limit);
  }
  assert(k.subsume_forward_parameters.clause_size_limit == c1.clause_size_limit);
  assert(k.subsume_forward_parameters.large_occurrence_limit ==
         c1.large_occurrence_limit);
  assert(k.subsume_forward_parameters.binary_occurrence_limit ==
         c1.binary_occurrence_limit);
}

// Canonical strategy limits restrict the canonical producer only.
static void canonical(bool c2, int field, uint32_t value, bool changed) {
  tactix_handle *s = tactix_init();
  if (field == 2) {
    add(s, {1, 2});
    add(s, {1, 2, 3});
  } else {
    add(s, {1, 2, 3});
    add(s, {1, 2, 3, 4});
  }
  uint32_t limits[] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
  limits[field] = value;
  if (c2)
    tactix_set_strengthen_limits(s, limits[0], limits[1]);
  else
    tactix_set_subsume_limits(s, limits[0], limits[1], limits[2]);
  assert(tactix_apply(s, rule(c2 ? "Strengthen" : "SubsumeForward")) ==
         (changed ? 0 : 1));
  tactix_release(s);
}

// Explicit legal C1/C2 instances are uncapped by the canonical limits.
static void explicit_size(bool c2, unsigned size, bool restricted) {
  tactix_handle *s = tactix_init();
  if (c2)
    add(s, {1, -2});
  else
    add(s, {1, 2});
  for (unsigned i = 1; i <= size; ++i) tactix_add(s, static_cast<int>(i));
  tactix_add(s, 0);
  if (restricted) {
    tactix_set_subsume_limits(s, 0, 0, 0);
    tactix_set_strengthen_limits(s, 0, 0);
  }
  const int id = rule(c2 ? "Strengthen" : "SubsumeForward");
  uint64_t before[4], again[4];
  assert(tactix_rule_instances(s, id, before, 0) == 0);
  assert(tactix_rule_instances(s, id, before, 4) == 1);
  assert(tactix_rule_instances(s, id, again, 4) == 1 && before[0] == again[0]);
  assert(tactix_apply_instance(s, id, before[0]) == 0);
  tactix_release(s);
}

static void explicit_population(bool c2, bool binary) {
  tactix_handle *s = tactix_init();
  const unsigned copies = binary ? 10001 : 1001;
  for (unsigned i = 0; i < copies; ++i) {
    if (binary)
      add(s, {1, 2});
    else
      add(s, {1, 2, 3});
  }
  add(s, {1, 2, 3, 4});
  const int id = rule(c2 ? "Strengthen" : "SubsumeForward");
  std::vector<uint64_t> choices(copies + 1);
  const int count = tactix_rule_instances(s, id, choices.data(), choices.size());
  assert(count == static_cast<int>(binary ? 1 : copies + 1));
  assert(tactix_apply_instance(s, id, choices[count - 1]) == 0);
  tactix_release(s);
}

// C2's touch-filtered canonical admission versus its unrestricted public surface.
static void admission(bool dense, bool filtered, bool actual_added_mark,
                      bool explicit_action) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {2, 3});
  k.strengthen_touch_mark = k.touch_epoch;
  const uint64_t watermark = k.strengthen_touch_mark;
  kernel_clear_subsume_material(k);
  kernel_touch_var(k, var_of(internalize(3)));
  kernel_mark_subsume_material(k, var_of(internalize(3)));
  kernel_assign_unit(k, internalize(-1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  const uint32_t entry = k.propagate_head;
  assert(!kernel_touched_since(k, 0, watermark));
  if (actual_added_mark) {
    kernel_touch_var(k, 0);
    kernel_mark_subsume_material(k, 0);
  }
  if (dense) kernel_flush_large_watches(k);
  k.touch_filter = filtered;
  const auto &rule = tactix_rules()[tactix_rule_id("Strengthen")];
  assert(rule.guard(k));
  uint64_t payload;
  assert(rule.instances(k, &payload, 1) == 1);
  const bool admitted = !filtered || actual_added_mark || explicit_action;
  const ApplyResult result =
      explicit_action ? rule.apply_instance(k, payload) : rule.apply(k);
  assert(result == (admitted ? ApplyResult::Ok : ApplyResult::NoOp));
  assert(k.stats.strengthen_checked == unsigned(admitted));
  assert(k.stats.strengthen_subsumed == unsigned(admitted));
  assert(kernel_touched_since(k, 0, watermark) == actual_added_mark);
  if (!admitted) {
    // A skipped canonical candidate is still a legal public action.
    assert(rule.instances(k, &payload, 1) == 1);
    assert(rule.apply_instance(k, payload) == ApplyResult::Ok);
    assert(k.stats.strengthen_subsumed == 1);
  }
  // The shared root-notification primitive stays available to C1 and CCE.
  kernel_touch_sync_fixed(k);
  assert(kernel_touched_since(k, 0, watermark));
  if (dense) {
    kernel_rebuild_large_watches(k);
    assert(default_resume_sparse_propagation(k, entry, true) == ApplyResult::Ok);
  }
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(2)) > 0 || k.value(internalize(3)) > 0);
  k.inconsistent = true;
  assert(!rule.guard(k));
}

static void independent_tail(bool dense, bool wide) {
  Fixture f;
  f.seed();
  if (wide) f.add({4, 5, 6, 7}); else f.add({4, 5, 6});
  f.add({4, 5});
  f.enter(dense);
  const auto search_ticks = f.k.stats.search_ticks;
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.value(internalize(3)) > 0);
  assert(f.k.stats.strengthen_checked == 2 && f.k.stats.strengthen_subsumed == 1);
  assert(f.k.stats.strengthened == 1 && f.k.strengthen_complete);
  assert(f.k.stats.search_ticks == search_ticks);
  assert(dense || !kernel_propagation_fixpoint(f.k));
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void roots_change_later_candidate(bool dense, bool satisfied) {
  Fixture f;
  f.seed();
  if (satisfied) f.add({3, 4, 5});
  else { f.add({-3, 4, 5}); f.add({-4}); }
  f.enter(dense);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.stats.strengthen_checked == 2 && f.k.stats.strengthened == 1);
  assert(satisfied ? f.k.stats.flushed == 1 : f.k.value(internalize(5)) > 0);
  assert(f.k.strengthen_complete);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

// Mid-round units propagate through binary (chain 0) and connected large
// (chain 1) clauses in trail order, with unit reasons.
static void unit_chain(bool dense, unsigned chain) {
  Fixture f;
  if (chain == 1) f.add({4, 5, 6}); // connect before the unit-producing candidate
  f.seed();
  if (chain == 0) {
    f.add({-3, 6}); f.add({-6, 7});
    f.add({8, 9, 10}); f.add({8, 9});
  } else {
    f.add({-3, -6}); f.add({-4});
  }
  f.enter(dense);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  const int expected[2][3] = {{3, 6, 7}, {3, -6, 5}};
  assert(f.k.trail.size() == f.entry + 3);
  for (unsigned i = 0; i < 3; ++i)
    assert(f.k.trail[f.entry + i] == internalize(expected[chain][i]));
  assert(f.k.assigned[var_of(internalize(expected[chain][chain + 1]))].reason ==
         UNIT_REASON);
  if (chain == 0)
    assert(f.k.stats.strengthen_subsumed == 1 && f.k.strengthen_complete);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void quota_after_unit(bool dense) {
  Fixture f;
  f.seed(); f.add({4, 5, 6}); f.add({4, 5});
  f.enter(dense);
  f.k.touch_filter = true;
  f.k.midround_budget = true;
  f.k.work_budget = 1;
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.value(internalize(3)) > 0);
  assert(f.k.stats.strengthen_checked == 1 && f.k.stats.budget_stops_c2 == 1);
  assert(f.k.stats.strengthen_rearmed == 1 && !f.k.strengthen_complete);
  f.k.midround_budget = false;
  f.k.work_budget = 0;
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.stats.strengthen_subsumed == 1);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void quota_equality_admits_tail(bool dense) {
  Fixture control;
  control.seed();
  control.enter(dense);
  assert(strengthen_apply(control.k) == ApplyResult::Ok);
  const uint64_t first_steps = control.k.stats.strengthen_visits;
  assert(first_steps > 1);
  Fixture f;
  f.seed(); f.add({4, 5, 6}); f.add({4, 5});
  f.enter(dense);
  f.k.midround_budget = true;
  f.k.work_budget = first_steps;
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.stats.strengthen_checked == 2 && f.k.stats.budget_stops_c2 == 0);
  assert(f.k.stats.strengthen_subsumed == 1 && f.k.strengthen_complete);
  assert(f.k.stats.strengthen_visits > first_steps);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void explicit_instance_stays_single_action() {
  Fixture f;
  f.seed(); f.add({4, 5, 6}); f.add({4, 5});
  f.enter(false);
  const auto &rule = tactix_rules().at(tactix_rule_id("Strengthen"));
  uint64_t instances[2];
  assert(rule.guard(f.k) && rule.instances(f.k, instances, 2) == 2);
  assert(rule.apply_instance(f.k, instances[0]) == ApplyResult::Ok);
  assert(f.k.value(internalize(3)) > 0);
  assert(f.k.stats.strengthen_checked == 1 && f.k.stats.strengthen_subsumed == 0);
  assert(rule.instances(f.k, instances, 2) == 1);
  assert(rule.apply_instance(f.k, instances[0]) == ApplyResult::Ok);
  assert(f.k.stats.strengthen_subsumed == 1);
  assert(f.finish(false) == 10);
}

// An asserted root unit with no id is known to later rules only through the
// root table, as the affine refresh's own check requires.
static void asserted_units_are_in_the_root_table(const Kernel &k) {
  for (const Lit literal : k.trail) {
    const Var v = var_of(literal);
    if (k.assigned[v].level || k.unit_ids[v] || k.assigned[v].reason != UNIT_REASON)
      continue;
    assert(v < k.dsr_root_values.size() &&
           k.dsr_root_values[v] == (negated(literal) ? -1 : 1));
  }
}

// A guarded contradiction with no initial unit or complementary binary pair:
// productive only after the targeted SSR unit and an optional propagation chain.
static void unit_proof(bool dense, unsigned chain, const std::string &stem) {
  ProofSink sink(stem);
  Fixture f(&sink.proof);
  if (chain == 2) f.add({4, 5, 6});
  f.seed();
  int consequence = 3;
  if (chain == 1) { f.add({-3, 6}); f.add({-6, 7}); consequence = 7; }
  if (chain == 2) { f.add({-3, -6}); f.add({-4}); consequence = 5; }
  if (chain == 3) { f.add({-3, 4, 5}); f.add({-4}); consequence = 5; }
  f.add({-consequence, 8, 9}); f.add({-consequence, 8, -9});
  f.add({-consequence, -8, 9}); f.add({-consequence, -8, -9});
  f.enter(dense);
  const ApplyResult result = strengthen_apply(f.k);
  assert(f.k.stats.strengthened >= 1 && f.k.value(internalize(3)) > 0);
  asserted_units_are_in_the_root_table(f.k);
  assert(result == ApplyResult::Unsat || f.finish(dense) == 20);
  assert(f.k.inconsistent);
  sink.save(f);
}

static bool observed;
static uint64_t forward_steps(const Kernel &k) { return k.stats.strengthen_visits; }
static uint64_t observe_next_slot(const Kernel &k) {
  if (!observed && k.stats.strengthened) {
    assert(k.dense && k.dense->track_order && !k.large_watches_connected);
    assert(k.stats.strengthen_checked == 1 && !k.strengthen_complete);
    assert(k.value(internalize(5)) > 0 && k.value(internalize(7)) > 0);
    assert(k.assigned[var_of(internalize(5))].reason == UNIT_REASON);
    assert(k.assigned[var_of(internalize(7))].reason == UNIT_REASON);
    assert(kernel_propagation_fixpoint(k));
    // Both newly satisfied tails are removed before the next slot.
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        for (Lit lit : *c) assert(k.value(lit) <= 0);
      ref += clause_actual_words(c);
    }
    kernel_check_invariants(k);
    observed = true;
  }
  return 0;
}

// The post-forward closure is visible before the next actual scheduler slot.
static void scheduler_chain() {
  Fixture f;
  f.seed(); f.add({-3, 4, 5}); f.add({-4});
  f.add({-5, 6, 7}); f.add({-6});
  f.k.stats.conflicts = 1;
  ScopedHeuristics h(f.k);
  DefaultPolicy policy;
  auto &scheduler = policy.inprocess();
  scheduler.pose_initialized();
  DefaultInprocessScheduler::SlotEntry forward;
  forward.rule = tactix_rule_id("Strengthen");
  forward.group = SlotGroup::Eliminate;
  forward.meter = forward_steps;
  forward.budget_aware = true;
  forward.permille = 0;
  scheduler.entries.push_back(forward);
  DefaultInprocessScheduler::SlotEntry next;
  next.rule = tactix_rule_id("SubsumeForward");
  next.group = SlotGroup::Eliminate;
  next.meter = observe_next_slot;
  scheduler.entries.push_back(next);
  observed = false;
  assert(scheduler.run_phase(f.k, SlotGroup::Eliminate, nullptr) == 0);
  assert(observed);
  f.model();
}

static void direct_chain_and_empty_suffix() {
  Fixture f;
  f.seed(); f.add({-3, 4, 5}); f.add({-4});
  f.add({-5, 6}); f.add({-6, 7, 8}); f.add({-7});
  f.partial();
  const auto search_ticks = f.k.stats.search_ticks;
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Ok);
  for (int lit : {5, 6, 8}) {
    assert(f.k.value(internalize(lit)) > 0);
    assert(f.k.assigned[var_of(internalize(lit))].reason == UNIT_REASON);
  }
  assert(f.k.stats.search_ticks == search_ticks);
  kernel_check_invariants(f.k);
  const auto props = f.k.stats.propagations;
  const auto trail = f.k.trail.size();
  assert(strengthen_reconnect_and_flush(f.k, trail) == ApplyResult::Ok);
  assert(f.k.stats.propagations == props && f.k.trail.size() == trail);
  kernel_check_invariants(f.k);
  f.model();
}

static void reconnect_conflict_proof(unsigned kind, const std::string &stem) {
  ProofSink sink(stem);
  Fixture f(&sink.proof);
  f.seed(); f.add({-3, 4, 5}); f.add({-4});
  if (kind == 0) f.add({-3, 4, -5});
  else if (kind == 1) { f.add({-3, 4, 6}); f.add({-5, -6}); }
  else { f.add({-5, 6, 7}); f.add({-6}); f.add({-5, 6, -7}); }
  f.partial();
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Unsat);
  assert(f.k.inconsistent && !f.k.conflict_valid);
  // Already inconsistent calls do not emit another refutation.
  const auto id = f.k.next_clause_id;
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Unsat);
  assert(f.k.next_clause_id == id);
  sink.save(f);
}

// argv[1], when given, keeps every proof with its CNF in that directory.
int main(int argc, char **argv) {
  const auto stem = [&](const std::string &name) {
    return argc > 1 ? std::string(argv[1]) + "/" + name : std::string();
  };
  readback();
  scoped_parameters();
  for (bool c2 : {false, true}) {
    for (uint32_t n : {0u, 3u}) canonical(c2, 0, n, false);
    for (uint32_t n : {4u, 5u, UINT32_MAX}) canonical(c2, 0, n, true);
    canonical(c2, 1, 0, false);
    canonical(c2, 1, UINT32_MAX, true);
    for (unsigned size : {999u, 1000u, 1001u})
      for (bool restricted : {false, true}) explicit_size(c2, size, restricted);
    for (bool binary : {false, true}) explicit_population(c2, binary);
  }
  for (uint32_t n : {0u, 1u, 2u, UINT32_MAX}) canonical(false, 2, n, n != 0);
  for (bool dense : {false, true}) {
    admission(dense, true, false, false);
    admission(dense, true, true, false);
    admission(dense, false, false, false);
    admission(dense, true, false, true);
    for (bool wide : {false, true}) independent_tail(dense, wide);
    for (bool satisfied : {false, true}) roots_change_later_candidate(dense, satisfied);
    for (unsigned chain : {0u, 1u}) unit_chain(dense, chain);
    quota_after_unit(dense);
    quota_equality_admits_tail(dense);
    for (unsigned chain = 0; chain != 4; ++chain)
      unit_proof(dense, chain, stem("forward-" + std::to_string(dense) + "-" +
                                    std::to_string(chain)));
  }
  explicit_instance_stays_single_action();
  scheduler_chain();
  direct_chain_and_empty_suffix();
  for (unsigned kind = 0; kind < 3; ++kind)
    reconnect_conflict_proof(kind, stem("caller-" + std::to_string(kind)));
  std::puts("Forward C1/C2 surface, admission, units, closure and proofs PASS");
}

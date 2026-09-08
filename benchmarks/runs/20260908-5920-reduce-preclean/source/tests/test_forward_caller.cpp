// Post-forward closure is visible before the next actual scheduler slot.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/bve/bve.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unistd.h>

using namespace tactix;
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
    std::vector<Lit> lits;
    for (int lit : clause) lits.push_back(internalize(lit));
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else if (lits.size() == 2)
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    else
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  void seed() { add({1, 2, 3}); add({-2, 3}); add({-1}); }
  void partial() {
    assert(propagate_apply(k) == ApplyResult::Ok);
    entry = k.propagate_head;
    kernel_flush_large_watches(k);
    kernel_dense_connect(k, occ, true);
    k.midround_budget = true;
    k.work_budget = 1;
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_checked == 1 && !k.strengthen_complete);
    assert(k.value(internalize(3)) > 0 && k.value(internalize(5)) == 0);
    k.midround_budget = false;
    k.work_budget = 0;
  }
  void model() {
    if (k.dense) kernel_dense_disconnect(k);
    if (!k.large_watches_connected) {
      kernel_rebuild_large_watches(k);
      assert(default_resume_sparse_propagation(k, entry, true) == ApplyResult::Ok);
    }
    assert(default_solve(k) == 10);
    kernel_extend_model(k);
    for (const auto &clause : inputs) {
      bool sat = false;
      for (int lit : clause) sat |= k.value(internalize(lit)) > 0;
      assert(sat);
    }
  }
};

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

static void preconnection_cleanup(bool empty_suffix) {
  Fixture f;
  if (empty_suffix) {
    f.add({3}); f.add({3, 4, 5});
    assert(propagate_apply(f.k) == ApplyResult::Ok);
    f.entry = f.k.propagate_head;
    kernel_flush_large_watches(f.k);
    kernel_dense_connect(f.k, f.occ, true);
  } else {
    f.seed(); f.add({3, 4, 5});
    f.partial();
  }
  assert(f.k.dense->large[internalize(4)].size() == 1);
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Ok);
  for (int external : {4, 5}) {
    const Lit lit = internalize(external);
    assert(f.k.dense->large[lit].empty());
    assert(f.k.dense->ordered[lit].empty());
    assert(bve_schedule_occurrence_count(f.k, lit) == 0);
  }
  kernel_check_invariants(f.k);
  f.model();
}

static void later_satisfaction_keeps_lazy_occurrence() {
  Fixture f;
  f.seed(); f.add({-3, 4, 5}); f.add({-4}); f.add({5, 8, 9});
  f.partial();
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Ok);
  assert(f.k.value(internalize(5)) > 0);
  // 5 was learned after the full image was built. Native lazy cleanup leaves
  // the garbage record on 8/9; do not globally compact every occurrence list.
  for (int external : {8, 9}) {
    const Lit lit = internalize(external);
    assert(f.k.dense->large[lit].size() == 1);
    assert(clause_at(f.k.arena, f.k.dense->large[lit][0])->garbage());
    assert(bve_schedule_occurrence_count(f.k, lit) == 1);
  }
  kernel_check_invariants(f.k);
  f.model();
}

static void reduce_roots_then_forward_reconnect() {
  Fixture f;
  f.add({-1, 3, 4}); f.add({1, 2}); f.add({-1, 7, 8});
  ScopedHeuristics h(f.k);
  f.k.h->stable = true;
  heuristics_update_scores(f.k);
  kernel_assign_decision(f.k, internalize(-1));
  assert(propagate_apply(f.k) == ApplyResult::Ok);
  kernel_assign_decision(f.k, internalize(5));
  kernel_assign_decision(f.k, internalize(6));
  const size_t roots_from = f.k.trail.size();
  f.add({-3}); f.add({-4});
  assert(propagate_apply(f.k) == ApplyResult::Ok);
  heuristics_consume_root_assignments(f.k, roots_from);
  const size_t old_trail = f.k.trail.size();
  default_prepare_reduction(f.k);
  assert(f.k.trail.size() < old_trail && !f.k.level);
  for (int external : {1, 2}) {
    const Var v = var_of(internalize(external));
    assert(f.k.assigned[v].level == 0);
    assert(!heuristics_var_active(f.k, v) && !heuristics_heap_contains(f.k, v));
  }
  const auto heap = f.k.h->scores.stack;
  f.entry = f.k.propagate_head;
  kernel_flush_large_watches(f.k);
  kernel_dense_connect(f.k, f.occ, true);
  assert(f.k.dense->large[internalize(7)].size() == 1);
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Ok);
  for (int external : {7, 8}) {
    const Lit lit = internalize(external);
    assert(f.k.dense->large[lit].empty());
    assert(f.k.dense->ordered[lit].empty());
    assert(bve_schedule_occurrence_count(f.k, lit) == 0);
    assert(heuristics_var_active(f.k, var_of(lit)));
  }
  assert(f.k.h->scores.stack == heap && kernel_propagation_fixpoint(f.k));
  kernel_check_invariants(f.k);
  f.model();
}

static void conflict_proof(unsigned kind, ProofFormat format,
                           const std::string &stem) {
  const bool keep = !stem.empty();
  const std::string path = keep ? stem + ".proof" :
      "/tmp/tactix-forward-caller-" + std::to_string(getpid()) + ".proof";
  Proof proof;
  assert(proof.open(path.c_str(), format));
  Fixture f(&proof);
  f.seed(); f.add({-3, 4, 5}); f.add({-4});
  f.add({3, 10, 11}); // Deleted before reconnection; later refutation must check.
  if (kind == 0) f.add({-3, 4, -5});
  else if (kind == 1) {
    f.add({-3, 4, 6}); f.add({-5, -6});
  } else {
    f.add({-5, 6, 7}); f.add({-6}); f.add({-5, 6, -7});
  }
  f.partial();
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Unsat);
  assert(f.k.inconsistent && !f.k.conflict_valid);
  // Already inconsistent calls do not emit another refutation.
  const auto id = f.k.next_clause_id;
  assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Unsat);
  assert(f.k.next_clause_id == id);
  proof.flush();
  if (keep) {
    std::ofstream cnf(stem + ".cnf");
    cnf << "p cnf 16 " << f.inputs.size() << '\n';
    for (const auto &clause : f.inputs) {
      for (int lit : clause) cnf << lit << ' ';
      cnf << "0\n";
    }
  } else std::remove(path.c_str());
}

int main(int argc, char **argv) {
  scheduler_chain();
  direct_chain_and_empty_suffix();
  preconnection_cleanup(false);
  preconnection_cleanup(true);
  later_satisfaction_keeps_lazy_occurrence();
  reduce_roots_then_forward_reconnect();
  for (unsigned kind = 0; kind < 3; ++kind)
    for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
      const std::string stem = argc > 1 ? std::string(argv[1]) + "/caller-" +
          std::to_string(kind) + (format == ProofFormat::Lrat ? "-lrat" : "-drat") : "";
      conflict_proof(kind, format, stem);
    }
  std::puts("PASS: next-slot closure, dense chain, empty suffix, three preconnection/lazy-order cases, six conflict proof cases");
}

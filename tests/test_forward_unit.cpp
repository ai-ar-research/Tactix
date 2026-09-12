// C2 continuation: actual mid-round units, the partial connection image,
// candidate root rechecks, proof-safe reasons, and full-propagation debt.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unistd.h>

using namespace tactix;

struct Fixture {
  Kernel k;
  std::vector<std::vector<int>> inputs;
  uint32_t entry = 0;
  explicit Fixture(Proof *proof = nullptr) {
    kernel_reserve_vars(k, 16);
    k.proof = proof;
    if (proof) proof->bind_variable_map(&k.internal_to_external);
  }
  void add(std::initializer_list<int> external) {
    inputs.emplace_back(external);
    std::vector<Lit> lits;
    for (int lit : external) lits.push_back(internalize(lit));
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
  void enter(bool dense) {
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(3)) == 0);
    entry = k.propagate_head;
    if (dense) kernel_flush_large_watches(k);
  }
  int finish(bool dense) {
    if (dense) {
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
};

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

static void binary_chain(bool dense) {
  Fixture f;
  f.seed();
  f.add({-3, 6}); f.add({-6, 7});
  f.add({8, 9, 10}); f.add({8, 9});
  f.enter(dense);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.trail.size() == f.entry + 3);
  assert(f.k.trail[f.entry] == internalize(3));
  assert(f.k.trail[f.entry + 1] == internalize(6));
  assert(f.k.trail[f.entry + 2] == internalize(7));
  assert(f.k.assigned[var_of(internalize(6))].reason == UNIT_REASON);
  assert(f.k.stats.strengthen_subsumed == 1 && f.k.strengthen_complete);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void connected_large_chain(bool dense) {
  Fixture f;
  f.add({4, 5, 6}); // connect before the unit-producing candidate
  f.seed();
  f.add({-3, -6}); f.add({-4});
  f.enter(dense);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.trail.size() == f.entry + 3);
  assert(f.k.trail[f.entry] == internalize(3));
  assert(f.k.trail[f.entry + 1] == internalize(-6));
  assert(f.k.trail[f.entry + 2] == internalize(5));
  assert(f.k.assigned[var_of(internalize(5))].reason == UNIT_REASON);
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

// Complete, independently checkable proof fixtures. A guarded contradiction
// has no initial unit or complementary binary pair; it becomes productive
// only after the targeted SSR unit and optional connected propagation chain.
static void proof_case(bool dense, unsigned chain, ProofFormat format,
                       const std::string &stem) {
  char temporary[] = "/tmp/tactix-forward-unit-XXXXXX";
  const bool keep = !stem.empty();
  const int fd = keep ? -1 : mkstemp(temporary);
  assert(keep || fd >= 0);
  if (fd >= 0) close(fd);
  const std::string path = keep ? stem + ".proof" : temporary;
  Proof proof;
  assert(proof.open(path.c_str(), format));
  Fixture f(&proof);
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
  assert(result == ApplyResult::Unsat || f.finish(dense) == 20);
  assert(f.k.inconsistent);
  proof.flush();
  if (keep) {
    std::ofstream cnf(stem + ".cnf");
    cnf << "p cnf 16 " << f.inputs.size() << '\n';
    for (const auto &clause : f.inputs) {
      for (int lit : clause) cnf << lit << ' ';
      cnf << "0\n";
    }
  } else {
    std::remove(path.c_str());
  }
}

int main(int argc, char **argv) {
  for (bool dense : {false, true}) {
    for (bool wide : {false, true}) independent_tail(dense, wide);
    for (bool satisfied : {false, true}) roots_change_later_candidate(dense, satisfied);
    binary_chain(dense);
    connected_large_chain(dense);
    quota_after_unit(dense);
    quota_equality_admits_tail(dense);
    for (unsigned chain = 0; chain != 4; ++chain)
      for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
        const std::string stem = argc > 1 ? std::string(argv[1]) + "/forward-" +
            std::to_string(dense) + "-" + std::to_string(chain) +
            (format == ProofFormat::Lrat ? "-lrat" : "-drat") : "";
        proof_case(dense, chain, format, stem);
      }
  }
  explicit_instance_stays_single_action();
  std::puts("PASS forward units: 17 state/model cases and16 proof cases");
}

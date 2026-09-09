// Retained root reasons must not shrink the Reduce candidate population.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
using namespace tactix;

static const RuleVTable &rule() {
  const int id = tactix_rule_id("Reduce");
  assert(id >= 0);
  return tactix_rules()[id];
}

static void pure_candidates(Kernel &k, std::vector<Ref> &out) {
  const auto arena = k.arena;
  const std::vector<Lit> trail(k.trail.begin(), k.trail.end());
  unsigned char stats[sizeof(Stats)];
  std::memcpy(stats, &k.stats, sizeof stats);
  const auto id = k.next_clause_id;
  reduce_candidates(k, out);
  uint64_t instances[8];
  assert(rule().instances(k, instances, 0) == 0);
  const auto n = rule().instances(k, instances, 8);
  assert(n == out.size() + 1);
  for (size_t count = 0; count <= out.size(); ++count)
    assert(std::find(instances, instances + n, count) != instances + n);
  assert(k.arena == arena && k.trail == trail && k.next_clause_id == id);
  assert(!std::memcmp(stats, &k.stats, sizeof stats));
}

static void root_reason(bool elevated, bool compact, bool unsat,
                        ProofFormat format, const std::string &stem) {
  const bool keep = !stem.empty();
  const std::string path = keep ? stem + ".proof" :
      "/tmp/tactix-root-lock-" + std::to_string(getpid()) + ".proof";
  Proof proof;
  assert(proof.open(path.c_str(), format));
  Kernel k;
  kernel_reserve_vars(k, 8);
  ScopedHeuristics h(k);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  std::vector<std::vector<int>> input = {{1,2,3,4}, {1,2,3,-4}, {-1}, {-2}};
  if (unsat) {
    input.push_back({-3,5,6}); input.push_back({-3,5,-6});
    input.push_back({-3,-5,6}); input.push_back({-3,-5,-6});
  } else input.push_back({-3,5,6});
  for (const auto &clause : input) {
    std::vector<Lit> lits;
    for (int lit : clause) lits.push_back(internalize(lit));
    const auto id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  const Lit learned[] = {internalize(1),internalize(2),internalize(3)};
  const uint64_t chain[] = {1,2};
  const auto learned_id = ++k.next_clause_id;
  proof.add_rup(learned_id, learned, 3, chain, 2);
  const Ref ref = kernel_add_large_clause(k, learned, 3, true, 3, learned_id);
  clause_at(k.arena, ref)->set_used(3);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.assigned[2].level == 0 && !k.assigned[2].binary && k.assigned[2].reason == ref);
  assert(k.value(internalize(3)) > 0 && !k.unflushed_root);
  if (format == ProofFormat::Lrat) assert(k.unit_ids[2] > learned_id);
  if (elevated) {
    kernel_assign_decision(k, internalize(7));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  reduce_prepare(k);
  assert(k.level == unsigned(elevated));
  std::vector<Ref> candidates;
  pure_candidates(k, candidates);
#ifdef TACTIX_EXPECT_OLD_REDUCE_ROOT_LOCK
  assert(candidates.empty());
  assert(reduce_apply_refs(k, &ref, 1, compact) == ApplyResult::NoOp);
  assert(k.stats.deleted == 0);
#else
  assert(candidates == std::vector<Ref>{ref});
  assert(reduce_apply_refs(k, &ref, 1, compact) == ApplyResult::Ok);
  assert(k.stats.deleted == 1 && k.assigned[2].reason == UNIT_REASON);
#endif
  assert(k.value(internalize(3)) > 0 && k.assigned[2].level == 0);
  if (elevated) assert(k.value(internalize(7)) > 0);
  kernel_check_invariants(k);
  assert(default_solve(k) == (unsat ? 20 : 10));
  if (!unsat) {
    kernel_extend_model(k);
    for (const auto &clause : input) {
      bool satisfied = false;
      for (int lit : clause) satisfied |= k.model_values[internalize(lit)] > 0;
      assert(satisfied);
    }
  }
  proof.flush();
  if (keep) {
    std::ofstream cnf(stem + ".cnf");
    cnf << "p cnf 8 " << input.size() << '\n';
    for (const auto &clause : input) {
      for (int lit : clause) cnf << lit << ' ';
      cnf << "0\n";
    }
  } else std::remove(path.c_str());
  std::printf("root elevated=%d compact=%d unsat=%d lrat=%d candidates=%zu deleted=%llu PASS\n",
              elevated, compact, unsat, format == ProofFormat::Lrat, candidates.size(),
              (unsigned long long)k.stats.deleted);
}

static void protected_reasons(bool pending) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics h(k);
  const Lit lits[] = {internalize(1),internalize(2),internalize(3)};
  const Ref ref = kernel_add_large_clause(k, lits, 3, true, 9);
  std::vector<Ref> candidates;
  pure_candidates(k, candidates);
  assert(candidates == std::vector<Ref>{ref});
  if (pending) {
    k.pending.valid = true;
    k.pending.ref = ref;
    k.pending.lits.assign(lits, lits + 3);
    k.pending.id = clause_at(k.arena, ref)->id();
  } else {
    kernel_assign_decision(k, internalize(-1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(-2));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.assigned[2].level == 2 && k.assigned[2].reason == ref);
  }
  pure_candidates(k, candidates);
  assert(candidates.empty());
  // Revalidate a payload captured before it became locked.
  assert(reduce_apply_refs(k, &ref, 1) == ApplyResult::NoOp);
  assert(!clause_at(k.arena, ref)->garbage() && !k.stats.deleted);
  assert(rule().apply_instance(k, 1) == ApplyResult::NoOp);
  k.pending.valid = false;
  assert(reduce_guard(k));
  k.conflict_valid = true; assert(!reduce_guard(k)); k.conflict_valid = false;
  k.inconsistent = true; assert(!reduce_guard(k)); k.inconsistent = false;
  kernel_check_invariants(k);
  std::printf("protected pending=%d stale payload PASS\n", pending);
}

int main(int argc, char **argv) {
  protected_reasons(false);
  protected_reasons(true);
  for (bool elevated : {false,true}) for (bool compact : {false,true})
    for (bool unsat : {false,true}) for (auto format : {ProofFormat::DratAscii,ProofFormat::Lrat}) {
      const std::string tag = "root-" + std::to_string(elevated) + "-" +
          std::to_string(compact) + "-" + std::to_string(unsat) +
          (format == ProofFormat::Lrat ? "-lrat" : "-drat");
      root_reason(elevated, compact, unsat, format,
                  argc > 1 && unsat ? std::string(argv[1]) + "/" + tag : "");
    }
  puts("PASS: root admission, pure public surface, protected/stale reasons, models and proofs");
}

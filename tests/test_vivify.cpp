// C3 Vivify unit tests: the three probe outcomes (implied literal,
// implied negation, probe conflict), root-satisfied deletion, LCM tier
// scope, instance surface, and exact state restoration.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/vivify/vivify.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static uint32_t live_large(Kernel &k, const Lit *lits = nullptr,
                           uint32_t n = 0) {
  uint32_t count = 0;
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      count++;
      if (lits && c->size == n) {
        bool all = true;
        for (uint32_t i = 0; i < n; i++) {
          bool has = false;
          for (const Lit l : *c)
            has |= (l == lits[i]);
          all &= has;
        }
        found |= all;
      }
    }
    ref += clause_words(c->size);
  }
  if (lits)
    assert(found);
  return count;
}

static void test_implied_literal_strengthens() {
  // C = (1 2 3) with binary (1 2): assuming ¬1 propagates 2 — C distills
  // to (1 2), stored as a fresh binary; C dies
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(vivify_guard(k));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid); // state restored
  assert(default_solve(k) == 10);
}

static void test_implied_negation_removes() {
  // C = (1 2 3) with binary (1 -2): assuming ¬1 propagates ¬2 — literal 2
  // is redundant in C; C distills to (1 3)
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0); // (1 3) became a binary
  assert(default_solve(k) == 10);
}

static void test_probe_conflict_strengthens() {
  // C = (1 2 3) with binaries (1 4) and (2 -4): assuming ¬1 gives 4,
  // assuming ¬2 gives ¬4 — conflict, so (1 2) is RUP and replaces C
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_binary_clause(k, internalize(2), internalize(-4));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_lcm_scope_and_satisfied_delete() {
  // a redundant tier-2 clause IS vivified (LCM scope); a redundant clause
  // beyond tier2 is not a candidate. A root-satisfied candidate is
  // deleted through the instance surface.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r1 = kernel_add_large_clause(k, c, 3, true, /*glue=*/2);
  const Lit d[3] = {internalize(1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, d, 3, true, /*glue=*/9); // beyond tier2
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  std::vector<Ref> cands;
  vivify_candidates(k, cands);
  assert(cands.size() == 1 && cands[0] == r1); // LCM scope: tier2 only
  assert(vivify_apply(k) == ApplyResult::Ok);  // r1 distilled to (1 2)
  assert(k.stats.vivified == 1);

  // satisfied deletion via the instance surface
  Kernel k2;
  kernel_reserve_vars(k2, 3);
  kernel_assign_unit(k2, internalize(1));
  const Lit e[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r2 = kernel_add_large_clause(k2, e, 3, false, 0);
  assert(propagate_apply(k2) == ApplyResult::Ok);
  assert(vivify_try(k2, r2) == ApplyResult::Ok);
  assert(k2.stats.vivify_deleted == 1);
  assert(live_large(k2) == 0);
}

static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref cref = kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Vivify");
  assert(id >= 0 && rules[id].tier == RuleTier::Inprocess);
  uint64_t insts[4];
  const uint32_t n = rules[id].instances(k, insts, 4);
  assert(n == 1 && insts[0] == cref);
  assert(rules[id].apply_instance(k, insts[0]) == ApplyResult::Ok);
  assert(k.stats.vivified == 1);
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp);
}

int main() {
  test_implied_literal_strengthens();
  test_implied_negation_removes();
  test_probe_conflict_strengthens();
  test_lcm_scope_and_satisfied_delete();
  test_instance_surface();
  puts("test_vivify OK");
  return 0;
}

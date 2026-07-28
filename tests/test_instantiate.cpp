// X4 Instantiate unit tests: the success shape (a rare literal removed after
// the assume-and-propagate probe conflicts), each of cadical's three candidate
// filters (occurrence limit, minimum size, "avoid learning units"), the
// no-conflict failure, model preservation, and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/instantiate/instantiate.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
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

static const Clause *find_clause(const Kernel &k, uint32_t size) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == size)
      return c;
    ref += clause_words(c->size);
  }
  return nullptr;
}

// THE SHAPE THE RULE EXISTS FOR. C = (1 ∨ 2 ∨ 3 ∨ 5) with literal 1 occurring
// nowhere else; (¬1 ∨ 4) and (¬1 ∨ ¬4) make {1} propagate to a conflict, so
// F ⊨ ¬1 ∨ (2 ∨ 3 ∨ 5), which resolves against C to give (2 ∨ 3 ∨ 5).
// Note ¬1 is NOT root-derivable by propagation — only by resolution — so this
// really is the instantiation test doing the work.
static void test_removes_rare_literal() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  addn(k, {1, 2, 3, 5});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
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

// The same success shape at the size boundary: a TERNARY candidate strengthens
// to a binary, which this kernel stores outside the arena (kernel_replace_
// clause's Binary outcome) — the path the quaternary case does not exercise.
static void test_ternary_becomes_binary() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2, 3});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  assert(instantiate_apply(k) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
  assert(find_clause(k, 3) == nullptr);         // candidate gone from the arena
  assert(k.binary_ids.size() >= 1);             // ... and living as a binary
  assert(default_solve(k) == 10);
}

// OCCURRENCE LIMIT (cadical instantiateocclim = 1): literal 1 now occurs in a
// second clause, so no literal of either clause is both rare and provable —
// the probes run and none of them fires.
static void test_occurrence_limit_rejects() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  addn(k, {1, 2, 3});
  addn(k, {1, 5, 6});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  assert(instantiate_apply(k) == ApplyResult::NoOp);
  assert(k.stats.instantiated == 0);
  assert(k.stats.instantiate_checked > 0); // the rare literals WERE probed
}

// "AVOID LEARNING UNITS" (cadical's `unassigned < 3`): with 3 falsified at the
// root the candidate has only two unassigned literals, so it is not a
// candidate at all — even though literal 1 is still rare and still provable.
static void test_unit_avoidance() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2, 3});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  kernel_assign_unit(k, internalize(-3));
  assert(!kernel_propagate(k));
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// MINIMUM CLAUSE SIZE: binaries are never candidates (INST_CLSLIM = 3), which
// is also what keeps the rule out of C2 Strengthen's territory.
static void test_binary_not_a_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// NO CONFLICT ⇒ NO REWRITE: the probe simply fails and the formula is
// untouched (the common case — this is a test, not a theorem prover).
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

// REDUNDANT CLAUSES ARE OUT OF SCOPE (deviation 1): the same shape with the
// candidate learned instead of original is left to C3 Vivify.
static void test_redundant_out_of_scope() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit l[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, l, 3, /*redundant=*/true, /*glue=*/2);
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// INSTANCE SURFACE (§5.1 v1): the enumeration yields the candidate's Ref, and
// applying that payload does exactly what the canonical round does.
static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2, 3});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  const std::vector<RuleVTable> &rules = tactix_rules();
  const RuleVTable *v = nullptr;
  for (const RuleVTable &r : rules)
    if (!std::strcmp(r.name, "Instantiate"))
      v = &r;
  assert(v && "the rule must be registry-visible (N1)");
  assert(v->guard(k));
  uint64_t out[8];
  const uint32_t n = v->instances(k, out, 8);
  assert(n == 1);
  assert(v->apply_instance(k, out[0]) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
  assert(default_solve(k) == 10);
}

// GUARD: off the root the rule is not applicable (D-ROOT).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2, 3});
  assert(instantiate_guard(k));
  k.level = 1;
  assert(!instantiate_guard(k));
  k.level = 0;
  k.inconsistent = true;
  assert(!instantiate_guard(k));
}

int main() {
  test_removes_rare_literal();
  test_ternary_becomes_binary();
  test_occurrence_limit_rejects();
  test_unit_avoidance();
  test_binary_not_a_candidate();
  test_probe_failure_is_a_noop();
  test_redundant_out_of_scope();
  test_instance_surface();
  test_guard();
  printf("test_instantiate: all passed\n");
  return 0;
}

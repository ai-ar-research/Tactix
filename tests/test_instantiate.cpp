// X4 Instantiate: the success shape (a rare literal removed after the
// assume-and-propagate probe conflicts), cadical's three candidate filters,
// model preservation, and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/instantiate/instantiate.hpp"
#include "rules/propagate/propagate.hpp"
#include "proof/proof.hpp"

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
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == size)
      return c;
    ref += clause_actual_words(c);
  }
  return nullptr;
}

static Kernel two_choice_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  addn(k, {1, 2, 3, 6});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  addn(k, {-2, 5});
  addn(k, {-2, -5});
  return k;
}

// The shape the rule exists for: literal 1 is rare and {1} propagates to a
// conflict, so F ⊨ ¬1 ∨ (2 ∨ 3 ∨ 5) resolves against C to drop 1 — and ¬1 is
// not root-derivable by propagation, so instantiation is doing the work.
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

// At the size boundary a ternary becomes a binary, which lives outside the
// arena (`kernel_replace_clause`'s Binary outcome).
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

// The complete negated strengthened clause already assumes -3. Its named
// root unit can occur in the probe's reason walk, but is not a legal LRAT
// propagation hint for the new clause (AM358-v2 H3's Instantiate class).
static void test_lrat_omits_root_unit_in_cube() {
  const std::string path = "/tmp/tactix-instantiate-cube-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit candidate[3] = {internalize(1), internalize(2),
                            internalize(3)};
  const Lit reason_pos[3] = {internalize(-1), internalize(3),
                             internalize(4)};
  const Lit reason_neg[3] = {internalize(-1), internalize(3),
                             internalize(-4)};
  kernel_add_large_clause(k, candidate, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, reason_pos, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, reason_neg, 3, false, 0, ++k.next_clause_id);
  const uint64_t root_id = ++k.next_clause_id;
  k.unit_ids[var_of(internalize(-3))] = root_id;
  kernel_assign_unit(k, internalize(-3));
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::Lrat));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  assert(!kernel_propagate(k));
  // Prove this fixture actually reaches the offending reason-walk path;
  // otherwise an unchanged emitter would also pass the output assertion.
  Kernel witness = k;
  witness.proof = nullptr;
  kernel_probing_begin(witness);
  for (const Lit assumption : {internalize(1), internalize(-2)}) {
    witness.level++;
    witness.frames.push_back(static_cast<uint32_t>(witness.trail.size()));
    kernel_assign(witness, assumption, witness.level, DECISION_REASON, false);
  }
  assert(kernel_propagate_probing(witness));
  std::vector<uint8_t> seen(witness.nvars, 0);
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  const uint64_t conflict_id =
      kernel_conflict_reason_walk(witness, seen, entries);
  assert(conflict_id != 0);
  assert(std::any_of(entries.begin(), entries.end(),
                     [&](const auto &entry) { return entry.second == root_id; }));
  const Ref candidate_ref = find_ref(k, 3);
  assert(candidate_ref != INVALID_REF);
  assert(instantiate_try_literal(k, candidate_ref, internalize(1)) ==
         ApplyResult::Ok);
  proof.flush();
  k.proof = nullptr;

  std::ifstream input(path);
  std::string line;
  bool found = false;
  while (std::getline(input, line)) {
    std::istringstream fields(line);
    uint64_t id;
    if (!(fields >> id) || id != k.next_clause_id)
      continue;
    fields >> std::ws;
    if (fields.peek() == 'd')
      continue;
    int literal;
    while (fields >> literal && literal != 0) {}
    uint64_t hint;
    while (fields >> hint && hint != 0)
      assert(hint != root_id);
    found = true;
  }
  assert(found);
  std::remove(path.c_str());
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

// Minimum clause size: binaries are never candidates (INST_CLSLIM = 3).
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
  const Lit l[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, l, 3, /*redundant=*/true, /*glue=*/2);
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  std::vector<Ref> cands;
  instantiate_candidates(k, cands);
  assert(cands.empty());
  assert(instantiate_apply(k) == ApplyResult::NoOp);
}

// Instance surface (plan §5.1): the payload is the candidate's Ref.
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

// The v1 clause payload retains its historical first-success behavior, while
// the additive semantic surface makes either successful literal selectable.
static void test_semantic_pair_surface() {
  const RuleVTable *rule = nullptr;
  for (const RuleVTable &candidate : tactix_rules())
    if (!std::strcmp(candidate.name, "Instantiate"))
      rule = &candidate;
  assert(rule && rule->semantic_instances && rule->apply_semantic_instance);

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
      instantiate_pair_of_instance(semantic[i], &ref, &literal);
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

// Guard: off the declared root boundary the rule is not applicable.
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

// N1: the touched-epoch filter is policy-only. With the watermark advanced the
// round declines, but `instances()` and `apply_instance` stay complete.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2, 3});
  addn(k, {-1, 4});
  addn(k, {-1, -4});
  k.instantiate_touch_mark = k.touch_epoch;
  assert(instantiate_apply(k) == ApplyResult::NoOp);
  assert(k.stats.instantiate_mark_skipped == 1);
  const std::vector<RuleVTable> &rules = tactix_rules();
  const RuleVTable *v = nullptr;
  for (const RuleVTable &r : rules)
    if (!std::strcmp(r.name, "Instantiate"))
      v = &r;
  assert(v && v->guard(k));
  uint64_t out[8];
  assert(v->instances(k, out, 8) == 1); // not narrowed by the mark
  assert(v->apply_instance(k, out[0]) == ApplyResult::Ok);
  assert(k.stats.instantiated == 1);
}

// Both interrupt sites. The literal-loop site sits past a failed probe's unwind
// and scope close, so the kernel must be left at a quiescent root, still SAT.
static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  { // the round loop over candidate clauses
    Kernel k;
    kernel_reserve_vars(k, 5);
    addn(k, {1, 2, 3, 5});
    addn(k, {-1, 4});
    addn(k, {-1, -4});
    KernelStopPublisher pub(k, &stop);
    assert(instantiate_apply(k) == ApplyResult::NoOp);
    assert(k.stats.instantiated == 0);
    assert(k.stats.instantiate_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
    assert(k.level == 0 && !k.inconsistent);
  }
  { // the per-candidate literal loop, through the single-instance entry
    Kernel k;
    kernel_reserve_vars(k, 5);
    addn(k, {1, 2, 3, 5});
    addn(k, {-1, 4});
    addn(k, {-1, -4});
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
    kernel_reserve_vars(k, 5);
    addn(k, {1, 2, 3, 5});
    addn(k, {-1, 4});
    addn(k, {-1, -4});
    const Ref cref = find_ref(k, 4);
    assert(instantiate_try(k, cref) == ApplyResult::Ok);
    assert(k.stats.instantiated == 1 && k.stats.instantiate_interrupted == 0);
    assert(default_solve(k) == 10);
  }
}

int main() {
  test_interrupt();
  test_removes_rare_literal();
  test_touch_filter_is_policy_only();
  test_ternary_becomes_binary();
  test_lrat_omits_root_unit_in_cube();
  test_occurrence_limit_rejects();
  test_unit_avoidance();
  test_binary_not_a_candidate();
  test_probe_failure_is_a_noop();
  test_redundant_out_of_scope();
  test_instance_surface();
  test_semantic_pair_surface();
  test_guard();
  printf("test_instantiate: all passed\n");
  return 0;
}

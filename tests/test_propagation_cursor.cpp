#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <unistd.h>
using namespace tactix;

static uint64_t digest = 1469598103934665603ull;
static void add(uint64_t n) { digest = (digest ^ n) * 1099511628211ull; }
static Lit lit(int n) { return internalize(n); }
static Ref clause(Kernel &k, std::initializer_list<int> input) {
  std::vector<Lit> ls;
  for (int n : input) ls.push_back(lit(n));
  const uint64_t id = ++k.next_clause_id;
  if (ls.size() == 2) {
    kernel_add_binary_clause(k, ls[0], ls[1], id);
    return INVALID_REF;
  }
  return kernel_add_large_clause(k, ls.data(), ls.size(), false, 0, id);
}
static void snapshot(const Kernel &k, bool conflict) {
  add(conflict); add(k.propagate_head); add(k.binary_only_head);
  add(k.conflict_valid); add(k.conflict_ref);
  if (k.conflict_valid && k.conflict_ref == INVALID_REF)
    for (Lit l : k.conflict_bin) add(l);
  add(k.level); add(k.unassigned); add(k.unassigned_active);
  add(k.unflushed_root); add(k.stats.propagations); add(k.stats.ticks);
  add(k.stats.search_ticks); add(k.stats.root_assignments);
  add(k.stats.jumped_reasons); add(k.delayed_used);
  for (Lit l : k.trail) {
    add(l);
    const Assigned &a = k.assigned[var_of(l)];
    add(a.level); add(a.trail); add(a.reason); add(a.binary); add(a.jumped);
  }
  for (auto v : k.values) add(v);
  for (auto v : k.arena) add(v);
  for (const auto &h : k.watches.heads) {
    add(h.offset); add(h.size); add(h.capacity); add(h.binary_count_and_order);
  }
  for (auto v : k.watches.arena) add(v);
  add(k.watches.holes);
}

static void fixture(unsigned kind, bool root, bool ignore, bool debt,
                    bool inprocessing, unsigned length) {
  Kernel k;
  kernel_reserve_vars(k, length + 4);
  k.inprocessing = inprocessing;
  Ref target = clause(k, {static_cast<int>(length + 1),
                         static_cast<int>(length + 2),
                         static_cast<int>(length + 3)});
  auto assign = [&](int n) {
    if (root) {
      k.unit_ids[var_of(lit(n))] = ++k.next_clause_id;
      kernel_assign_unit(k, lit(n));
    } else kernel_assign_decision(k, lit(n));
  };
  if (kind == 1) {
    for (unsigned i = 1; i < length; ++i)
      clause(k, {-static_cast<int>(i), static_cast<int>(i + 1)});
    assign(1);
  } else if (kind == 2 || kind == 3 || kind == 6) {
    target = clause(k, {-1, -2, kind == 3 ? -3 : 3});
    assign(1); assign(2);
    if (kind == 3) assign(3);
    if (kind == 6) kernel_delete_clause(k, clause_at(k.arena, target));
  } else if (kind == 4) {
    clause(k, {-1, -2});
    assign(1); assign(2); assign(3);
  } else if (kind == 5) {
    target = clause(k, {-1, 2, 3, 4});
    assign(1);
  }
  // The used trail grows within ReservedTrail's preallocated backing store.
  assert(k.trail.capacity() >= length + 4);
  if (debt) {
    const bool binary_conflict = kernel_propagate_binary(k);
    assert(binary_conflict == (kind == 4));
    k.conflict_valid = false; // the full scan must rediscover a binary conflict
  }
  const uint32_t start = std::min(k.propagate_head, k.binary_only_head);
  // The guard is the cursor contract: work is pending exactly when the trail
  // beyond the settled cursor (binary-only debt included) is unconsumed.
  assert(propagate_guard(k).eligible == (start < k.trail.size()));
  const uint64_t previous = k.stats.propagations;
  const bool conflict = kernel_propagate(k, ignore ? target : INVALID_REF);
  const bool expected_conflict = kind == 4 || (kind == 3 && !ignore);
  assert(conflict == expected_conflict && k.conflict_valid == conflict);
  assert(k.binary_only_head == UINT32_MAX && !k.delayed_used);
  assert(k.stats.propagations - previous == k.propagate_head - start);
  if (conflict) assert(k.propagate_head == 1);
  else assert(k.propagate_head == k.trail.size());
  if (kind == 1) {
    assert(k.trail.size() == length);
    for (unsigned i = 1; i <= length; ++i) assert(k.value(lit(i)) > 0);
  }
  if (kind == 2) assert(k.value(lit(3)) == (ignore ? 0 : 1));
  if (kind == 6) assert(!k.value(lit(3)));
  if (kind == 5) assert(clause_at(k.arena, target)->searched == 2);
  snapshot(k, conflict);
  if (!conflict) {
    const uint64_t propagated = k.stats.propagations, ticks = k.stats.ticks;
    assert(!kernel_propagate(k, ignore ? target : INVALID_REF));
    assert(k.stats.propagations == propagated && k.stats.ticks == ticks);
    snapshot(k, false);
  }
  // Registered Propagate retains its canonical action and guard contract. It
  // has no instance surface: both raw slots are null by the registry contract
  // (RuleVTable::instances), and the canonical-only enumeration lives in the
  // API's tactix_rule_instances, pinned by tests/test_api_instances.c. A raw
  // slot is therefore never called from a fixture.
  const auto &rule = tactix_rules()[tactix_rule_id("Propagate")];
  assert(rule.guard == propagate_guard && rule.apply == propagate_apply);
  assert(!rule.instances && !rule.apply_instance);
  // Every exit above leaves the guard false: a conflict exit records the
  // conflict, and a fixpoint exit publishes the fully consumed trail.
  assert(!propagate_guard(k));
}

// The canonical instance through the registry dispatch that tactix_apply and
// tactix_apply_instance(TACTIX_INSTANCE_CANONICAL) share: the guard admits the
// pending trail, the published cursor and count are the scanned literals, and
// a root conflict refutes.
static void registry_dispatch(bool refute) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const auto &rule = tactix_rules()[tactix_rule_id("Propagate")];
  clause(k, {-1, 2});
  if (refute) clause(k, {-1, -2});
  k.unit_ids[var_of(lit(1))] = ++k.next_clause_id;
  kernel_assign_unit(k, lit(1));
  assert(rule.guard(k));
  const ApplyResult result = apply_rule(rule, k);
  assert(result == (refute ? ApplyResult::Unsat : ApplyResult::Ok));
  assert(k.inconsistent == refute && !k.conflict_valid);
  assert(k.propagate_head == (refute ? 1u : 2u) && k.stats.propagations ==
                                                       (refute ? 1u : 2u));
  assert(refute ? k.value(lit(2)) != 0 : k.value(lit(2)) > 0);
  assert(!rule.guard(k));
}

// Publishing an implied root fact for a later DSR step is proof bookkeeping.
// It must not populate unit_ids: that vector distinguishes solver-side
// asserted units and changing it perturbs subsequent simplification/search.
static void dsr_publication_is_semantically_inert() {
  char path[] = "/tmp/tactix-dsr-root-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  {
    Kernel k;
    kernel_reserve_vars(k, 2);
    Proof proof;
    assert(proof.open(path, ProofFormat::DsrAscii));
    k.proof = &proof;
    clause(k, {-1, 2});
    k.unit_ids[var_of(lit(1))] = ++k.next_clause_id;
    kernel_assign_unit(k, lit(1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    const Var derived = var_of(lit(2));
    assert(k.value(lit(2)) > 0);
    assert(k.unit_ids[derived] == 0);
    assert(k.dsr_root_values[derived] == 1);
    const uint64_t after_first_publication = k.next_clause_id;
    kernel_emit_root_units(k, 0);
    assert(k.next_clause_id == after_first_publication);
  }
  remove(path);
}

int main() {
  unsigned cases = 0;
  for (unsigned kind = 0; kind < 7; ++kind)
    for (bool root : {false, true})
      for (bool ignore : {false, true})
        for (bool debt : {false, true})
          for (bool inprocessing : {false, true})
            for (unsigned length : {32u, 257u}) {
              fixture(kind, root, ignore, debt, inprocessing, length);
              ++cases;
            }
  assert(cases == 224);
  registry_dispatch(false);
  registry_dispatch(true);
  dsr_publication_is_semantically_inert();
  std::printf("propagation-cursor: %u fixed cases, digest=%016llx\n", cases,
              static_cast<unsigned long long>(digest));
}

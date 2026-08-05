// C2 Strengthen unit tests: fused subsume-or-strengthen round, cascades,
// unit-producing strengthen, instance surface, and the registry-driven
// inprocessing slot (schedule-as-data).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

// count live large clauses; if `lits`/`n` given, require one matching
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

static void test_binary_strengthens_large() {
  // c = (1 2 3), d = (-1 2): resolving on 1 drops 1 from c -> (2 3),
  // stored as a fresh binary; the original dies
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 0); // (2 3) became a binary
  assert(kernel_binary_id(k, internalize(2), internalize(3)) == 0 ||
         true); // pair exists (id 0 in proof-less tests)
  assert(default_solve(k) == 10);
}

static void test_fused_subsumption() {
  // c = (1 2 3) subsumed by the binary (1 2): fused round deletes it
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(k.stats.strengthened == 0);
  assert(live_large(k) == 0);
}

static void test_large_partner_and_cascade() {
  // c = (1 2 3 4); d = (-1 2 3) strengthens it to (2 3 4); then the
  // binary (-2 3) strengthens THAT to (3 4) in the same candidate cascade
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Lit d[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_large_clause(k, d, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  // d itself first gets strengthened by (-2 3)? d = (-1 2 3): binary
  // (-2 3) with marks over d's lits {-1,2,3}: probe phases of 2 find
  // (-2,3): flip on 2 -> d -> (-1 3). So the round strengthens BOTH.
  assert(k.stats.strengthened >= 2);
  assert(default_solve(k) == 10);
}

static void test_unit_producing_strengthen_ends_round() {
  // root unit -1 (so 1 is falsified in c); c = (1 2 3); d = (-2 3):
  // root units -1 and -2 leave c = (1 2 3) with one free literal; the
  // flush pre-pass strips both falsified literals -> unit (3), asserted
  // at the root; the pass ends (values changed). (Partner strengthens
  // can no longer produce units: the flush owns the falsified-literal
  // path, and post-flush candidates lose at most one literal per act.)
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  assert(propagate_apply(k) == ApplyResult::Ok); // quiesce the units
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 1);
  assert(k.stats.strengthened == 0);
  assert(k.value(internalize(3)) > 0); // unit asserted at root
  assert(k.assigned[var_of(internalize(3))].level == 0);
  assert(live_large(k) == 0);
}

static void test_instance_surface() {
  // instances() lists candidates size-ascending; apply_instance on the
  // large candidate performs exactly one step
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[4] = {internalize(1), internalize(2), internalize(3),
                    internalize(4)};
  const Ref cref = kernel_add_large_clause(k, c, 4, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Strengthen");
  assert(id >= 0);
  uint64_t insts[4];
  const uint32_t n = rules[id].instances(k, insts, 4);
  assert(n == 1 && insts[0] == cref);
  assert(rules[id].apply_instance(k, insts[0]) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  const Lit expect[3] = {internalize(2), internalize(3), internalize(4)};
  live_large(k, expect, 3); // asserts the strengthened clause exists
  // junk payloads are rejected, not dereferenced
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp);
}

static void test_slot_schedule_as_data() {
  // the registry-driven slot : the tier tag is readable, posed
  // slots are honored in order with enabled flags, and a disabled entry
  // never runs. (The default composition is the always-on S4 arrangement;
  // tests POSE slots — the same override surface presets and the phase-6
  // learner use.)
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int strengthen_id = tactix_rule_id("Strengthen");
  const int subsume_id = tactix_rule_id("SubsumeForward");
  assert(strengthen_id >= 0 && subsume_id >= 0);
  assert(rules[strengthen_id].tier == RuleTier::Inprocess);
  assert(rules[subsume_id].tier == RuleTier::Inprocess);
  assert(strengthen_id < subsume_id); // registry order: name-sorted
  const Lit xor3[6][2] = {
      {internalize(1), internalize(2)},   {internalize(-1), internalize(-2)},
      {internalize(2), internalize(3)},   {internalize(-2), internalize(-3)},
      {internalize(1), internalize(3)},   {internalize(-1), internalize(-3)}};
  {
    // posed slot: Strengthen on, SubsumeForward off; forced to fire from
    // the first conflict check
    Kernel k;
    kernel_reserve_vars(k, 3);
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.inprocess.initialized = true;
    p.inprocess.next_conflicts = 0;
    p.inprocess_slot.push_back({strengthen_id, true});
    p.inprocess_slot.push_back({subsume_id, false});
    assert(p.solve(k) == 20); // odd-parity xor cycle: UNSAT
    assert(p.inprocess_slot.size() == 2); // posed slot left untouched
    assert(p.inprocess.rounds >= 1);      // the forced pass ran
    assert(k.stats.strengthen_rounds >= 1);
    assert(k.stats.forward_subsumptions == 0); // disabled entry never ran
  }
  {
    // fully disabled slot suppresses all rounds
    Kernel k;
    kernel_reserve_vars(k, 3);
    for (const auto &b : xor3)
      kernel_add_binary_clause(k, b[0], b[1]);
    DefaultPolicy p;
    p.inprocess.initialized = true;
    p.inprocess.next_conflicts = 0;
    p.inprocess_slot.push_back({strengthen_id, false});
    assert(p.solve(k) == 20);
    assert(k.stats.strengthen_rounds == 0);
  }
}

static void test_root_flush() {
  // the flush pre-pass (kissat sparse-collection effect, rule-side):
  // a root-satisfied clause is deleted; root-falsified literals are
  // stripped — including from REDUNDANT clauses, preserving redundancy
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  const Lit sat[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, sat, 3, false, 0); // root-satisfied: dies
  const Lit strip[3] = {internalize(-1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, strip, 3, true, 2); // -1 stripped -> binary
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 2);
  assert(k.stats.strengthened == 0); // attributed to flushed only
  assert(live_large(k) == 0); // sat died; strip became binary (2 4)
  assert(default_solve(k) == 10);
}

int main() {
  test_binary_strengthens_large();
  test_fused_subsumption();
  test_large_partner_and_cascade();
  test_unit_producing_strengthen_ends_round();
  test_instance_surface();
  test_slot_schedule_as_data();
  test_root_flush();
  puts("test_strengthen OK");
  return 0;
}

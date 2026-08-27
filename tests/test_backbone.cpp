// Backbone unit tests: binary-implication failed-literal detection, the
// binary-only restriction that distinguishes it from Probe, and the
// instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <string>

#include <unistd.h>

using namespace tactix;

// Binary propagation alone conflicts, so ¬1 is derived as a root unit.
static void test_binary_failed_literal() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(backbone_guard(k));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.stats.backbone_computations == 1);
  assert(k.stats.backbone_failed == 1);
  assert(k.stats.backbone_rounds >= 1);
  assert(k.value(internalize(-1)) > 0); // ¬1 asserted at root, un-propagated
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_canonical_computation_counter_advances() {
  Kernel k;
  kernel_reserve_vars(k, 1);
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(k.stats.backbone_computations == 2);
}

// The conflict-causing probe need not be the strongest failed literal.  Here
// 1 implies 2 and 2 forks to both 3 and ¬3.  Binary-UIP analysis learns ¬2;
// replaying it at root then propagates ¬1.  Independent probes learn only ¬1
// from the same first conflict and miss the reference algorithm's UIP unit.
static void test_chained_binary_uip_learns_internal_predecessor() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-2)) > 0);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.backbone_failed == 1);
  assert(k.level == 0 && !k.conflict_valid);
}

static void test_root_replay_is_outside_backbone_private_meter() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(-3));
  k.tick_sink = &k.stats.backbone_ticks;
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.tick_sink == &k.stats.backbone_ticks);
  k.tick_sink = nullptr;
  // The speculative probes contribute to both totals.  Replaying the derived
  // root unit contributes only to the global tick counter, as in Kissat.
  assert(k.stats.ticks > k.stats.backbone_ticks);
}

static void test_root_replay_propagates_large_clause_closure() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(-4));
  const Lit large[3] = {internalize(1), internalize(3), internalize(5)};
  kernel_add_large_clause(k, large, 3, false, 0);

  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(-3)) > 0);
  // Binary-only speculative propagation cannot derive 5; the post-round full
  // root replay must do so from (1 v 3 v 5) after learning -1 and -3.
  assert(k.value(internalize(5)) > 0);
  assert(k.assigned[var_of(internalize(5))].level == 0);
}

// `propagate_head` means "every clause this literal watches has been
// inspected".  Binary-only propagation moves that cursor while skipping every
// large watch, so a literal it covers that STAYS on the trail must not be
// credited as fully propagated: the next full propagation has to rescan it.
// Otherwise the trail stands at a claimed fixpoint that still hides a
// large-clause unit, and a total assignment built on it falsifies a clause.
static void test_binary_propagation_leaves_full_propagation_pending() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const Lit large[3] = {internalize(-1), internalize(-2), internalize(3)};
  kernel_add_large_clause(k, large, 3, false, 0);

  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0);
  assert(k.propagate_head == k.trail.size()); // the binary scan ran to its end
  assert(k.value(internalize(3)) == 0);       // the large watches were skipped
  assert(!kernel_propagation_fixpoint(k));
  assert(propagate_guard(k));

  assert(!kernel_propagate(k));
  assert(kernel_propagation_fixpoint(k));
  assert(k.value(internalize(3)) > 0);
  assert(k.assigned[var_of(internalize(3))].level == 0);
}

// Same obligation `kernel_propagate_probing` already carries: this speculative
// propagator can force a literal at level zero and never passes through
// `propagate_service`, so it must name that literal itself.  A root literal
// whose `unit_ids` entry is still 0 truncates every later chain citing it at
// LRAT's line terminator.
static void test_binary_propagation_names_its_root_units_for_lrat() {
  const std::string path = "/tmp/tactix-binprop-root-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  {
    Kernel k;
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    k.proof = &proof;
    kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(-1), internalize(2), 2);
    k.next_clause_id = 2;
    k.unit_ids[var_of(internalize(1))] = 1; // input unit (1) is clause 1
    kernel_assign_unit(k, internalize(1));

    assert(!kernel_propagate_binary(k));
    assert(k.value(internalize(2)) > 0);
    assert(k.assigned[var_of(internalize(2))].level == 0);
    assert(k.unit_ids[var_of(internalize(2))] != 0);
    proof.flush();
    k.proof = nullptr;
  }
  FILE *file = fopen(path.c_str(), "rb");
  assert(file);
  char bytes[64] = {};
  const size_t size = fread(bytes, 1, sizeof bytes, file);
  fclose(file);
  remove(path.c_str());
  // the input unit's id, then the binary it propagated through
  assert(std::string(bytes, size) == "3 2 0 1 2 0\n");
}

// A round retracts every literal it put on the speculative trail, so the
// replayed unit batch is propagated from the batch's own trail position.
// Leaving the level-zero stamps in place instead strands them behind the
// cursor with only their binary watches scanned, and a large clause watched
// solely by literals the round derived FIRST is then never revisited.
static void test_round_exit_rescans_every_speculative_root_literal() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-2), internalize(-4));
  const Lit large[3] = {internalize(1), internalize(2), internalize(5)};
  kernel_add_large_clause(k, large, 3, false, 0);

  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(-2)) > 0);
  // (1 v 2 v 5) is watched only by the two literals the round derived first,
  // and it is unit once both of them are root-false.
  assert(k.value(internalize(5)) > 0);
  assert(k.assigned[var_of(internalize(5))].level == 0);
  assert(kernel_propagation_fixpoint(k));
}

// Kissat retains the candidates falsified by one accumulated assumption trail
// and tries that smaller stack in the next round even when the first round
// found no unit. A fresh full scan, or stopping on an empty unit batch, misses
// this failure of -1.
static void test_falsified_candidate_survives_to_next_round() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.stats.backbone_rounds >= 2);
}

// A chained-UIP unit is not a root assignment until the complete round has
// flushed its speculative trail.  Here candidate 1 first succeeds, candidate
// 2 then fails and derives -2, and temporary -2 implies 3.  Under a one-round
// budget, Kissat retains the now-false -3 candidate and prioritizes it for the
// next computation.  Eagerly stamping -2 at root removes -3 instead.
static void test_chained_unit_stays_speculative_until_round_end() {
  bool exercised = false;
  for (uint64_t budget = 1; budget <= 256 && !exercised; budget++) {
    Kernel k;
    kernel_reserve_vars(k, 5);
    kernel_add_binary_clause(k, internalize(-2), internalize(5));
    kernel_add_binary_clause(k, internalize(-2), internalize(-5));
    kernel_add_binary_clause(k, internalize(2), internalize(3));
    k.work_budget = budget;
    k.tick_sink = &k.stats.backbone_ticks;
    const ApplyResult result = backbone_apply(k);
    k.tick_sink = nullptr;
    if (k.stats.backbone_failed != 1 || k.stats.backbone_rounds != 1)
      continue;
    exercised = true;
    assert(result == ApplyResult::Ok);
    assert(k.value(internalize(-2)) > 0);
    assert(k.value(internalize(3)) > 0);
    assert(k.backbone_priority[2] & 2u); // retained candidate -3
  }
  assert(exercised);
}

static void test_candidate_priority_persists_between_computations() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  k.work_budget = 1;
  k.tick_sink = &k.stats.backbone_ticks;
  assert(backbone_apply(k) == ApplyResult::NoOp);
  k.tick_sink = nullptr;
  assert(k.backbone_priority[0] || k.backbone_priority[1]);
  assert(!(k.backbone_priority[0] & 1u));
  assert(!(k.backbone_priority[1] & 1u));
}

static void test_candidate_priority_survives_variable_compaction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.backbone_priority = {1, 2, 3};
  k.var_status.assign(3, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  assert(kernel_compact_variables(k));
  assert(k.nvars == 2);
  assert(k.backbone_priority[0] == 1);
  assert(k.backbone_priority[1] == 3);
}

static void test_chained_binary_uip_lrat_order() {
  const std::string path = "/tmp/tactix-backbone-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  {
    Kernel k;
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    k.proof = &proof;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2), 1);
    kernel_add_binary_clause(k, internalize(-2), internalize(3), 2);
    kernel_add_binary_clause(k, internalize(-2), internalize(-3), 3);
    k.next_clause_id = 3;
    assert(backbone_apply(k) == ApplyResult::Ok);
    proof.flush();
    k.proof = nullptr;
  }
  FILE *file = fopen(path.c_str(), "rb");
  assert(file);
  char bytes[64] = {};
  const size_t size = fread(bytes, 1, sizeof bytes, file);
  fclose(file);
  remove(path.c_str());
  // Replaying the temporary -2 batch at root also emits its root implication
  // -1 with the derived unit and original binary as the LRAT chain.
  assert(std::string(bytes, size) ==
         "4 -2 0 2 3 0\n"
         "5 -1 0 4 1 0\n");
}

// The binary-only restriction: 1 fails only through the size-3 clauses, so
// Backbone finds nothing where Probe finds 1 failed — opposite by design.
static void test_binary_only_misses_large_clause_failure() {
  const Lit a[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit b[3] = {internalize(-1), internalize(-2), internalize(-3)};
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2
    kernel_add_large_clause(k, a, 3, false, 0);
    kernel_add_large_clause(k, b, 3, false, 0);
    assert(backbone_apply(k) == ApplyResult::NoOp); // binary-only sees nothing
    assert(k.stats.backbone_failed == 0);
    assert(k.stats.backbone_probed >= 1); // it tried: 1 and ¬2 are BIG heads
    assert(k.trail.empty());
  }
  { // identical formula: full-propagation Probe does find 1 failed
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_large_clause(k, a, 3, false, 0);
    kernel_add_large_clause(k, b, 3, false, 0);
    assert(probe_apply(k) == ApplyResult::Ok);
    assert(k.stats.probe_failed >= 1);
    assert(k.value(internalize(-1)) > 0);
  }
}

static void test_no_failed_literal_noop() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 → 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 → 3
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(k.stats.backbone_failed == 0);
  assert(k.trail.empty());
  assert(default_solve(k) == 10);
}

static void test_binary_meter_charges_scanned_prefix() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const Lit large[3] = {internalize(-1), internalize(3), internalize(4)};
  for (unsigned i = 0; i < 40; i++)
    kernel_add_large_clause(k, large, 3, false, 0);
  kernel_collect_garbage(k);
  assert(k.watches.heads[internalize(-1)].binaries_first());

  const uint64_t before = k.stats.ticks;
  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0);
  // Kissat fetches the first large watch before its early-stop type test, so
  // the charged prefix contains the binary and that large watch's first word.
  assert(k.stats.binprop_words == 2);
  assert(k.stats.ticks - before == 3); // two-word prefix, then an empty scan
}

static void test_binary_meter_charges_first_large_word() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit large[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, large, 3, false, 0);
  assert(k.watches.heads[internalize(-1)].binaries_first());

  const uint64_t before = k.stats.ticks;
  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.stats.binprop_words == 1);
  assert(k.stats.ticks - before == 2);
}

static void test_binary_meter_skips_conflicting_scan() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));

  const uint64_t ticks_before = k.stats.ticks;
  const uint64_t scans_before = k.stats.binprop_scans;
  const uint64_t words_before = k.stats.binprop_words;
  assert(kernel_propagate_binary(k));
  assert(k.conflict_valid);
  // Kissat returns directly from the conflicting binary watch, before the
  // Backbone scan contributes to its private meter or scan diagnostics.
  assert(k.stats.ticks == ticks_before);
  assert(k.stats.binprop_scans == scans_before);
  assert(k.stats.binprop_words == words_before);
}

static void test_binary_order_is_per_list() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit large[3] = {internalize(-1), internalize(3), internalize(4)};
  kernel_add_large_clause(k, large, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));

  const Lit affected = internalize(-1);
  const Lit unaffected = internalize(-5);
  assert(!k.watches.heads[affected].binaries_first());
  assert(k.watches.heads[unaffected].binaries_first());

  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0); // binary behind the large was scanned
}

static void test_chained_binary_implication_uses_current_level() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  assert(k.level == 2);
  assert(!kernel_propagate_binary(k, /*use_current_level=*/true));
  assert(k.value(internalize(3)) > 0);
  assert(k.assigned[var_of(internalize(3))].level == 2);
}

// both polarities fail via binaries: UNSAT once the derived batch propagates
static void test_both_polarities_fail_unsat() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(-3));
  assert(backbone_apply(k) == ApplyResult::Unsat);
  assert(k.stats.backbone_failed >= 1);
  assert(k.inconsistent);
}

static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Backbone");
  assert(id >= 0 && rules[id].tier == RuleTier::Inprocess);
  uint64_t insts[8];
  const uint32_t n = rules[id].instances(k, insts, 8);
  assert(n >= 1);
  assert(rules[id].apply_instance(k, static_cast<uint64_t>(1)) ==
         ApplyResult::Ok); // probing literal 1 derives ¬1
  assert(k.stats.backbone_failed == 1);
  assert(k.value(internalize(-1)) > 0);
  // invalid payloads are rejected (N1: apply_instance is total); a fresh
  // kernel, since the derivation above left k non-quiescent
  Kernel k2;
  kernel_reserve_vars(k2, 2);
  kernel_add_binary_clause(k2, internalize(-1), internalize(2));
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(999999)) ==
         ApplyResult::NoOp);
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(0)) ==
         ApplyResult::NoOp);
}

int main() {
  test_binary_failed_literal();
  test_canonical_computation_counter_advances();
  test_chained_binary_uip_learns_internal_predecessor();
  test_root_replay_is_outside_backbone_private_meter();
  test_root_replay_propagates_large_clause_closure();
  test_binary_propagation_leaves_full_propagation_pending();
  test_binary_propagation_names_its_root_units_for_lrat();
  test_round_exit_rescans_every_speculative_root_literal();
  test_falsified_candidate_survives_to_next_round();
  test_chained_unit_stays_speculative_until_round_end();
  test_candidate_priority_persists_between_computations();
  test_candidate_priority_survives_variable_compaction();
  test_chained_binary_uip_lrat_order();
  test_binary_only_misses_large_clause_failure();
  test_no_failed_literal_noop();
  test_binary_meter_charges_scanned_prefix();
  test_binary_meter_charges_first_large_word();
  test_binary_meter_skips_conflicting_scan();
  test_binary_order_is_per_list();
  test_chained_binary_implication_uses_current_level();
  test_both_polarities_fail_unsat();
  test_instance_surface();
  puts("test_backbone OK");
  return 0;
}

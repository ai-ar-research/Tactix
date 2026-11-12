// Backbone: binary-implication failed literals, the binary-only restriction
// that distinguishes it from Probe, Kissat's meters, and the instance surface.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rule_test_utils.hpp"
#include "rules/backbone/backbone.hpp"
#include "rules/probe/probe.hpp"
#include "rules/propagate/propagate.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"

// Binary propagation alone conflicts, so ¬1 is derived as a root unit.
TEST(test_binary_failed_literal) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2});
  assert(backbone_guard(k));
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.stats.backbone_computations == 1);
  assert(k.stats.backbone_failed == 1);
  assert(k.stats.backbone_rounds >= 1);
  assert(k.value(internalize(-1)) > 0); // ¬1 asserted at root, un-propagated
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

// 1 implies 2, which forks to 3 and ¬3. Binary-UIP analysis learns ¬2 (the
// reference algorithm's UIP unit), whose root replay propagates ¬1. The
// replay is charged to the global ticks only, as in Kissat.
TEST(test_chained_binary_uip_learns_internal_predecessor) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-2, 3});
  add_external_clause(k, {-2, -3});
  k.tick_sink = &k.stats.backbone_ticks;
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.tick_sink == &k.stats.backbone_ticks);
  k.tick_sink = nullptr;
  assert(k.stats.ticks > k.stats.backbone_ticks);
  assert(k.value(internalize(-2)) > 0);
  assert(k.value(internalize(-1)) > 0);
  assert(k.stats.backbone_failed == 1);
  assert(k.level == 0 && !k.conflict_valid);
}

TEST(test_root_replay_propagates_large_clause_closure) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2});
  add_external_clause(k, {-3, 4});
  add_external_clause(k, {-3, -4});
  add_external_clause(k, {1, 3, 5});

  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(-3)) > 0);
  // Only the post-round full root replay derives 5.
  assert(k.value(internalize(5)) > 0);
  assert(k.assigned[var_of(internalize(5))].level == 0);
}

// Binary-only propagation skips large watches, so a literal it covers stays
// pending for the next full propagation.
TEST(test_binary_propagation_leaves_full_propagation_pending) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2, 3});

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

// The replayed unit batch propagates from its own trail position, so a large
// clause watched only by literals the round derived first is revisited.
TEST(test_round_exit_rescans_every_speculative_root_literal) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {-1, 3});
  add_external_clause(k, {-1, -3});
  add_external_clause(k, {-2, 4});
  add_external_clause(k, {-2, -4});
  add_external_clause(k, {1, 2, 5});

  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(-1)) > 0);
  assert(k.value(internalize(-2)) > 0);
  // (1 v 2 v 5) is watched only by the two literals the round derived first,
  // and it is unit once both of them are root-false.
  assert(k.value(internalize(5)) > 0);
  assert(k.assigned[var_of(internalize(5))].level == 0);
  assert(kernel_propagation_fixpoint(k));
}

// -1 fails only in the second round, which retries the candidates the first
// round's assumption trail falsified (as Kissat does).
static void second_round_failure(Kernel &k) {
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {1, 3});
  add_external_clause(k, {1, -3});
}

TEST(test_falsified_candidate_survives_to_next_round) {
  Kernel k;
  second_round_failure(k);
  assert(backbone_apply(k) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0);
  assert(k.stats.backbone_rounds >= 2);
}

TEST(test_round_limits_are_strategy_parameters) {
  for (uint32_t maximum : {0u, 1u, 2u}) {
    Kernel k;
    second_round_failure(k);
    k.backbone_parameters = {maximum, maximum};
    const ApplyResult result = backbone_apply(k);
    assert(k.stats.backbone_rounds == maximum);
    if (maximum < 2) {
      assert(result == ApplyResult::NoOp);
      assert(k.value(internalize(1)) == 0);
    } else {
      assert(result == ApplyResult::Ok);
      assert(k.value(internalize(1)) > 0);
    }
  }

  Kernel growing;
  second_round_failure(growing);
  growing.backbone_parameters = {1, 2};
  assert(backbone_apply(growing) == ApplyResult::NoOp);
  assert(growing.stats.backbone_rounds == 1);
  assert(backbone_apply(growing) == ApplyResult::Ok);
  assert(growing.stats.backbone_rounds >= 2);
  assert(growing.value(internalize(1)) > 0);
}

// A chained-UIP unit stays speculative until the round ends: candidate 2
// fails and derives -2, which implies 3, and under a one-round budget the
// now-false -3 candidate is retained with priority (as in Kissat).
TEST(test_chained_unit_stays_speculative_until_round_end) {
  bool exercised = false;
  for (uint64_t budget = 1; budget <= 256 && !exercised; budget++) {
    Kernel k;
    kernel_reserve_vars(k, 5);
    add_external_clause(k, {-2, 5});
    add_external_clause(k, {-2, -5});
    add_external_clause(k, {2, 3});
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

TEST(test_candidate_priority_persists_between_computations) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  add_external_clause(k, {-1, 2});
  k.work_budget = 1;
  k.tick_sink = &k.stats.backbone_ticks;
  assert(backbone_apply(k) == ApplyResult::NoOp);
  k.tick_sink = nullptr;
  assert(k.backbone_priority[0] || k.backbone_priority[1]);
  assert(!(k.backbone_priority[0] & 1u));
  assert(!(k.backbone_priority[1] & 1u));
}

TEST(test_candidate_priority_survives_variable_compaction) {
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

TEST(test_chained_binary_uip_proof_order) {
  TemporaryFile file;
  {
    Kernel k;
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
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
  assert(read_file(file.path) == "-2 0\n-1 0\n");
}

// The binary-only restriction: 1 fails only through the size-3 clauses, so
// Backbone finds nothing where Probe finds 1 failed — opposite by design.
TEST(test_binary_only_misses_large_clause_failure) {
  for (bool probe : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    add_external_clause(k, {-1, 2});
    add_external_clause(k, {-1, -2, 3});
    add_external_clause(k, {-1, -2, -3});
    if (probe) {
      assert(probe_apply(k) == ApplyResult::Ok);
      assert(k.stats.probe_failed >= 1);
      assert(k.value(internalize(-1)) > 0);
    } else {
      assert(backbone_apply(k) == ApplyResult::NoOp);
      assert(k.stats.backbone_failed == 0);
      assert(k.stats.backbone_probed >= 1); // 1 and ¬2 are BIG heads
      assert(k.trail.empty());
    }
  }
}

TEST(test_no_failed_literal_noop) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2}); // 1 → 2
  add_external_clause(k, {-2, 3}); // 2 → 3
  assert(backbone_apply(k) == ApplyResult::NoOp);
  assert(k.stats.backbone_failed == 0);
  assert(k.trail.empty());
  assert(default_solve(k) == 10);
}

TEST(test_binary_meter_charges_scanned_prefix) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {-1, 2});
  for (unsigned i = 0; i < 40; i++)
    add_external_clause(k, {-1, 3, 4});
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

TEST(test_binary_meter_charges_first_large_word) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2, 3});
  assert(k.watches.heads[internalize(-1)].binaries_first());

  const uint64_t before = k.stats.ticks;
  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.stats.binprop_words == 1);
  assert(k.stats.ticks - before == 2);
}

TEST(test_binary_meter_skips_conflicting_scan) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2});
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

TEST(test_binary_order_is_per_list) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {-1, 3, 4});
  add_external_clause(k, {-1, 2});

  const Lit affected = internalize(-1);
  const Lit unaffected = internalize(-5);
  assert(!k.watches.heads[affected].binaries_first());
  assert(k.watches.heads[unaffected].binaries_first());

  kernel_assign_decision(k, internalize(1));
  assert(!kernel_propagate_binary(k));
  assert(k.value(internalize(2)) > 0); // binary behind the large was scanned
}

TEST(test_chained_binary_implication_uses_current_level) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 3});
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  assert(k.level == 2);
  assert(!kernel_propagate_binary(k, /*use_current_level=*/true));
  assert(k.value(internalize(3)) > 0);
  assert(k.assigned[var_of(internalize(3))].level == 2);
}

// both polarities fail via binaries: UNSAT once the derived batch propagates
TEST(test_both_polarities_fail_unsat) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2});
  add_external_clause(k, {1, 3});
  add_external_clause(k, {1, -3});
  assert(backbone_apply(k) == ApplyResult::Unsat);
  assert(k.stats.backbone_failed >= 1);
  assert(k.inconsistent);
}

TEST(test_instance_surface) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  add_external_clause(k, {-1, 2});
  add_external_clause(k, {-1, -2});
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
  add_external_clause(k2, {-1, 2});
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(999999)) ==
         ApplyResult::NoOp);
  assert(rules[id].apply_instance(k2, static_cast<uint64_t>(0)) ==
         ApplyResult::NoOp);
}

// Backbone retracts even temporary level-zero implications. Their cached
// unit ids name the retracted polarity and cannot be reused for a later value.
TEST(test_speculative_unwind_invalidates_unit_ids) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.unit_ids[0] = 10;
  kernel_assign_unit(k, internalize(1));
  const uint32_t prefix = k.trail.size();
  k.unit_ids[1] = 11;
  kernel_assign(k, internalize(-2), 0, UNIT_REASON, false);
  kernel_unwind_speculation(k, prefix);
  assert(k.value(internalize(1)) > 0 && k.unit_ids[0] == 10);
  assert(k.value(internalize(2)) == 0 && k.unit_ids[1] == 0);
}

// A root literal published to the DSR proof inside a speculative round is
// retracted with the round; the invariants accept that state once the
// round's replay has refuted the formula.
TEST(test_refuted_replay_may_leave_a_published_root_retracted) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  k.proof = &proof;
  const uint32_t prefix = k.trail.size();
  kernel_assign(k, internalize(-2), 0, UNIT_REASON, false);
  kernel_materialize_dsr_root_unit(k, internalize(-2));
  kernel_check_invariants(k);
  kernel_unwind_speculation(k, prefix);
  assert(k.value(internalize(2)) == 0);
  k.inconsistent = true; // the replay closed the refutation first
  kernel_check_invariants(k);
  k.proof = nullptr;
}

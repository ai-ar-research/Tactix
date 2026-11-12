// BVE: the round-local occurrence index, the candidate schedule, elimination
// and its bound, the gate reductions, and model reconstruction through the
// witness stack. Rule semantics live in the rule calculus.
#undef NDEBUG
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "occurrence_test_utils.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

struct BveScoreNotifications {
  std::vector<std::pair<Var, bool>> events;
};

static void record_bve_score_notification(Kernel &, void *state, Var v,
                                          bool reinsert) {
  static_cast<BveScoreNotifications *>(state)->events.emplace_back(v,
                                                                    reinsert);
}

// The dense BVE scan state: occurrence lists, the schedule occurrence image
// and score notifications, all detached on destruction.
struct ScanProbe {
  Kernel &k;
  OccIndex dense;
  std::vector<uint32_t> image;
  BveScoreNotifications notifications;
  explicit ScanProbe(Kernel &k) : k(k) { kernel_dense_connect(k, dense, true); }
  void snapshot() {
    image.assign(2 * k.nvars, 0);
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
      image[lit] = bve_schedule_occurrence_count(k, lit);
    k.bve_occurrence_count_image = &image;
  }
  void listen() {
    k.bve_score_update_hook = record_bve_score_notification;
    k.bve_score_update_state = &notifications;
  }
  ~ScanProbe() {
    k.bve_score_update_hook = nullptr;
    k.bve_score_update_state = nullptr;
    k.bve_occurrence_count_image = nullptr;
    kernel_dense_disconnect(k);
  }
};

// Cached physical dense-stack sizes (Kissat): binary slots change eagerly, a
// garbage large Ref counts until compaction, rewrites touch only moved literals.
static void test_schedule_occurrence_image_lifecycle() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  const Lit c = internalize(3), d = internalize(4);
  kernel_add_binary_clause(k, a, b);
  const Lit first_lits[3] = {a, c, d};
  const Ref first_ref = kernel_add_large_clause(k, first_lits, 3, false, 0);

  ScanProbe probe(k);
  probe.snapshot();
  assert(probe.image[a] == 2 && probe.image[b] == 1 && probe.image[c] == 1 && probe.image[d] == 1);

  kernel_add_binary_clause(k, a, b);
  assert(probe.image[a] == 3 && probe.image[b] == 2);
  assert(kernel_delete_binary_one_copy(k, a, b));
  assert(probe.image[a] == 2 && probe.image[b] == 1);
  assert(kernel_delete_binary_all_copies(k, a, b));
  assert(probe.image[a] == 1 && probe.image[b] == 0);

  const Ref second_ref =
      kernel_add_large_clause(k, first_lits, 3, false, 0);
  assert(probe.image[a] == 2 && probe.image[c] == 2 && probe.image[d] == 2);
  const Lit replacement[3] = {a, b, d};
  kernel_replace_large_clause_in_place(k, second_ref, ++k.next_clause_id,
                                       replacement, 3, 0);
  assert(probe.image[a] == 2 && probe.image[b] == 1 && probe.image[c] == 1 && probe.image[d] == 2);

  kernel_delete_clause(k, clause_at(k.arena, first_ref));
  assert(probe.image[a] == 2 && probe.image[c] == 1 && probe.image[d] == 2 &&
         "lazy large garbage remains a physical dense occurrence");
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    assert(bve_schedule_occurrence_count(k, lit) == probe.image[lit]);
}

// Garbage before an oversized clause is compacted (Kissat), with no rescore of
// the popped variable.
static void test_candidate_gather_compacts_garbage_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1);
  const Lit stale_lits[3] = {pivot, internalize(2), internalize(3)};
  const Lit live_lits[3] = {pivot, internalize(4), internalize(5)};
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);
  kernel_add_large_clause(k, live_lits, 3, false, 0);

  ScanProbe probe(k);
  probe.snapshot();
  kernel_delete_clause(k, clause_at(k.arena, stale));
  assert(probe.image[pivot] == 2 && probe.image[internalize(2)] == 1);

  // The remaining live ternary exceeds this diagnostic admission ceiling, so
  // the candidate fails after its physical occurrence stacks are inspected.
  k.bve_clause_size_limit = 2;
  probe.listen();
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(probe.image[pivot] == 1);
  assert(probe.image[internalize(2)] == 1 &&
         "only the inspected pivot stack is compacted");
  assert(large_occurrences(probe.dense, pivot).size() == 1 && large_occurrences(probe.dense, pivot)[0] != stale);
  bool stale_connected = false;
  for (const OccIndex::Entry &entry : probe.dense.ordered[pivot])
    stale_connected |= !entry.binary && entry.payload == stale;
  assert(!stale_connected);
  assert(!k.bve_retry[var_of(pivot)]);
  assert(probe.notifications.events.empty());
}

// occurrences_literal stops at the first live oversized clause: later garbage
// is untouched and the popped pivot is not rescored.
static void test_candidate_gather_stops_before_late_garbage() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1);
  const Lit oversized_lits[3] = {pivot, internalize(2), internalize(3)};
  const Lit stale_lits[3] = {pivot, internalize(4), internalize(5)};
  kernel_add_large_clause(k, oversized_lits, 3, false, 0);
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);

  ScanProbe probe(k);
  probe.snapshot();
  kernel_delete_clause(k, clause_at(k.arena, stale));
  assert(probe.image[pivot] == 2);

  probe.listen();
  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(probe.image[pivot] == 2);
  assert(large_occurrences(probe.dense, pivot).size() == 2);
  bool stale_connected = false;
  for (const OccIndex::Entry &entry : probe.dense.ordered[pivot])
    stale_connected |= !entry.binary && entry.payload == stale;
  assert(stale_connected);
  assert(probe.notifications.events.empty());
}

// An oversized polarity returns at generate_resolvents' occurrence-limit guard
// without publishing the other polarity's compaction rescore.
static void test_oversized_polarity_suppresses_other_compaction_rescore() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1), opposite = internalize(-1);
  const Lit stale_lits[3] = {pivot, internalize(2), internalize(3)};
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);
  const Lit oversized_lits[3] = {opposite, internalize(4), internalize(5)};
  kernel_add_large_clause(k, oversized_lits, 3, false, 0);

  ScanProbe probe(k);
  probe.snapshot();
  kernel_delete_clause(k, clause_at(k.arena, stale));

  probe.listen();
  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(probe.image[pivot] == 0 && probe.image[opposite] == 1);
  assert(probe.notifications.events.empty() &&
         "an oversized polarity bypasses the later failed-resolution rescore");
}

// The linear prefix compaction scan neither skips a deleted binary's successor
// nor reads past the first oversized clause.
static void test_candidate_gather_mutating_prefix_boundary() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1), early = internalize(2);
  const Lit late = internalize(5);
  kernel_add_binary_clause(k, pivot, early);
  const Lit stale_lits[3] = {pivot, internalize(3), internalize(4)};
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);
  const Lit oversized[3] = {pivot, internalize(3), internalize(4)};
  kernel_add_large_clause(k, oversized, 3, false, 0);

  ScanProbe probe(k);
  // Dense entry creates the binary-prefix/arena order.  A binary created
  // while connected is then appended after that oversized arena occurrence.
  kernel_add_binary_clause(k, pivot, late);
  assert(probe.dense.ordered[pivot].size() == 4);
  assert(probe.dense.ordered[pivot][0].binary &&
         probe.dense.ordered[pivot][0].payload == early);
  assert(!probe.dense.ordered[pivot][1].binary &&
         probe.dense.ordered[pivot][1].payload == stale);
  assert(!probe.dense.ordered[pivot][2].binary);
  assert(probe.dense.ordered[pivot][3].binary &&
         probe.dense.ordered[pivot][3].payload == late);
  kernel_assign_unit(k, early);
  kernel_assign_unit(k, late);
  probe.snapshot();
  kernel_delete_clause(k, clause_at(k.arena, stale));
  probe.listen();

  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  // Declined on the oversized antecedent, but the scan retired the satisfied
  // binary first: a state change, reported as `Ok`.
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::Ok);
  assert(k.stats.bve_eliminated == 0);
  assert(!kernel_binary_live(k, pivot, early));
  assert(kernel_binary_live(k, pivot, late) &&
         "the suffix beyond the oversized occurrence remains untouched");
  assert(probe.image[pivot] == 2 && probe.image[early] == 0 && probe.image[late] == 1);
  assert(large_occurrences(probe.dense, pivot).size() == 1 &&
         large_occurrences(probe.dense, pivot)[0] != stale);
  assert(probe.notifications.events.size() == 1);
  assert(probe.notifications.events[0] == std::make_pair(var_of(early), true));
}

static void test_flush_units_while_connected() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit unit = internalize(1), a = internalize(2), b = internalize(3),
            binary_other = internalize(4);
  const Lit satisfied[3] = {unit, a, b};
  const Ref ref = kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_binary_clause(k, unit, binary_other);

  ScanProbe probe(k);
  probe.snapshot();
  probe.listen();
  const auto &events = probe.notifications.events;

  kernel_assign_unit(k, unit);
  k.propagate_head = static_cast<uint32_t>(k.trail.size());
  bve_flush_units_while_connected(k, 0);
  assert(clause_at(k.arena, ref)->garbage());
  assert(large_occurrences(probe.dense, unit).empty() && probe.image[unit] == 1);
  assert(large_occurrences(probe.dense, a).size() == 1 && large_occurrences(probe.dense, b).size() == 1 &&
         "only the satisfying unit's dense stack is compacted");
  assert(kernel_binary_live(k, unit, binary_other));
  assert(std::find(events.begin(), events.end(), std::make_pair(var_of(unit), true)) ==
         events.end());
  for (const Var v : {var_of(a), var_of(b), var_of(binary_other)})
    assert(std::find(events.begin(), events.end(),
                     std::make_pair(v, true)) != events.end());
}

// redundant (learned) large clauses are not in the elimination set.
static void test_occurrences_skip_redundant() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3}, true, 2); // learned
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  assert(occ.count(internalize(1)) == 0); // redundant clause not counted
}

// Assigned variables are not candidates; an occurrence-free variable is, and is
// eliminated as pure (kissat eliminate.c schedule_variables).
static void test_schedule_excludes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3}); // (1∨2∨3); var4 unused
  kernel_assign_unit(k, internalize(-3));     // 3 assigned (false)
  std::vector<Var> cand;
  bve_schedule(k, cand);
  assert(cand.size() == 3);
  for (const Var v : cand)
    assert(v != var_of(internalize(3))); // assigned
  assert(std::find(cand.begin(), cand.end(), var_of(internalize(4))) !=
         cand.end()); // in no elimination clause: scheduled, pure
}

// N1 and Kissat parity: an extension variable is a legal BVE candidate.
static void test_schedule_includes_extension_variable() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.first_extension_var = 2;
  kernel_add_binary_clause(k, internalize(3), internalize(1));
  kernel_add_binary_clause(k, internalize(-3), internalize(2));
  std::vector<Var> cand;
  bve_schedule(k, cand);
  assert(std::find(cand.begin(), cand.end(), var_of(internalize(3))) !=
         cand.end());
}

// Kissat's eliminateclslim (strategy-owned) declines a 101-literal antecedent
// before definition extraction, without mutation or Kitten work.
static void test_antecedent_clause_size_limit() {
  Kernel k;
  kernel_reserve_vars(k, 102);
  std::vector<Lit> long_clause;
  long_clause.reserve(101);
  long_clause.push_back(internalize(1));
  for (int dimacs = 2; dimacs <= 101; ++dimacs)
    long_clause.push_back(internalize(dimacs));
  kernel_add_large_clause(k, long_clause.data(), long_clause.size(), false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(102));
  k.bve_clause_size_limit = 100;

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  const size_t arena_before = k.arena.size();
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(k.arena.size() == arena_before);
  assert(!kernel_var_retired(k, var_of(internalize(1))));
  assert(k.stats.bve_definitions_checked == 0);
  kernel_dense_disconnect(k);
}

static void test_large_resolvent_is_irredundant_with_zero_glue() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {-1, 4, 5});
  k.bve_gate_mask = 0;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  size_t live = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_actual_words(c);
    if (c->garbage())
      continue;
    live++;
    assert(c->size == 4);
    assert(!c->redundant());
    assert(c->glue() == 0);
  }
  assert(live == 1);
}

static void test_elimination_retains_duplicate_binary_resolvent() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(2), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  k.bve_gate_mask = 0;
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  // Kissat adds every non-tautological resolvent.  Its next forward pass, not
  // BVE's commit path, owns duplicate-binary removal.
  assert(kernel_binary_copies(k, internalize(2), internalize(3)) == 2);
}

// A stale heap candidate isolated by an earlier pure elimination is still
// retired as pure (Kissat resolve.c).
static void test_scheduled_candidate_that_becomes_isolated_is_retired() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const Lit x = internalize(1), y = internalize(2);
  kernel_add_binary_clause(k, x, y);

  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  uint64_t candidates[2];
  assert(vt.instances(k, candidates, 2) == 2);
  const uint64_t saved_x = lit_instance(x);

  // Eliminating pure y deletes (x ∨ y), leaving scheduled x isolated.
  assert(vt.apply_instance(k, lit_instance(y)) == ApplyResult::Ok);
  assert(!kernel_binary_live(k, x, y));
  assert(!kernel_var_retired(k, var_of(x)));
  const size_t witnesses_before = k.reconstruction.size();

  assert(vt.apply_instance(k, saved_x) == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(x)));
  assert(k.stats.bve_eliminated == 2);
  assert(k.reconstruction.size() == witnesses_before &&
         "an isolated pivot needs no reconstruction witness");

  // Missing retired values are completed by the standard model extension;
  // the one original clause must still be satisfied.
  kernel_extend_model(k);
  assert(k.values[x] > 0 || k.values[y] > 0);
}

// Pose a (possibly wrong) value for an external literal before extension.
static void set_value(Kernel &k, int dimacs, bool val) {
  const Lit l = internalize(dimacs);
  k.values[l] = val ? 1 : -1;
  k.values[negate(l)] = val ? -1 : 1;
}

template <size_t N>
static void assert_satisfied(const Kernel &k, const int (&orig)[N][2]) {
  for (const auto &cl : orig)
    assert((k.values[internalize(cl[0])] > 0 || k.values[internalize(cl[1])] > 0) &&
           "every original clause satisfied by the extended model");
}

// Model reconstruction through ρ: hand back a model of the reduced formula
// with 1 at the wrong value, and every original clause must survive extend.
static void test_eliminate_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));  // (1∨2)
  kernel_add_binary_clause(k, internalize(-1), internalize(3)); // (¬1∨3)
  const int orig[2][2] = {{1, 2}, {-1, 3}};
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(1), internalize(2)));
  assert(has_binary(k, internalize(2), internalize(3))); // the resolvent
  set_value(k, 2, false);
  set_value(k, 3, true);
  set_value(k, 1, false); // deliberately wrong — extend must recover 1 = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "1 reconstructed to true");
  assert_satisfied(k, orig);
}

// Both x and t of x = -t retired and unassigned: extension completes the
// assignment before replay so the windows do not double-flip the pivot.
static void test_eliminate_frozen_pivot_reconstruction() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // (1∨2)
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // (¬1∨¬2)
  const int orig[2][2] = {{1, 2}, {-1, -2}};
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  // no value is ever assigned to 1 or 2 — both reach extend at 0
  kernel_extend_model(k);
  assert(k.values[internalize(1)] != 0 && "pivot definite after extend");
  assert(k.values[internalize(2)] != 0 && "partner definite after extend");
  assert_satisfied(k, orig);
}

// A failed candidate stops at the first non-tautological resolvent beyond
// pos+neg+bound (Kissat): 2*9 at bound 0 charges exactly 12 attempts.
static void test_bound_does_not_spend_future_work() {
  Kernel k;
  kernel_reserve_vars(k, 12); // x=1, positive partners 2..3, negative 4..12
  for (int i = 2; i <= 3; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(i));
  for (int j = 4; j <= 12; j++)
    kernel_add_binary_clause(k, internalize(-1), internalize(j));
  k.bve_gate_mask = 0;
  k.bve_bound = 0;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(k.stats.bve_resolutions == 12 &&
         "bound-0 BVE must stop at the first over-limit resolvent");
}

// resolve.c learns a collapsed-pair unit before the bound check; the unit and
// the antecedents it satisfies survive a kept pivot.
static void test_bound_failure_retains_immediate_unit() {
  Kernel k;
  kernel_reserve_vars(k, 9); // x=1, u=2, a=3..5, b=6..8, z=9
  const Lit x = internalize(1), nx = internalize(-1);
  const Lit u = internalize(2), z = internalize(9);
  kernel_add_binary_clause(k, x, u); // first positive antecedent
  for (int a = 3; a <= 5; ++a)
    kernel_add_binary_clause(k, x, internalize(a));
  const Lit unit_partner[3] = {nx, u, z};
  const Ref satisfied =
      kernel_add_large_clause(k, unit_partner, 3, false, 0);
  for (int b = 6; b <= 8; ++b)
    kernel_add_binary_clause(k, nx, internalize(b));
  kernel_assign_unit(k, internalize(-9)); // d1's active image is (¬x ∨ u)

  k.bve_gate_mask = 0;
  k.bve_backward = false;
  k.bve_bound = 0; // base 4+4=8; the ninth resolvent rejects the pivot
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  // The pivot is kept, but the attempt learned a root and retired the
  // antecedent it satisfies: a state change, reported as `Ok`.
  assert(vt.apply_instance(k, lit_instance(x)) == ApplyResult::Ok);
  assert(k.stats.bve_eliminated == 0);
  assert(k.value(u) > 0 &&
         "the immediate resolvent unit must survive the later bound failure");
  assert(!kernel_var_retired(k, var_of(x)) &&
         "the rejected pivot itself must remain active");
  assert(clause_at(k.arena, satisfied)->garbage() &&
         "a large antecedent satisfied by the immediate unit is deleted now");
  assert(k.stats.bve_resolutions == 10 &&
         "the first satisfied pair costs an attempt, then its garbage large "
         "antecedent is skipped by later rows");
}

// An AND/OR gate x↔OR(a₁..a₆) lets BVE resolve only gate×antecedent, dropping
// the 16 antecedent×antecedent resolvents that put this shape past the bound.
static void test_gate_and_or_fires() {
  Kernel k;
  kernel_reserve_vars(k, 15); // x=1, a=2..7, p=8..11, q=12..15
  Lit base[7] = {internalize(-1), internalize(2), internalize(3), internalize(4),
                 internalize(5),  internalize(6), internalize(7)};
  kernel_add_large_clause(k, base, 7, false, 0); // (¬x∨a₁∨…∨a₆)
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(-i)); // (x∨¬aᵢ)
  for (int p = 8; p <= 11; p++)
    kernel_add_binary_clause(k, internalize(1), internalize(p)); // (x∨pₖ) antecedent
  for (int q = 12; q <= 15; q++)
    kernel_add_binary_clause(k, internalize(-1), internalize(q)); // (¬x∨qₗ) antecedent
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(1), internalize(8)));    // var 1 eliminated
  assert(has_binary(k, internalize(-2), internalize(12)));   // gate×ant (¬a₁∨q₁)
  assert(!has_binary(k, internalize(8), internalize(12)));   // ant×ant (p₁∨q₁) skipped
}

// Every duplicate binary of an AND/OR input is a gate member with its own
// resolvent (Kissat).
static void test_gate_and_or_retains_duplicate_binary_gate_copies() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x=1, inputs=2,3, antecedent=4
  Lit base[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, base, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 1);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 2);
  kernel_add_binary_clause(k, internalize(1), internalize(-3), 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(4), 4);
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(kernel_binary_copies(k, internalize(-2), internalize(4)) == 2);
  assert(kernel_binary_copies(k, internalize(-3), internalize(4)) == 1);
}

// Kissat's AND scan: for a partner outside the base, the first physical copy
// stays an antecedent and later copies enter the gate.
static void test_gate_and_or_consumes_one_unrelated_duplicate_as_antecedent() {
  Kernel k;
  kernel_reserve_vars(k, 5); // x=1, inputs=2,3, duplicated ant=4, other ant=5
  Lit base[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, base, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 1);
  kernel_add_binary_clause(k, internalize(1), internalize(-3), 2);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 3);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(5), 5);
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(kernel_binary_copies(k, internalize(4), internalize(5)) == 1 &&
         "the later unrelated duplicate is a Kissat gate member");
}

// gates.c get_antecedents splits by raw-watch equality with one gate cursor, so
// an earlier duplicate takes the gate slot: the base meets (1,5) before (1,4).
static Ref first_large_ref(const Kernel &k, std::vector<int> want) {
  std::sort(want.begin(), want.end());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      std::vector<int> got;
      for (const Lit l : *c)
        got.push_back(externalize(l));
      std::sort(got.begin(), got.end());
      if (got == want)
        return ref;
    }
    ref += clause_actual_words(c);
  }
  return INVALID_REF;
}

static void test_gate_and_or_earlier_duplicate_takes_the_gate_slot() {
  Kernel k;
  kernel_reserve_vars(k, 5); // x=1, inputs=2,3, duplicated ant=4, other ant=5
  Lit base[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, base, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 1);
  kernel_add_binary_clause(k, internalize(1), internalize(-3), 2);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 3);
  kernel_add_binary_clause(k, internalize(1), internalize(5), 4);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 5);
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  const Ref via5 = first_large_ref(k, {2, 3, 5});
  const Ref via4 = first_large_ref(k, {2, 3, 4});
  assert(via5 != INVALID_REF && via4 != INVALID_REF);
  assert(via5 < via4 && "the antecedent stream is (1,5) then the later (1,4)");
}

// A live large clause whose literal set is exactly `want` (order-insensitive).
static bool has_large(const Kernel &k, std::vector<int> want) {
  return first_large_ref(k, std::move(want)) != INVALID_REF;
}

// equivalences.c takes the first negative-side binary whose partner's
// complement is marked: (-a v b) is added before (a v -b).
static void test_gate_equivalence_takes_first_negative_side_binary() {
  Kernel k;
  kernel_reserve_vars(k, 3); // x=1, partners a=2, b=3
  kernel_add_binary_clause(k, internalize(1), internalize(2), 1);   // (x∨a)
  kernel_add_binary_clause(k, internalize(1), internalize(3), 2);   // (x∨b)
  kernel_add_binary_clause(k, internalize(-1), internalize(-3), 3); // (¬x∨¬b)
  kernel_add_binary_clause(k, internalize(-1), internalize(-2), 4); // (¬x∨¬a)
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(has_binary(k, internalize(-2), internalize(3)));
  assert(has_binary(k, internalize(2), internalize(-3)));
  // gate x = ¬b: (x∨b)×(¬x∨¬a) = (b∨¬a) is resolved first, then (¬x∨¬b)×(x∨a)
  assert(kernel_binary_id(k, internalize(-2), internalize(3)) <
         kernel_binary_id(k, internalize(2), internalize(-3)));
}

// ands.c gates are physical binaries only: a root-shortened large clause stays
// an antecedent.
static void test_gate_and_or_root_shortened_large_stays_antecedent() {
  Kernel k;
  kernel_reserve_vars(k, 6); // x=1, inputs=2,3, partner=4, false tail=5, ant=6
  Lit base[3] = {internalize(-1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, base, 3, false, 0);
  Lit shortened[3] = {internalize(1), internalize(4), internalize(5)};
  kernel_add_large_clause(k, shortened, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 1);
  kernel_add_binary_clause(k, internalize(1), internalize(-3), 2);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(6), 4);
  kernel_assign_unit(k, internalize(-5));
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(4), internalize(6)) &&
         "(x∨4∨5) is not a gate member, so it never meets (¬x∨6)");
  assert(has_large(k, {2, 3, 4}));
  assert(has_binary(k, internalize(-2), internalize(6)));
  assert(has_binary(k, internalize(-3), internalize(6)));
}

// Kissat accepts a root-shortened large clause as the AND/OR base.
static void test_gate_and_or_accepts_root_shortened_large_base() {
  Kernel k;
  kernel_reserve_vars(k, 6); // x=1, input=2, ants=4,5, false base tail=6
  Lit base[3] = {internalize(-1), internalize(2), internalize(6)};
  kernel_add_large_clause(k, base, 3, false, 0);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), 1);
  kernel_add_binary_clause(k, internalize(1), internalize(4), 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(5), 3);
  kernel_assign_unit(k, internalize(-6));
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(has_binary(k, internalize(-2), internalize(5)));
  assert(has_binary(k, internalize(2), internalize(4)));
  assert(!has_binary(k, internalize(4), internalize(5)) &&
         "antecedent x antecedent is skipped for the shortened AND gate");
}

// kissat_mark_binaries accepts only physical binary inputs: a root-shortened
// large input is no gate implication.
static void test_gate_and_or_rejects_root_shortened_large_inputs() {
  Kernel k;
  kernel_reserve_vars(k, 12); // x=1, inputs=2,3, false tails=4,5
  add_external_clause(k, {1, -2, -3});
  add_external_clause(k, {-1, 2, 4});
  add_external_clause(k, {-1, 3, 5});
  kernel_add_binary_clause(k, internalize(1), internalize(6));
  kernel_add_binary_clause(k, internalize(1), internalize(7));
  kernel_add_binary_clause(k, internalize(-1), internalize(8));
  kernel_add_binary_clause(k, internalize(-1), internalize(9));
  kernel_assign_unit(k, internalize(-4));
  kernel_assign_unit(k, internalize(-5));
  k.bve_gate_mask = BVE_GATE_AND_OR;
  k.bve_bound = 0;
  k.bve_backward = false;

  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(k.stats.bve_gates_and_or == 0);
  assert(!kernel_var_retired(k, var_of(internalize(1))));
}

// An equivalence gate x↔y eliminates x as a substitution: antecedents get x
// replaced by ±y, ant×ant is skipped, and the model reconstructs to x = y.
static void test_gate_equivalence_substitution() {
  Kernel k;
  kernel_reserve_vars(k, 4); // x=1, y=2, ant vars 3,4
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // (¬x∨y)  gate
  kernel_add_binary_clause(k, internalize(1), internalize(-2)); // (x∨¬y)  gate
  kernel_add_binary_clause(k, internalize(1), internalize(3));  // (x∨3)   antecedent
  kernel_add_binary_clause(k, internalize(-1), internalize(4)); // (¬x∨4)  antecedent
  const int orig[4][2] = {{-1, 2}, {1, -2}, {1, 3}, {-1, 4}};
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(has_binary(k, internalize(-2), internalize(4)));    // (¬y∨4): x→y in ant⁻
  assert(has_binary(k, internalize(2), internalize(3)));     // (y∨3):  x→y in ant⁺
  assert(!has_binary(k, internalize(3), internalize(4)));    // (3∨4) ant×ant skipped
  set_value(k, 2, true);  // y = T
  set_value(k, 4, true);  // needed by (¬y∨4)
  set_value(k, 3, false);
  set_value(k, 1, false); // wrong — extend must recover x = y = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "x reconstructed to y = true");
  assert_satisfied(k, orig);
}

// ITE x = (c ? -e : -t) with 6 antecedent binaries per side: 60 pairs exceed
// base+bound = 32, so only the gate's 24 charged pairs fire.
static void test_gate_ite_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 16); // x=1, c=2, t=3, e=4, ant p=5..10, q=11..16
    const int gate[4][3] = {
        {1, 2, 3}, {1, -2, 4}, {-1, 2, -3}, {-1, -2, -4}};
    for (const auto &g : gate) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 5; p <= 10; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 11; q <= 16; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // ITE off. The definition extractor comes off with it: the sub-solver
    // refutes this shape too and would keep the arm firing.
    Kernel k;
    k.bve_gate_mask = ~(BVE_GATE_ITE | BVE_GATE_DEFINITION);
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_ite == 0);
  }
  { // ITE on → the gate reduction fires
    Kernel k;
    k.bve_gate_mask = BVE_GATE_ITE;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_gates_ite == 1);
    // occ⁺·occ⁻ = 64 charged pairs become 24 → 40 saved
    assert(k.stats.bve_gate_pairs_saved == 40);
    assert(!has_binary(k, internalize(5), internalize(11))); // ant×ant skipped
  }
}

// Kissat ifthenelse.c recognizes an ITE with one ternary shortened to a binary;
// the antecedent resolvent of (x v -3) and (-x v 4) is skipped.
static void test_gate_ite_accepts_shortened_binary_clause() {
  Kernel k;
  kernel_reserve_vars(k, 7); // x=1; trace vars 61,841,896,62,898,953 -> 2..7
  const int large[5][3] = {{-1, -2, 3}, {-4, 2, 1}, {-1, 5, 6},
                           {-7, -5, 1}, {-1, -5, 7}};
  for (const auto &clause : large) {
    Lit lits[3] = {internalize(clause[0]), internalize(clause[1]),
                   internalize(clause[2])};
    kernel_add_large_clause(k, lits, 3, false, 0);
  }
  kernel_add_binary_clause(k, internalize(-6), internalize(1));
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(1));
  k.bve_gate_mask = BVE_GATE_ITE;
  k.bve_bound = 16;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(k.stats.bve_gates_ite == 1);
  assert(!has_binary(k, internalize(-3), internalize(4)) &&
         "ITE reduction must not resolve antecedent against antecedent");
}

// XOR x = a ^ b, as for ITE; the ITE detector declines it.
static void test_gate_xor_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 15); // x=1, a=2, b=3, ant p=4..9, q=10..15
    const int gate[4][3] = {
        {1, -2, 3}, {1, 2, -3}, {-1, 2, 3}, {-1, -2, -3}};
    for (const auto &g : gate) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 4; p <= 9; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 10; q <= 15; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // XOR off (the rest on, definition extractor off for the reason above)
    Kernel k;
    k.bve_gate_mask = ~(BVE_GATE_XOR | BVE_GATE_DEFINITION);
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_ite == 0 && "ITE must decline an XOR shape");
  }
  { // XOR on → the gate reduction fires, and the model still reconstructs
    Kernel k;
    k.bve_gate_mask = BVE_GATE_XOR;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_gates_xor == 1);
    assert(k.stats.bve_gate_pairs_saved == 40);
    set_value(k, 2, true);   // a = T
    set_value(k, 3, false);  // b = F  ⇒ the gate forces x = a⊕b = T
    for (int p = 4; p <= 9; p++)
      set_value(k, p, false); // every (x∨p) antecedent needs x
    for (int q = 10; q <= 15; q++)
      set_value(k, q, true);  // every (¬x∨q) antecedent already satisfied
    set_value(k, 1, false);   // deliberately wrong
    kernel_extend_model(k);
    assert(k.values[internalize(1)] > 0 && "x reconstructed to a⊕b = true");
  }
}

// A live large clause with exactly this stored literal order.
static bool has_large_ordered(const Kernel &k, const std::vector<int> &want) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == want.size()) {
      bool equal = true;
      for (uint32_t i = 0; i < c->size; ++i)
        equal &= externalize(c->lits[i]) == want[i];
      if (equal)
        return true;
    }
    ref += clause_actual_words(c);
  }
  return false;
}

// definition.c keeps root-false tails as free Kitten literals, so (y v f),(-y)
// is no core and the antecedent resolvent stays.
static void test_definition_retains_root_false_physical_literals() {
  Kernel k;
  kernel_reserve_vars(k, 7); // x=1, y=2, f=3, pos ant=4,5, neg ant=6,7
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  add_external_clause(k, {1, 4, 5});
  add_external_clause(k, {-1, 6, 7});
  kernel_assign_unit(k, internalize(-3));
  k.bve_gate_mask = BVE_GATE_DEFINITION;
  k.bve_backward = false;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(k.stats.bve_gates_definition == 0 &&
         "the physical Kitten environment remains satisfiable");
  assert(has_large(k, {4, 5, 6, 7}) &&
         "no artificial definition may skip antecedent x antecedent");
}

// A general definition no syntactic matcher sees: (x v a),(x v b) and
// (-x v -a v c),(-x v -b v -c) are UNSAT once x is removed. Its gate resolvents
// are not tautologies, so the definition type sets resolve_gate.
static void test_gate_definition_fires() {
  auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 12); // x=1, a=2, b=3, c=4, ant p=5..8, q=9..12
    kernel_add_binary_clause(k, internalize(1), internalize(2)); // (x∨a)
    kernel_add_binary_clause(k, internalize(1), internalize(3)); // (x∨b)
    const int g1[3] = {-1, -2, 4}, g2[3] = {-1, -3, -4};
    for (const int *g : {g1, g2}) {
      Lit ls[3] = {internalize(g[0]), internalize(g[1]), internalize(g[2])};
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    for (int p = 5; p <= 8; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 9; q <= 12; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // every syntactic type on, definitions off → nothing matches, bound refuses
    Kernel k;
    k.bve_gate_mask = ~BVE_GATE_DEFINITION;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
    assert(k.stats.bve_gates_equiv == 0 && k.stats.bve_gates_and_or == 0 &&
           k.stats.bve_gates_ite == 0 && k.stats.bve_gates_xor == 0 &&
           "no syntactic matcher may claim this shape");
    assert(k.stats.bve_definitions_checked == 0); // the mask really gates it
  }
  { // definitions on and nothing else → the sub-solver extracts the gate
    Kernel k;
    assert(k.bve_definition_budget == 1000000 &&
           "definitionticks must match Kissat's first-core allowance");
    assert(k.bve_definition_cores == 2 &&
           "definitioncores must match Kissat's refined-core default");
    assert(k.bve_definition_core_budget_multiplier == 10 &&
           "the refined-core allowance must match the historical 10x value");
    k.bve_gate_mask = BVE_GATE_DEFINITION;
    build(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
    assert(k.stats.bve_definitions_checked == 1);
    assert(k.stats.bve_gates_definition == 1);
    assert(k.stats.bve_definition_budget == 0); // the tick ceiling did not bind
    // 36 pairs, 16 of them antecedent×antecedent and skipped
    assert(k.stats.bve_gate_pairs_saved == 16);
    assert(!has_binary(k, internalize(1), internalize(5))); // x eliminated
    assert(has_binary(k, internalize(2), internalize(9)));  // gate×ant (a∨q₁)
    assert(!has_binary(k, internalize(5), internalize(9))); // ant×ant skipped
    // the resolve_gate witness: a gate×gate resolvent that is NOT a tautology
    assert(has_large(k, {2, -3, -4}) && "gate x gate resolvent (a∨¬b∨¬c)");
    assert(has_large_ordered(k, {-3, -4, 2}) &&
           "resolve.c copies the second antecedent before the first");
  }
  { // thread-local allocation scratch must not carry Kitten session state
    uint64_t ticks[2] = {0, 0}, saved[2] = {0, 0}, resolutions[2] = {0, 0};
    for (uint32_t repeat = 0; repeat < 2; ++repeat) {
      Kernel k;
      k.bve_gate_mask = BVE_GATE_DEFINITION;
      build(k);
      assert(vt.apply_instance(k, x) == ApplyResult::Ok);
      ticks[repeat] = k.stats.bve_definition_ticks;
      saved[repeat] = k.stats.bve_gate_pairs_saved;
      resolutions[repeat] = k.stats.bve_resolutions;
    }
    assert(ticks[0] == ticks[1] && saved[0] == saved[1] &&
           resolutions[0] == resolutions[1] &&
           "fresh kernels must restart Kitten's deterministic session seed");
  }
}

// Kissat definition.c: a one-sided core lifts its refutation to a unit before
// resolvent planning; this core needs learned intermediate clauses.
static void test_definition_one_sided_core_emits_unit() {
  const auto build = [](Kernel &k, int negative_clauses) {
    kernel_reserve_vars(k, static_cast<uint32_t>(3 + negative_clauses));
    const int positive[4][3] = {
        {1, 2, 3}, {1, 2, -3}, {1, -2, 3}, {1, -2, -3}};
    for (const auto &clause : positive) {
      Lit lits[3] = {internalize(clause[0]), internalize(clause[1]),
                     internalize(clause[2])};
      kernel_add_large_clause(k, lits, 3, false, 0);
    }
    for (int i = 0; i < negative_clauses; ++i)
      kernel_add_binary_clause(k, internalize(-1), internalize(4 + i));
  };
  Kernel k;
  build(k, 1);
  k.bve_gate_mask = BVE_GATE_DEFINITION;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && "one-sided core proves the pivot");
  assert(k.stats.bve_definition_units == 1);
  assert(k.stats.bve_gates_definition == 1 &&
         "the successful extractor is counted even without a pair plan");
  assert(k.stats.bve_eliminated == 0 && "unit extraction is not elimination");
  assert(has_binary(k, internalize(-1), internalize(4)) &&
         "resolve.c exits before deleting the pivot clauses");

  // With no refinement allowance and unprofitable plain BVE, the typed
  // multiplier alone decides.
  Kernel limited;
  build(limited, 10);
  limited.bve_gate_mask = BVE_GATE_DEFINITION;
  limited.bve_definition_core_budget_multiplier = 0;
  assert(vt.apply_instance(limited, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(limited.stats.bve_definitions_checked == 1);
  assert(limited.stats.bve_gates_definition == 0);
  assert(limited.stats.bve_definition_budget == 1 &&
         "the explicit zero refinement allowance must bind");
}

// Random formulas with definition gadgets: every survivor model extends to an
// original model; `repaired` counts models that needed the witnesses.
static void test_definition_reconstruction_exhaustive() {
  const uint32_t nvars = 8;
  uint64_t rng = 0x243f6a8885a308d3ull;
  const auto next = [&rng]() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
  };
  uint64_t gates = 0, eliminated = 0, models = 0, repaired = 0, trials = 0;
  for (int trial = 0; trial < 400; trial++) {
    std::vector<std::vector<int>> f;
    // a definitional gadget on a random pivot, antecedents from random clauses
    std::vector<int> v(nvars);
    for (uint32_t i = 0; i < nvars; i++)
      v[i] = (int)i + 1;
    for (uint32_t i = nvars; i > 1; i--)
      std::swap(v[i - 1], v[next() % i]);
    const int x = v[0], a = v[1], b = v[2], c = v[3];
    f.push_back({x, a});
    f.push_back({x, b});
    f.push_back({-x, -a, c});
    f.push_back({-x, -b, -c});
    const int nclauses = 4 + (int)(next() % 10);
    for (int i = 0; i < nclauses; i++) {
      std::vector<int> cl;
      const size_t want = 2 + (size_t)(next() % 2);
      while (cl.size() < want) {
        const int w = 1 + (int)(next() % nvars);
        bool dup = false;
        for (const int e : cl)
          dup |= (e == w || e == -w);
        if (!dup)
          cl.push_back((next() & 1) ? w : -w);
      }
      f.push_back(cl);
    }
    Kernel k;
    k.bve_gate_mask = BVE_GATE_DEFINITION; // every gate comes from the sub-solver
    kernel_reserve_vars(k, nvars);
    for (const auto &cl : f) {
      std::vector<Lit> ls;
      for (const int e : cl)
        ls.push_back(internalize(e));
      if (ls.size() == 2)
        kernel_add_binary_clause(k, ls[0], ls[1]);
      else
        kernel_add_large_clause(k, ls.data(), (uint32_t)ls.size(), false, 0);
    }
    if (k.inconsistent)
      continue;
    const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
    const uint64_t before = k.stats.bve_gates_definition;
    for (uint32_t w = 0; w < nvars; w++)
      if (vt.apply_instance(k, lit_instance(internalize((int)w + 1))) ==
          ApplyResult::Unsat)
        break;
    if (k.inconsistent)
      continue;
    gates += k.stats.bve_gates_definition - before;
    eliminated += k.stats.bve_eliminated;
    if (!k.stats.bve_eliminated)
      continue;
    trials++;

    // what survives: root units, live binaries, live large clauses
    std::vector<std::vector<int>> surviving;
    for (uint32_t i = 0; i < k.trail.size(); i++)
      surviving.push_back({externalize(k.trail[i])});
    for (Lit l = 0; l < 2 * k.nvars; l++)
      for (const WatchEntry ws : k.watches.entries(l))
        if (ws.binary() && ws.other() > l)
          surviving.push_back({externalize(l), externalize(ws.other())});
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *cl = clause_at(k.arena, ref);
      if (!cl->garbage()) {
        std::vector<int> lits;
        for (const Lit l : *cl)
          lits.push_back(externalize(l));
        surviving.push_back(lits);
      }
      ref += clause_actual_words(cl);
    }

    const auto sat = [&k](const std::vector<int> &cl) {
      for (const int e : cl)
        if (k.values[internalize(e)] > 0)
          return true;
      return false;
    };
    const std::vector<int8_t> saved(k.values.begin(), k.values.end());
    for (uint32_t bits = 0; bits < (1u << nvars); bits++) {
      for (uint32_t w = 0; w < nvars; w++) {
        const Lit p = internalize((int)w + 1);
        const int8_t val = (bits >> w) & 1 ? 1 : -1;
        k.values[p] = val;
        k.values[negate(p)] = (int8_t)-val;
      }
      bool ok = true;
      for (const auto &cl : surviving)
        ok &= sat(cl);
      if (!ok)
        continue;
      models++;
      bool needed = false;
      for (const auto &cl : f)
        needed |= !sat(cl);
      repaired += needed;
      k.model_extended = false;
      kernel_extend_model(k);
      for (const auto &cl : f)
        if (!sat(cl)) {
          std::printf("trial %d: reconstruction left a clause unsatisfied:",
                      trial);
          for (const int e : cl)
            std::printf(" %d", e);
          std::printf("\n");
          assert(false);
        }
    }
    std::copy(saved.begin(), saved.end(), k.values.begin());
  }
  std::printf("  definition reconstruction: %llu trials, %llu gates, "
              "%llu eliminated, %llu models (%llu needed repair)\n",
              (unsigned long long)trials, (unsigned long long)gates,
              (unsigned long long)eliminated, (unsigned long long)models,
              (unsigned long long)repaired);
  assert(gates > 0 && "no definition was ever extracted");
  assert(repaired > 0 && "no model ever needed rho: the check was vacuous");
}

// Kernel::bve_gate_mask flips the gate-only elimination between NoOp and Ok.
static void test_gate_mask() {
  auto build_and_or = [](Kernel &k) {
    kernel_reserve_vars(k, 15); // x=1, a=2..7, p=8..11, q=12..15
    Lit base[7] = {internalize(-1), internalize(2), internalize(3),
                   internalize(4),  internalize(5), internalize(6),
                   internalize(7)};
    kernel_add_large_clause(k, base, 7, false, 0);
    for (int i = 2; i <= 7; i++)
      kernel_add_binary_clause(k, internalize(1), internalize(-i));
    for (int p = 8; p <= 11; p++)
      kernel_add_binary_clause(k, internalize(1), internalize(p));
    for (int q = 12; q <= 15; q++)
      kernel_add_binary_clause(k, internalize(-1), internalize(q));
  };
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  const uint64_t x = lit_instance(internalize(1));
  { // AND/OR disabled → full cross product exceeds the bound → NoOp
    Kernel k;
    k.bve_gate_mask = BVE_GATE_EQUIV; // AND/OR off
    build_and_or(k);
    assert(vt.apply_instance(k, x) == ApplyResult::NoOp);
  }
  { // AND/OR enabled → the gate reduction fires
    Kernel k;
    k.bve_gate_mask = BVE_GATE_AND_OR; // AND/OR on (equivalence off)
    build_and_or(k);
    assert(vt.apply_instance(k, x) == ApplyResult::Ok);
  }
}

// Retry filter (kissat flags->eliminate): an unchanged neighbourhood leaves
// the schedule, a change rearms it; the API stays unrestricted.
static void test_retry_filter() {
  Kernel k;
  kernel_reserve_vars(k, 14); // the bound-skip formula: x=1, a=2..7, b=8..13
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(i));
  for (int j = 8; j <= 13; j++)
    kernel_add_binary_clause(k, internalize(-1), internalize(j));
  const Var x = var_of(internalize(1));
  // the rule's advice, which the policy consults; see `offered` for N1
  auto worth_retry = [&](Var v) { return k.bve_retry[v] != 0; };
  // N1: the enumeration still offers the variable whatever the advice says.
  auto offered = [&](Var v) {
    std::vector<Var> cand;
    bve_schedule(k, cand);
    for (const Var c : cand)
      if (c == v)
        return true;
    return false;
  };
  assert(worth_retry(x)); // fresh variable: retry-worthy
  assert(offered(x));
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);   // 36 resolvents > 6+6+16: bound exceeded
  assert(!worth_retry(x));     // attempted, nothing changed: not worth a retry
  assert(offered(x));          // ...but still offered by instances() (N1)
  // and still applicable via the API (N1): the same NoOp, computed not assumed
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // Pinned Kissat's `mark_added_literal` does not set flags->eliminate: adding
  // an irredundant clause alone does not retry x (var 14 is otherwise unused).
  kernel_add_binary_clause(k, internalize(1), internalize(14));
  assert(!worth_retry(x));
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // Semantic removal is what rearms elimination.
  assert(kernel_delete_binary(k, internalize(1), internalize(14)));
  assert(worth_retry(x));
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);
  assert(!worth_retry(x));
  // a redundant clause on x is not: BVE never resolves it, and marking on
  // learned-clause traffic would defeat the filter
  add_external_clause(k, {1, 2, 3}, true, 3);
  assert(!worth_retry(x));
  // A root assignment alone is no eliminate notification (Kissat): only a later
  // clause or literal removal calls mark_removed_literal.
  kernel_assign_unit(k, internalize(-2));
  assert(!worth_retry(x));
}

static void test_large_rewrite_retries_only_removed_literals() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Ref ref = add_external_clause(k, {1, 2, 3, 4});
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);

  const Lit replacement[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_replace_large_clause_in_place(k, ref, ++k.next_clause_id,
                                       replacement, 3, 0);
  assert(!k.bve_retry[var_of(internalize(1))]);
  assert(!k.bve_retry[var_of(internalize(2))]);
  assert(!k.bve_retry[var_of(internalize(3))]);
  assert(k.bve_retry[var_of(internalize(4))]);
  kernel_check_invariants(k);
}

// Kissat dense order: existing binaries, then the arena stream; a later binary
// resolvent is appended after the large antecedents.
static void test_dense_bve_preserves_mixed_connected_order() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  auto L = [](int dimacs) { return internalize(dimacs); };
  const Lit p1[3] = {L(1), L(2), L(3)};
  const Lit p2[3] = {L(1), L(4), L(5)};
  const Lit n1[3] = {L(-1), L(7), L(8)};
  kernel_add_large_clause(k, p1, 3, false, 0);
  kernel_add_large_clause(k, p2, 3, false, 0);
  kernel_add_large_clause(k, n1, 3, false, 0);

  OccIndex occ;
  kernel_dense_connect(k, occ, true);
  kernel_add_binary_clause(k, L(1), L(6)); // appended after p1 and p2
  k.bve_gate_mask = 0;
  k.bve_backward = false;
  k.bve_bound = 8;
  const Ref fresh_begin = static_cast<Ref>(k.arena.size());
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(L(1))) == ApplyResult::Ok);

  std::vector<std::vector<Lit>> fresh;
  for (Ref ref = fresh_begin; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      fresh.emplace_back(c->begin(), c->end());
    ref += clause_actual_words(c);
  }
  assert(fresh.size() == 3);
  assert((fresh[0] == std::vector<Lit>{L(2), L(3), L(7), L(8)}));
  assert((fresh[1] == std::vector<Lit>{L(4), L(5), L(7), L(8)}));
  assert((fresh[2] == std::vector<Lit>{L(6), L(7), L(8)}));
  kernel_dense_disconnect(k);
  kernel_check_invariants(k);
}

// Wrong-SAT regression: backward strengthening of a victim to one literal
// asserts that unit.
static void test_backward_strengthen_to_unit_asserts() {
  Kernel k;
  k.bve_backward = true; // explicitly exercise the optional proof rule
  kernel_reserve_vars(k, 6);
  auto L = [](int d) { return internalize(d); };
  auto add3 = [&](int a, int b, int c) {
    const Lit v[3] = {L(a), L(b), L(c)};
    kernel_add_large_clause(k, v, 3, /*redundant=*/false, /*glue=*/0);
  };
  // Eliminating 1 yields (-4), (2 v 3 v 4) and (2 v 3 v -4); the unit is asserted
  // before the backward drain, so the drain sees {2, 3}.
  kernel_add_binary_clause(k, L(1), L(-4));  // P1
  add3(1, 2, 3);                             // P2
  kernel_add_binary_clause(k, L(-1), L(4));  // N1
  kernel_add_binary_clause(k, L(-1), L(-4)); // N2
  // victim d=(2∨¬3∨4): c ⊆ d with 3 flipped and 4 root-false → dlits={2}.
  add3(2, -3, 4);
  // padding inflates occ(3) so the drain's least-occurring pick is best=2 —
  // d holds ¬3, so a best=3 walk would never visit it.
  add3(3, 5, 6);
  add3(3, 5, -6);
  k.bve_gate_mask = 0; // plain resolution: exactly the resolvent set above
  OccIndex occ;
  kernel_dense_connect(k, occ, true);
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(L(1))) == ApplyResult::Ok);
  assert(k.value(L(-4)) > 0 && "the elimination's own unit landed");
  assert(k.stats.bve_bwd_strengthened >= 1 && "the drain reached the victim");
  assert(k.stats.bve_bwd_units >= 1 && "the strengthening collapsed to a unit");
  assert(k.value(L(2)) > 0 &&
         "the collapsed unit is ASSERTED, not dropped (wrong-SAT regression)");
  kernel_dense_disconnect(k);
}

// Binary arm: a size-2 resolvent strengthens a binary victim to an asserted
// unit and unwatches the pair.
static void test_backward_binary_victim_strengthen_to_unit() {
  Kernel k;
  k.bve_backward = true; // explicitly exercise the optional proof rule
  kernel_reserve_vars(k, 6);
  auto L = [](int d) { return internalize(d); };
  auto add3 = [&](int a, int b, int c) {
    const Lit v[3] = {L(a), L(b), L(c)};
    kernel_add_large_clause(k, v, 3, /*redundant=*/false, /*glue=*/0);
  };
  // Eliminating 1 resolves (1∨2∨3)×(¬1∨2) to the single size-2 resolvent
  // c=(2∨3), which enters the drain as a binary subsumer.
  add3(1, 2, 3);                            // P1
  kernel_add_binary_clause(k, L(-1), L(2)); // N1
  // victim (2∨¬3): c resolves with it on 3 → the unit (2).
  kernel_add_binary_clause(k, L(2), L(-3));
  // padding inflates occ(3) past occ(2) so the least-occurring pick is
  // best=2 — the victim holds ¬3, so a best=3 walk would never visit it.
  add3(3, 5, 6);
  add3(3, 5, -6);
  k.bve_gate_mask = 0; // plain resolution: exactly the resolvent above
  OccIndex occ;
  kernel_dense_connect(k, occ, true);
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(L(1))) == ApplyResult::Ok);
  assert(k.stats.bve_bwd_strengthened >= 1 &&
         "the drain reached the binary victim");
  assert(k.stats.bve_bwd_units >= 1 && "the strengthening collapsed to a unit");
  assert(k.value(L(2)) > 0 && "the collapsed unit is asserted");
  assert(!kernel_binary_live(k, L(2), L(-3)) && "the victim pair is deleted");
  kernel_dense_disconnect(k);
}

int main() {
  test_schedule_occurrence_image_lifecycle();
  test_candidate_gather_compacts_garbage_occurrences();
  test_candidate_gather_stops_before_late_garbage();
  test_oversized_polarity_suppresses_other_compaction_rescore();
  test_candidate_gather_mutating_prefix_boundary();
  test_flush_units_while_connected();
  test_occurrences_skip_redundant();
  test_schedule_excludes();
  test_schedule_includes_extension_variable();
  test_antecedent_clause_size_limit();
  test_large_resolvent_is_irredundant_with_zero_glue();
  test_elimination_retains_duplicate_binary_resolvent();
  test_scheduled_candidate_that_becomes_isolated_is_retired();
  test_eliminate_reconstruction();
  test_eliminate_frozen_pivot_reconstruction();
  test_bound_does_not_spend_future_work();
  test_bound_failure_retains_immediate_unit();
  test_gate_and_or_fires();
  test_gate_and_or_retains_duplicate_binary_gate_copies();
  test_gate_and_or_consumes_one_unrelated_duplicate_as_antecedent();
  test_gate_and_or_earlier_duplicate_takes_the_gate_slot();
  test_gate_equivalence_takes_first_negative_side_binary();
  test_gate_and_or_root_shortened_large_stays_antecedent();
  test_gate_and_or_accepts_root_shortened_large_base();
  test_gate_and_or_rejects_root_shortened_large_inputs();
  test_gate_equivalence_substitution();
  test_gate_ite_fires();
  test_gate_ite_accepts_shortened_binary_clause();
  test_gate_xor_fires();
  test_definition_retains_root_false_physical_literals();
  test_gate_definition_fires();
  test_definition_one_sided_core_emits_unit();
  test_definition_reconstruction_exhaustive();
  test_gate_mask();
  test_retry_filter();
  test_large_rewrite_retries_only_removed_literals();
  test_dense_bve_preserves_mixed_connected_order();
  test_backward_strengthen_to_unit_asserts();
  test_backward_binary_victim_strengthen_to_unit();
  puts("test_bve OK");
  return 0;
}

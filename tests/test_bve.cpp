// BVE: the round-local occurrence index, the candidate schedule, elimination
// and its bound, the gate reductions, and model reconstruction through the
// witness stack. Rule semantics live in docs/calculus.md.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "occurrence_test_utils.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;

// guard: the pure precondition is a root state (level 0, not refuted).
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  assert(bve_guard(k)); // empty trail at level 0
  kernel_assign_decision(k, internalize(1));
  assert(!bve_guard(k)); // level > 0 is not a root state
  kernel_backtrack(k, 0);
  assert(bve_guard(k));
  k.inconsistent = true;
  assert(!bve_guard(k)); // refuted
}

static void test_lrat_shared_parent_root_unit_once() {
  const std::string path = "/tmp/tactix-bve-shared-root-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".lrat";
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.bve_gate_mask = 0;
  const Lit positive[3] = {internalize(1), internalize(2),
                           internalize(4)};
  const Lit negative[3] = {internalize(-1), internalize(3),
                           internalize(4)};
  kernel_add_large_clause(k, positive, 3, false, 0, ++k.next_clause_id);
  kernel_add_large_clause(k, negative, 3, false, 0, ++k.next_clause_id);
  const uint64_t root_id = ++k.next_clause_id;
  k.unit_ids[var_of(internalize(-4))] = root_id;
  kernel_assign_unit(k, internalize(-4));
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::Lrat));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  const RuleVTable &bve = tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::Ok);
  proof.flush();
  k.proof = nullptr;

  std::ifstream input(path);
  std::string line;
  bool found = false;
  while (std::getline(input, line)) {
    std::istringstream fields(line);
    uint64_t id;
    if (!(fields >> id) || id != root_id + 1)
      continue;
    fields >> std::ws;
    if (fields.peek() == 'd')
      continue;
    int literal;
    while (fields >> literal && literal != 0) {}
    uint64_t hint;
    uint32_t root_count = 0;
    while (fields >> hint && hint != 0)
      root_count += hint == root_id;
    assert(root_count == 1);
    found = true;
  }
  assert(found);
  std::remove(path.c_str());
}

// occurrence index: both polarities, large + binaries, active literals only.
static void test_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0); // (1∨2∨3)
  kernel_add_binary_clause(k, internalize(-1), internalize(2));       // (¬1∨2)

  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  // flat counts: +1 large only; ¬1 binary only; +2 in both; +3 large only
  assert(occ.count(internalize(1)) == 1);
  assert(occ.count(internalize(-1)) == 1);
  assert(occ.count(internalize(2)) == 2);
  assert(occ.count(internalize(-2)) == 0);
  assert(occ.count(internalize(3)) == 1);
}

// Dynamic BVE caches Kissat's physical dense-stack sizes so score updates are
// O(1). Binary slots change eagerly; a garbage large Ref remains counted until
// dense compaction, while an in-place membership rewrite changes only the
// literals that actually leave or enter the physical occurrence lists.
static void test_schedule_occurrence_image_lifecycle() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit a = internalize(1), b = internalize(2);
  const Lit c = internalize(3), d = internalize(4);
  kernel_add_binary_clause(k, a, b);
  const Lit first_lits[3] = {a, c, d};
  const Ref first_ref = kernel_add_large_clause(k, first_lits, 3, false, 0);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  assert(image[a] == 2 && image[b] == 1 && image[c] == 1 && image[d] == 1);

  kernel_add_binary_clause(k, a, b);
  assert(image[a] == 3 && image[b] == 2);
  assert(kernel_delete_binary_one_copy(k, a, b));
  assert(image[a] == 2 && image[b] == 1);
  assert(kernel_delete_binary_all_copies(k, a, b));
  assert(image[a] == 1 && image[b] == 0);

  const Ref second_ref =
      kernel_add_large_clause(k, first_lits, 3, false, 0);
  assert(image[a] == 2 && image[c] == 2 && image[d] == 2);
  const Lit replacement[3] = {a, b, d};
  kernel_replace_large_clause_in_place(k, second_ref, ++k.next_clause_id,
                                       replacement, 3, 0);
  assert(image[a] == 2 && image[b] == 1 && image[c] == 1 && image[d] == 2);

  kernel_delete_clause(k, clause_at(k.arena, first_ref));
  assert(image[a] == 2 && image[c] == 1 && image[d] == 2 &&
         "lazy large garbage remains a physical dense occurrence");
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    assert(bve_schedule_occurrence_count(k, lit) == image[lit]);
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

struct BveScoreNotifications {
  std::vector<std::pair<Var, bool>> events;
};

static void record_bve_score_notification(Kernel &, void *state, Var v,
                                          bool reinsert) {
  static_cast<BveScoreNotifications *>(state)->events.emplace_back(v,
                                                                    reinsert);
}

// Kissat compacts garbage references encountered before an oversized clause,
// even though that candidate is then rejected.  The failed polarity scan does
// not request a heap-score update for the already-popped variable.
static void test_candidate_gather_compacts_garbage_occurrences() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1);
  const Lit stale_lits[3] = {pivot, internalize(2), internalize(3)};
  const Lit live_lits[3] = {pivot, internalize(4), internalize(5)};
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);
  kernel_add_large_clause(k, live_lits, 3, false, 0);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  kernel_delete_clause(k, clause_at(k.arena, stale));
  assert(image[pivot] == 2 && image[internalize(2)] == 1);

  // The remaining live ternary exceeds this diagnostic admission ceiling, so
  // the candidate fails after its physical occurrence stacks are inspected.
  k.bve_clause_size_limit = 2;
  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(image[pivot] == 1);
  assert(image[internalize(2)] == 1 &&
         "only the inspected pivot stack is compacted");
  assert(large_occurrences(dense, pivot).size() == 1 && large_occurrences(dense, pivot)[0] != stale);
  bool stale_connected = false;
  for (const OccIndex::Entry &entry : dense.ordered[pivot])
    stale_connected |= !entry.binary && entry.payload == stale;
  assert(!stale_connected);
  assert(!k.bve_retry[var_of(pivot)]);
  assert(notifications.events.empty());

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

// `occurrences_literal` stops at the first live oversized clause.  Garbage
// later in that same physical polarity is not inspected or compacted, and a
// failed scan does not request a score update for the already-popped pivot.
static void test_candidate_gather_stops_before_late_garbage() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1);
  const Lit oversized_lits[3] = {pivot, internalize(2), internalize(3)};
  const Lit stale_lits[3] = {pivot, internalize(4), internalize(5)};
  kernel_add_large_clause(k, oversized_lits, 3, false, 0);
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  kernel_delete_clause(k, clause_at(k.arena, stale));
  assert(image[pivot] == 2);

  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;
  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(image[pivot] == 2);
  assert(large_occurrences(dense, pivot).size() == 2);
  bool stale_connected = false;
  for (const OccIndex::Entry &entry : dense.ordered[pivot])
    stale_connected |= !entry.binary && entry.payload == stale;
  assert(stale_connected);
  assert(notifications.events.empty());

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

// If either polarity returns UINT_MAX for an oversized antecedent,
// kissat_generate_resolvents returns at its occurrence-limit guard.  It does
// not publish the update flag accumulated by successful compaction of the
// other polarity because that flag belongs only to a later resolution failure.
static void test_oversized_polarity_suppresses_other_compaction_rescore() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1), opposite = internalize(-1);
  const Lit stale_lits[3] = {pivot, internalize(2), internalize(3)};
  const Ref stale = kernel_add_large_clause(k, stale_lits, 3, false, 0);
  const Lit oversized_lits[3] = {opposite, internalize(4), internalize(5)};
  kernel_add_large_clause(k, oversized_lits, 3, false, 0);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  kernel_delete_clause(k, clause_at(k.arena, stale));

  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;
  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::NoOp);
  assert(image[pivot] == 0 && image[opposite] == 1);
  assert(notifications.events.empty() &&
         "an oversized polarity bypasses the later failed-resolution rescore");

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

static void test_candidate_gather_deletes_satisfied_binary() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit pivot = internalize(1), satisfied = internalize(2);
  kernel_add_binary_clause(k, pivot, satisfied);
  const Lit live_lits[3] = {pivot, internalize(4), internalize(5)};
  kernel_add_large_clause(k, live_lits, 3, false, 0);
  kernel_assign_unit(k, satisfied);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;

  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  // The candidate declines on the oversized antecedent, but its occurrence
  // scan already retired the satisfied binary and wrote that deletion: the
  // application changed solver state, which is what `Ok` reports.
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::Ok);
  assert(k.stats.bve_eliminated == 0 && !kernel_var_retired(k, var_of(pivot)));
  assert(!kernel_binary_live(k, pivot, satisfied));
  assert(image[pivot] == 1 && image[satisfied] == 0);
  assert(notifications.events.size() == 1);
  assert(notifications.events[0] ==
         std::make_pair(var_of(satisfied), true));

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

// Mutating the connected stack while scanning must neither skip the successor
// of a deleted binary nor inspect entries beyond the first oversized clause.
// This is the ordered transition used by the linear prefix compaction scan.
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

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  // Dense entry creates the binary-prefix/arena order.  A binary created
  // while connected is then appended after that oversized arena occurrence.
  kernel_add_binary_clause(k, pivot, late);
  assert(dense.ordered[pivot].size() == 4);
  assert(dense.ordered[pivot][0].binary &&
         dense.ordered[pivot][0].payload == early);
  assert(!dense.ordered[pivot][1].binary &&
         dense.ordered[pivot][1].payload == stale);
  assert(!dense.ordered[pivot][2].binary);
  assert(dense.ordered[pivot][3].binary &&
         dense.ordered[pivot][3].payload == late);
  kernel_assign_unit(k, early);
  kernel_assign_unit(k, late);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  kernel_delete_clause(k, clause_at(k.arena, stale));
  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;

  k.bve_clause_size_limit = 2;
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  // Declined on the oversized antecedent, but the scan retired the satisfied
  // binary first: a state change, reported as `Ok`.
  assert(vt.apply_instance(k, lit_instance(pivot)) == ApplyResult::Ok);
  assert(k.stats.bve_eliminated == 0);
  assert(!kernel_binary_live(k, pivot, early));
  assert(kernel_binary_live(k, pivot, late) &&
         "the suffix beyond the oversized occurrence remains untouched");
  assert(image[pivot] == 2 && image[early] == 0 && image[late] == 1);
  assert(large_occurrences(dense, pivot).size() == 1 &&
         large_occurrences(dense, pivot)[0] != stale);
  assert(notifications.events.size() == 1);
  assert(notifications.events[0] == std::make_pair(var_of(early), true));

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

static void test_flush_units_while_connected() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit unit = internalize(1), a = internalize(2), b = internalize(3),
            binary_other = internalize(4);
  const Lit satisfied[3] = {unit, a, b};
  const Ref ref = kernel_add_large_clause(k, satisfied, 3, false, 0);
  kernel_add_binary_clause(k, unit, binary_other);

  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  std::vector<uint32_t> image(2 * k.nvars);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    image[lit] = bve_schedule_occurrence_count(k, lit);
  k.bve_occurrence_count_image = &image;
  BveScoreNotifications notifications;
  k.bve_score_update_hook = record_bve_score_notification;
  k.bve_score_update_state = &notifications;

  kernel_assign_unit(k, unit);
  k.propagate_head = static_cast<uint32_t>(k.trail.size());
  bve_flush_units_while_connected(k, 0);
  assert(clause_at(k.arena, ref)->garbage());
  assert(large_occurrences(dense, unit).empty() && image[unit] == 1);
  assert(large_occurrences(dense, a).size() == 1 && large_occurrences(dense, b).size() == 1 &&
         "only the satisfying unit's dense stack is compacted");
  assert(kernel_binary_live(k, unit, binary_other));
  assert(std::find(notifications.events.begin(), notifications.events.end(),
                   std::make_pair(var_of(unit), true)) ==
         notifications.events.end());
  for (const Var v : {var_of(a), var_of(b), var_of(binary_other)})
    assert(std::find(notifications.events.begin(), notifications.events.end(),
                     std::make_pair(v, true)) != notifications.events.end());

  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  kernel_dense_disconnect(k);
}

// redundant (learned) large clauses are not in the elimination set.
static void test_occurrences_skip_redundant() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/true, /*glue=*/2); // learned
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  assert(occ.count(internalize(1)) == 0); // redundant clause not counted
}

// schedule: Kissat's structural score p*n-p-n. Here all three variables score
// -1, so the rule-local surface retains variable order; DefaultPolicy adds the
// search-relevancy tie-break and dynamic heap pose.
static void test_schedule_order() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);                  // (1∨2∨3)
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // (¬1∨¬2)
  std::vector<Var> cand;
  bve_schedule(k, cand);
  assert(cand.size() == 3);
  assert(cand[0] == var_of(internalize(1)));
  assert(cand[1] == var_of(internalize(2)));
  assert(cand[2] == var_of(internalize(3)));
}

// assigned variables are not candidates; a variable in no elimination clause
// is one (kissat eliminate.c schedule_variables schedules every active
// variable with its `eliminate` flag set, and generate_resolvents then
// treats the empty polarity as pure and eliminates it).
static void test_schedule_excludes() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0); // (1∨2∨3); var4 unused
  kernel_assign_unit(k, internalize(-3));     // 3 assigned (false)
  std::vector<Var> cand;
  bve_schedule(k, cand);
  assert(cand.size() == 3);
  for (const Var v : cand)
    assert(v != var_of(internalize(3))); // assigned
  assert(std::find(cand.begin(), cand.end(), var_of(internalize(4))) !=
         cand.end()); // in no elimination clause: scheduled, pure
}

// N1 and Kissat parity: an extension variable is still a legal BVE candidate.
// DefaultPolicy may pay for reversing Factor, but the rule surface cannot hide
// that sound elimination and Kissat's elimination heap does not hide it.
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

// N1 surface: candidates are enumerated, an out-of-range payload is a NoOp.
static void test_instance_surface() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const int rid = tactix_rule_id("BVE");
  assert(rid >= 0);
  const RuleVTable &vt = tactix_rules()[rid];
  assert(vt.guard(k) && vt.instances && vt.apply_instance);
  uint64_t insts[8];
  const uint32_t n = vt.instances(k, insts, 8);
  assert(n >= 1); // at least one candidate
  Lit r;
  assert(lit_of_instance(insts[0], &r)); // decodable var payload
  assert(insts[0] != INSTANCE_CANONICAL);
  assert(vt.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp); // out of range
}

// Kissat's `eliminateclslim` applies to input antecedents before definition
// extraction, not only to generated resolvents.  Keep it strategy-owned so an
// unrestricted public rule remains legal, but once installed a 101-literal
// antecedent must decline without mutating the formula or spending Kitten work.
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

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    if (Watch{ws[i]}.binary()) {
      if (Watch{ws[i]}.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// (1∨2),(¬1∨3) resolve to (2∨3): var 1 vanishes, witnesses are pushed.
static void test_eliminate_basic() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));  // (1∨2)
  kernel_add_binary_clause(k, internalize(-1), internalize(3)); // (¬1∨3)
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(!has_binary(k, internalize(1), internalize(2)));  // originals gone
  assert(!has_binary(k, internalize(-1), internalize(3)));
  assert(has_binary(k, internalize(2), internalize(3)));   // resolvent present
  assert(!k.reconstruction.empty());                       // witnesses pushed
}

static void test_large_resolvent_is_irredundant_with_zero_glue() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit positive[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit negative[3] = {internalize(-1), internalize(4), internalize(5)};
  kernel_add_large_clause(k, positive, 3, false, 0);
  kernel_add_large_clause(k, negative, 3, false, 0);
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

// Kissat keeps candidates in a mutable elimination heap.  An earlier pure
// elimination can remove the last clause of a candidate that was already in
// that heap; resolve.c still classifies the resulting zero/zero candidate as
// pure and retires it.  The saved public instance models that stale heap entry.
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
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(2, false);
  set(3, true);
  set(1, false); // deliberately wrong — extend must recover 1 = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "1 reconstructed to true");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// An eliminated pivot whose partner is ALSO never assigned (both retired, as
// DefaultPolicy leaves them since variable retirement): extend must complete
// the assignment before replay, or the both-sides windows double-flip the
// pivot (the portsfin N2 incident; the toy is x ≡ ¬t via (x∨t),(¬x∨¬t),
// whose resolvent is tautological, so nothing constrains x,t after apply).
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
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// A variable whose resolvent count exceeds occ⁺+occ⁻+bound is not eliminated:
// 6 (x∨aᵢ) × 6 (¬x∨bⱼ) → 36 resolvents > 6+6+16.
static void test_bound_skip() {
  Kernel k;
  kernel_reserve_vars(k, 13); // x=1, a=2..7, b=8..13
  for (int i = 2; i <= 7; i++)
    kernel_add_binary_clause(k, internalize(1), internalize(i)); // (1∨aᵢ)
  for (int j = 8; j <= 13; j++)
    kernel_add_binary_clause(k, internalize(-1), internalize(j)); // (¬1∨bⱼ)
  const RuleVTable &vt = tactix_rules()[tactix_rule_id("BVE")];
  assert(vt.apply_instance(k, lit_instance(internalize(1))) ==
         ApplyResult::NoOp);                               // 36 > 28: kept
  assert(has_binary(k, internalize(1), internalize(2)));   // var 1 still present
}

// A failed candidate must not resolve ahead to a future ramp level. Kissat
// stops on the first non-tautological
// resolvent beyond pos+neg+CURRENT bound: 2*9 with bound 0 has base 11 and
// therefore charges exactly 12 attempts before declining.
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

// resolve.c learns a unit as soon as a pair collapses, before it knows whether
// the candidate's later pairs fit the growth bound.  That root and any large
// antecedent it satisfies survive the kept pivot; only buffered non-unit
// resolvents are discarded.  This is the small form of the Rank-33 divergence.
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

// Kissat marks AND/OR inputs by literal and then copies every matching binary
// watch into the gate stack.  Duplicate binary clauses are therefore all gate
// members and each contributes its own resolvent; collapsing them to one index
// changes the resolvent multiset and dynamic elimination-heap updates.
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

// Pinned Kissat's AND scan also has a less obvious duplicate rule: for a
// partner not used by the base clause, the first physical watch consumes the
// literal mark and remains an antecedent, while later copies enter the gate.
// This changes resolvent contents even when the charged-pair count is equal.
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

// gates.c get_antecedents splits the stream by raw-watch equality with one
// cursor over the gate stack, not by the extractor's marks: once the required
// binaries are consumed, an EARLIER copy of the unrelated duplicate is raw-equal
// to the gate copy and takes its slot, so the later copy becomes the antecedent
// at its own position.  The resolvent order shows it: the base then meets (1,5)
// before (1,4).
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

static bool has_large(const Kernel &k, std::vector<int> want); // defined below

// equivalences.c marks the positive side's physical binaries and takes the
// FIRST negative-side physical binary whose partner's complement is marked.
// With two equivalence partners in crossed order the chosen pair decides the
// resolvent insertion order: (¬a∨b) is added before (a∨¬b).
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

// ands.c pushes physical binary watches only.  A large clause whose root-false
// literal leaves pivot+partner open is an antecedent, never a gate member, so
// it meets the base clause instead of the opposite antecedents.
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

// Pinned Kissat scans physical large bases after root-false stripping. A base
// may therefore have only pivot+input active and still define a gate when the
// binary side has another distinct (antecedent) partner. The physical binary
// equivalence detector cannot recognize this case.
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

// Kissat permits a root-shortened physical large clause as the AND/OR base,
// but `ands.c::kissat_mark_binaries` accepts only physical binary watches as
// its input implications.  Treating a shortened large input as a binary can
// invent a syntactic gate, suppress antecedent×antecedent resolvents, and
// change the mutable BVE trajectory even though the transformation remains
// satisfiability preserving.
static void test_gate_and_or_rejects_root_shortened_large_inputs() {
  Kernel k;
  kernel_reserve_vars(k, 12); // x=1, inputs=2,3, false tails=4,5
  const Lit base[3] = {internalize(1), internalize(-2), internalize(-3)};
  const Lit input_a[3] = {internalize(-1), internalize(2), internalize(4)};
  const Lit input_b[3] = {internalize(-1), internalize(3), internalize(5)};
  kernel_add_large_clause(k, base, 3, false, 0);
  kernel_add_large_clause(k, input_a, 3, false, 0);
  kernel_add_large_clause(k, input_b, 3, false, 0);
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
  auto set = [&](int dimacs, bool val) {
    const Lit l = internalize(dimacs);
    k.values[l] = val ? 1 : -1;
    k.values[negate(l)] = val ? -1 : 1;
  };
  set(2, true);  // y = T
  set(4, true);  // needed by (¬y∨4)
  set(3, false);
  set(1, false); // wrong — extend must recover x = y = true
  kernel_extend_model(k);
  assert(k.values[internalize(1)] > 0 && "x reconstructed to y = true");
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// An ITE gate x ↔ (c ? ¬e : ¬t) as four ternaries, plus 6 antecedent binaries
// per side: 60 non-tautological pairs against base+bound = 32, so only the gate
// reduction's 24 charged pairs can fire. The mask arm attributes the firing.
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

// Pinned Kissat `ifthenelse.c` also recognizes an ITE after one of the two
// opposite-side ternaries has shortened to a binary.  This is the exact
// candidate-897 shape from the Rank-15 trace (renumbered): the two negative
// ternaries define the ITE, (x∨¬6) is its shortened positive gate clause,
// and (x∨¬3) / (¬x∨4) are antecedents whose resolvent must be skipped.
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

// An arity-2 XOR gate x ↔ a⊕b, same arithmetic as the ITE case. The ITE
// detector must decline it (its two branch literals share a variable), so the
// off arm is a real negative; model reconstruction is pinned here too.
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
    auto set = [&](int dimacs, bool val) {
      const Lit l = internalize(dimacs);
      k.values[l] = val ? 1 : -1;
      k.values[negate(l)] = val ? -1 : 1;
    };
    set(2, true);   // a = T
    set(3, false);  // b = F  ⇒ the gate forces x = a⊕b = T
    for (int p = 4; p <= 9; p++)
      set(p, false); // every (x∨p) antecedent needs x
    for (int q = 10; q <= 15; q++)
      set(q, true);  // every (¬x∨q) antecedent already satisfied
    set(1, false);   // deliberately wrong
    kernel_extend_model(k);
    assert(k.values[internalize(1)] > 0 && "x reconstructed to a⊕b = true");
  }
}

// a live large clause whose literal set is exactly `want` (order-insensitive)
static bool has_large(const Kernel &k, std::vector<int> want) {
  std::sort(want.begin(), want.end());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      std::vector<int> got;
      for (const Lit l : *c)
        got.push_back(externalize(l));
      std::sort(got.begin(), got.end());
      if (got == want)
        return true;
    }
    ref += clause_actual_words(c);
  }
  return false;
}

// A live large clause with exactly this stored literal order. BVE occurrence
// notifications follow that order, so set equality is not enough for source
// parity with Kissat's connect_resolvents path.
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

// Pinned definition.c imports a physical large occurrence with only the pivot
// removed. A root-false tail remains a free Kitten literal. Stripping it first
// would turn (y∨f),(¬y) into an artificial UNSAT core and incorrectly skip the
// large antecedent×antecedent resolvent below.
static void test_definition_retains_root_false_physical_literals() {
  Kernel k;
  kernel_reserve_vars(k, 7); // x=1, y=2, f=3, pos ant=4,5, neg ant=6,7
  const Lit pos_gate[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit pos_ant[3] = {internalize(1), internalize(4), internalize(5)};
  const Lit neg_ant[3] = {internalize(-1), internalize(6), internalize(7)};
  kernel_add_large_clause(k, pos_gate, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_add_large_clause(k, pos_ant, 3, false, 0);
  kernel_add_large_clause(k, neg_ant, 3, false, 0);
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

// A general definition, i.e. one no syntactic matcher sees: for pivot x,
//   gate⁺ (x∨a) (x∨b)      gate⁻ (¬x∨¬a∨c) (¬x∨¬b∨¬c)
// reduces to an UNSAT set, so those four clauses define x. Unlike the syntactic
// shapes its gate×gate resolvents are not tautologies —
// (x∨a)⊗(¬x∨¬b∨¬c) = (a∨¬b∨¬c) — which is why the definition type sets
// `resolve_gate`; eliminating without that resolvent would be unsound.
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

// Kissat definition.c: if the embedded UNSAT core uses only one pivot side,
// lift its learned refutation by appending the pivot and commit the final unit
// before any resolvent planning. The four positive reduced clauses are the
// smallest no-UP refutation of all assignments to a,b, so deriving x requires
// learned intermediate clauses rather than a direct main-formula RUP.
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

  // This core needs search again after the shuffle. With no allowance for
  // that refinement and enough opposite antecedents to make ordinary BVE
  // unprofitable, the typed multiplier alone controls the outcome.
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

// Model reconstruction through a definition-derived elimination. Random
// formulas seeded with definitional gadgets, only the definition type enabled,
// every model of the survivor extended through ρ and checked against the
// original clauses; `repaired` counts the models that genuinely needed ρ.
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

// Gate-type selection via Kernel::bve_gate_mask. The AND/OR variable of
// test_gate_and_or_fires eliminates only through the gate reduction, so the
// mask must flip the same elimination between NoOp and Ok.
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

// Retry filter (Kernel::bve_retry, kissat flags->eliminate): a variable BVE
// already attempted against an unchanged neighbourhood leaves the schedule, and
// any change to it puts the variable back. The advice narrows, the API cannot.
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
  const Lit r[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, r, 3, /*redundant=*/true, /*glue=*/3);
  assert(!worth_retry(x));
  // A root assignment alone is not a Kissat eliminate notification. In dense
  // mode a root-false literal stays physically in its clause, and only an
  // actual later clause/literal removal calls mark_removed_literal. The old
  // fixed-neighbour proxy rearmed x here and created a non-reference second
  // BVE-schedule member on Rank 34.
  kernel_assign_unit(k, internalize(-2));
  assert(!worth_retry(x));
}

static void test_large_rewrite_retries_only_removed_literals() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit original[4] = {internalize(1), internalize(2), internalize(3),
                           internalize(4)};
  const Ref ref = kernel_add_large_clause(k, original, 4, false, 0);
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

// Kissat dense entry places the pre-existing binaries first, then the complete
// arena occurrence stream. A binary resolvent created later is appended to
// that mixed stream; it must not jump back ahead of those large antecedents.
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

// Wrong-SAT regression: a backward strengthening that collapses a victim to
// ONE literal must assert that unit. kernel_replace_clause deletes the victim
// and by contract leaves unit semantics to the caller, so dropping it silently
// weakens the formula — an UNSAT Tier C instance answered SAT with the unit
// present in its own proof stream but absent from the kernel (PROGRESS Log,
// the bve_backward unit-drop entry).
static void test_backward_strengthen_to_unit_asserts() {
  Kernel k;
  k.bve_backward = true; // explicitly exercise the optional proof rule
  kernel_reserve_vars(k, 6);
  auto L = [](int d) { return internalize(d); };
  auto add3 = [&](int a, int b, int c) {
    const Lit v[3] = {L(a), L(b), L(c)};
    kernel_add_large_clause(k, v, 3, /*redundant=*/false, /*glue=*/0);
  };
  // Eliminating 1 resolves to: the unit (¬4) from (1∨¬4)×(¬1∨¬4), the large
  // resolvent c=(2∨3∨4) from (1∨2∨3)×(¬1∨4), and (2∨3∨¬4). The unit is
  // asserted before the backward drain runs, so c enters the drain with 4
  // absorbed root-false: marked literals {2,3}, size 2.
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

// The binary arm of the same drain: a size-2 resolvent re-enters as a
// subsumer and strengthens a BINARY victim to a unit, which is asserted, and
// the victim pair leaves the watch lists.
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
  test_guard();
  test_lrat_shared_parent_root_unit_once();
  test_occurrences();
  test_schedule_occurrence_image_lifecycle();
  test_candidate_gather_compacts_garbage_occurrences();
  test_candidate_gather_stops_before_late_garbage();
  test_oversized_polarity_suppresses_other_compaction_rescore();
  test_candidate_gather_deletes_satisfied_binary();
  test_candidate_gather_mutating_prefix_boundary();
  test_flush_units_while_connected();
  test_occurrences_skip_redundant();
  test_schedule_order();
  test_schedule_excludes();
  test_schedule_includes_extension_variable();
  test_instance_surface();
  test_antecedent_clause_size_limit();
  test_eliminate_basic();
  test_large_resolvent_is_irredundant_with_zero_glue();
  test_elimination_retains_duplicate_binary_resolvent();
  test_scheduled_candidate_that_becomes_isolated_is_retired();
  test_eliminate_reconstruction();
  test_eliminate_frozen_pivot_reconstruction();
  test_bound_skip();
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

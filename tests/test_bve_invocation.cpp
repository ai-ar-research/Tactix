#undef NDEBUG
#include "kernel/kernel.hpp"
#include "temporary_file.hpp"
#include "rule_test_utils.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/occurrence_execution.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <span>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

static const RuleVTable &bve_rule() {
  return tactix_rules()[static_cast<size_t>(tactix_rule_id("BVE"))];
}

// One session per atomic application, as the online executor applies a
// payload; the session restores the caller's BVE pose before this returns.
static InvocationResult apply_one(Kernel &k, const BveParameters &parameters,
                                  uint64_t instance, InvocationBudget &budget,
                                  uint64_t revision = 0) {
  BveInvocationSession session(bve_rule(), k, parameters, budget, revision);
  return session.apply_one(instance);
}

static InvocationResult apply_one(Kernel &k, const BveParameters &parameters,
                                  uint64_t instance) {
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  return apply_one(k, parameters, instance, budget);
}

// A selected batch as the online executor applies it: one session per member
// over a shared budget; an exhausted or stale member declines unmutated.
static std::vector<InvocationResult>
apply_batch(Kernel &k, const BveParameters &parameters,
            std::span<const uint64_t> instances, InvocationBudget &budget) {
  std::vector<InvocationResult> results;
  for (const uint64_t instance : instances)
    results.push_back(apply_one(k, parameters, instance, budget));
  return results;
}

// A generated pass, as the online executor prepares CompleteCurrentPass: the
// complete current scope is enumerated before the session poses parameters.
static std::vector<InvocationResult>
apply_complete_pass(Kernel &k, const BveParameters &parameters) {
  std::vector<uint64_t> instances;
  bve_complete_instances(k, bve_rule(), parameters, instances);
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  return apply_batch(k, parameters, instances, budget);
}

static void add_pair(Kernel &k, int pivot, int positive_partner,
                     int negative_partner) {
  kernel_add_binary_clause(k, internalize(pivot),
                           internalize(positive_partner));
  kernel_add_binary_clause(k, internalize(-pivot),
                           internalize(negative_partner));
}

static bool has_live_large(Kernel &k, std::vector<Lit> want) {
  std::sort(want.begin(), want.end());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      std::vector<Lit> got(clause->begin(), clause->end());
      std::sort(got.begin(), got.end());
      if (got == want)
        return true;
    }
    ref += clause_actual_words(clause);
  }
  return false;
}

static void build_two_pivots(Kernel &k) {
  kernel_reserve_vars(k, 6);
  add_pair(k, 1, 3, 4);
  add_pair(k, 2, 5, 6);
}

static void test_empty_pivot_progress_distinguishes_retirement() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const uint64_t first = lit_instance(k, internalize(1));
  assert(apply_rule_instance(bve_rule(), k, first) == ApplyResult::Ok);
  const auto witness = k.reconstruction;
  const auto retired = k.retired_events;
  const uint64_t clauses = k.next_clause_id;
  assert(apply_rule_instance(bve_rule(), k, first) == ApplyResult::NoOp);
  assert(k.stats.bve_eliminated == 1 && k.reconstruction == witness);
  assert(k.retired_events == retired && k.next_clause_id == clauses);
  // An active variable with no occurrences still leaves the active population;
  // the no-op repair must not turn that legal elimination into a restriction.
  assert(apply_rule_instance(bve_rule(), k, lit_instance(k, internalize(3))) ==
         ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(3))));
  assert(apply_rule(bve_rule(), k) == ApplyResult::Ok);
  assert(apply_rule(bve_rule(), k) == ApplyResult::NoOp);
  assert(k.stats.bve_eliminated == 3 && k.unassigned_active == 0);
  kernel_check_invariants(k);
}

static void test_one_instance_matches_legacy_and_restores_parameters() {
  Kernel legacy, configured;
  kernel_reserve_vars(legacy, 3);
  kernel_reserve_vars(configured, 3);
  add_pair(legacy, 1, 2, 3);
  add_pair(configured, 1, 2, 3);
  legacy.bve_gate_mask = configured.bve_gate_mask = 0;
  legacy.bve_bound = configured.bve_bound = 7;
  legacy.bve_occurrence_limit = configured.bve_occurrence_limit = 19;
  legacy.bve_clause_size_limit = configured.bve_clause_size_limit = 23;
  legacy.bve_resolvent_count_limit =
      configured.bve_resolvent_count_limit = 27;
  legacy.bve_xor_arity_limit = configured.bve_xor_arity_limit = 4;
  legacy.bve_resolvent_size_limit = configured.bve_resolvent_size_limit = 29;
  legacy.bve_backward_occurrence_limit =
      configured.bve_backward_occurrence_limit = 31;
  legacy.bve_definition_budget = configured.bve_definition_budget = 37;
  legacy.bve_definition_cores = configured.bve_definition_cores = 3;
  legacy.bve_definition_core_budget_multiplier =
      configured.bve_definition_core_budget_multiplier = 43;
  legacy.bve_backward = configured.bve_backward = false;
  legacy.bve_on_the_fly_strengthening =
      configured.bve_on_the_fly_strengthening = true;

  const uint64_t pivot = lit_instance(internalize(1));
  const ApplyResult expected = apply_rule_instance(bve_rule(), legacy, pivot);
  const BveParameters saved = BveParameters::from_kernel(configured);
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const InvocationResult actual =
      apply_one(configured, saved, pivot, budget, 41);

  assert(expected == ApplyResult::Ok && actual.rule_result == expected);
  assert(actual.status == InvocationStatus::Completed);
  assert(actual.selection_complete && actual.applications == 1);
  assert(actual.made_rule_progress && actual.configuration_revision == 41);
  assert(actual.work == configured.stats.bve_resolutions &&
         budget.consumed() == actual.work);
  assert(configured.stats.bve_resolutions == legacy.stats.bve_resolutions);
  assert(configured.stats.bve_eliminated == legacy.stats.bve_eliminated);
  assert(configured.reconstruction == legacy.reconstruction);
  assert(kernel_binary_live(configured, internalize(2), internalize(3)) ==
         kernel_binary_live(legacy, internalize(2), internalize(3)));
  const BveParameters restored = BveParameters::from_kernel(configured);
  assert(restored.gate_mask == saved.gate_mask);
  assert(restored.growth_bound == saved.growth_bound);
  assert(restored.occurrence_limit == saved.occurrence_limit);
  assert(restored.clause_size_limit == saved.clause_size_limit);
  assert(restored.resolvent_count_limit == saved.resolvent_count_limit);
  assert(restored.xor_arity_limit == saved.xor_arity_limit);
  assert(restored.resolvent_size_limit == saved.resolvent_size_limit);
  assert(restored.backward_occurrence_limit ==
         saved.backward_occurrence_limit);
  assert(restored.definition_budget == saved.definition_budget);
  assert(restored.definition_cores == saved.definition_cores);
  assert(restored.definition_core_budget_multiplier ==
         saved.definition_core_budget_multiplier);
  assert(restored.backward == saved.backward);
  assert(restored.on_the_fly_strengthening ==
         saved.on_the_fly_strengthening);
  assert(restored.traversal == saved.traversal);
  kernel_check_invariants(configured);
}

static void test_per_call_bound_is_effective_and_scoped() {
  Kernel k;
  kernel_reserve_vars(k, 12);
  for (int i = 2; i <= 3; ++i)
    kernel_add_binary_clause(k, internalize(1), internalize(i));
  for (int i = 4; i <= 12; ++i)
    kernel_add_binary_clause(k, internalize(-1), internalize(i));
  k.bve_gate_mask = 0;
  k.bve_bound = 0;
  BveParameters posed = BveParameters::from_kernel(k);
  posed.growth_bound = 16;
  const InvocationResult result =
      apply_one(k, posed, lit_instance(internalize(1)));
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok && result.work == 18);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(k.bve_bound == 0 && "the invocation must restore the caller's pose");
}

static InvocationResult invoke_fastel(Kernel &k, uint32_t pivot,
                                      uint32_t resolvent_limit) {
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  parameters.growth_bound = UINT32_MAX;
  parameters.resolvent_count_limit = resolvent_limit;
  parameters.resolvent_size_limit = UINT32_MAX;
  parameters.backward = false;
  parameters.traversal = BveTraversal::FastEliminate;
  return apply_one(k, parameters, lit_instance(k, internalize(pivot)));
}

static InvocationResult invoke_otf(Kernel &k, uint32_t pivot, bool enabled,
                                   uint32_t resolvent_limit = 0) {
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  parameters.growth_bound = UINT32_MAX;
  parameters.resolvent_count_limit = resolvent_limit;
  parameters.resolvent_size_limit = UINT32_MAX;
  parameters.backward = false;
  parameters.on_the_fly_strengthening = enabled;
  parameters.traversal = BveTraversal::Eliminate;
  return apply_one(k, parameters, lit_instance(k, internalize(pivot)));
}

static void test_otf_single_strengthening_precedes_resolvent_cap() {
  Kernel disabled, enabled;
  for (Kernel *k : {&disabled, &enabled}) {
    kernel_reserve_vars(*k, 3);
    const Lit positive[3] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(*k, positive, 3, false, 0);
    kernel_add_binary_clause(*k, internalize(-1), internalize(2));
  }

  const InvocationResult off = invoke_otf(disabled, 1, false);
  assert(off.status == InvocationStatus::NoChange);
  assert(!kernel_var_retired(disabled, var_of(internalize(1))));
  assert(disabled.stats.bve_otf_strengthened == 0);

  const InvocationResult on = invoke_otf(enabled, 1, true);
  assert(on.status == InvocationStatus::Completed);
  assert(on.rule_result == ApplyResult::Ok);
  assert(kernel_var_retired(enabled, var_of(internalize(1))));
  assert(kernel_binary_live(enabled, internalize(2), internalize(3)));
  assert(enabled.stats.bve_resolutions == 1);
  assert(enabled.stats.bve_otf_strengthened == 1);
  assert(enabled.stats.bve_otf_subsumed == 0);
  assert(enabled.stats.strengthened == 1);
  assert(!enabled.bve_on_the_fly_strengthening &&
         "the invocation must restore the caller's BVE pose");
  kernel_check_invariants(disabled);
  kernel_check_invariants(enabled);
}

static void test_otf_size_order_reaches_short_parent_before_cap() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  add_external_clause(k, {1, 2, 3, 4});
  const Ref shortened =
      add_external_clause(k, {1, 5, 6});
  kernel_add_binary_clause(k, internalize(-1), internalize(5));

  OccIndex index;
  execution::OccurrenceSession occurrences(
      k, execution::OccurrenceSessionRequest{
             index, execution::OccurrenceAttachment::Build, true});
  const InvocationResult result = invoke_otf(k, 1, true, 0);
  assert(result.status == InvocationStatus::Completed);
  assert(!kernel_var_retired(k, var_of(internalize(1))));
  assert(k.stats.bve_otf_strengthened == 1);
  assert(clause_at(k.arena, shortened)->garbage());
  assert(kernel_binary_live(k, internalize(5), internalize(6)));
  occurrences.close();
  kernel_check_invariants(k);
}

static void test_otf_replacement_updates_live_schedule_without_pivot_retry() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  const Lit first_negative[3] = {internalize(-1), internalize(2),
                                 internalize(4)};
  const Ref strengthened =
      kernel_add_large_clause(k, first_negative, 3, false, 0);
  add_external_clause(k, {-1, 4, 5});

  OccIndex index;
  execution::OccurrenceSession occurrences(
      k, execution::OccurrenceSessionRequest{
             index, execution::OccurrenceAttachment::Build, true});
  k.bve_retry.assign(k.nvars, 1);
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  parameters.growth_bound = UINT32_MAX;
  parameters.resolvent_count_limit = 0;
  parameters.resolvent_size_limit = UINT32_MAX;
  parameters.on_the_fly_strengthening = true;
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const uint64_t pivot = lit_instance(k, internalize(1));
  {
    BveInvocationSession session(bve_rule(), k, parameters, budget);
    session.order_by_score_heap(&pivot, 1);
    assert(session.next_ordered() == pivot && session.ordering_empty());
    const InvocationResult result = session.apply_one(pivot);
    assert(result.status == InvocationStatus::Completed);
    assert(!kernel_var_retired(k, var_of(internalize(1))));
    assert(k.stats.bve_otf_strengthened == 1);
    assert(!k.bve_retry[var_of(internalize(1))]);
    while (!session.ordering_empty())
      assert(session.next_ordered() != pivot &&
             "OTF must not reinsert the pivot being tested");
    assert(clause_at(k.arena, strengthened)->garbage());
    for (const Lit lit : first_negative)
      for (const OccIndex::Entry entry : k.dense->ordered[lit])
        assert(entry.binary || entry.payload != strengthened);
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
      uint32_t physical = k.dense->large_size(lit);
      for (const WatchEntry watch : k.watches.entries(lit))
        physical += watch.binary();
      assert(bve_schedule_occurrence_count(k, lit) == physical);
    }
    session.finish_ordering();
  }
  occurrences.close();
  kernel_check_invariants(k);
}

static void test_otf_double_strengthening_subsumes_both_parents() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Ref positive_ref =
      add_external_clause(k, {1, 2, 3});
  const Ref negative_ref =
      add_external_clause(k, {-1, 2, 3});

  const InvocationResult result = invoke_otf(k, 1, true);
  assert(result.status == InvocationStatus::Completed);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(clause_at(k.arena, positive_ref)->garbage());
  assert(clause_at(k.arena, negative_ref)->garbage());
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(k.stats.bve_resolutions == 1);
  assert(k.stats.bve_otf_strengthened == 1);
  assert(k.stats.bve_otf_subsumed == 1);
  assert(k.stats.strengthened == 1);
  kernel_check_invariants(k);
}

static void test_otf_add_precedes_parent_deletes() {
  TemporaryFile file;
  const std::string &path = file.path;
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3}, false, 0, ++k.next_clause_id);
  add_external_clause(k, {-1, 2, 3}, false, 0, ++k.next_clause_id);

  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  assert(invoke_otf(k, 1, true).status == InvocationStatus::Completed);
  proof.flush();
  k.proof = nullptr;

  std::ifstream input(path);
  std::string line;
  size_t line_number = 0, addition_line = 0;
  size_t positive_deletion_line = 0, negative_deletion_line = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line == "d 1 2 3 0")
      positive_deletion_line = line_number;
    else if (line == "d -1 2 3 0")
      negative_deletion_line = line_number;
    else if (line == "2 3 0")
      addition_line = line_number;
  }
  assert(addition_line && positive_deletion_line && negative_deletion_line);
  assert(addition_line < positive_deletion_line &&
         addition_line < negative_deletion_line &&
         "the OTF RUP clause must precede both antecedent deletions");
  kernel_check_invariants(k);
}

static void test_fastel_self_strengthening_precedes_resolvent_cap() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(-1), internalize(2));

  const InvocationResult result = invoke_fastel(k, 1, 0);
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(k.stats.strengthened == 1);
  kernel_check_invariants(k);

  // At the raw cap SC2024 skips its mutating feasibility scan.
  Kernel capped;
  kernel_reserve_vars(capped, 3);
  add_external_clause(capped, {1, 2, 3});
  kernel_add_binary_clause(capped, internalize(-1), internalize(2));
  assert(invoke_fastel(capped, 1, 1).rule_result == ApplyResult::Ok);
  assert(kernel_var_retired(capped, var_of(internalize(1))));
  assert(kernel_binary_live(capped, internalize(2), internalize(3)));
  assert(capped.stats.strengthened == 0);
}

static Ref build_fastel_collapsed_binary_large(Kernel &k,
                                                bool explicit_ids = false) {
  kernel_reserve_vars(k, 6);
  const auto id = [&]() { return explicit_ids ? ++k.next_clause_id : 0; };
  const Ref victim_ref =
      add_external_clause(k, {1, 2, 3}, false, 0, id());
  add_external_clause(k, {1, 4, 5}, false, 0, id());
  kernel_add_binary_clause(k, internalize(-1), internalize(2), id());
  kernel_add_binary_clause(k, internalize(-1), internalize(6), id());
  const uint64_t root_id = id();
  if (root_id)
    k.unit_ids[var_of(internalize(-3))] = root_id;
  kernel_assign_unit(k, internalize(-3));
  return victim_ref;
}

static void test_fastel_collapsed_binary_large_deletes_source_on_decline() {
  Kernel k;
  const Ref victim = build_fastel_collapsed_binary_large(k);

  const InvocationResult result = invoke_fastel(k, 1, 0);
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok && result.made_rule_progress);
  assert(!kernel_var_retired(k, var_of(internalize(1))) &&
         "the later unresolved pair must still decline the pivot");
  assert(k.value(internalize(2)) > 0 &&
         "the collapsed self-strengthening must assert its unit");
  assert(clause_at(k.arena, victim)->garbage() &&
         "SC2024 deletes the large source of binary x large collapsed SSR");
  assert(k.stats.strengthened == 1);
  assert(kernel_binary_live(k, internalize(-1), internalize(6)));
  kernel_check_invariants(k);
}

static void test_fastel_collapsed_proof_add_precedes_source_delete() {
  TemporaryFile file;
  const std::string &path = file.path;
  Kernel k;
  build_fastel_collapsed_binary_large(k, true);
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;

  const InvocationResult result = invoke_fastel(k, 1, 0);
  assert(result.status == InvocationStatus::Completed);
  proof.flush();
  k.proof = nullptr;

  std::ifstream input(path);
  std::string line;
  size_t line_number = 0, unit_line = 0, deletion_line = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.rfind("d ", 0) == 0) {
      if (line.find("1 2 3 0") != std::string::npos ||
          line.find("1 3 2 0") != std::string::npos ||
          line.find("2 1 3 0") != std::string::npos ||
          line.find("2 3 1 0") != std::string::npos ||
          line.find("3 1 2 0") != std::string::npos ||
          line.find("3 2 1 0") != std::string::npos)
        deletion_line = line_number;
    } else if (!unit_line && line.rfind("2 ", 0) == 0) {
      // A staged DSR unit precedes the deletion block through its split helper.
      unit_line = line_number;
    }
  }
  assert(unit_line && deletion_line && unit_line < deletion_line &&
         "the collapsed RUP unit must be emitted before its source is deleted");
  kernel_check_invariants(k);
}

static void test_fastel_prunes_stale_resolvent_without_proof_sink() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));

  const InvocationResult result = invoke_fastel(k, 1, 1);
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(!has_live_large(k, {internalize(2), internalize(3),
                             internalize(4)}) &&
         "a tentative resolvent of a replaced parent must not be published");
  assert(k.stats.strengthened == 1);
  kernel_check_invariants(k);
}

static void test_fastel_decline_retains_checked_strengthening() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit strengthened[3] = {internalize(1), internalize(2),
                               internalize(3)};
  const Ref strengthened_ref =
      kernel_add_large_clause(k, strengthened, 3, false, 0);
  add_external_clause(k, {1, 4, 5});
  kernel_add_binary_clause(k, internalize(-1), internalize(2));

  const InvocationResult result = invoke_fastel(k, 1, 0);
  assert(result.status == InvocationStatus::Completed);
  assert(result.rule_result == ApplyResult::Ok &&
         "the retained strengthening is progress even when the pivot stays");
  assert(!kernel_var_retired(k, var_of(internalize(1))));
  assert(clause_at(k.arena, strengthened_ref)->garbage());
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(k.stats.strengthened == 1);
  kernel_check_invariants(k);
}

static void test_fastel_strengthening_is_atomic_at_budget_boundary() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  const Ref strengthened_ref =
      add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {1, 4, 5});
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  add_pair(k, 6, 7, 8);

  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  parameters.growth_bound = UINT32_MAX;
  parameters.resolvent_count_limit = 0;
  parameters.resolvent_size_limit = UINT32_MAX;
  parameters.backward = false;
  parameters.traversal = BveTraversal::FastEliminate;
  const uint64_t selected[] = {lit_instance(k, internalize(1)),
                               lit_instance(k, internalize(6))};
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::bounded(1));
  const auto results = apply_batch(k, parameters, selected, budget);

  // The budget interrupts only between atomic pivots: the first pivot's checked
  // strengthening survives and the next pivot is untouched.
  assert(results[0].applications == 1 && results[0].made_rule_progress);
  assert(results[0].work >= 1 &&
         results[0].budget_status == BudgetStatus::Exhausted);
  assert(results[1].status == InvocationStatus::BudgetExhausted);
  assert(results[1].applications == 0 && !results[1].selection_complete);
  assert(!kernel_var_retired(k, var_of(internalize(1))));
  assert(clause_at(k.arena, strengthened_ref)->garbage());
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
  assert(!kernel_var_retired(k, var_of(internalize(6))));
  assert(kernel_binary_live(k, internalize(6), internalize(7)));
  assert(kernel_binary_live(k, internalize(-6), internalize(8)));
  kernel_check_invariants(k);
}

static void build_xor3(Kernel &k) {
  kernel_reserve_vars(k, 4);
  // Forbid every assignment of odd parity to x,a,b,c.
  for (uint32_t assignment = 0; assignment < 16; ++assignment) {
    if (__builtin_popcount(assignment) % 2 == 0)
      continue;
    Lit clause[4];
    for (uint32_t v = 0; v < 4; ++v)
      clause[v] = make_lit(v, (assignment >> v) & 1u);
    kernel_add_large_clause(k, clause, 4, false, 0);
  }
}

static void test_per_call_xor_arity_is_effective_and_scoped() {
  Kernel low, high;
  build_xor3(low);
  build_xor3(high);
  const uint64_t pivot = lit_instance(internalize(1));

  BveParameters low_parameters = BveParameters::from_kernel(low);
  low_parameters.gate_mask = BVE_GATE_XOR;
  low_parameters.xor_arity_limit = 2;
  assert(apply_one(low, low_parameters, pivot).rule_result == ApplyResult::Ok);
  assert(low.stats.bve_gates_xor == 0);

  high.bve_xor_arity_limit = 2; // caller state restored after the posed call
  BveParameters high_parameters = BveParameters::from_kernel(high);
  high_parameters.gate_mask = BVE_GATE_XOR;
  high_parameters.xor_arity_limit = 3;
  assert(apply_one(high, high_parameters, pivot).rule_result ==
         ApplyResult::Ok);
  assert(high.stats.bve_gates_xor == 1);
  assert(high.bve_xor_arity_limit == 2);
}

static void test_pass_session_poses_once_and_keeps_cumulative_budget() {
  Kernel k;
  build_two_pivots(k);
  k.bve_gate_mask = 0;
  k.bve_bound = 0;
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.growth_bound = 16;
  InvocationBudget budget(WorkUnit::BveTicks, WorkLimit::bounded(1));
  {
    BveInvocationSession session(bve_rule(), k, parameters, budget, 53);
    assert(session.valid() && k.bve_bound == 16);
    const InvocationResult first =
        session.apply_one(lit_instance(internalize(1)));
    assert(first.status == InvocationStatus::Completed);
    assert(first.applications == 1 && first.work == 1);
    assert(first.made_rule_progress &&
           first.budget_status == BudgetStatus::Exhausted);
    assert(first.configuration_revision == 53 && budget.consumed() == 1);
    // The budget stops the pass between atomic pivots.
    const InvocationResult second =
        session.apply_one(lit_instance(internalize(2)));
    assert(second.status == InvocationStatus::BudgetExhausted);
    assert(second.budget_status == BudgetStatus::Exhausted);
    assert(second.applications == 0 && !second.selection_complete);
    assert(budget.consumed() == 1);
    assert(kernel_var_retired(k, var_of(internalize(1))));
    assert(!kernel_var_retired(k, var_of(internalize(2))));
  }
  assert(k.bve_bound == 0 && "the session must restore the caller pose");
  kernel_check_invariants(k);
}

static void test_complete_pass_and_stale_batch_are_distinct() {
  Kernel pass;
  build_two_pivots(pass);
  BveParameters parameters = BveParameters::from_kernel(pass);
  parameters.gate_mask = 0;
  const auto full = apply_complete_pass(pass, parameters);
  assert(full.size() == 6);
  for (const InvocationResult &one : full)
    assert(one.status == InvocationStatus::Completed &&
           one.applications == 1 && one.selection_complete);
  assert(pass.stats.bve_eliminated == 6);

  Kernel stale;
  kernel_reserve_vars(stale, 2);
  kernel_add_binary_clause(stale, internalize(1), internalize(2));
  kernel_add_binary_clause(stale, internalize(-1), internalize(2));
  const uint64_t selected[] = {lit_instance(internalize(1)),
                               lit_instance(internalize(2))};
  const BveParameters current = BveParameters::from_kernel(stale);
  InvocationBudget batch_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  const auto changed = apply_batch(stale, current, selected, batch_budget);
  // The first member's unit resolvent assigns the second member.
  assert(changed[0].applications == 1);
  assert(changed[1].status == InvocationStatus::StaleRequest);
  assert(changed[1].applications == 0 && !changed[1].selection_complete);
  assert(stale.value(internalize(2)) > 0);
  // A retired pivot is stale as well.
  assert(apply_one(stale, current, selected[0]).status ==
         InvocationStatus::StaleRequest);
}

static void test_pure_scope_filters_only_generated_complete_passes() {
  Kernel generated;
  kernel_reserve_vars(generated, 3);
  kernel_add_binary_clause(generated, internalize(1), internalize(2));
  kernel_add_binary_clause(generated, internalize(-2), internalize(3));
  BveParameters pure = BveParameters::from_kernel(generated);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  const auto filtered = apply_complete_pass(generated, pure);
  assert(filtered.size() == 2);
  for (const InvocationResult &one : filtered)
    assert(one.status == InvocationStatus::Completed);
  assert(kernel_var_retired(generated, var_of(internalize(1))));
  assert(kernel_var_retired(generated, var_of(internalize(3))));
  assert(!kernel_var_retired(generated, var_of(internalize(2))));

  Kernel explicit_instance;
  kernel_reserve_vars(explicit_instance, 3);
  add_pair(explicit_instance, 1, 2, 3);
  pure = BveParameters::from_kernel(explicit_instance);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  const InvocationResult unfiltered =
      apply_one(explicit_instance, pure, lit_instance(internalize(1)));
  assert(unfiltered.status == InvocationStatus::Completed);
  assert(unfiltered.applications == 1 && unfiltered.made_rule_progress);
  assert(kernel_var_retired(explicit_instance, var_of(internalize(1))));
  kernel_check_invariants(generated);
  kernel_check_invariants(explicit_instance);
}

// PureClosure selects, from one occurrence image, the sequence that repeating
// PureOnly to a fixpoint selects pass by pass.
static void test_pure_closure_matches_repeated_pure_passes() {
  const auto build = [](Kernel &k) {
    kernel_reserve_vars(k, 8);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(-2), internalize(3));
    kernel_add_binary_clause(k, internalize(-3), internalize(4));
    add_external_clause(k, {-4, 5, 6});
    kernel_add_binary_clause(k, internalize(-5), internalize(-6));
    kernel_add_binary_clause(k, internalize(7), internalize(8));
    kernel_add_binary_clause(k, internalize(-7), internalize(-8));
  };
  const auto pass = [](Kernel &k, BveCandidateScope scope) {
    BveParameters pure = BveParameters::from_kernel(k);
    pure.candidate_scope = scope;
    pure.gate_mask = 0;
    uint64_t applications = 0;
    for (const InvocationResult &one : apply_complete_pass(k, pure)) {
      assert(one.status == InvocationStatus::Completed ||
             one.status == InvocationStatus::NoChange);
      applications += one.applications;
    }
    return applications;
  };
  Kernel repeated, closure;
  build(repeated);
  build(closure);
  uint64_t repeated_applications = 0, passes = 0;
  while (const uint64_t applied = pass(repeated, BveCandidateScope::PureOnly)) {
    repeated_applications += applied;
    ++passes;
  }
  assert(passes > 1 && "the fixture needs more than one PureOnly layer");
  assert(pass(closure, BveCandidateScope::PureClosure) == repeated_applications);
  assert(pass(closure, BveCandidateScope::PureClosure) == 0);
  for (int variable = 1; variable <= 8; ++variable)
    assert(kernel_var_retired(repeated, var_of(internalize(variable))) ==
           kernel_var_retired(closure, var_of(internalize(variable))));
  // The 7/8 equivalence has no pure literal at any layer.
  assert(!kernel_var_retired(closure, var_of(internalize(7))));
  assert(closure.reconstruction == repeated.reconstruction);
  kernel_check_invariants(repeated);
  kernel_check_invariants(closure);
}

static void test_pure_scope_inside_unordered_dense_state() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));

  OccIndex occurrences;
  kernel_dense_connect(k, occurrences, /*track_order=*/false);
  BveParameters pure = BveParameters::from_kernel(k);
  pure.candidate_scope = BveCandidateScope::PureOnly;
  pure.gate_mask = 0;
  const auto results = apply_complete_pass(k, pure);
  assert(results.size() == 2);
  for (const InvocationResult &one : results)
    assert(one.status == InvocationStatus::Completed);
  assert(kernel_var_retired(k, var_of(internalize(1))));
  assert(kernel_var_retired(k, var_of(internalize(3))));
  assert(!kernel_var_retired(k, var_of(internalize(2))));
  kernel_check_invariants(k);
  kernel_dense_disconnect(k);
}

static void test_invalid_zero_budget_and_stop_do_not_mutate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_pair(k, 1, 2, 3);
  const uint64_t pivot = lit_instance(internalize(1));
  BveParameters invalid = BveParameters::from_kernel(k);
  invalid.xor_arity_limit = BVE_XOR_ARITY_CAPACITY + 1;
  InvocationBudget bad_budget(WorkUnit::BveTicks, WorkLimit::unlimited());
  {
    BveInvocationSession session(bve_rule(), k, invalid, bad_budget);
    assert(!session.valid());
    assert(session.apply_one(pivot).status == InvocationStatus::InvalidRequest);
  }
  assert(k.bve_xor_arity_limit != invalid.xor_arity_limit);

  // The canonical marker, a non-literal payload and an unknown variable are
  // invalid instances.
  const BveParameters parameters = BveParameters::from_kernel(k);
  for (const uint64_t instance :
       {INSTANCE_CANONICAL, uint64_t{0}, lit_instance(internalize(99))})
    assert(apply_one(k, parameters, instance).status ==
           InvocationStatus::InvalidRequest);
  assert(k.stats.bve_resolutions == 0);

  InvocationBudget zero(WorkUnit::BveTicks, WorkLimit::bounded(0));
  assert(apply_one(k, parameters, pivot, zero).status ==
         InvocationStatus::BudgetExhausted);
  assert(k.stats.bve_resolutions == 0);

  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(apply_one(k, parameters, pivot).status ==
         InvocationStatus::Interrupted);
  assert(k.stats.bve_resolutions == 0);
  k.stop = nullptr;
}

static void test_terminal_result_is_not_overwritten_by_completion() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_unit(k, internalize(-2));
  const InvocationResult result = apply_one(
      k, BveParameters::from_kernel(k), lit_instance(internalize(1)));
  assert(result.status == InvocationStatus::Unsat);
  assert(result.rule_result == ApplyResult::Unsat);
  assert(result.selection_complete && result.applications == 1);
}

static std::vector<std::vector<Lit>>
run_traversal_fixture(BveTraversal traversal, bool binary_positive) {
  Kernel k;
  kernel_reserve_vars(k, 7);
  auto L = [](int dimacs) { return internalize(dimacs); };
  if (binary_positive) {
    kernel_add_binary_clause(k, L(1), L(2));
    const Lit negative[3] = {L(-1), L(4), L(5)};
    kernel_add_large_clause(k, negative, 3, false, 0);
  } else {
    const Lit positive[3] = {L(1), L(2), L(3)};
    const Lit negative0[3] = {L(-1), L(4), L(5)};
    const Lit negative1[3] = {L(-1), L(6), L(7)};
    kernel_add_large_clause(k, positive, 3, false, 0);
    kernel_add_large_clause(k, negative0, 3, false, 0);
    kernel_add_large_clause(k, negative1, 3, false, 0);
  }
  const Ref fresh_begin = static_cast<Ref>(k.arena.size());
  BveParameters parameters = BveParameters::from_kernel(k);
  parameters.gate_mask = 0;
  parameters.growth_bound = 8;
  parameters.traversal = traversal;
  const InvocationResult result =
      apply_one(k, parameters, lit_instance(k, L(1)));
  assert(result.status == InvocationStatus::Completed);
  std::vector<std::vector<Lit>> fresh;
  for (Ref ref = fresh_begin; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage())
      fresh.emplace_back(clause->begin(), clause->end());
    ref += clause_actual_words(clause);
  }
  kernel_check_invariants(k);
  return fresh;
}

static void test_typed_fastel_traversal_owns_resolvent_pose() {
  auto L = [](int dimacs) { return internalize(dimacs); };
  const auto ordinary =
      run_traversal_fixture(BveTraversal::Eliminate, false);
  const auto fast =
      run_traversal_fixture(BveTraversal::FastEliminate, false);
  assert(ordinary.size() == 2 && fast.size() == 2);
  assert((ordinary[0] == std::vector<Lit>{L(4), L(5), L(2), L(3)}));
  assert((fast[0] == std::vector<Lit>{L(2), L(3), L(4), L(5)}));

  const auto ordinary_mixed =
      run_traversal_fixture(BveTraversal::Eliminate, true);
  const auto fast_mixed =
      run_traversal_fixture(BveTraversal::FastEliminate, true);
  assert(ordinary_mixed.size() == 1 && fast_mixed.size() == 1);
  assert((ordinary_mixed[0] == std::vector<Lit>{L(4), L(5), L(2)}));
  assert((fast_mixed[0] == std::vector<Lit>{L(2), L(4), L(5)}));
}

// A candidate declining on growth after installing units ends the canonical
// round with Ok: later candidates would see an incomplete root closure.
static void build_declining_unit_formula(Kernel &k) {
  static const int clauses[][3] = {
      {-7, -4, 0}, {4, 8, 5},  {2, 1, 0},  {-3, -8, 4},  {-8, -1, 0},
      {2, 3, -7},  {2, -3, 0}, {2, 6, -8}, {-7, -4, -2}, {-6, 3, 0},
      {-3, 7, 0},  {-8, 6, 4}, {1, -6, 0}, {6, -5, 0},   {5, -1, -4},
      {5, -1, -7}, {8, 3, -5}, {3, 6, -2}, {3, 6, 0},    {7, 5, 0}};
  kernel_reserve_vars(k, 8);
  for (const auto &clause : clauses) {
    Lit lits[3];
    uint32_t size = 0;
    for (const int lit : clause)
      if (lit)
        lits[size++] = internalize(lit);
    if (size == 2)
      kernel_add_binary_clause(k, lits[0], lits[1]);
    else
      kernel_add_large_clause(k, lits, size, false, 0);
  }
  k.bve_bound = 0;
}

static void test_round_ends_when_declined_candidate_changes_state() {
  Kernel k;
  build_declining_unit_formula(k);

  const Lit fixed = internalize(3);
  std::vector<Var> schedule;
  bve_schedule(k, schedule);
  assert(!schedule.empty() && schedule.back() == var_of(fixed));
  assert(k.value(fixed) == 0 && k.bve_retry[var_of(fixed)]);

  // Every candidate declines at bound zero; an earlier one learns the unit.
  const uint64_t proof_ids = k.next_clause_id;
  assert(apply_rule(bve_rule(), k) == ApplyResult::Ok);
  assert(k.value(fixed) > 0 && k.assigned[var_of(fixed)].level == 0);
  assert(k.next_clause_id > proof_ids && k.stats.bve_eliminated == 0);
  // The fixed pivot was never attempted: an attempt clears its retry bit.
  assert(k.bve_retry[var_of(fixed)] && !kernel_var_retired(k, var_of(fixed)));

  // Later rounds start from the drained root and never schedule the fixed
  // variable; BVE must not retire a variable the root assignment owns.
  for (ApplyResult r = ApplyResult::Ok; r == ApplyResult::Ok;) {
    r = apply_rule(bve_rule(), k);
    assert(r == ApplyResult::Ok || r == ApplyResult::NoOp);
    assert(k.bve_retry[var_of(fixed)]);
    assert(!kernel_var_retired(k, var_of(fixed)));
  }
  uint32_t trail_copies = 0;
  for (const Lit lit : k.trail)
    trail_copies += var_of(lit) == var_of(fixed);
  assert(trail_copies == 1);
  kernel_check_invariants(k);
}

// An instance that declines on growth after installing a unit reports Ok;
// elimination is read from retirement.
static void test_instance_decline_that_changes_state_reports_ok() {
  Kernel probe;
  build_declining_unit_formula(probe);
  std::vector<Var> schedule;
  bve_schedule(probe, schedule);
  bool found = false;
  for (const Var pivot : schedule) {
    Kernel k;
    build_declining_unit_formula(k);
    const size_t trail = k.trail.size();
    const uint64_t proof_ids = k.next_clause_id;
    const ApplyResult result = apply_rule_instance(
        bve_rule(), k, lit_instance(k, make_lit(pivot, false)));
    if (k.stats.bve_eliminated) {
      assert(result == ApplyResult::Ok && kernel_var_retired(k, pivot));
      continue;
    }
    assert(!kernel_var_retired(k, pivot));
    const bool changed =
        k.trail.size() != trail || k.next_clause_id != proof_ids;
    assert((result == ApplyResult::Ok) == changed);
    found |= changed;
  }
  assert(found && "the fixture no longer has a state-changing decline");
}

int main() {
  test_empty_pivot_progress_distinguishes_retirement();
  test_round_ends_when_declined_candidate_changes_state();
  test_instance_decline_that_changes_state_reports_ok();
  test_one_instance_matches_legacy_and_restores_parameters();
  test_per_call_bound_is_effective_and_scoped();
  test_otf_single_strengthening_precedes_resolvent_cap();
  test_otf_size_order_reaches_short_parent_before_cap();
  test_otf_replacement_updates_live_schedule_without_pivot_retry();
  test_otf_double_strengthening_subsumes_both_parents();
  test_otf_add_precedes_parent_deletes();
  test_fastel_self_strengthening_precedes_resolvent_cap();
  test_fastel_collapsed_binary_large_deletes_source_on_decline();
  test_fastel_collapsed_proof_add_precedes_source_delete();
  test_fastel_prunes_stale_resolvent_without_proof_sink();
  test_fastel_decline_retains_checked_strengthening();
  test_fastel_strengthening_is_atomic_at_budget_boundary();
  test_per_call_xor_arity_is_effective_and_scoped();
  test_pass_session_poses_once_and_keeps_cumulative_budget();
  test_complete_pass_and_stale_batch_are_distinct();
  test_pure_scope_filters_only_generated_complete_passes();
  test_pure_closure_matches_repeated_pure_passes();
  test_pure_scope_inside_unordered_dense_state();
  test_invalid_zero_budget_and_stop_do_not_mutate();
  test_terminal_result_is_not_overwritten_by_completion();
  test_typed_fastel_traversal_owns_resolvent_pose();
  std::puts("test_bve_invocation OK");
}

#undef NDEBUG
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_consequence_action.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>

using namespace tactix;
using namespace tactix::affine_semantic_extension;

static std::vector<std::vector<int32_t>> affine_formula(bool complete = true) {
  auto first = clausal_form({1, 2, 3, 4, 5}, false);
  auto second = clausal_form({1, 2, 3, 4, 6}, true);
  if (!complete) first.pop_back();
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

static void add_clause(Kernel &kernel,
                       const std::vector<int32_t> &external) {
  std::vector<Lit> literals;
  for (const int32_t literal : external)
    literals.push_back(kernel_internalize(kernel, literal));
  const uint64_t id = ++kernel.next_clause_id;
  if (literals.size() == 2)
    kernel_add_binary_clause(kernel, literals[0], literals[1], id);
  else
    kernel_add_large_clause(kernel, literals.data(), literals.size(), false, 0,
                            id);
}

static void install(Kernel &kernel,
                    const std::vector<std::vector<int32_t>> &formula,
                    uint32_t variables = 6) {
  kernel_reserve_vars(kernel, variables);
  for (const auto &clause : formula) add_clause(kernel, clause);
  assert(kernel_propagation_fixpoint(kernel));
}

static strategy::AffineConsequenceParameters parameters() {
  strategy::AffineConsequenceParameters result;
  result.maximum_clauses = 1000;
  result.maximum_snapshot_literals = 10000;
  result.maximum_snapshot_work = 100000;
  result.maximum_groups = 100;
  result.maximum_patterns = 1000;
  result.maximum_elimination_work = 1000000;
  result.maximum_arity = 8;
  result.maximum_intermediate_width = 16;
  result.maximum_sources = 16;
  result.maximum_output_width = 4;
  result.maximum_proof_additions = 100000;
  result.maximum_proof_deletions = 100000;
  result.maximum_journal_bytes = 1000000;
  result.consequence = 0;
  return result;
}

static strategy::AffineConsequenceParameters basis_parameters() {
  auto result = parameters();
  result.semantics =
      strategy::AffineConsequenceSemantics::CertifiedBasisV2;
  result.maximum_intermediate_width = 32;
  result.basis_maximum_work = 10000000;
  result.basis_maximum_intermediate_width = 32;
  result.basis_maximum_sources = 32;
  result.basis_maximum_premises = 10000;
  result.basis_maximum_nodes = 10000;
  result.basis_maximum_direct_width = 3;
  result.maximum_external_extent = 100000;
  result.maximum_session_keys = 10000;
  result.maximum_session_work = 1000000;
  return result;
}

static long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

static void state_only_guard() {
  Kernel kernel;
  install(kernel, affine_formula());
  assert(!affine_consequence_action_guard(kernel));
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  // Disabled typed parameters do not hide an otherwise legal action.
  assert(affine_consequence_action_guard(kernel));

  kernel.level = 1;
  assert(!affine_consequence_action_guard(kernel));
  kernel.level = 0;
  kernel.dense = reinterpret_cast<OccIndex *>(uintptr_t{1});
  assert(!affine_consequence_action_guard(kernel));
  kernel.dense = nullptr;
  kernel.semantic_variable_candidate_pending = true;
  assert(!affine_consequence_action_guard(kernel));
  kernel.semantic_variable_candidate_pending = false;
  kernel.structural_action_sequence = UINT64_MAX;
  assert(!affine_consequence_action_guard(kernel));
  std::fclose(records);
}

static void declines_preserve_identity_and_state() {
  Kernel kernel;
  install(kernel, affine_formula(false));
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;
  kernel.affine_consequence_parameters = parameters();
  const uint64_t proof_id = kernel.next_clause_id;
  const uint64_t binaries = kernel.watches.binary_clauses();
  const size_t arena = kernel.arena.size();

  const auto no_structure = affine_consequence_action_apply(kernel);
  assert(no_structure.live.status == AffineLiveStatus::NoStructure);
  assert(no_structure.live.result == ApplyResult::NoOp);
  assert(no_structure.proposed_action_id == 41 &&
         !no_structure.committed_action_id &&
         no_structure.live.recognition_work);
  assert(kernel.structural_action_sequence == 40 &&
         kernel.next_clause_id == proof_id &&
         kernel.watches.binary_clauses() == binaries &&
         kernel.arena.size() == arena && !position(records));
  assert(!kernel.semantic_variable_candidate_pending);

  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const auto interrupted = affine_consequence_action_apply(kernel);
  assert(interrupted.live.status == AffineLiveStatus::Interrupted);
  assert(interrupted.live.result == ApplyResult::NoOp &&
         interrupted.proposed_action_id == 41 &&
         !interrupted.committed_action_id);
  assert(kernel.structural_action_sequence == 40 &&
         kernel.next_clause_id == proof_id && !position(records));
  kernel.stop = nullptr;

  kernel.affine_consequence_parameters.maximum_output_width = 5;
  const auto invalid = affine_consequence_action_apply(kernel);
  assert(invalid.live.status == AffineLiveStatus::Invalid);
  assert(!invalid.proposed_action_id && !invalid.committed_action_id);
  assert(kernel.structural_action_sequence == 40 && !position(records));
  std::fclose(records);
}

static void exhaustion_retries_same_identity() {
  Kernel kernel;
  install(kernel, affine_formula());
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 70;
  kernel.affine_consequence_parameters = parameters();
  kernel.affine_consequence_parameters.maximum_proof_deletions = 0;

  const auto exhausted = affine_consequence_action_apply(kernel);
  assert(exhausted.live.status == AffineLiveStatus::ResourceExhausted);
  assert(exhausted.live.preflight_deletions > 0 &&
         exhausted.live.journal_bytes == 0);
  assert(exhausted.proposed_action_id == 71 &&
         !exhausted.committed_action_id);
  assert(kernel.structural_action_sequence == 70 && !position(records));
  assert(kernel.stats.affine_consequence_attempts == 1 &&
         kernel.stats.affine_consequence_resource_exhausted == 1 &&
         kernel.stats.affine_consequence_preflight_deletions > 0);

  kernel.affine_consequence_parameters.maximum_proof_deletions = 100000;
  strategy::begin_semantic_variable_candidate_action(kernel);
  const auto applied = affine_consequence_action_apply(kernel);
  assert(applied.live.status == AffineLiveStatus::Applied);
  assert(applied.live.result == ApplyResult::Ok);
  assert(applied.proposed_action_id == 71 &&
         applied.committed_action_id == 71 && applied.semantic_id == 1);
  assert(applied.live.snapshot_work && applied.live.recognition_work &&
         applied.live.proof_addition_work && applied.live.journal_work &&
         applied.live.journal_bytes == static_cast<uint64_t>(position(records)));
  assert(applied.candidate_staged &&
         kernel.structural_action_sequence == 71 &&
         kernel.semantic_variable_candidate_pending);
  assert(kernel.stats.affine_consequence_attempts == 2);
  assert(kernel.stats.affine_consequence_applied == 1);
  assert(kernel.stats.affine_consequence_candidate_variables == 2);
  assert(kernel.stats.affine_consequence_published == applied.live.published);

  ++kernel.strategy_epoch;
  strategy::finish_semantic_variable_candidate_action(kernel,
                                                       ApplyResult::Ok);
  const auto candidates = strategy::semantic_variable_candidates(kernel);
  assert(candidates.size() == 1 && candidates.front().action_id == 71 &&
         candidates.front().producer ==
             strategy::SemanticVariableProducer::Affine &&
         candidates.front().existing_variables == 2 &&
         candidates.front().introduced_variables == 0);
  const auto *candidate =
      strategy::semantic_variable_candidate(kernel, candidates.front().id);
  assert(candidate && candidate->variables.size() == 2);
  assert(candidate->variables[0] ==
         (strategy::SemanticVariable{
             5, strategy::SemanticVariableRole::ExistingTheory, 1}));
  assert(candidate->variables[1] ==
         (strategy::SemanticVariable{
             6, strategy::SemanticVariableRole::ExistingTheory, 1}));
  std::fclose(records);
}

static void propagation_is_deferred() {
  Kernel kernel;
  install(kernel, affine_formula());
  kernel_assign_unit(kernel, kernel_internalize(kernel, -5));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(kernel));
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.affine_consequence_parameters = parameters();

  const auto applied = affine_consequence_action_apply(kernel);
  assert(applied.live.status == AffineLiveStatus::Applied &&
         applied.live.result == ApplyResult::Ok &&
         applied.live.propagation_pending && applied.candidate_staged);
  assert(!kernel_propagation_fixpoint(kernel));
  assert(!kernel.value(kernel_internalize(kernel, 6)));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  assert(kernel.value(kernel_internalize(kernel, 6)) > 0);
  std::fclose(records);
}

static void terminal_result_commits_without_candidate() {
  Kernel kernel;
  auto even = clausal_form({1, 2, 3}, false);
  auto odd = clausal_form({1, 2, 3}, true);
  even.insert(even.end(), odd.begin(), odd.end());
  install(kernel, even, 3);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 9;
  kernel.affine_consequence_parameters = parameters();

  const auto terminal = affine_consequence_action_apply(kernel);
  assert(terminal.live.status == AffineLiveStatus::Applied &&
         terminal.live.result == ApplyResult::Unsat);
  assert(terminal.committed_action_id == 10 &&
         kernel.structural_action_sequence == 10);
  assert(!terminal.candidate_staged &&
         !kernel.semantic_variable_candidate_pending && kernel.inconsistent);
  std::fclose(records);
}

static void certified_basis_v2_uses_persistent_session() {
  Kernel kernel;
  install(kernel, affine_formula());
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 80;
  kernel.affine_consequence_parameters = basis_parameters();
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;

  // The online executor and registry both use the one-argument public entry;
  // V2 must delegate through the attached solver-owned basis session.
  const auto applied = affine_consequence_action_apply(kernel);
  assert(applied.semantics ==
         strategy::AffineConsequenceSemantics::CertifiedBasisV2);
  assert(applied.status == AffineConsequenceActionStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.proposed_action_id == 81 &&
         applied.committed_action_id == 81 && applied.semantic_id == 81);
  assert(applied.basis.live.definitions == 4 &&
         applied.basis.live.extent_growth == 4);
  assert(applied.basis.live.existing_theory_variables ==
         std::vector<int32_t>({1, 2, 3, 4, 5, 6}));
  assert(applied.basis.live.introduced_theory_variables ==
         std::vector<int32_t>({7, 8, 9, 10}));
  assert(applied.candidate_staged && session.committed_keys() &&
         kernel.semantic_variable_candidate_pending);
  assert(kernel.stats.affine_consequence_attempts == 1 &&
         kernel.stats.affine_consequence_applied == 1 &&
         kernel.stats.affine_consequence_basis_work > 0 &&
         kernel.stats.affine_consequence_rows > 0 &&
         kernel.stats.affine_consequence_definitions == 4 &&
         kernel.stats.affine_consequence_extent_growth == 4 &&
         kernel.stats.affine_consequence_session_work > 0 &&
         kernel.stats.affine_consequence_transaction_keys ==
             applied.basis.transaction_keys &&
         kernel.stats.affine_consequence_committed_keys ==
             applied.basis.committed_keys &&
         kernel.stats.affine_consequence_candidate_variables == 10);
  std::fclose(records);
}

static void certified_basis_v2_duplicate_keeps_identity() {
  Kernel kernel;
  auto first = clausal_form({1, 2, 3}, false);
  auto second = clausal_form({1, 2, 4}, true);
  first.insert(first.end(), second.begin(), second.end());
  install(kernel, first, 4);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 90;
  kernel.affine_consequence_parameters = basis_parameters();
  AffineBasisActionSession session;
  kernel.affine_basis_action_session = &session;

  const auto applied = affine_consequence_action_apply(kernel, session);
  assert(applied.status == AffineConsequenceActionStatus::Applied &&
         applied.result == ApplyResult::Ok &&
         applied.committed_action_id == 91 && session.committed_keys() == 1);
  strategy::begin_semantic_variable_candidate_action(kernel);
  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, 3),
                              kernel_internalize(kernel, 4)));
  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -3),
                              kernel_internalize(kernel, -4)));
  const uint64_t clause_id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const long journal = position(records);
  const auto duplicate = affine_consequence_action_apply(kernel, session);
  assert(duplicate.status ==
         AffineConsequenceActionStatus::AlreadyPublished);
  assert(duplicate.result == ApplyResult::NoOp &&
         duplicate.proposed_action_id == 92 &&
         !duplicate.committed_action_id);
  assert(kernel.structural_action_sequence == 91 &&
         kernel.next_clause_id == clause_id && kernel.arena.size() == arena &&
         position(records) == journal && session.committed_keys() == 1);
  assert(kernel.stats.affine_consequence_attempts == 2 &&
         kernel.stats.affine_consequence_applied == 1 &&
         kernel.stats.affine_consequence_already_published == 1);
  std::fclose(records);
}

int main() {
  state_only_guard();
  declines_preserve_identity_and_state();
  exhaustion_retries_same_identity();
  propagation_is_deferred();
  terminal_result_commits_without_candidate();
  certified_basis_v2_uses_persistent_session();
  certified_basis_v2_duplicate_keeps_identity();
  std::puts("affine consequence action: transactional identity, effort, advice, and deferred propagation PASS");
}

#pragma once

// Typed rule profiles as tactix_online_parameters, the inverse of the C API's
// online-step conversion (api.cpp online_parameters). Include after tactix.h
// (renamed as in api.cpp) and the strategy parameter headers. A family, or a
// value, without a C form returns false; callers verify the round trip by
// installing the encoding and comparing the installed profile.

#include "strategy/inprocess_program.hpp"

#include <variant>
#include <vector>

namespace public_encoding {

using namespace tactix;

struct Encoded {
  tactix_online_parameters parameters{};
  std::vector<int> projection;
  std::vector<tactix_online_affine_scope> scopes;
  std::vector<tactix_online_walk_score_point> curve;
  Encoded() = default;
  Encoded(const Encoded &) = delete;
  Encoded &operator=(const Encoded &) = delete;
};

inline int setting(bool value) {
  return value ? TACTIX_ONLINE_SETTING_ENABLE : TACTIX_ONLINE_SETTING_DISABLE;
}

template <class P> bool encode_one(const P &, Encoded &) { return false; }

inline bool encode_one(const strategy::AnalyzeParameters &p, Encoded &e) {
  auto &c = e.parameters.value.analyze;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_ANALYZE;
  c.minimize_depth_limit = p.minimize_depth_limit;
  c.chronological_levels = p.chronological_levels;
  c.bump_reason_limit = p.bump_reason_limit;
  c.bump_reason_depth = p.bump_reason_depth;
  c.eager_subsume = p.eager_subsume;
  c.conflict_reuse = p.conflict_reuse;
  c.disable_jump_reasons = !p.jump_reasons;
  c.enable_jump_reasons_bigbig = p.jump_reasons_bigbig;
  c.disable_shrink_glue_promotion = !p.shrink_glue_promote;
  c.exclude_extension_reason_bumps = !p.bump_extension_reasons;
  return true;
}

inline bool encode_one(const strategy::BveParameters &p, Encoded &e) {
  auto &c = e.parameters.value.bve;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  c.gate_mask = p.gate_mask;
  c.growth_bound = p.growth_bound;
  c.occurrence_limit = p.occurrence_limit;
  c.clause_size_limit = p.clause_size_limit;
  c.xor_arity_limit = p.xor_arity_limit;
  c.resolvent_size_limit = p.resolvent_size_limit;
  c.backward_occurrence_limit = p.backward_occurrence_limit;
  c.definition_cores = p.definition_cores;
  c.definition_core_budget_multiplier = p.definition_core_budget_multiplier;
  c.definition_budget = p.definition_budget;
  c.backward = p.backward;
  c.candidate_scope = static_cast<int>(p.candidate_scope);
  // Zero means unlimited in C; an explicit zero cap has no C form.
  if (!p.resolvent_count_limit) return false;
  c.resolvent_count_limit = p.resolvent_count_limit;
  c.traversal = static_cast<int>(p.traversal);
  return true;
}

inline bool encode_one(const strategy::CongruenceParameters &p, Encoded &e) {
  auto &c = e.parameters.value.congruence;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_CONGRUENCE;
  c.and_arity_limit = p.and_arity_limit;
  c.xor_arity_limit = p.xor_arity_limit;
  c.gate_count_limit = p.gate_count_limit;
  c.binary_watch_order = static_cast<int>(p.binary_watch_order);
  return true;
}

inline bool encode_one(const strategy::InstantiateParameters &p, Encoded &e) {
  auto &c = e.parameters.value.instantiate;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_INSTANTIATE;
  c.minimum_clause_size = p.minimum_clause_size;
  c.occurrence_limit = p.occurrence_limit;
  c.minimum_unassigned = p.minimum_unassigned;
  c.new_material_threshold = p.new_material_threshold;
  c.include_redundant = p.include_redundant;
  return true;
}

inline bool encode_one(const strategy::BackboneParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BACKBONE;
  e.parameters.value.backbone = {p.initial_rounds, p.maximum_rounds};
  return true;
}

inline bool encode_one(const strategy::VivifyParameters &p, Encoded &e) {
  auto &c = e.parameters.value.vivify;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  c.scope = p.scope;
  c.irredundant_sort_ratio = p.irredundant_sort_ratio;
  c.reuse = p.reuse;
  c.flush = p.flush;
  return true;
}

inline bool encode_one(const strategy::BceParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BCE;
  e.parameters.value.bce.occurrence_limit = p.occurrence_limit;
  return true;
}

inline bool encode_one(const strategy::TernaryParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_TERNARY;
  e.parameters.value.ternary = {p.occurrence_limit, p.maximum_add_percent};
  return true;
}

inline bool encode_one(const strategy::FactorParameters &p, Encoded &e) {
  auto &c = e.parameters.value.factor;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_FACTOR;
  c.maximum_clause_size = p.maximum_clause_size;
  c.growth_bound = p.growth_bound;
  c.candidate_rounds = p.candidate_rounds;
  c.composite = p.composite;
  c.demote_fresh = p.demote_fresh;
  return true;
}

inline bool encode_one(const strategy::TransitiveReduceParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_TRANSITIVE;
  e.parameters.value.transitive = {p.early_exit, p.live_meter,
                                   p.checked_budget_meter};
  return true;
}

inline bool encode_one(const strategy::SubstituteParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTE;
  e.parameters.value.substitute.demote_substituted = p.demote_substituted;
  return true;
}

inline bool encode_one(const strategy::ReduceParameters &p, Encoded &e) {
  auto &c = e.parameters.value.reduce;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_REDUCE;
  if (!p.victim_denominator) return false; // zero selects the default in C
  c.tier1 = p.tier1;
  c.tier2 = p.tier2;
  c.victim_numerator = p.victim_numerator;
  c.victim_denominator = p.victim_denominator;
  c.tier_protection_mode = setting(p.protect_tiers);
  c.compact_collection_mode = setting(p.compact_collection);
  c.reduction_bookkeeping_mode = setting(p.reduction_bookkeeping);
  return true;
}

inline bool encode_one(const strategy::WalkParameters &p, Encoded &e) {
  using W = strategy::WalkParameters;
  if (p.objective != W::Objective::LegacyWalk ||
      p.admission != W::Admission::Legacy || p.preparation_budget ||
      !p.even_break_base_milli || p.score_curve.empty())
    return false; // no C form; zero selects a default in C
  auto &c = e.parameters.value.walk;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
  c.step_budget = p.step_budget;
  c.random_seed = p.random_seed;
  c.seed_candidate = p.seed_candidate;
  c.even_break_base_milli = p.even_break_base_milli;
  for (const auto &point : p.score_curve)
    e.curve.push_back({point.average_clause_size_milli, point.break_base_milli});
  c.score_curve = e.curve.data();
  c.score_curve_count = static_cast<int>(e.curve.size());
  return true;
}

inline bool encode_one(const strategy::ReorderParameters &p, Encoded &e) {
  if (!p.clause_decay_milli || !p.binary_weight_milli ||
      !p.balanced_polarity_milli)
    return false; // zero selects a default in C
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_REORDER;
  e.parameters.value.reorder = {p.maximum_clause_size, p.clause_decay_milli,
                                p.binary_weight_milli,
                                p.balanced_polarity_milli};
  return true;
}

inline bool encode_one(const strategy::ParityModelParameters &p, Encoded &e) {
  auto &c = e.parameters.value.parity_model;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_PARITY_MODEL;
  c.maximum_variables = p.maximum_variables;
  c.maximum_groups = p.maximum_groups;
  c.maximum_constraints = p.maximum_constraints;
  c.minimum_group_clauses = p.minimum_group_clauses;
  c.minimum_equations = p.minimum_equations;
  c.maximum_free_variables = p.maximum_free_variables;
  c.candidate_budget = p.candidate_budget;
  c.refutation_maximum_variables = p.refutation_maximum_variables;
  c.refutation_maximum_groups = p.refutation_maximum_groups;
  c.refutation_maximum_constraints = p.refutation_maximum_constraints;
  c.refutation_maximum_arity = p.refutation_maximum_arity;
  c.refutation_probe_constraints = p.refutation_probe_constraints;
  c.refutation_minimum_probe_repeats = p.refutation_minimum_probe_repeats;
  c.refutation_maximum_proof_additions = p.refutation_maximum_proof_additions;
  return true;
}

inline bool encode_one(const strategy::OnehotModelParameters &p, Encoded &e) {
  auto &c = e.parameters.value.onehot_model;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL;
  c.maximum_variables = p.maximum_variables;
  c.maximum_domains = p.maximum_domains;
  c.maximum_constraints = p.maximum_constraints;
  c.maximum_equation_arity = p.maximum_equation_arity;
  c.minimum_domains = p.minimum_domains;
  c.minimum_equations = p.minimum_equations;
  c.maximum_free_domains = p.maximum_free_domains;
  c.candidate_budget = p.candidate_budget;
  c.refutation_maximum_variables = p.refutation_maximum_variables;
  c.refutation_maximum_groups = p.refutation_maximum_groups;
  c.refutation_maximum_constraints = p.refutation_maximum_constraints;
  c.refutation_maximum_arity = p.refutation_maximum_arity;
  c.refutation_maximum_proof_additions = p.refutation_maximum_proof_additions;
  return true;
}

inline bool encode_one(const strategy::BalancedSignatureModelParameters &p,
                       Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL;
  e.parameters.value.balanced_signature_model = {
      p.maximum_balls, p.maximum_width, p.restarts, p.steps_per_restart,
      p.samples_per_step, p.seed, p.completion_rounds};
  return true;
}

inline bool encode_one(const strategy::FermatModelParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL;
  e.parameters.value.fermat_model = {p.maximum_bits, p.comparator_tail_clauses,
                                     p.rho_constants, p.rho_iterations};
  return true;
}

inline bool encode_one(const strategy::LookupCircuitModelParameters &p,
                       Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL;
  e.parameters.value.lookup_circuit_model = {
      p.maximum_fanin, p.maximum_fanout, p.maximum_primary_inputs,
      p.maximum_support, p.maximum_vector_gate_evaluations};
  return true;
}

inline bool encode_one(const strategy::SimonModelParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_SIMON_MODEL;
  e.parameters.value.simon_model = {p.maximum_rounds, p.maximum_candidates};
  return true;
}

inline bool encode_one(const strategy::StconnProofParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_STCONN_PROOF;
  e.parameters.value.stconn_proof = {p.maximum_width, p.maximum_columns,
                                     p.maximum_rup_clauses};
  return true;
}

inline bool encode_one(const strategy::SignedCardinalityProofParameters &p,
                       Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF;
  e.parameters.value.signed_cardinality_proof = {
      p.maximum_variables, p.maximum_groups, p.maximum_constraints,
      p.maximum_group_arity, p.maximum_bdd_nodes, p.maximum_proof_additions};
  return true;
}

inline bool encode_one(const strategy::ExactCoverModelParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL;
  e.parameters.value.exact_cover_model = {p.block_arity, p.maximum_nodes,
                                          p.restarts, p.restart_nodes, p.seed};
  return true;
}

inline bool encode_one(const strategy::IndependentSetModelParameters &p,
                       Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL;
  e.parameters.value.independent_set_model = {
      p.maximum_vertices, p.maximum_steps, p.completion_rounds, p.seed};
  return true;
}

inline bool encode_one(const strategy::RamseyStartupParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP;
  e.parameters.value.ramsey_startup = {
      p.recognition_clauses, p.recognition_variables, p.recognition_pair_cells,
      p.recognition_work, p.certificate_work, p.maximum_nodes,
      p.maximum_additions, p.maximum_external_extent};
  return true;
}

inline bool encode_one(const strategy::GraphLabelingParameters &p, Encoded &e) {
  auto &c = e.parameters.value.graph_labeling;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING;
  c.maximum_vertices = p.maximum_vertices;
  c.workspace_bytes = p.workspace_bytes;
  for (size_t stage = 0; stage < TACTIX_GRAPH_STAGES; ++stage)
    c.work[stage] = p.work[stage];
  c.maximum_dfs_vertices = p.maximum_dfs_vertices;
  c.maximum_dfs_nodes = p.maximum_dfs_nodes;
  c.maximum_dfs_starts = p.maximum_dfs_starts;
  c.early_dfs_choices = p.early_dfs_choices;
  c.repair_rounds = p.repair_rounds;
  c.anneal_iterations = p.anneal_iterations;
  c.stall_anneal_iterations = p.stall_anneal_iterations;
  c.seed = p.seed;
  return true;
}

inline bool encode_one(const strategy::AffineModelParameters &p, Encoded &e) {
  auto &c = e.parameters.value.affine_model;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL;
  c.maximum_variables = p.maximum_variables;
  c.maximum_constraints = p.maximum_constraints;
  c.maximum_arity = p.maximum_arity;
  c.maximum_projection = p.maximum_projection;
  c.minimum_equations = p.minimum_equations;
  c.maximum_entries = p.maximum_entries;
  c.import_bytes = p.import_bytes;
  c.linear_bytes = p.linear_bytes;
  c.table_bytes = p.table_bytes;
  c.maximum_work = p.maximum_work;
  c.candidate_budget = p.candidate_budget;
  c.permutations = p.permutations;
  c.seed = p.seed;
  c.projection_mode = static_cast<int>(p.projection_mode);
  c.automatic_negated = p.automatic_negated;
  c.shuffle_first = p.shuffle_first;
  c.free_phase = p.free_phase;
  c.residual_phase = p.residual_phase;
  e.projection = p.projection;
  c.projection = e.projection.data();
  c.projection_count = static_cast<int>(e.projection.size());
  for (const auto &scope : p.scopes)
    e.scopes.push_back({scope.left_size, scope.left_weight, scope.right_weight,
                        scope.total_weight});
  c.scopes = e.scopes.data();
  c.scope_count = static_cast<int>(e.scopes.size());
  return true;
}

inline bool encode(const strategy::InprocessParameters &parameters, Encoded &e) {
  e.parameters = {};
  return std::visit([&](const auto &p) { return encode_one(p, e); }, parameters);
}

} // namespace public_encoding

#pragma once

// Typed rule profiles as tactix_online_parameters, the inverse of the C API's
// online-step conversion (api.cpp online_parameters). Include after tactix.h
// (renamed as in api.cpp) and the strategy parameter headers. A family, or a
// value, without a C form returns false; callers verify the round trip by
// installing the encoding and comparing the installed profile.

#include "api/parameter_fields.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/scoped_rule_parameters.hpp"

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

#define TACTIX_ENCODE_COPY(c, t) target.c = source.t;
#define TACTIX_ENCODE_NONZERO(c, t) target.c = source.t;
#define TACTIX_ENCODE_SETTING(c, t) target.c = setting(source.t);
#define TACTIX_ENCODE_CHOICE(c, t) target.c = static_cast<int>(source.t) + 1;
#define TACTIX_ENCODE_CAST(c, t) target.c = static_cast<decltype(target.c)>(source.t);
#define TACTIX_ENCODE_INVERT(c, t) target.c = !source.t;
#define TACTIX_ENCODE_FIELD(kind, c, t) TACTIX_ENCODE_##kind(c, t)
// Writes the table's C fields of `target` from typed `source`.
#define TACTIX_ENCODE_TABLE(TABLE)                                             \
  [](auto &target, const auto &source) { TABLE(TACTIX_ENCODE_FIELD) }

// A family whose C mirror shares its strategy field list.
#define TACTIX_ENCODE_LISTED(Family, KIND, member)                             \
  inline bool encode_one(const strategy::Family &p, Encoded &e) {              \
    e.parameters.kind = TACTIX_ONLINE_PARAMETERS_##KIND;                       \
    strategy::copy_parameter_fields<strategy::Family>(e.parameters.value.member, p); \
    return true;                                                               \
  }
TACTIX_ENCODE_LISTED(BalancedSignatureModelParameters, BALANCED_SIGNATURE_MODEL,
                     balanced_signature_model)
TACTIX_ENCODE_LISTED(FermatModelParameters, FERMAT_MODEL, fermat_model)
TACTIX_ENCODE_LISTED(LookupCircuitModelParameters, LOOKUP_CIRCUIT_MODEL,
                     lookup_circuit_model)
TACTIX_ENCODE_LISTED(SimonModelParameters, SIMON_MODEL, simon_model)
TACTIX_ENCODE_LISTED(StconnProofParameters, STCONN_PROOF, stconn_proof)
TACTIX_ENCODE_LISTED(SignedCardinalityProofParameters, SIGNED_CARDINALITY_PROOF,
                     signed_cardinality_proof)
TACTIX_ENCODE_LISTED(ExactCoverModelParameters, EXACT_COVER_MODEL, exact_cover_model)
TACTIX_ENCODE_LISTED(IndependentSetModelParameters, INDEPENDENT_SET_MODEL,
                     independent_set_model)
#undef TACTIX_ENCODE_LISTED

// A family whose C fields are all in its field table.
#define TACTIX_ENCODE_TABLED(Family, KIND, member, TABLE)                      \
  inline bool encode_one(const strategy::Family &p, Encoded &e) {              \
    e.parameters.kind = TACTIX_ONLINE_PARAMETERS_##KIND;                       \
    TACTIX_ENCODE_TABLE(TABLE)(e.parameters.value.member, p);                  \
    return true;                                                               \
  }
TACTIX_ENCODE_TABLED(AnalyzeParameters, ANALYZE, analyze, TACTIX_ANALYZE_FIELDS)
TACTIX_ENCODE_TABLED(InstantiateParameters, INSTANTIATE, instantiate,
                     TACTIX_INSTANTIATE_FIELDS)
TACTIX_ENCODE_TABLED(SweepParameters, SWEEP, sweep, TACTIX_SWEEP_FIELDS)
TACTIX_ENCODE_TABLED(FactorParameters, FACTOR, factor, TACTIX_FACTOR_FIELDS)
TACTIX_ENCODE_TABLED(ParityModelParameters, PARITY_MODEL, parity_model,
                     TACTIX_PARITY_MODEL_FIELDS)
TACTIX_ENCODE_TABLED(OnehotModelParameters, ONEHOT_MODEL, onehot_model,
                     TACTIX_ONEHOT_MODEL_FIELDS)
#undef TACTIX_ENCODE_TABLED

inline bool encode_one(const strategy::BveParameters &p, Encoded &e) {
  // Zero means unlimited in C; an explicit zero cap has no C form.
  if (!p.resolvent_count_limit) return false;
  auto &c = e.parameters.value.bve;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BVE;
  TACTIX_ENCODE_TABLE(TACTIX_BVE_FIELDS)(c, p);
  c.resolvent_count_limit = p.resolvent_count_limit;
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
  c.band = p.band;
  c.band_budget = p.band_budget;
  c.band_offset = p.band_offset;
  return true;
}

inline bool encode_one(const strategy::SubsumeForwardParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSUME;
  e.parameters.value.subsume = {p.clause_size_limit, p.large_occurrence_limit,
                                p.binary_occurrence_limit};
  return true;
}

inline bool encode_one(const strategy::StrengthenParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_STRENGTHEN;
  e.parameters.value.strengthen = {p.clause_size_limit, p.occurrence_limit};
  return true;
}

inline bool encode_one(const strategy::BceParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_BCE;
  e.parameters.value.bce.occurrence_limit = p.occurrence_limit;
  e.parameters.value.bce.preserve_query = p.preserve_query;
  return true;
}

inline bool encode_one(const strategy::TernaryParameters &p, Encoded &e) {
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_TERNARY;
  e.parameters.value.ternary = {p.occurrence_limit, p.maximum_add_percent};
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
  e.parameters.value.substitute.max_rounds = p.max_rounds;
  e.parameters.value.substitute.round_effort_ceiling = p.round_effort_ceiling;
  return true;
}

inline bool encode_one(const strategy::ReduceParameters &p, Encoded &e) {
  if (!p.victim_denominator) return false; // zero selects the default in C
  auto &c = e.parameters.value.reduce;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_REDUCE;
  TACTIX_ENCODE_TABLE(TACTIX_REDUCE_FIELDS)(c, p);
  c.victim_numerator = p.victim_numerator;
  c.victim_denominator = p.victim_denominator;
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
  TACTIX_ENCODE_TABLE(TACTIX_WALK_FIELDS)(c, p);
  c.smoothing_old_numerator = p.smoothing_old_numerator;
  c.smoothing_denominator = p.smoothing_denominator;
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
  TACTIX_ENCODE_TABLE(TACTIX_REORDER_FIELDS)(e.parameters.value.reorder, p);
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
  TACTIX_ENCODE_TABLE(TACTIX_GRAPH_LABELING_FIELDS)(c, p);
  for (size_t stage = 0; stage < TACTIX_GRAPH_STAGES; ++stage)
    c.work[stage] = p.work[stage];
  return true;
}

inline bool encode_one(const strategy::AffineModelParameters &p, Encoded &e) {
  auto &c = e.parameters.value.affine_model;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL;
  TACTIX_ENCODE_TABLE(TACTIX_AFFINE_MODEL_FIELDS)(c, p);
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

// An SR profile in complete-profile mode: every orbitopal, general and graph
// field is read whatever the producer, so the C form is exact. Explicit
// ENABLE/DISABLE and offset enum values make it exact; zero-means-default
// fields with a typed zero have no C form.
inline bool encode_one(const SubstitutionRedundancyParameters &p, Encoded &e) {
  auto &c = e.parameters.value.substitution_redundancy;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  const GraphSymmetryParameters &g = p.graph_symmetry;
  if (!p.minimum_orbitopal_clique_columns || !p.orbitopal_base_probe_factor ||
      !g.schreier_domain_limit || !g.error_bound ||
      !g.binary_graph_density_permille)
    return false; // zero selects the typed default in C
  c.complete_profile_mode = TACTIX_ONLINE_SETTING_ENABLE;
  c.maximum_validation_ticks = p.maximum_validation_ticks;
  c.producer = static_cast<int>(p.producer);
  TACTIX_ENCODE_TABLE(TACTIX_ORBITOPAL_FIELDS)(c, p);
  c.orbitopal_refinement_mode = setting(p.maximum_orbitopal_refinement_orbits != 0);
  c.maximum_orbitopal_refinement_orbits = p.maximum_orbitopal_refinement_orbits;
  c.maximum_detection_ticks = p.maximum_detection_ticks;
  c.maximum_general_candidates_per_advance = p.maximum_general_candidates_per_advance;
  TACTIX_ENCODE_TABLE(TACTIX_GRAPH_SYMMETRY_FIELDS)(c, g);
  c.symmetry_clause_scope = g.clause_scope == SymmetryClauseScope::All
                                ? TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL
                                : TACTIX_ONLINE_SYMMETRY_CLAUSES_UNIQUE_LITERALS;
  return true;
}

inline bool encode_one(const strategy::AffineConsequenceParameters &p, Encoded &e) {
  auto &c = e.parameters.value.affine_consequence;
  e.parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  strategy::copy_parameter_fields<strategy::AffineImageFields>(c, p);
  c.consequence = p.consequence;
  c.semantics = p.semantics == strategy::AffineConsequenceSemantics::CertifiedBasisV2
                    ? TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2
                    : TACTIX_ONLINE_AFFINE_CONSEQUENCE_SELECTED_V1;
  strategy::copy_parameter_fields<strategy::AffineBasisFields>(c, p);
  return true;
}

inline bool encode(const strategy::InprocessParameters &parameters, Encoded &e) {
  e.parameters = {};
  return std::visit([&](const auto &p) { return encode_one(p, e); }, parameters);
}

#undef TACTIX_ENCODE_TABLE
#undef TACTIX_ENCODE_FIELD
#undef TACTIX_ENCODE_INVERT
#undef TACTIX_ENCODE_CAST
#undef TACTIX_ENCODE_CHOICE
#undef TACTIX_ENCODE_SETTING
#undef TACTIX_ENCODE_NONZERO
#undef TACTIX_ENCODE_COPY

} // namespace public_encoding

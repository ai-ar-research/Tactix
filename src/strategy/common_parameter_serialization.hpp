#pragma once

#include "strategy/inprocess_program.hpp"

#include <ostream>

namespace tactix::strategy {

enum class CommonParameterFormat : uint8_t {
  DefaultConfiguration,
  OnlineProgram,
};

// Serialize the parameter alternatives shared by the stable default-config
// text and the online strategy grammar. False means that this alternative is
// owned by a format-specific serializer.
inline bool serialize_common_parameters(std::ostream &out,
                                        const ActionParameters &parameters,
                                        CommonParameterFormat format) {
  const bool online = format == CommonParameterFormat::OnlineProgram;
  if (const auto *p = std::get_if<BveParameters>(&parameters)) {
    out << (online ? ":bve5(" : ":bve(") << p->gate_mask << ','
        << p->growth_bound << ',' << p->occurrence_limit << ','
        << p->clause_size_limit << ',' << p->xor_arity_limit << ','
        << p->resolvent_size_limit << ',' << p->backward_occurrence_limit
        << ',' << p->definition_budget << ',' << p->definition_cores << ','
        << p->definition_core_budget_multiplier << ',' << p->backward << ','
        << static_cast<unsigned>(p->candidate_scope) << ','
        << p->resolvent_count_limit << ','
        << static_cast<unsigned>(p->traversal) << ','
        << p->on_the_fly_strengthening << ')';
  } else if (const auto *p =
                 std::get_if<SubsumeForwardParameters>(&parameters)) {
    out << ":c1(" << p->clause_size_limit << ',' << p->large_occurrence_limit
        << ',' << p->binary_occurrence_limit << ')';
  } else if (const auto *p = std::get_if<StrengthenParameters>(&parameters)) {
    out << ":c2(" << p->clause_size_limit << ',' << p->occurrence_limit << ')';
  } else if (const auto *p =
                 std::get_if<ParityConsequenceParameters>(&parameters)) {
    out << ":parity-consequence(" << p->maximum_clauses << ','
        << p->maximum_groups << ',' << p->selection_work << ','
        << p->intermediate_width << ',' << p->source_count << ','
        << p->output_width << ',' << p->descending << ','
        << p->maximum_proof_additions << ',' << p->maximum_tree_variables
        << ',' << p->consequence << ')';
  } else if (const auto *p = std::get_if<CongruenceParameters>(&parameters)) {
    out << ":congruence(" << p->and_arity_limit << ',' << p->gate_count_limit
        << ',' << p->xor_arity_limit;
    if (!online)
      out << ',' << static_cast<unsigned>(p->binary_watch_order);
    out << ')';
  } else if (const auto *p = std::get_if<InstantiateParameters>(&parameters)) {
    out << ":instantiate(" << p->minimum_clause_size << ','
        << p->occurrence_limit << ',' << p->minimum_unassigned << ','
        << p->include_redundant << ',' << p->new_material_threshold << ')';
  } else if (const auto *p = std::get_if<BackboneParameters>(&parameters)) {
    out << ":backbone(" << p->initial_rounds << ',' << p->maximum_rounds
        << ')';
  } else if (const auto *p = std::get_if<SweepParameters>(&parameters)) {
    out << ":sweep(" << p->max_depth << ',' << p->max_depth_ceiling << ','
        << p->max_env_vars << ',' << p->max_env_vars_ceiling << ','
        << p->max_env_clauses << ',' << p->max_env_clauses_ceiling << ','
        << p->max_candidates << ',' << p->env_ticks << ','
        << p->max_proof_steps << ',' << p->max_seeds << ',' << p->round_ticks
        << ',' << p->strip_false << ',' << p->measure_closure << ','
        << p->full_partition << ',' << p->skip_embedded_fixed << ','
        << p->flip_rounds << ',' << p->resume << ','
        << p->force_general_proof << ')';
  } else if (const auto *p = std::get_if<VivifyParameters>(&parameters)) {
    out << ":vivify(" << unsigned(p->scope) << ',' << p->reuse << ','
        << p->flush << ',' << p->irredundant_sort_ratio;
    // Only the append-only online grammar exposes the optional band ledger.
    if (online && (p->band || p->band_budget || p->band_offset))
      out << ',' << p->band << ',' << p->band_budget << ',' << p->band_offset;
    out << ')';
  } else {
    return false;
  }
  return true;
}

} // namespace tactix::strategy

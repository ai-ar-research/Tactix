#include "rules/semantic_extension/gf3_general_row_action.hpp"

#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/action_epilogue.hpp"
#include "rules/semantic_extension/gf3_live_common.hpp"

#include <algorithm>
#include <utility>

namespace tactix {

struct Gf3GeneralRowActionSessionAccess {
  static void commit(Gf3GeneralRowActionSession &session,
                     const Kernel &kernel, Gf3GeneralRowOwner owner,
                     std::vector<Gf3GeneralRowActionSelection> keys) {
    session.owner_ = &kernel;
    session.snapshot_ = std::move(owner);
    session.keys_ = std::move(keys);
  }
};

namespace {

Gf3GeneralRowActionStatus status(Gf3GeneralRowLiveStatus value) {
  using S = Gf3GeneralRowLiveStatus;
  if (const auto failure =
          strategy::map_standard_action_failure<Gf3GeneralRowActionStatus>(
              value))
    return *failure;
  switch (value) {
  case S::NoStructure: return Gf3GeneralRowActionStatus::NoStructure;
  case S::NoConsequence: return Gf3GeneralRowActionStatus::NoConsequence;
  case S::Duplicate: return Gf3GeneralRowActionStatus::AlreadyPublished;
  case S::Ready:
  case S::Applied: return Gf3GeneralRowActionStatus::Applied;
  default: std::abort();
  }
}

bool retainable(const Gf3GeneralRowActionSelection &selection,
                const Gf3GeneralRowActionLimits &limits,
                uint64_t &work) {
  return selection.identity.size() <= limits.maximum_session_identity_bytes &&
         add_bounded(selection.identity.size() + 1,
                     limits.maximum_session_work, work);
}

Gf3GeneralRowLiveRequest request(
    const Kernel &kernel, const Gf3GeneralRowActionLimits &limits,
    const Gf3GeneralRowActionSelection &selection, uint64_t action_id,
    const Gf3GeneralRowOwner *owner) {
  Gf3GeneralRowLiveRequest result;
  result.limits = limits.live;
  result.selection = {selection.outcome, selection.tuple};
  result.selected_identity = selection.identity;
  result.action_id = action_id;
  result.semantic_id = action_id;
  result.records = kernel.structural_provenance;
  result.retained_owner = owner;
  return result;
}

} // namespace

bool Gf3GeneralRowActionLimits::valid() const {
  return live.valid() && maximum_session_keys && maximum_session_work &&
         maximum_session_identity_bytes;
}

bool gf3_general_row_action_selection_valid(
    const Gf3GeneralRowActionSelection &selection) {
  return gf3_live_common::valid_hex_identity(selection.identity, "g3r1:");
}

RuleEligibility gf3_general_row_action_guard(const Kernel &kernel) {
  const auto *session = kernel.gf3_general_row_action_session;
  const bool retained_search_owner = session && session->retained() &&
                                     session->belongs_to(kernel);
  return StructuralActionGuard(kernel)
      .fact("consistent", !kernel.inconsistent)
      .fact("root-or-retained-search", !kernel.level || retained_search_owner)
      .fact("assignment-free-or-retained-search",
            kernel.trail.empty() || retained_search_owner)
      .quiescent()
      .identity()
      .clausal_certificate()
      .finish();
}

Gf3GeneralRowActionResult gf3_general_row_action_apply(
    Kernel &kernel, Gf3GeneralRowActionSession &session,
    const Gf3GeneralRowActionLimits &limits,
    const Gf3GeneralRowActionSelection &selection) {
  Gf3GeneralRowActionResult output;
  if (!gf3_general_row_action_guard(kernel)) {
    output.status = Gf3GeneralRowActionStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() ||
      !gf3_general_row_action_selection_valid(selection))
    return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = Gf3GeneralRowActionStatus::Stale;
    return output;
  }
  if (session.keys().size() > limits.maximum_session_keys) {
    output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
    return output;
  }
  const auto position =
      std::lower_bound(session.keys().begin(), session.keys().end(), selection);
  if (position != session.keys().end() && *position == selection) {
    output.status = Gf3GeneralRowActionStatus::AlreadyPublished;
    return output;
  }
  if (session.keys().size() == limits.maximum_session_keys ||
      !retainable(selection, limits, output.session_work)) {
    output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
    return output;
  }
  uint64_t retained_bytes = selection.identity.size();
  for (const auto &key : session.keys()) {
    if (!add_bounded(key.identity.size() + 1, limits.maximum_session_work,
                     output.session_work) ||
        key.identity.size() >
            limits.maximum_session_identity_bytes - retained_bytes) {
      output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
      return output;
    }
    retained_bytes += key.identity.size();
  }
  output.session_identity_bytes = retained_bytes;
  std::vector<Gf3GeneralRowActionSelection> keys =
      sorted_with(session.keys(), position, selection);

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  output.live = gf3_general_row_live_apply(
      kernel, request(kernel, limits, selection, output.proposed_action_id,
                      session.owner()));
  output.status = status(output.live.status);
  output.result = output.live.result;
  if (output.status != Gf3GeneralRowActionStatus::Applied) return output;

  Gf3GeneralRowActionSessionAccess::commit(
      session, kernel, std::move(output.live.owner), std::move(keys));
  kernel.structural_action_sequence = output.proposed_action_id;
  output.committed_action_id = output.proposed_action_id;
  return output;
}

void record_gf3_general_row_action_feedback(
    Kernel &kernel, const Gf3GeneralRowActionResult &result) {
  auto &s = kernel.stats;
  const auto &e = result.live.effort;
  ++s.gf3_general_row_attempts;
  s.gf3_general_row_source_clauses += e.source_clauses;
  s.gf3_general_row_source_literals += e.source_literals;
  s.gf3_general_row_capture_work += e.capture_work;
  s.gf3_general_row_bridge_work += e.bridge_work;
  s.gf3_general_row_planning_work += e.planning_work;
  s.gf3_general_row_validation_work += e.validation_work;
  s.gf3_general_row_duplicate_scan_work += e.duplicate_scan_work;
  s.gf3_general_row_revalidation_work += e.revalidation_work;
  s.gf3_general_row_proof_additions += e.proof_additions;
  s.gf3_general_row_proof_literals += e.proof_literal_occurrences;
  s.gf3_general_row_arena_words += e.arena_words;
  s.gf3_general_row_journal_bytes += e.journal_bytes;
  s.gf3_general_row_identity_bytes += e.identity_bytes;
  s.gf3_general_row_session_work += result.session_work;
  s.gf3_general_row_session_identity_bytes += result.session_identity_bytes;
  const bool standard_status = strategy::record_standard_action_status(
      result.status,
      {s.gf3_general_row_applied, s.gf3_general_row_invalid,
       s.gf3_general_row_inapplicable, s.gf3_general_row_exhausted,
       s.gf3_general_row_interrupted, s.gf3_general_row_unsupported_proof,
       s.gf3_general_row_stale});
  if (result.status == Gf3GeneralRowActionStatus::Applied) {
    s.gf3_general_row_installed_clauses +=
        result.live.installed_clause_id != 0;
    s.gf3_general_row_semantic_variables +=
        result.live.semantic_variables.size();
  } else if (!standard_status &&
             result.status == Gf3GeneralRowActionStatus::NoStructure)
    ++s.gf3_general_row_no_structure;
  else if (!standard_status &&
           result.status == Gf3GeneralRowActionStatus::NoConsequence)
    ++s.gf3_general_row_no_consequence;
  else if (!standard_status &&
           result.status == Gf3GeneralRowActionStatus::AlreadyPublished)
    ++s.gf3_general_row_already_published;
}

} // namespace tactix

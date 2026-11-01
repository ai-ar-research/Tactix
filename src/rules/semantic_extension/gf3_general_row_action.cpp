#include "rules/semantic_extension/gf3_general_row_action.hpp"

#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include "rules/structural_action_guard.hpp"

#include <algorithm>
#include <limits>
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

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

uint64_t saturated(uint64_t first, uint64_t second) {
  return second > UINT64_MAX - first ? UINT64_MAX : first + second;
}

void accumulate(Gf3GeneralRowLiveEffort &target,
                const Gf3GeneralRowLiveEffort &source) {
#define ADD(field) target.field = saturated(target.field, source.field)
  ADD(source_clauses); ADD(source_literals); ADD(capture_work);
  ADD(bridge_work); ADD(planning_work); ADD(validation_work);
  ADD(duplicate_scan_work); ADD(revalidation_work); ADD(proof_additions);
  ADD(proof_literal_occurrences); ADD(arena_words); ADD(journal_bytes);
  ADD(identity_bytes);
#undef ADD
}

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
         add_work(selection.identity.size() + 1,
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
  if (selection.identity.size() <= 5 ||
      selection.identity.rfind("g3r1:", 0) ||
      (selection.identity.size() - 5) % 2)
    return false;
  return std::all_of(selection.identity.begin() + 5,
                     selection.identity.end(), [](char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
  });
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
    if (!add_work(key.identity.size() + 1, limits.maximum_session_work,
                  output.session_work) ||
        key.identity.size() >
            limits.maximum_session_identity_bytes - retained_bytes) {
      output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
      return output;
    }
    retained_bytes += key.identity.size();
  }
  output.session_identity_bytes = retained_bytes;
  std::vector<Gf3GeneralRowActionSelection> keys;
  keys.reserve(session.keys().size() + 1);
  keys.insert(keys.end(), session.keys().begin(), position);
  keys.push_back(selection);
  keys.insert(keys.end(), position, session.keys().end());

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

bool Gf3GeneralRowActionOfferWindow::valid() const {
  return maximum_candidates && maximum_offers && maximum_enumeration_work &&
         (!have_after || gf3_general_row_action_selection_valid(after));
}

Gf3GeneralRowActionOfferResult gf3_general_row_action_offer(
    Kernel &kernel, const Gf3GeneralRowActionSession &session,
    const Gf3GeneralRowActionLimits &limits,
    const Gf3GeneralRowActionOfferWindow &window) {
  Gf3GeneralRowActionOfferResult output;
  if (!gf3_general_row_action_guard(kernel)) {
    output.status = Gf3GeneralRowActionStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || !window.valid()) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = Gf3GeneralRowActionStatus::Stale;
    return output;
  }

  Gf3GeneralRowOwner owner;
  const Gf3GeneralRowOwner *selected_owner = session.owner();
  if (!selected_owner) {
    const auto captured = gf3_general_row_live_capture(
        kernel, limits.live, owner, output.live_effort);
    if (captured != Gf3GeneralRowLiveStatus::Ready) {
      output.status = status(captured);
      output.complete = captured == Gf3GeneralRowLiveStatus::NoStructure;
      return output;
    }
    selected_owner = &owner;
  } else {
    const auto validated = gf3_general_row_live_validate_owner(
        kernel, limits.live, *selected_owner, output.live_effort);
    if (validated != Gf3GeneralRowLiveStatus::Ready) {
      output.status = status(validated);
      return output;
    }
  }

  for (uint32_t outcome = 0;
       outcome < selected_owner->bridge.gaussian.outcomes.size(); ++outcome) {
    for (uint64_t tuple = 0;; ++tuple) {
      if (output.candidates == window.maximum_candidates ||
          !add_work(1, window.maximum_enumeration_work,
                    output.enumeration_work)) {
        output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
        output.offers.clear();
        return output;
      }
      gf3_general_row::Request candidate{outcome, tuple};
      const auto inspected = gf3_general_row::inspect(
          selected_owner->affine.theory, selected_owner->bridge, candidate,
          limits.live.planner);
      if (inspected.status == gf3_general_row::Status::InvalidRequest) break;
      if (inspected.status != gf3_general_row::Status::Planned) continue;
      ++output.candidates;
      Gf3GeneralRowActionSelection selection{outcome, tuple, {}};
      if (!gf3_general_row_live_identity(*selected_owner, candidate,
                                         limits.live.maximum_identity_bytes,
                                         selection.identity)) {
        output.status = Gf3GeneralRowActionStatus::ResourceExhausted;
        output.offers.clear();
        return output;
      }
      if (window.have_after && !(window.after < selection)) continue;
      if (std::binary_search(session.keys().begin(), session.keys().end(),
                             selection)) {
        ++output.committed_keys_skipped;
        continue;
      }
      const auto admitted = gf3_general_row_live_inspect(
          kernel, request(kernel, limits, selection,
                          kernel.structural_action_sequence + 1,
                          selected_owner));
      accumulate(output.live_effort, admitted.effort);
      if (admitted.status == Gf3GeneralRowLiveStatus::Duplicate) {
        ++output.committed_keys_skipped;
        continue;
      }
      if (admitted.status != Gf3GeneralRowLiveStatus::Ready) {
        output.status = status(admitted.status);
        output.offers.clear();
        return output;
      }
      if (output.offers.size() == window.maximum_offers) {
        output.status = Gf3GeneralRowActionStatus::Applied;
        return output;
      }
      output.offers.push_back(selection);
      output.resume_after = selection;
      output.have_resume_after = true;
      if (tuple == std::numeric_limits<uint64_t>::max()) break;
    }
  }
  output.complete = true;
  output.status = output.offers.empty()
                      ? Gf3GeneralRowActionStatus::AlreadyPublished
                      : Gf3GeneralRowActionStatus::Applied;
  return output;
}

void record_gf3_general_row_action_feedback(
    Kernel &kernel, const Gf3GeneralRowActionResult &result) {
  auto &s = kernel.stats;
  ++s.gf3_general_row_attempts;
  s.gf3_general_row_source_clauses += result.live.effort.source_clauses;
  s.gf3_general_row_source_literals += result.live.effort.source_literals;
  s.gf3_general_row_capture_work += result.live.effort.capture_work;
  s.gf3_general_row_bridge_work += result.live.effort.bridge_work;
  s.gf3_general_row_planning_work += result.live.effort.planning_work;
  s.gf3_general_row_validation_work += result.live.effort.validation_work;
  s.gf3_general_row_duplicate_scan_work +=
      result.live.effort.duplicate_scan_work;
  s.gf3_general_row_revalidation_work +=
      result.live.effort.revalidation_work;
  s.gf3_general_row_proof_additions += result.live.effort.proof_additions;
  s.gf3_general_row_proof_literals +=
      result.live.effort.proof_literal_occurrences;
  s.gf3_general_row_arena_words += result.live.effort.arena_words;
  s.gf3_general_row_journal_bytes += result.live.effort.journal_bytes;
  s.gf3_general_row_identity_bytes += result.live.effort.identity_bytes;
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

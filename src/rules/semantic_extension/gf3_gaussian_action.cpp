#include "rules/semantic_extension/gf3_gaussian_action.hpp"

#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include "rules/structural_action_guard.hpp"

#include <algorithm>
#include <limits>

namespace tactix {

struct Gf3GaussianActionSessionAccess {
  static void commit(Gf3GaussianActionSession &session, const Kernel &kernel,
                     std::vector<std::string> keys) {
    session.owner_ = &kernel;
    session.keys_.swap(keys);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

gf3_gaussian_live::Request request(const Kernel &kernel,
                                   const Gf3GaussianActionLimits &limits,
                                   uint64_t action_id,
                                   const std::string &identity = {}) {
  gf3_gaussian_live::Request result;
  result.recognition = limits.recognition;
  result.certificate = limits.certificate;
  result.maximum_capture_work = limits.maximum_capture_work;
  result.maximum_revalidation_work = limits.maximum_revalidation_work;
  result.maximum_proof_additions = limits.maximum_proof_additions;
  result.maximum_proof_literal_occurrences =
      limits.maximum_proof_literal_occurrences;
  result.maximum_journal_bytes = limits.maximum_journal_bytes;
  result.maximum_identity_bytes = limits.maximum_identity_bytes;
  result.maximum_external_extent = limits.maximum_external_extent;
  result.action_id = action_id;
  result.semantic_id = action_id;
  result.records = kernel.structural_provenance;
  result.selected_identity = identity;
  return result;
}

Gf3GaussianActionStatus status(gf3_gaussian_live::Status value) {
  using S = gf3_gaussian_live::Status;
  if (const auto failure =
          strategy::map_standard_action_failure<Gf3GaussianActionStatus>(
              value))
    return *failure;
  switch (value) {
  case S::Applied: return Gf3GaussianActionStatus::Applied;
  case S::NoStructure: return Gf3GaussianActionStatus::NoStructure;
  case S::NoSupportedOutcome:
    return Gf3GaussianActionStatus::NoSupportedOutcome;
  case S::UnsupportedProofSplit:
    return Gf3GaussianActionStatus::UnsupportedProof;
  case S::Duplicate:
  case S::AlreadyApplied:
    return Gf3GaussianActionStatus::AlreadyPublished;
  case S::Prepared: return Gf3GaussianActionStatus::Applied;
  default: std::abort();
  }
}

bool valid_identity(const std::string &identity) {
  if (identity.size() <= 5 || identity.rfind("g3b1:", 0) ||
      (identity.size() - 5) % 2)
    return false;
  return std::all_of(identity.begin() + 5, identity.end(), [](char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
  });
}

} // namespace

bool Gf3GaussianActionLimits::valid() const {
  const auto &r = recognition;
  const auto &c = certificate;
  const auto &b = c.bridge;
  const auto &g = b.gaussian;
  return r.maximum_source_clauses && r.maximum_source_literals &&
         r.maximum_domains && r.maximum_equations &&
         r.maximum_equation_arity >= 2 && r.maximum_tuples_per_equation &&
         b.maximum_work && b.maximum_source_clauses &&
         b.maximum_source_literals && b.maximum_domains &&
         b.maximum_equations && b.maximum_terms &&
         b.maximum_authentication_references &&
         b.maximum_references_per_equation && g.maximum_work &&
         g.maximum_fill_entries && g.maximum_intermediate_width &&
         g.maximum_provenance_terms && g.maximum_derivations &&
         g.maximum_retained_plan_bytes && c.maximum_work &&
         c.maximum_events && c.maximum_event_literals &&
         c.maximum_active_clauses &&
         c.maximum_retained_source_references && c.maximum_event_width &&
         c.maximum_branch_domains && c.maximum_consequences &&
         maximum_capture_work && maximum_revalidation_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_journal_bytes && maximum_identity_bytes &&
         maximum_external_extent && maximum_session_keys &&
         maximum_session_work && maximum_session_identity_bytes;
}

RuleEligibility gf3_gaussian_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, true)
      .identity()
      .clausal_certificate()
      .finish();
}

Gf3GaussianActionOfferResult gf3_gaussian_action_offer(
    Kernel &kernel, const Gf3GaussianActionSession &session,
    const Gf3GaussianActionLimits &limits) {
  Gf3GaussianActionOfferResult output;
  if (!gf3_gaussian_action_guard(kernel)) {
    output.status = Gf3GaussianActionStatus::Inapplicable;
    return output;
  }
  if (!limits.valid()) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = Gf3GaussianActionStatus::Stale;
    return output;
  }
  if (session.keys().size() > limits.maximum_session_keys) {
    output.status = Gf3GaussianActionStatus::ResourceExhausted;
    return output;
  }
  gf3_gaussian_live::Prepared prepared;
  output.live = gf3_gaussian_live::inspect(
      kernel, request(kernel, limits, kernel.structural_action_sequence + 1),
      prepared);
  if (output.live.status != gf3_gaussian_live::Status::Prepared) {
    output.status = status(output.live.status);
    output.complete =
        output.status == Gf3GaussianActionStatus::NoStructure ||
        output.status == Gf3GaussianActionStatus::NoSupportedOutcome ||
        output.status == Gf3GaussianActionStatus::AlreadyPublished;
    return output;
  }
  const std::string &identity = prepared.batch_identity();
  output.candidates = 1;
  if (identity.size() > limits.maximum_session_identity_bytes ||
      !add_work(identity.size() + 1, limits.maximum_session_work,
                output.session_work)) {
    output.status = Gf3GaussianActionStatus::ResourceExhausted;
    return output;
  }
  for (const auto &key : session.keys())
    if (!add_work(key.size() + 1, limits.maximum_session_work,
                  output.session_work)) {
      output.status = Gf3GaussianActionStatus::ResourceExhausted;
      return output;
    }
  output.session_identity_bytes = identity.size();
  if (!std::binary_search(session.keys().begin(), session.keys().end(), identity))
    output.offers.push_back(identity);
  output.complete = true;
  output.status = output.offers.empty()
                      ? Gf3GaussianActionStatus::AlreadyPublished
                      : Gf3GaussianActionStatus::Applied;
  return output;
}

Gf3GaussianActionResult gf3_gaussian_action_apply(
    Kernel &kernel, Gf3GaussianActionSession &session,
    const Gf3GaussianActionLimits &limits, const std::string &identity) {
  Gf3GaussianActionResult output;
  output.identity = identity;
  if (!gf3_gaussian_action_guard(kernel)) {
    output.status = Gf3GaussianActionStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || !valid_identity(identity)) return output;
  if (identity.size() > limits.maximum_session_identity_bytes) {
    output.status = Gf3GaussianActionStatus::ResourceExhausted;
    return output;
  }
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = Gf3GaussianActionStatus::Stale;
    return output;
  }
  if (session.keys().size() > limits.maximum_session_keys) {
    output.status = Gf3GaussianActionStatus::ResourceExhausted;
    return output;
  }
  const auto position =
      std::lower_bound(session.keys().begin(), session.keys().end(), identity);
  if (position != session.keys().end() && *position == identity) {
    output.status = Gf3GaussianActionStatus::AlreadyPublished;
    return output;
  }
  if (session.keys().size() == limits.maximum_session_keys ||
      !add_work(identity.size() + 1, limits.maximum_session_work,
                output.session_work)) {
    output.status = Gf3GaussianActionStatus::ResourceExhausted;
    return output;
  }
  for (const auto &key : session.keys())
    if (!add_work(key.size() + 1, limits.maximum_session_work,
                  output.session_work)) {
      output.status = Gf3GaussianActionStatus::ResourceExhausted;
      return output;
    }
  uint64_t retained_bytes = identity.size();
  for (const auto &key : session.keys()) {
    if (key.size() > limits.maximum_session_identity_bytes - retained_bytes) {
      output.status = Gf3GaussianActionStatus::ResourceExhausted;
      return output;
    }
    retained_bytes += key.size();
  }
  output.session_identity_bytes = retained_bytes;
  std::vector<std::string> merged;
  merged.reserve(session.keys().size() + 1);
  merged.insert(merged.end(), session.keys().begin(), position);
  merged.push_back(identity);
  merged.insert(merged.end(), position, session.keys().end());

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  output.live = gf3_gaussian_live::apply(
      kernel, request(kernel, limits, output.proposed_action_id, identity));
  output.status = status(output.live.status);
  output.result = output.live.result;
  if (output.status != Gf3GaussianActionStatus::Applied) return output;

  Gf3GaussianActionSessionAccess::commit(session, kernel, std::move(merged));
  kernel.structural_action_sequence = output.proposed_action_id;
  output.committed_action_id = output.proposed_action_id;
  return output;
}

void record_gf3_gaussian_action_feedback(
    Kernel &kernel, const Gf3GaussianActionResult &result) {
  ++kernel.stats.gf3_gaussian_attempts;
  kernel.stats.gf3_gaussian_source_clauses += result.live.effort.source_clauses;
  kernel.stats.gf3_gaussian_source_literals += result.live.effort.source_literals;
  kernel.stats.gf3_gaussian_capture_work += result.live.effort.capture_work;
  kernel.stats.gf3_gaussian_bridge_work += result.live.effort.bridge_work;
  kernel.stats.gf3_gaussian_certificate_work +=
      result.live.effort.certificate_work;
  kernel.stats.gf3_gaussian_validation_work +=
      result.live.effort.validation_work;
  kernel.stats.gf3_gaussian_revalidation_work +=
      result.live.effort.revalidation_work;
  kernel.stats.gf3_gaussian_proof_additions +=
      result.live.effort.proof_additions;
  kernel.stats.gf3_gaussian_proof_literals +=
      result.live.effort.proof_literal_occurrences;
  kernel.stats.gf3_gaussian_installed_clauses +=
      result.live.effort.installed_clauses;
  kernel.stats.gf3_gaussian_duplicate_clauses +=
      result.live.effort.duplicate_clauses;
  kernel.stats.gf3_gaussian_journal_work += result.live.effort.journal_work;
  kernel.stats.gf3_gaussian_journal_bytes += result.live.effort.journal_bytes;
  kernel.stats.gf3_gaussian_session_work += result.session_work;
  kernel.stats.gf3_gaussian_identity_work += result.live.effort.identity_work;
  kernel.stats.gf3_gaussian_identity_bytes += result.live.effort.identity_bytes;
  kernel.stats.gf3_gaussian_session_identity_bytes +=
      result.session_identity_bytes;
  const bool standard_status = strategy::record_standard_action_status(
      result.status,
      {kernel.stats.gf3_gaussian_applied, kernel.stats.gf3_gaussian_invalid,
       kernel.stats.gf3_gaussian_inapplicable,
       kernel.stats.gf3_gaussian_exhausted,
       kernel.stats.gf3_gaussian_interrupted,
       kernel.stats.gf3_gaussian_unsupported_proof,
       kernel.stats.gf3_gaussian_stale});
  if (result.status == Gf3GaussianActionStatus::Applied) {
    kernel.stats.gf3_gaussian_units += result.live.published_units.size();
    kernel.stats.gf3_gaussian_conflicts += result.live.contradiction;
  } else if (!standard_status &&
             result.status == Gf3GaussianActionStatus::NoStructure)
    ++kernel.stats.gf3_gaussian_no_structure;
  else if (!standard_status &&
           result.status == Gf3GaussianActionStatus::NoSupportedOutcome)
    ++kernel.stats.gf3_gaussian_no_supported_outcome;
  else if (!standard_status &&
           result.status == Gf3GaussianActionStatus::AlreadyPublished)
    ++kernel.stats.gf3_gaussian_already_published;
}

} // namespace tactix

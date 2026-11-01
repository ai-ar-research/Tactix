#include "rules/semantic_extension/exact_cover_action.hpp"

#include "rules/structural_action_guard.hpp"
#include "strategy/action_status_statistics.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cstdlib>
#include <limits>
#include <vector>

namespace tactix {
namespace {

struct RecordExactCoverExtension {
  Kernel &kernel;
  ExactCoverExtensionActionResult &result;
  ~RecordExactCoverExtension() {
    auto &stats = kernel.stats;
    const auto &live = result.live;
    ++stats.exact_cover_extension_attempts;
    stats.exact_cover_extension_source_clauses += live.source_clauses;
    stats.exact_cover_extension_source_literals += live.source_literals;
    stats.exact_cover_extension_recognition_work += live.recognition_work;
    stats.exact_cover_extension_revalidation_work += live.revalidation_work;
    stats.exact_cover_extension_definitions += live.definitions;
    stats.exact_cover_extension_additions += live.additions;
    stats.exact_cover_extension_literal_occurrences +=
        live.literal_occurrences;
    stats.exact_cover_extension_journal_bytes += live.journal_bytes;
    stats.exact_cover_extension_extent_growth += live.extent_growth;
    if (result.semantic_candidate_staged)
      stats.exact_cover_extension_candidate_variables +=
          live.introduced_theory_variables.size();
    if (!strategy::record_standard_action_status(
            live.status, TACTIX_STANDARD_ACTION_STATUS_COUNTERS(
                             stats, exact_cover_extension))) {
      switch (live.status) {
      case ExactCoverLiveStatus::NoStructure:
        ++stats.exact_cover_extension_no_structure;
        break;
      case ExactCoverLiveStatus::AlreadyPublished:
        ++stats.exact_cover_extension_already_published;
        break;
      default: break;
      }
    }
  }
};

} // namespace

RuleEligibility exact_cover_extension_action_guard(const Kernel &k) {
  return root_action_guard(k, true)
      .fact("session-attached",
            k.exact_cover_extension_action_session != nullptr)
      .identity()
      .clausal_certificate()
      .semantic_candidate()
      .finish();
}

ExactCoverExtensionActionResult exact_cover_extension_action_apply(
    Kernel &k, ExactCoverExtensionActionSession &session) {
  ExactCoverExtensionActionResult result;
  RecordExactCoverExtension record{k, result};
  if (!exact_cover_extension_action_guard(k) ||
      k.exact_cover_extension_action_session != &session) {
    result.live.status = ExactCoverLiveStatus::Inapplicable;
    return result;
  }

  const auto &parameters = k.exact_cover_extension_parameters;
  if (!parameters.valid())
    return result;
  const uint64_t prospective_action_id =
      k.structural_action_sequence + 1;

  ExactCoverLiveRequest request;
  request.arity = parameters.arity;
  request.maximum_items = parameters.maximum_items;
  request.maximum_source_clauses = parameters.maximum_source_clauses;
  request.maximum_source_literals = parameters.maximum_source_literals;
  request.maximum_recognition_work = parameters.maximum_recognition_work;
  request.maximum_premise_references =
      parameters.maximum_premise_references;
  request.maximum_definitions = parameters.maximum_definitions;
  request.maximum_additions = parameters.maximum_additions;
  request.maximum_literal_occurrences =
      parameters.maximum_literal_occurrences;
  request.maximum_journal_bytes = parameters.maximum_journal_bytes;
  request.maximum_extent_growth = parameters.maximum_extent_growth;
  request.maximum_external_extent = parameters.maximum_external_extent;
  request.action_id = prospective_action_id;
  request.records = k.structural_provenance;
  request.session = &session.live_session;

  result.live = exact_cover_live_apply(k, request);
  if (result.live.status != ExactCoverLiveStatus::Applied ||
      result.live.result == ApplyResult::NoOp)
    return result;

  if (result.live.result == ApplyResult::Ok) {
    std::vector<strategy::SemanticVariable> candidates;
    candidates.reserve(result.live.introduced_theory_variables.size());
    for (const uint32_t external : result.live.introduced_theory_variables)
      candidates.push_back({external,
                            strategy::SemanticVariableRole::IntroducedTheory,
                            1});
    // Publication already committed. Guard and the live adapter guarantee the
    // staging preconditions; failure here is an internal atomicity violation.
    if (!strategy::stage_semantic_variable_candidate(
            k, strategy::SemanticVariableProducer::ExactCover,
            prospective_action_id, candidates))
      std::abort();
    result.semantic_candidate_staged = true;
  }

  k.structural_action_sequence = prospective_action_id;
  result.action_id = prospective_action_id;
  return result;
}

ApplyResult exact_cover_extension_rule_apply(Kernel &kernel) {
  if (!exact_cover_extension_action_guard(kernel)) return ApplyResult::NoOp;
  return exact_cover_extension_action_apply(
             kernel, *kernel.exact_cover_extension_action_session)
      .live.result;
}

TACTIX_REGISTER_RULE(ExactCoverExtension, RuleTier::Inprocess,
                     exact_cover_extension_action_guard,
                     exact_cover_extension_rule_apply);

} // namespace tactix

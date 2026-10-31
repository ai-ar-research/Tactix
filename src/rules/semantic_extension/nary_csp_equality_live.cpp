#include "rules/semantic_extension/nary_csp_equality_live.hpp"

#include "proof/proof.hpp"
#include "rules/semantic_extension/bounded_journal.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <string>

namespace tactix {
struct NaryCspEqualitySessionAccess {
  static void commit(NaryCspEqualitySession &session, const Kernel &kernel,
                     std::vector<NaryCspEqualityKey> keys,
                     std::vector<NaryCspEqualityRecord> records) {
    session.owner_ = &kernel;
    session.keys_ = std::move(keys);
    session.records_ = std::move(records);
  }
};

namespace {

namespace equality = nary_csp_equality;

using Journal = semantic_extension::BoundedJournal<>;

std::vector<std::vector<int32_t>> owner_domains(
    const NaryCspRelationOwnerSnapshot &owner) {
  std::vector<std::vector<int32_t>> result;
  for (const auto &domain : owner.relation.domains)
    result.push_back(domain.values);
  return result;
}

NaryCspEqualityKey make_key(const NaryCspRelationOwnerSnapshot &owner,
                            const equality::Selection &selection) {
  NaryCspEqualityKey key;
  key.domains = owner_domains(owner);
  key.first_equal_domain =
      std::min(selection.left_domain, selection.right_domain);
  key.second_equal_domain =
      std::max(selection.left_domain, selection.right_domain);
  key.residual_domain = selection.residual_domain;
  return key;
}

NaryCspRelationCaptureRequest capture_request(
    const NaryCspRelationOwnerSnapshot &owner,
    const equality::Selection &selection,
    const NaryCspEqualityLimits &limits) {
  NaryCspRelationCaptureRequest request;
  request.domains = owner_domains(owner);
  (void)selection;
  request.maximum_source_clauses = limits.maximum_source_clauses;
  request.maximum_source_literals = limits.maximum_source_literals;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_domains = 3;
  request.maximum_domain_values = limits.maximum_domain_values;
  request.maximum_forbidden_tuples = limits.maximum_forbidden_tuples;
  request.maximum_external_extent = limits.maximum_external_extent;
  return request;
}

NaryCspEqualityLiveStatus capture_status(NaryCspRelationCaptureStatus status) {
  switch (status) {
  case NaryCspRelationCaptureStatus::Interrupted:
    return NaryCspEqualityLiveStatus::Interrupted;
  case NaryCspRelationCaptureStatus::ResourceExhausted:
    return NaryCspEqualityLiveStatus::ResourceExhausted;
  case NaryCspRelationCaptureStatus::Inapplicable:
    return NaryCspEqualityLiveStatus::Inapplicable;
  case NaryCspRelationCaptureStatus::NoStructure:
  case NaryCspRelationCaptureStatus::Stale:
    // A previously retained owner has lost its exact live source image.
    return NaryCspEqualityLiveStatus::Stale;
  default:
    return NaryCspEqualityLiveStatus::Invalid;
  }
}

NaryCspEqualityLiveStatus plan_status(equality::Status status) {
  switch (status) {
  case equality::Status::NoConsequence:
    return NaryCspEqualityLiveStatus::NoConsequence;
  case equality::Status::BoundExceeded:
  case equality::Status::EffortExhausted:
    return NaryCspEqualityLiveStatus::ResourceExhausted;
  default:
    return NaryCspEqualityLiveStatus::Invalid;
  }
}

bool render_journal(const NaryCspEqualityRequest &request,
                    const NaryCspRelationOwnerSnapshot &owner,
                    const equality::Plan &plan, uint64_t first_id,
                    Journal &journal) {
  if (!journal.append("{\"kind\":\"nary-csp-equality-begin\",\"action\":" +
                      std::to_string(request.action_id) +
                      ",\"semantic\":" +
                      std::to_string(request.semantic_id) +
                      ",\"left_domain\":" +
                      std::to_string(plan.selection.left_domain) +
                      ",\"right_domain\":" +
                      std::to_string(plan.selection.right_domain) +
                      ",\"residual_domain\":" +
                      std::to_string(plan.selection.residual_domain) + "}\n"))
    return false;
  for (const auto &premise : owner.source) {
    if (!journal.append("{\"kind\":\"nary-csp-equality-source\",\"action\":" +
                        std::to_string(request.action_id) +
                        ",\"occurrence\":" +
                        std::to_string(premise.occurrence) +
                        ",\"native_proof_id\":" +
                        std::to_string(premise.native_proof_id) +
                        ",\"clause\":[") ||
        !journal.integers(premise.literals) || !journal.append("]}\n"))
      return false;
  }
  for (size_t domain = 0; domain < owner.relation.domains.size(); ++domain) {
    if (!journal.append("{\"kind\":\"nary-csp-equality-domain\",\"action\":" +
                        std::to_string(request.action_id) +
                        ",\"domain\":" + std::to_string(domain) +
                        ",\"values\":[") ||
        !journal.integers(owner.relation.domains[domain].values) ||
        !journal.append("]}\n"))
      return false;
  }
  for (size_t index = 0; index < plan.additions.size(); ++index) {
    if (!journal.append("{\"kind\":\"nary-csp-equality-addition\",\"action\":" +
                        std::to_string(request.action_id) +
                        ",\"clause_id\":" +
                        std::to_string(first_id + index) +
                        ",\"proof\":\"" +
                        (plan.additions[index].proof == equality::ProofStep::Rup
                             ? "rup"
                             : "rat") +
                        "\",\"clause\":[") ||
        !journal.integers(plan.additions[index].clause) ||
        !journal.append("]}\n"))
      return false;
  }
  return journal.append(
      "{\"kind\":\"nary-csp-equality-complete\",\"action\":" +
      std::to_string(request.action_id) +
      ",\"projection\":\"omit-exact-aligned-equality\",\"extension\":" +
      std::to_string(plan.projection.extension_variable) + "}\n");
}

} // namespace

bool NaryCspEqualityLimits::valid() const {
  return maximum_source_clauses && maximum_source_literals &&
         maximum_recognition_work && maximum_domain_values >= 2 &&
         maximum_forbidden_tuples && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_journal_bytes && maximum_extent_growth &&
         maximum_external_extent && maximum_session_keys;
}

NaryCspEqualityLiveResult nary_csp_equality_live_apply(
    Kernel &kernel, NaryCspEqualitySession &session,
    const NaryCspEqualityRequest &request) {
  NaryCspEqualityLiveResult result;
  if (!request.owner || !request.limits.valid() || !request.action_id ||
      !request.semantic_id || !request.records ||
      (session.retained() && !session.belongs_to(kernel)))
    return result;
  std::array<uint32_t, 3> coordinates{
      request.selection.left_domain, request.selection.right_domain,
      request.selection.residual_domain};
  std::sort(coordinates.begin(), coordinates.end());
  if (coordinates != std::array<uint32_t, 3>{0, 1, 2}) return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = NaryCspEqualityLiveStatus::UnsupportedProof;
    return result;
  }
  result.key = make_key(*request.owner, request.selection);
  if (std::binary_search(session.keys().begin(), session.keys().end(),
                         result.key)) {
    result.status = NaryCspEqualityLiveStatus::Duplicate;
    return result;
  }
  if (session.keys().size() >= request.limits.maximum_session_keys) {
    result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
    return result;
  }

  NaryCspRelationOwnerSnapshot current;
  if (request.owner_freshly_captured) {
    current = *request.owner;
    result.source_clauses = current.source.size();
    result.source_literals = current.source_literals;
  } else {
    const auto captured = nary_csp_relation_capture_owner(
        kernel,
        capture_request(*request.owner, request.selection, request.limits),
        current);
    result.recognition_work = captured.recognition_work;
    result.source_clauses = captured.source_clauses;
    result.source_literals = captured.source_literals;
    if (captured.status != NaryCspRelationCaptureStatus::Applied) {
      result.status = capture_status(captured.status);
      return result;
    }
    current.capture_work = request.owner->capture_work;
    current.external_extent = request.owner->external_extent;
    if (current != *request.owner) {
      result.status = NaryCspEqualityLiveStatus::Stale;
      return result;
    }
  }
  const uint64_t extent = kernel.external_to_internal.size();
  if (extent >= INT32_MAX || extent + 1 > request.limits.maximum_external_extent) {
    result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
    return result;
  }
  uint64_t remaining = request.limits.maximum_recognition_work -
                       std::min(request.limits.maximum_recognition_work,
                                result.recognition_work);
  const equality::Bounds bounds{
      request.limits.maximum_source_clauses,
      request.limits.maximum_source_literals,
      request.limits.maximum_domain_values,
      request.limits.maximum_forbidden_tuples,
      request.limits.maximum_proof_additions,
      request.limits.maximum_proof_literal_occurrences,
      static_cast<int32_t>(extent + 1)};
  equality::Plan plan = equality::plan_equality(
      current.source, current.relation, request.selection, bounds,
      [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  result.planning_work = plan.consumed_work;
  result.recognition_work += plan.consumed_work;
  if (plan.status != equality::Status::Planned) {
    result.status = plan_status(plan.status);
    return result;
  }

  uint64_t arena_words = 0;
  for (const auto &addition : plan.additions) {
    const uint64_t proof = uint64_t{1} + addition.clause.size() +
                           addition.antecedents.size();
    if (proof > request.limits.maximum_proof_work -
                    std::min(request.limits.maximum_proof_work,
                             result.proof_work) ||
        addition.clause.size() >
            request.limits.maximum_proof_literal_occurrences -
                std::min(request.limits.maximum_proof_literal_occurrences,
                         result.proof_literal_occurrences)) {
      result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
      return result;
    }
    result.proof_work += proof;
    result.proof_literal_occurrences += addition.clause.size();
    const uint64_t words = addition.clause.size() + CLAUSE_HEADER_WORDS;
    if (words > UINT64_MAX - arena_words) {
      result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
      return result;
    }
    arena_words += words;
  }
  result.planned_additions = plan.additions.size();
  result.extent_growth = 1;
  if (plan.additions.size() > request.limits.maximum_proof_additions ||
      request.limits.maximum_extent_growth < 1 ||
      kernel.nvars == UINT32_MAX ||
      kernel.next_clause_id > uint64_t(INT64_MAX) - plan.additions.size() ||
      kernel.arena.size() >= INVALID_REF ||
      arena_words >= INVALID_REF - kernel.arena.size()) {
    result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
    return result;
  }

  const uint64_t first_id = kernel.next_clause_id + 1;
  Journal journal{request.limits.maximum_journal_bytes};
  if (!render_journal(request, current, plan, first_id, journal)) {
    result.status = NaryCspEqualityLiveStatus::ResourceExhausted;
    return result;
  }
  result.journal_bytes = journal.bytes.size();
  std::vector<NaryCspEqualityKey> keys = session.keys();
  keys.insert(std::lower_bound(keys.begin(), keys.end(), result.key),
              result.key);
  std::vector<NaryCspEqualityRecord> records = session.records();
  NaryCspEqualityRecord record;
  record.key = result.key;
  record.extension_variable =
      static_cast<uint32_t>(plan.projection.extension_variable);
  for (const auto &addition : plan.additions)
    if (addition.clause.size() == 3 &&
        (addition.clause[0] == plan.projection.extension_variable ||
         addition.clause[0] == -plan.projection.extension_variable))
      record.definition_clauses.push_back(addition.clause);
  records.push_back(std::move(record));
  std::sort(records.begin(), records.end(), [](const auto &a, const auto &b) {
    return a.key < b.key;
  });
  if (kernel_terminating(kernel)) {
    result.status = NaryCspEqualityLiveStatus::Interrupted;
    return result;
  }

  kernel.close_startup_certificate();
  const Var fresh = kernel_new_extension_vars(kernel, 1);
  if (kernel_externalize(kernel, make_lit(fresh, false)) !=
      plan.projection.extension_variable)
    std::abort();
  std::vector<uint64_t> ids;
  for (const auto &addition : plan.additions) {
    std::vector<Lit> literals;
    for (int32_t external : addition.clause) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
        std::abort();
      literals.push_back(literal);
    }
    const uint64_t id = ++kernel.next_clause_id;
    if (kernel.proof) {
      if (addition.proof == equality::ProofStep::Rup)
        kernel.proof->add_rup(id, literals.data(), literals.size(), nullptr, 0);
      else
        kernel.proof->add_rat(id, literals.data(), literals.size(), nullptr, 0);
    }
    ids.push_back(id);
    kernel_add_large_clause(kernel, literals.data(), literals.size(),
                            addition.proof == equality::ProofStep::Rup,
                            addition.proof == equality::ProofStep::Rup
                                ? literals.size()
                                : 0,
                            id);
  }
  if (std::fwrite(journal.bytes.data(), 1, journal.bytes.size(),
                  request.records) != journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  NaryCspEqualitySessionAccess::commit(session, kernel, std::move(keys),
                                       std::move(records));

  result.status = NaryCspEqualityLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.emitted_additions = plan.additions.size();
  result.first_published_clause_id = ids.front();
  result.last_published_clause_id = ids.back();
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  result.projection = {
      static_cast<uint32_t>(plan.projection.extension_variable),
      plan.projection.left_values,
      plan.projection.right_values,
      plan.projection.exact_aligned_equality,
      plan.projection.project_from_external_model,
      plan.projection.reconstruction_witness_required};
  for (const auto &addition : plan.additions)
    if (addition.proof == equality::ProofStep::Rup)
      result.published_consequences.push_back(addition.clause);
  result.semantic_variables.push_back(
      {static_cast<uint32_t>(plan.projection.extension_variable),
       strategy::SemanticVariableRole::IntroducedTheory, 1});
  result.semantic_candidate_ready = true;
  return result;
}

} // namespace tactix

#include "rules/semantic_extension/gf3_gaussian_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <set>
#include <utility>

namespace tactix::gf3_gaussian_live {

struct PreparedAccess {
  static Gf3AffineOwnerSnapshot &owner(Prepared &prepared) {
    return prepared.owner_;
  }
  static gf3_gaussian_certificate::Plan &certificate(Prepared &prepared) {
    return prepared.certificate_;
  }
  static std::vector<uint64_t> &event_ids(Prepared &prepared) {
    return prepared.event_ids_;
  }
  static std::vector<uint8_t> &install(Prepared &prepared) {
    return prepared.install_;
  }
  static std::string &batch_identity(Prepared &prepared) {
    return prepared.batch_identity_;
  }
  static std::string &journal(Prepared &prepared) {
    return prepared.journal_;
  }
  static Result &result(Prepared &prepared) { return prepared.prepared_; }
  static Request &request(Prepared &prepared) { return prepared.request_; }
  static const Request &request(const Prepared &prepared) {
    return prepared.request_;
  }
};

namespace {

bool add(uint64_t &value, uint64_t amount) {
  if (amount > std::numeric_limits<uint64_t>::max() - value) return false;
  value += amount;
  return true;
}

void append_u8(std::string &bytes, uint8_t value) {
  bytes.push_back(static_cast<char>(value));
}

void append_u32(std::string &bytes, uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8)
    append_u8(bytes, static_cast<uint8_t>(value >> shift));
}

void append_u64(std::string &bytes, uint64_t value) {
  for (unsigned shift = 0; shift != 64; shift += 8)
    append_u8(bytes, static_cast<uint8_t>(value >> shift));
}

void append_reference(std::string &bytes,
                      const gf3_affine::SourceReference &reference,
                      const Gf3AffineOwnerSnapshot &owner) {
  append_u64(bytes, reference.occurrence);
  append_u64(bytes, reference.native_proof_id);
  if (!reference.occurrence || reference.occurrence > owner.sources.size()) {
    append_u64(bytes, UINT64_MAX);
    return;
  }
  const auto &clause = owner.sources[reference.occurrence - 1].clause;
  append_u64(bytes, clause.size());
  for (int32_t literal : clause)
    append_u32(bytes, static_cast<uint32_t>(literal));
}

std::string hex(std::string_view bytes) {
  static constexpr char digit[] = "0123456789abcdef";
  std::string result = "g3g1:";
  result.reserve(result.size() + 2 * bytes.size());
  for (unsigned char byte : bytes) {
    result.push_back(digit[byte >> 4]);
    result.push_back(digit[byte & 15]);
  }
  return result;
}

std::string outcome_identity(
    const gf3_gaussian_certificate::Consequence &consequence,
    const gf3_gaussian_bridge::Plan &bridge,
    const Gf3AffineOwnerSnapshot &owner) {
  std::string bytes;
  append_u8(bytes, static_cast<uint8_t>(consequence.kind));
  append_u32(bytes, consequence.derivation);
  append_u8(bytes, consequence.rhs);
  append_u64(bytes, consequence.row.size());
  for (const auto &term : consequence.row) {
    append_u32(bytes, term.variable);
    append_u8(bytes, term.coefficient);
  }
  append_u64(bytes, consequence.sources.size());
  for (const auto &source : consequence.sources) {
    append_u64(bytes, source.combination.source_id);
    append_u8(bytes, source.combination.coefficient);
    append_u32(bytes, source.authentication_index);
    if (source.authentication_index >= bridge.authentication.size()) {
      append_u64(bytes, UINT64_MAX);
      continue;
    }
    const auto &authentication =
        bridge.authentication[source.authentication_index];
    append_u64(bytes, authentication.source_id);
    append_u32(bytes, authentication.equation_index);
    append_u64(bytes, authentication.domains.size());
    for (const auto &domain : authentication.domains) {
      append_u32(bytes, domain.domain);
      append_reference(bytes, domain.at_least_one, owner);
      for (const auto &reference : domain.at_most_one)
        append_reference(bytes, reference, owner);
    }
    append_u64(bytes, authentication.forbidden_tuples.size());
    for (const auto &tuple : authentication.forbidden_tuples) {
      append_u64(bytes, tuple.tuple);
      append_reference(bytes, tuple.premise, owner);
    }
  }
  return hex(bytes);
}

bool sized_add(uint64_t &value, uint64_t amount) {
  return amount <= UINT64_MAX - value && (value += amount, true);
}

bool reference_bytes(uint64_t &bytes,
                     const gf3_affine::SourceReference &reference,
                     const Gf3AffineOwnerSnapshot &owner) {
  if (!sized_add(bytes, 24)) return false;
  if (!reference.occurrence || reference.occurrence > owner.sources.size())
    return true;
  const uint64_t literals = owner.sources[reference.occurrence - 1].clause.size();
  return literals <= (UINT64_MAX - bytes) / 4 &&
         sized_add(bytes, 4 * literals);
}

bool outcome_identity_bytes(
    const gf3_gaussian_certificate::Consequence &consequence,
    const gf3_gaussian_bridge::Plan &bridge,
    const Gf3AffineOwnerSnapshot &owner, uint64_t &encoded) {
  uint64_t bytes = 1 + 4 + 1 + 8;
  if (consequence.row.size() > (UINT64_MAX - bytes) / 5 ||
      !sized_add(bytes, 5 * consequence.row.size()) ||
      !sized_add(bytes, 8))
    return false;
  for (const auto &source : consequence.sources) {
    if (!sized_add(bytes, 8 + 1 + 4)) return false;
    if (source.authentication_index >= bridge.authentication.size()) {
      if (!sized_add(bytes, 8)) return false;
      continue;
    }
    const auto &authentication =
        bridge.authentication[source.authentication_index];
    if (!sized_add(bytes, 8 + 4 + 8)) return false;
    for (const auto &domain : authentication.domains) {
      if (!sized_add(bytes, 4) ||
          !reference_bytes(bytes, domain.at_least_one, owner))
        return false;
      for (const auto &reference : domain.at_most_one)
        if (!reference_bytes(bytes, reference, owner)) return false;
    }
    if (!sized_add(bytes, 8)) return false;
    for (const auto &tuple : authentication.forbidden_tuples)
      if (!sized_add(bytes, 8) ||
          !reference_bytes(bytes, tuple.premise, owner))
        return false;
  }
  if (bytes > (UINT64_MAX - 5) / 2) return false;
  encoded = 5 + 2 * bytes;
  return true;
}

bool applicable(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

bool valid(const Request &request) {
  const auto &recognition = request.recognition;
  const auto &certificate = request.certificate;
  return request.maximum_capture_work && request.maximum_revalidation_work &&
         request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences &&
         request.maximum_journal_bytes && request.maximum_external_extent &&
         request.maximum_identity_bytes &&
         (request.selected_identity.empty() ||
          request.selected_identity.size() <= request.maximum_identity_bytes) &&
         request.action_id && request.semantic_id && request.records &&
         recognition.maximum_source_clauses &&
         recognition.maximum_source_literals && recognition.maximum_domains &&
         recognition.maximum_equations &&
         recognition.maximum_equation_arity >= 2 &&
         recognition.maximum_tuples_per_equation &&
         certificate.bridge.maximum_work &&
         certificate.bridge.maximum_source_clauses &&
         certificate.bridge.maximum_source_literals &&
         certificate.bridge.maximum_domains &&
         certificate.bridge.maximum_equations &&
         certificate.bridge.maximum_terms &&
         certificate.bridge.maximum_authentication_references &&
         certificate.bridge.maximum_references_per_equation &&
         certificate.bridge.gaussian.maximum_work &&
         certificate.bridge.gaussian.maximum_fill_entries &&
         certificate.bridge.gaussian.maximum_intermediate_width &&
         certificate.bridge.gaussian.maximum_provenance_terms &&
         certificate.bridge.gaussian.maximum_derivations &&
         certificate.bridge.gaussian.maximum_retained_plan_bytes &&
         certificate.maximum_work && certificate.maximum_events &&
         certificate.maximum_event_literals &&
         certificate.maximum_active_clauses &&
         certificate.maximum_retained_source_references &&
         certificate.maximum_event_width &&
         certificate.maximum_branch_domains &&
         certificate.maximum_consequences;
}

Gf3AffineLiveRequest owner_request(const Request &request) {
  Gf3AffineLiveRequest result;
  result.recognition = request.recognition;
  result.maximum_recognition_work = request.maximum_capture_work;
  result.maximum_revalidation_work = request.maximum_revalidation_work;
  return result;
}

Status owner_status(Gf3AffineLiveStatus status) {
  switch (status) {
  case Gf3AffineLiveStatus::Applied: return Status::Prepared;
  case Gf3AffineLiveStatus::Inapplicable: return Status::Inapplicable;
  case Gf3AffineLiveStatus::NoStructure: return Status::NoStructure;
  case Gf3AffineLiveStatus::ResourceExhausted:
    return Status::ResourceExhausted;
  case Gf3AffineLiveStatus::Interrupted: return Status::Interrupted;
  case Gf3AffineLiveStatus::Stale: return Status::Stale;
  case Gf3AffineLiveStatus::UnsupportedProof:
    return Status::UnsupportedProof;
  case Gf3AffineLiveStatus::Invalid:
  case Gf3AffineLiveStatus::NoConsequence:
  case Gf3AffineLiveStatus::Duplicate: return Status::Invalid;
  }
  std::abort();
}

Status bridge_status(gf3_gaussian_bridge::Status status) {
  switch (status) {
  case gf3_gaussian_bridge::Status::Complete: return Status::Prepared;
  case gf3_gaussian_bridge::Status::LimitExceeded:
    return Status::ResourceExhausted;
  case gf3_gaussian_bridge::Status::Interrupted: return Status::Interrupted;
  case gf3_gaussian_bridge::Status::InvalidTheory:
  case gf3_gaussian_bridge::Status::InvalidPlan: return Status::Invalid;
  }
  std::abort();
}

Status certificate_status(gf3_gaussian_certificate::Status status) {
  switch (status) {
  case gf3_gaussian_certificate::Status::Complete: return Status::Prepared;
  case gf3_gaussian_certificate::Status::NoSupportedOutcome:
    return Status::NoSupportedOutcome;
  case gf3_gaussian_certificate::Status::LimitExceeded:
    return Status::ResourceExhausted;
  case gf3_gaussian_certificate::Status::Interrupted:
    return Status::Interrupted;
  case gf3_gaussian_certificate::Status::InvalidRequest:
  case gf3_gaussian_certificate::Status::InvalidTheory:
  case gf3_gaussian_certificate::Status::InvalidBridge:
  case gf3_gaussian_certificate::Status::InvalidCertificate:
  case gf3_gaussian_certificate::Status::Uncertifiable:
    return Status::Invalid;
  }
  std::abort();
}

bool append(std::string &output, uint64_t maximum, const std::string &text) {
  if (output.size() > maximum || text.size() > maximum - output.size())
    return false;
  output += text;
  return true;
}

std::string integers(const std::vector<int32_t> &values) {
  std::string result;
  for (size_t index = 0; index < values.size(); ++index)
    result += (index ? "," : "") + std::to_string(values[index]);
  return result;
}

std::string row_terms(const std::vector<gf3_gaussian::Term> &row) {
  std::string result;
  for (size_t index = 0; index < row.size(); ++index)
    result += (index ? "," : "") +
              std::string("{\"variable\":") +
              std::to_string(row[index].variable) +
              ",\"coefficient\":" +
              std::to_string(row[index].coefficient) + "}";
  return result;
}

std::string source_terms(
    const std::vector<gf3_gaussian_certificate::AuthenticatedSource> &sources) {
  std::string result;
  for (size_t index = 0; index < sources.size(); ++index) {
    const auto &source = sources[index];
    result += (index ? "," : "") +
              std::string("{\"source_id\":") +
              std::to_string(source.combination.source_id) +
              ",\"coefficient\":" +
              std::to_string(source.combination.coefficient) +
              ",\"authentication_index\":" +
              std::to_string(source.authentication_index) + "}";
  }
  return result;
}

std::string domain_authentications(
    const std::vector<gf3_gaussian_bridge::DomainAuthentication> &domains) {
  std::string result;
  for (size_t index = 0; index < domains.size(); ++index) {
    const auto &domain = domains[index];
    result += (index ? "," : "") + std::string("{\"domain\":") +
              std::to_string(domain.domain) + ",\"at_least_one\":" +
              std::to_string(domain.at_least_one.occurrence) +
              ",\"at_most_one\":[";
    for (size_t pair = 0; pair < domain.at_most_one.size(); ++pair)
      result += (pair ? "," : "") +
                std::to_string(domain.at_most_one[pair].occurrence);
    result += "]}";
  }
  return result;
}

std::string tuple_authentications(
    const std::vector<gf3_gaussian_bridge::TupleAuthentication> &tuples) {
  std::string result;
  for (size_t index = 0; index < tuples.size(); ++index)
    result += (index ? "," : "") + std::string("{\"tuple\":") +
              std::to_string(tuples[index].tuple) +
              ",\"occurrence\":" +
              std::to_string(tuples[index].premise.occurrence) + "}";
  return result;
}

bool make_journal(const Request &request, Prepared &prepared) {
  std::string journal;
  const std::string identity =
      "\"action\":" + std::to_string(request.action_id) +
      ",\"semantic\":" + std::to_string(request.semantic_id);
  if (!append(journal, request.maximum_journal_bytes,
              "{" + identity +
                  ",\"kind\":\"gf3-gaussian-begin\",\"proof\":\"rup\","
                  "\"publication\":\"unit-or-conflict-only\","
                  "\"selection\":\"" + request.selected_identity + "\"}\n"))
    return false;
  for (const auto &source : PreparedAccess::owner(prepared).sources)
    if (!append(journal, request.maximum_journal_bytes,
                "{" + identity +
                    ",\"kind\":\"gf3-gaussian-source\",\"occurrence\":" +
                    std::to_string(source.occurrence) +
                    ",\"native_proof_id\":" +
                    std::to_string(source.native_proof_id) +
                    ",\"clause\":[" + integers(source.clause) + "]}\n"))
      return false;
  for (const auto &authentication : prepared.bridge().authentication)
    if (!append(
            journal, request.maximum_journal_bytes,
            "{" + identity +
                ",\"kind\":\"gf3-gaussian-authentication\",\"source_id\":" +
                std::to_string(authentication.source_id) +
                ",\"equation_index\":" +
                std::to_string(authentication.equation_index) +
                ",\"domains\":[" +
                domain_authentications(authentication.domains) +
                "],\"forbidden_tuples\":[" +
                tuple_authentications(authentication.forbidden_tuples) +
                "]}\n"))
      return false;
  auto &certificate = PreparedAccess::certificate(prepared);
  auto &event_ids = PreparedAccess::event_ids(prepared);
  auto &install = PreparedAccess::install(prepared);
  auto &prepared_result = PreparedAccess::result(prepared);
  for (size_t index = 0; index < certificate.consequences.size(); ++index) {
    const auto &consequence = certificate.consequences[index];
    const char *kind =
        consequence.kind ==
                gf3_gaussian_certificate::ConsequenceKind::Conflict
            ? "conflict"
            : "unit-equation";
    if (!append(journal, request.maximum_journal_bytes,
                "{" + identity +
                    ",\"kind\":\"gf3-gaussian-consequence\",\"index\":" +
                    std::to_string(index) + ",\"outcome\":\"" + kind +
                    "\",\"derivation\":" +
                    std::to_string(consequence.derivation) +
                    ",\"row\":[" + row_terms(consequence.row) +
                    "],\"rhs\":" + std::to_string(consequence.rhs) +
                    ",\"source_combination\":[" +
                    source_terms(consequence.sources) + "]}\n"))
      return false;
  }
  for (size_t index = 0; index < certificate.events.size(); ++index) {
    const auto &event = certificate.events[index];
    const char *role =
        event.role == gf3_gaussian_certificate::EventRole::Auxiliary
            ? "auxiliary"
        : event.role == gf3_gaussian_certificate::EventRole::Consequence
            ? "consequence"
            : "conflict";
    if (!append(journal, request.maximum_journal_bytes,
                "{" + identity +
                    ",\"kind\":\"gf3-gaussian-rup\",\"event\":" +
                    std::to_string(index) + ",\"clause_id\":" +
                    std::to_string(event_ids[index]) +
                    ",\"consequence\":" +
                    std::to_string(event.consequence) +
                    ",\"role\":\"" + role + "\",\"install\":" +
                    (install[index] ? "true" : "false") +
                    ",\"clause\":[" + integers(event.clause) + "]}\n"))
      return false;
  }
  if (!append(journal, request.maximum_journal_bytes,
              "{" + identity +
                  ",\"kind\":\"gf3-gaussian-complete\",\"events\":" +
                  std::to_string(certificate.events.size()) +
                  ",\"installed\":" +
                  std::to_string(
                      prepared_result.effort.installed_clauses) +
                  ",\"duplicates\":" +
                  std::to_string(
                      prepared_result.effort.duplicate_clauses) +
                  "}\n"))
    return false;
  PreparedAccess::journal(prepared) = std::move(journal);
  return true;
}

bool plan_events(Kernel &kernel, const Request &request, Prepared &prepared,
                 Result &result) {
  const auto &events = PreparedAccess::certificate(prepared).events;
  if (events.size() > request.maximum_proof_additions ||
      kernel.next_clause_id > uint64_t(INT64_MAX) - events.size()) {
    result.status = Status::ResourceExhausted;
    return false;
  }
  std::set<std::vector<int32_t>> installed;
  auto &event_ids = PreparedAccess::event_ids(prepared);
  auto &install_flags = PreparedAccess::install(prepared);
  event_ids.reserve(events.size());
  install_flags.reserve(events.size());
  uint64_t next_id = kernel.next_clause_id;
  for (const auto &event : events) {
    if ((event.role == gf3_gaussian_certificate::EventRole::Auxiliary &&
         event.clause.empty()) ||
        (event.role == gf3_gaussian_certificate::EventRole::Consequence &&
         event.clause.size() != 1) ||
        (event.role == gf3_gaussian_certificate::EventRole::Conflict &&
         !event.clause.empty())) {
      result.status = Status::Invalid;
      return false;
    }
    if (!add(result.effort.proof_additions, 1) ||
        !add(result.effort.proof_literal_occurrences, event.clause.size()) ||
        result.effort.proof_literal_occurrences >
            request.maximum_proof_literal_occurrences) {
      result.status = Status::ResourceExhausted;
      return false;
    }
    event_ids.push_back(++next_id);
    bool install = event.role !=
                   gf3_gaussian_certificate::EventRole::Auxiliary;
    if (install && !installed.insert(event.clause).second) {
      install = false;
      ++result.effort.duplicate_clauses;
    }
    install_flags.push_back(install);
    result.effort.installed_clauses += install;
  }
  if (!result.effort.installed_clauses) {
    result.status = Status::Duplicate;
    return false;
  }
  return true;
}

bool same_request(const Request &request, const Prepared &prepared) {
  const Request &retained = PreparedAccess::request(prepared);
  const auto same_recognition = [](const auto &a, const auto &b) {
    return a.maximum_source_clauses == b.maximum_source_clauses &&
           a.maximum_source_literals == b.maximum_source_literals &&
           a.maximum_domains == b.maximum_domains &&
           a.maximum_equations == b.maximum_equations &&
           a.maximum_equation_arity == b.maximum_equation_arity &&
           a.maximum_tuples_per_equation == b.maximum_tuples_per_equation;
  };
  const auto same_gaussian = [](const auto &a, const auto &b) {
    return a.maximum_work == b.maximum_work &&
           a.maximum_fill_entries == b.maximum_fill_entries &&
           a.maximum_intermediate_width == b.maximum_intermediate_width &&
           a.maximum_provenance_terms == b.maximum_provenance_terms &&
           a.maximum_derivations == b.maximum_derivations &&
           a.maximum_retained_plan_bytes == b.maximum_retained_plan_bytes;
  };
  const auto same_bridge = [&](const auto &a, const auto &b) {
    return a.maximum_work == b.maximum_work &&
           a.maximum_source_clauses == b.maximum_source_clauses &&
           a.maximum_source_literals == b.maximum_source_literals &&
           a.maximum_domains == b.maximum_domains &&
           a.maximum_equations == b.maximum_equations &&
           a.maximum_terms == b.maximum_terms &&
           a.maximum_authentication_references ==
               b.maximum_authentication_references &&
           a.maximum_references_per_equation ==
               b.maximum_references_per_equation &&
           same_gaussian(a.gaussian, b.gaussian);
  };
  const auto same_certificate = [&](const auto &a, const auto &b) {
    return same_bridge(a.bridge, b.bridge) &&
           a.maximum_work == b.maximum_work &&
           a.maximum_events == b.maximum_events &&
           a.maximum_event_literals == b.maximum_event_literals &&
           a.maximum_active_clauses == b.maximum_active_clauses &&
           a.maximum_retained_source_references ==
               b.maximum_retained_source_references &&
           a.maximum_event_width == b.maximum_event_width &&
           a.maximum_branch_domains == b.maximum_branch_domains &&
           a.maximum_consequences == b.maximum_consequences;
  };
  return same_recognition(request.recognition, retained.recognition) &&
         same_certificate(request.certificate, retained.certificate) &&
         request.maximum_capture_work == retained.maximum_capture_work &&
         request.maximum_revalidation_work ==
             retained.maximum_revalidation_work &&
         request.maximum_proof_additions ==
             retained.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences ==
             retained.maximum_proof_literal_occurrences &&
         request.maximum_journal_bytes == retained.maximum_journal_bytes &&
         request.maximum_identity_bytes == retained.maximum_identity_bytes &&
         request.maximum_external_extent == retained.maximum_external_extent &&
         request.action_id == retained.action_id &&
         request.semantic_id == retained.semantic_id &&
         request.records == retained.records &&
         request.selected_identity == retained.selected_identity;
}

} // namespace

Result prepare_internal(Kernel &kernel, const Request &request,
                        Prepared &output, bool atomic) {
  Result result;
  if (!valid(request)) return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Status::UnsupportedProof;
    return result;
  }
  if (kernel.proof && !atomic) {
    result.status = Status::UnsupportedProofSplit;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = kernel_terminating(kernel) ? Status::Interrupted
                                                : Status::Inapplicable;
    return result;
  }
  if (kernel.external_to_internal.size() > request.maximum_external_extent) {
    result.status = Status::ResourceExhausted;
    return result;
  }

  Prepared prepared;
  prepared.request_ = request;
  prepared.starting_clause_id_ = kernel.next_clause_id;
  prepared.arena_generation_ = kernel.arena_generation;
  prepared.starting_nvars_ = kernel.nvars;
  prepared.starting_extent_ = kernel.external_to_internal.size();

  const Gf3AffineLiveRequest live = owner_request(request);
  const auto captured =
      gf3_affine_live_capture_owner(kernel, live, prepared.owner_);
  result.effort.source_clauses = captured.source_clauses;
  result.effort.source_literals = captured.source_literals;
  result.effort.capture_work = captured.recognition_work;
  if (captured.status != Gf3AffineLiveStatus::Applied) {
    result.status = owner_status(captured.status);
    return result;
  }

  uint64_t bridge_work = 0;
  bool bridge_interrupted = false;
  const auto bridge_charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      bridge_interrupted = true;
      return false;
    }
    bridge_work += amount;
    return true;
  };
  auto bridged = gf3_gaussian_bridge::build(
      prepared.owner_.theory, request.certificate.bridge, bridge_charge);
  result.effort.bridge_work = bridge_work;
  if (bridged.status != gf3_gaussian_bridge::Status::Complete) {
    result.status = bridge_interrupted ? Status::Interrupted
                                       : bridge_status(bridged.status);
    return result;
  }
  prepared.bridge_ = std::move(bridged.plan);

  uint64_t certificate_work = 0;
  bool certificate_interrupted = false;
  const auto certificate_charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      certificate_interrupted = true;
      return false;
    }
    certificate_work += amount;
    return true;
  };
  auto certified = gf3_gaussian_certificate::build(
      prepared.owner_.theory, prepared.bridge_, request.certificate,
      certificate_charge);
  result.effort.certificate_work = certificate_work;
  if (certified.status != gf3_gaussian_certificate::Status::Complete) {
    result.status = certificate_interrupted
                        ? Status::Interrupted
                        : certificate_status(certified.status);
    return result;
  }
  prepared.certificate_ = std::move(certified.plan);
  std::vector<std::string> identities;
  identities.reserve(prepared.certificate_.consequences.size());
  for (const auto &consequence : prepared.certificate_.consequences) {
    uint64_t encoded = 0;
    if (!outcome_identity_bytes(consequence, prepared.bridge_,
                                prepared.owner_, encoded) ||
        result.effort.identity_bytes > request.maximum_identity_bytes ||
        encoded > request.maximum_identity_bytes -
                      result.effort.identity_bytes) {
      result.status = Status::ResourceExhausted;
      return result;
    }
    std::string identity = outcome_identity(consequence, prepared.bridge_,
                                            prepared.owner_);
    if (identity.size() != encoded ||
        !add(result.effort.identity_bytes, identity.size())) {
      result.status = Status::ResourceExhausted;
      return result;
    }
    identities.push_back(std::move(identity));
  }
  uint64_t batch_binary_bytes = 8;
  for (const std::string &identity : identities)
    if (!sized_add(batch_binary_bytes, 8) ||
        !sized_add(batch_binary_bytes, identity.size())) {
      result.status = Status::ResourceExhausted;
      return result;
    }
  if (batch_binary_bytes > (UINT64_MAX - 5) / 2) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  const uint64_t batch_encoded_bytes = 5 + 2 * batch_binary_bytes;
  if (result.effort.identity_bytes > request.maximum_identity_bytes ||
      batch_encoded_bytes > request.maximum_identity_bytes -
                                result.effort.identity_bytes) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  std::string batch_bytes;
  append_u64(batch_bytes, identities.size());
  for (const std::string &identity : identities) {
    append_u64(batch_bytes, identity.size());
    batch_bytes.append(identity);
  }
  PreparedAccess::batch_identity(prepared) = hex(batch_bytes);
  prepared.batch_identity_.replace(0, 5, "g3b1:");
  if (prepared.batch_identity_.size() != batch_encoded_bytes ||
      !add(result.effort.identity_bytes, prepared.batch_identity_.size())) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  if (!request.selected_identity.empty() &&
      request.selected_identity != prepared.batch_identity_) {
    result.status = Status::Stale;
    return result;
  }
  if (!plan_events(kernel, request, prepared, result)) {
    return result;
  }
  prepared.prepared_ = result;
  if (!make_journal(request, prepared)) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  result.effort.journal_bytes = prepared.journal_.size();
  result.effort.journal_work = prepared.journal_.size();
  prepared.prepared_ = result;
  prepared.prepared_.effort.journal_work = prepared.journal_.size();
  prepared.prepared_.effort.journal_bytes = prepared.journal_.size();
  prepared.ready_ = true;
  result = prepared.prepared_;
  result.status = Status::Prepared;
  prepared.prepared_.status = Status::Prepared;
  output = std::move(prepared);
  return result;
}

Result publish_internal(Kernel &kernel, const Request &request,
                        Prepared &prepared, bool atomic) {
  Result result = prepared.prepared_;
  result.result = ApplyResult::NoOp;
  if (!prepared.ready_ || !valid(request) || !same_request(request, prepared)) {
    result.status = Status::Invalid;
    return result;
  }
  if (prepared.spent_) {
    result.status = Status::AlreadyApplied;
    return result;
  }
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Status::UnsupportedProof;
    return result;
  }
  if (kernel.proof && !atomic) {
    result.status = Status::UnsupportedProofSplit;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = kernel_terminating(kernel) ? Status::Interrupted
                                                : Status::Inapplicable;
    return result;
  }
  if (kernel.next_clause_id != prepared.starting_clause_id_ ||
      kernel.arena_generation != prepared.arena_generation_ ||
      kernel.nvars != prepared.starting_nvars_ ||
      kernel.external_to_internal.size() != prepared.starting_extent_) {
    result.status = Status::Stale;
    return result;
  }

  uint64_t validation_work = 0;
  bool validation_interrupted = false;
  const auto validation_charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      validation_interrupted = true;
      return false;
    }
    validation_work += amount;
    return true;
  };
  const auto checked = gf3_gaussian_certificate::validate(
      prepared.owner_.theory, prepared.bridge_, prepared.certificate_,
      request.certificate, validation_charge);
  result.effort.validation_work = validation_work;
  if (checked.status != gf3_gaussian_certificate::Status::Complete) {
    result.status = validation_interrupted
                        ? Status::Interrupted
                        : certificate_status(checked.status);
    return result;
  }
  const auto live = gf3_affine_live_validate_owner(
      kernel, owner_request(request), prepared.owner_);
  if (!add(result.effort.revalidation_work, live.recognition_work) ||
      !add(result.effort.revalidation_work, live.revalidation_work)) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  if (live.status != Gf3AffineLiveStatus::Applied) {
    result.status = owner_status(live.status);
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    return result;
  }

  result.published_units.reserve(result.effort.installed_clauses);

  kernel.close_startup_certificate();
  for (size_t index = 0; index < prepared.certificate_.events.size(); ++index) {
    const auto &event = prepared.certificate_.events[index];
    if (++kernel.next_clause_id != prepared.event_ids_[index]) std::abort();
    if (kernel.proof &&
        !kernel.proof->add_external_rup(
            kernel.next_clause_id,
            event.clause.empty() ? nullptr : event.clause.data(),
            event.clause.size()))
      std::abort();
    if (!prepared.install_[index]) continue;
    if (event.clause.empty()) {
      kernel.inconsistent = true;
      result.contradiction = true;
      continue;
    }
    if (event.clause.size() != 1) std::abort();
    const Lit unit = kernel_internalize(kernel, event.clause.front());
    if (unit == INVALID_LIT || kernel_var_retired(kernel, var_of(unit)) ||
        kernel.value(unit))
      std::abort();
    kernel.unit_ids[var_of(unit)] = kernel.next_clause_id;
    kernel_assign_unit(kernel, unit);
    result.published_units.push_back(event.clause.front());
  }
  if (std::fwrite(prepared.journal_.data(), 1, prepared.journal_.size(),
                  request.records) != prepared.journal_.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  prepared.spent_ = true;
  result.status = Status::Applied;
  result.result = result.contradiction ? ApplyResult::Unsat : ApplyResult::Ok;
  result.effort.journal_bytes = prepared.journal_.size();
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  return result;
}

Result prepare(Kernel &kernel, const Request &request, Prepared &output) {
  return prepare_internal(kernel, request, output, false);
}

Result inspect(Kernel &kernel, const Request &request, Prepared &output) {
  return prepare_internal(kernel, request, output, true);
}

Result apply(Kernel &kernel, const Request &request, Prepared &prepared) {
  return publish_internal(kernel, request, prepared, false);
}

Result apply(Kernel &kernel, const Request &request) {
  Prepared prepared;
  Result result = prepare_internal(kernel, request, prepared, true);
  if (result.status != Status::Prepared) return result;
  return publish_internal(kernel, request, prepared, true);
}

} // namespace tactix::gf3_gaussian_live

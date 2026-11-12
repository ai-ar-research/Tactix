#include "rules/semantic_extension/gf3_general_row_live.hpp"

#include "strategy/action_status_mapping.hpp"

#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_certificate_common.hpp"
#include "rules/semantic_extension/gf3_live_common.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"
#include "rules/semantic_extension/live_adapter.hpp"
#include "rules/structural_text.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <set>

namespace tactix {
namespace {

using ClauseKey = std::vector<int32_t>;
using Meter = InterruptibleWork;
using gf3_certificate_common::canonical;
using gf3_live_common::append_reference;
using gf3_live_common::append_u8;
using gf3_live_common::append_u32;
using gf3_live_common::append_u64;
using gf3_live_common::owner_request;

Gf3GeneralRowLiveStatus owner_status(Gf3AffineLiveStatus status) {
  if (const auto failure =
          strategy::map_standard_action_failure<Gf3GeneralRowLiveStatus>(
              status))
    return *failure;
  switch (status) {
  case Gf3AffineLiveStatus::Applied:
    return Gf3GeneralRowLiveStatus::Ready;
  case Gf3AffineLiveStatus::NoStructure:
  case Gf3AffineLiveStatus::NoConsequence:
    return Gf3GeneralRowLiveStatus::NoStructure;
  case Gf3AffineLiveStatus::Duplicate:
    return Gf3GeneralRowLiveStatus::Invalid;
  default: std::abort();
  }
}

Gf3GeneralRowLiveStatus planner_status(gf3_general_row::Status status) {
  using S = gf3_general_row::Status;
  switch (status) {
  case S::Planned: return Gf3GeneralRowLiveStatus::Ready;
  case S::NoConsequence: return Gf3GeneralRowLiveStatus::NoConsequence;
  case S::Duplicate: return Gf3GeneralRowLiveStatus::Duplicate;
  case S::LimitExceeded:
    return Gf3GeneralRowLiveStatus::ResourceExhausted;
  case S::Interrupted: return Gf3GeneralRowLiveStatus::Interrupted;
  case S::InvalidTheory:
  case S::InvalidBridge: return Gf3GeneralRowLiveStatus::Stale;
  case S::InvalidRequest:
  case S::InvalidCertificate:
  case S::Uncertifiable: return Gf3GeneralRowLiveStatus::Invalid;
  }
  std::abort();
}

bool count_reference(uint64_t maximum, uint64_t &total,
                     const gf3_affine::SourceReference &reference,
                     const Gf3AffineOwnerSnapshot &owner) {
  if (!reference.occurrence || reference.occurrence > owner.sources.size())
    return false;
  const auto &clause = owner.sources[reference.occurrence - 1].clause;
  return clause.size() <= (UINT64_MAX - 24) / 4 &&
         add_bounded(24 + 4 * clause.size(), maximum, total);
}

bool subsumed(const std::vector<ClauseKey> &live, const ClauseKey &candidate) {
  for (const auto &clause : live)
    if (std::includes(candidate.begin(), candidate.end(), clause.begin(),
                      clause.end()))
      return true;
  return false;
}

bool make_journal(const Gf3GeneralRowLiveRequest &request,
                  const gf3_general_row::Plan &plan,
                  const std::string &identity,
                  const std::vector<uint64_t> &ids,
                  JournalText &journal) {
  journal = JournalText(request.records);
  const uint64_t maximum = request.limits.maximum_journal_bytes;
  const auto prefix = [&](auto &out) {
    out("{\"action\":", request.action_id, ",\"semantic\":",
        request.semantic_id);
  };
  if (!journal.append(maximum, prefix,
                      ",\"kind\":\"gf3-general-row-begin\","
                      "\"identity\":\"", identity, "\","
                      "\"outcome\":", plan.request.outcome, ",\"tuple\":",
                      plan.request.tuple, ",\"clause\":[",
                      journal_list(plan.consequence), "]}\n"))
    return false;
  for (size_t i = 0; i < plan.events.size(); ++i) {
    const char *role = plan.events[i].role ==
                               gf3_general_row::EventRole::Consequence
                           ? "consequence"
                           : "auxiliary";
    if (!journal.append(maximum, prefix,
                        ",\"kind\":\"gf3-general-row-rup\",\"event\":", i,
                        ",\"clause_id\":", ids[i], ",\"role\":\"", role,
                        "\",\"clause\":[", journal_list(plan.events[i].clause),
                        "]}\n"))
      return false;
  }
  return journal.append(maximum, prefix,
                        ",\"kind\":\"gf3-general-row-complete\"}\n");
}

struct Preflight {
  Gf3GeneralRowLiveResult result;
  std::vector<uint64_t> ids;
  JournalText journal;
};

Preflight preflight(Kernel &kernel, const Gf3GeneralRowLiveRequest &request) {
  Preflight output;
  auto &result = output.result;
  if (!request.limits.valid() || !request.action_id || !request.semantic_id ||
      !request.records)
    return output;
  if (!live_admitted(kernel,
                     request.retained_owner ? gf3_live_common::owner_guard
                                            : gf3_live_common::root_ready,
                     result.status))
    return output;
  if (kernel.external_to_internal.size() >
      request.limits.maximum_external_extent) {
    result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
    return output;
  }

  if (request.retained_owner) {
    result.owner = *request.retained_owner;
    result.status = gf3_general_row_live_validate_owner(
        kernel, request.limits, result.owner, result.effort);
  } else {
    result.status = gf3_general_row_live_capture(
        kernel, request.limits, result.owner, result.effort);
  }
  if (result.status != Gf3GeneralRowLiveStatus::Ready) return output;

  if (!gf3_general_row_live_identity(result.owner, request.selection,
                                     request.limits.maximum_identity_bytes,
                                     result.identity)) {
    result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
    return output;
  }
  result.effort.identity_bytes = result.identity.size();
  if (!request.selected_identity.empty() &&
      request.selected_identity != result.identity) {
    result.status = Gf3GeneralRowLiveStatus::Stale;
    return output;
  }

  Meter planning{kernel, request.limits.planner.maximum_work};
  auto planned = gf3_general_row::build(
      result.owner.affine.theory, result.owner.bridge, request.selection,
      request.limits.planner,
      [&](uint64_t amount) { return planning.charge(amount); });
  result.effort.planning_work = planning.used;
  if (planned.status != gf3_general_row::Status::Planned) {
    result.status = planning.interrupted ? Gf3GeneralRowLiveStatus::Interrupted
                                         : planner_status(planned.status);
    return output;
  }
  result.plan = std::move(planned.plan);

  Meter duplicate{kernel, request.limits.maximum_duplicate_scan_work};
  std::vector<ClauseKey> live;
  if (!gf3_live_common::scan_live_external_clauses(
          kernel, duplicate, [&](ClauseKey clause) {
            live.push_back(canonical(std::move(clause)));
          })) {
    result.effort.duplicate_scan_work = duplicate.used;
    result.status = duplicate.interrupted
                        ? Gf3GeneralRowLiveStatus::Interrupted
                        : Gf3GeneralRowLiveStatus::ResourceExhausted;
    return output;
  }
  result.effort.duplicate_scan_work = duplicate.used;
  if (subsumed(live, canonical(result.plan.consequence))) {
    result.status = Gf3GeneralRowLiveStatus::Duplicate;
    return output;
  }

  if (result.plan.events.empty() ||
      result.plan.events.back().role !=
          gf3_general_row::EventRole::Consequence ||
      result.plan.events.back().clause != result.plan.consequence ||
      result.plan.events.size() > request.limits.maximum_proof_additions ||
      kernel.next_clause_id >
          uint64_t(INT64_MAX) - result.plan.events.size()) {
    result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
    return output;
  }
  uint64_t next_id = kernel.next_clause_id;
  for (const auto &event : result.plan.events) {
    if (result.effort.proof_literal_occurrences >
            request.limits.maximum_proof_literal_occurrences ||
        event.clause.size() >
            request.limits.maximum_proof_literal_occurrences -
                result.effort.proof_literal_occurrences) {
      result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
      return output;
    }
    result.effort.proof_literal_occurrences += event.clause.size();
    ++result.effort.proof_additions;
    output.ids.push_back(++next_id);
  }
  if (result.plan.consequence.size() >= 3) {
    result.effort.arena_words =
        result.plan.consequence.size() + CLAUSE_HEADER_WORDS;
    if (result.effort.arena_words > request.limits.maximum_arena_words ||
        kernel.arena.size() >= INVALID_REF ||
        result.effort.arena_words >= INVALID_REF - kernel.arena.size()) {
      result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
      return output;
    }
  }

  Gf3GeneralRowLiveEffort checked_effort;
  const auto checked_status = gf3_general_row_live_validate_owner(
      kernel, request.limits, result.owner, checked_effort);
  result.effort.validation_work = checked_effort.bridge_work;
  result.effort.revalidation_work = checked_effort.capture_work;
  if (checked_status != Gf3GeneralRowLiveStatus::Ready) {
    result.status = checked_status;
    return output;
  }
  if (!make_journal(request, result.plan, result.identity, output.ids,
                    output.journal)) {
    result.status = Gf3GeneralRowLiveStatus::ResourceExhausted;
    return output;
  }
  result.effort.journal_bytes = output.journal.size();
  result.status = Gf3GeneralRowLiveStatus::Ready;
  return output;
}

} // namespace

bool Gf3GeneralRowLiveLimits::valid() const {
  return recognition.valid() && planner.bridge.valid() && planner.valid() &&
         maximum_capture_work && maximum_duplicate_scan_work &&
         maximum_revalidation_work && maximum_proof_additions &&
         maximum_proof_literal_occurrences && maximum_arena_words &&
         maximum_journal_bytes && maximum_identity_bytes &&
         maximum_external_extent;
}

Gf3GeneralRowLiveStatus gf3_general_row_live_capture(
    Kernel &kernel, const Gf3GeneralRowLiveLimits &limits,
    Gf3GeneralRowOwner &owner, Gf3GeneralRowLiveEffort &effort) {
  if (!limits.valid()) return Gf3GeneralRowLiveStatus::Invalid;
  const auto captured = gf3_affine_live_capture_owner(
      kernel, owner_request(limits), owner.affine);
  effort.source_clauses = captured.source_clauses;
  effort.source_literals = captured.source_literals;
  effort.capture_work = captured.recognition_work;
  if (captured.status != Gf3AffineLiveStatus::Applied)
    return owner_status(captured.status);
  Meter work{kernel, limits.planner.bridge.maximum_work};
  auto bridged = gf3_gaussian_bridge::build(
      owner.affine.theory, limits.planner.bridge,
      [&](uint64_t amount) { return work.charge(amount); });
  effort.bridge_work = work.used;
  if (bridged.status != gf3_gaussian_bridge::Status::Complete)
    return work.interrupted ? Gf3GeneralRowLiveStatus::Interrupted
           : bridged.status == gf3_gaussian_bridge::Status::LimitExceeded
               ? Gf3GeneralRowLiveStatus::ResourceExhausted
               : Gf3GeneralRowLiveStatus::Invalid;
  owner.bridge = std::move(bridged.plan);
  return Gf3GeneralRowLiveStatus::Ready;
}

Gf3GeneralRowLiveStatus gf3_general_row_live_validate_owner(
    Kernel &kernel, const Gf3GeneralRowLiveLimits &limits,
    const Gf3GeneralRowOwner &owner, Gf3GeneralRowLiveEffort &effort) {
  if (!limits.valid()) return Gf3GeneralRowLiveStatus::Invalid;
  const auto validated = gf3_affine_live_validate_owner(
      kernel, owner_request(limits), owner.affine);
  effort.source_clauses = validated.source_clauses;
  effort.source_literals = validated.source_literals;
  effort.capture_work = validated.recognition_work +
                        validated.revalidation_work;
  if (validated.status != Gf3AffineLiveStatus::Applied)
    return owner_status(validated.status);
  Meter work{kernel, limits.planner.bridge.maximum_work};
  const auto checked = gf3_gaussian_bridge::validate(
      owner.affine.theory, owner.bridge, limits.planner.bridge,
      [&](uint64_t amount) { return work.charge(amount); });
  effort.bridge_work = work.used;
  if (checked.status != gf3_gaussian_bridge::Status::Complete)
    return work.interrupted ? Gf3GeneralRowLiveStatus::Interrupted
           : checked.status == gf3_gaussian_bridge::Status::LimitExceeded
               ? Gf3GeneralRowLiveStatus::ResourceExhausted
               : Gf3GeneralRowLiveStatus::Stale;
  return Gf3GeneralRowLiveStatus::Ready;
}

bool gf3_general_row_live_identity(const Gf3GeneralRowOwner &owner,
                                   const gf3_general_row::Request &request,
                                   uint64_t maximum_bytes,
                                   std::string &identity) {
  if (request.outcome >= owner.bridge.gaussian.outcomes.size()) return false;
  const auto &outcome = owner.bridge.gaussian.outcomes[request.outcome];
  if (outcome.kind != gf3_gaussian::RowOutcomeKind::Derived ||
      !outcome.derivation ||
      outcome.derivation > owner.bridge.gaussian.derivations.size())
    return false;
  const auto &row = owner.bridge.gaussian.derivations[outcome.derivation - 1];
  if (maximum_bytes < 5) return false;
  const uint64_t maximum_raw = (maximum_bytes - 5) / 2;
  uint64_t required = 0;
  if (!add_bounded(4 + 8 + 4 + 1 + 8, maximum_raw, required) ||
      row.row.size() > (UINT64_MAX - required) / 5 ||
      !add_bounded(5 * row.row.size(), maximum_raw, required) ||
      !add_bounded(8, maximum_raw, required))
    return false;
  for (const auto &source : row.provenance) {
    if (!source.source_id ||
        source.source_id > owner.bridge.authentication.size() ||
        !add_bounded(8 + 1 + 4 + 8, maximum_raw, required))
      return false;
    const auto &auth = owner.bridge.authentication[source.source_id - 1];
    for (const auto &domain : auth.domains) {
      if (!add_bounded(4, maximum_raw, required) ||
          !count_reference(maximum_raw, required, domain.at_least_one,
                           owner.affine))
        return false;
      for (const auto &amo : domain.at_most_one)
        if (!count_reference(maximum_raw, required, amo, owner.affine))
          return false;
    }
    if (!add_bounded(8, maximum_raw, required)) return false;
    for (const auto &tuple : auth.forbidden_tuples)
      if (!add_bounded(8, maximum_raw, required) ||
          !count_reference(maximum_raw, required, tuple.premise,
                           owner.affine))
        return false;
  }
  std::string bytes;
  bytes.reserve(static_cast<size_t>(required));
  append_u32(bytes, request.outcome);
  append_u64(bytes, request.tuple);
  append_u32(bytes, outcome.derivation);
  append_u8(bytes, row.rhs);
  append_u64(bytes, row.row.size());
  for (const auto &term : row.row) {
    append_u32(bytes, term.variable);
    append_u8(bytes, term.coefficient);
  }
  append_u64(bytes, row.provenance.size());
  for (const auto &source : row.provenance) {
    if (!source.source_id || source.source_id > owner.bridge.authentication.size())
      return false;
    append_u64(bytes, source.source_id);
    append_u8(bytes, source.coefficient);
    const auto &auth = owner.bridge.authentication[source.source_id - 1];
    append_u32(bytes, auth.equation_index);
    append_u64(bytes, auth.domains.size());
    for (const auto &domain : auth.domains) {
      append_u32(bytes, domain.domain);
      if (!append_reference(bytes, domain.at_least_one, owner.affine))
        return false;
      for (const auto &amo : domain.at_most_one)
        if (!append_reference(bytes, amo, owner.affine)) return false;
    }
    append_u64(bytes, auth.forbidden_tuples.size());
    for (const auto &tuple : auth.forbidden_tuples) {
      append_u64(bytes, tuple.tuple);
      if (!append_reference(bytes, tuple.premise, owner.affine)) return false;
    }
  }
  identity = gf3_live_common::hex("g3r1:", bytes, true);
  return !identity.empty() && identity.size() <= maximum_bytes;
}

Gf3GeneralRowLiveResult gf3_general_row_live_inspect(
    Kernel &kernel, const Gf3GeneralRowLiveRequest &request) {
  return preflight(kernel, request).result;
}

Gf3GeneralRowLiveResult gf3_general_row_live_apply(
    Kernel &kernel, const Gf3GeneralRowLiveRequest &request) {
  Preflight prepared = preflight(kernel, request);
  auto &result = prepared.result;
  if (result.status != Gf3GeneralRowLiveStatus::Ready) return result;
  if (kernel_terminating(kernel)) {
    result.status = Gf3GeneralRowLiveStatus::Interrupted;
    return result;
  }

  kernel.close_startup_certificate();
  result.proof_clause_ids.reserve(result.plan.events.size());
  for (size_t index = 0; index < result.plan.events.size(); ++index) {
    const auto &event = result.plan.events[index];
    const auto internal = internal_clause_or_abort(kernel, event.clause);
    const uint64_t id = ++kernel.next_clause_id;
    if (id != prepared.ids[index]) std::abort();
    if (kernel.proof)
      kernel.proof->add_rup(id, internal.data(), internal.size());
    result.proof_clause_ids.push_back(id);
    if (index + 1 != result.plan.events.size()) continue;
    bool relevant_to_trail = false;
    if (kernel.level) {
      uint32_t unassigned = 0;
      bool satisfied = false;
      for (Lit literal : internal) {
        const int8_t value = kernel.value(literal);
        satisfied |= value > 0;
        unassigned += value == 0;
      }
      relevant_to_trail = !satisfied && unassigned <= 1;
    }
    install_redundant_clause(kernel, internal, id);
    if (relevant_to_trail)
      kernel_rearm_theory_clause(kernel, internal.data(), internal.size());
    result.installed_clause_id = id;
  }
  prepared.journal.write(request.records);
  std::set<int32_t> semantic;
  for (const auto &term : result.plan.duplicate_key.row) {
    const auto &domain = result.owner.affine.theory.domains[term.variable - 1];
    semantic.insert(domain.values.begin(), domain.values.end());
  }
  result.semantic_variables.assign(semantic.begin(), semantic.end());
  result.status = Gf3GeneralRowLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  return result;
}

} // namespace tactix

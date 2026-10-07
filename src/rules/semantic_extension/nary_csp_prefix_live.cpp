#include "rules/semantic_extension/nary_csp_prefix_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace tactix {
namespace {

namespace prefix = nary_csp_prefix;
using ClauseKey = std::vector<int32_t>;

struct Meter {
  const Kernel &kernel;
  uint64_t limit = 0;
  uint64_t used = 0;
  bool interrupted = false;
  bool exhausted = false;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (used > limit || amount > limit - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
};

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(variable(first), first) <
         std::pair(variable(second), second);
}

ClauseKey normalized(ClauseKey key) {
  std::sort(key.begin(), key.end());
  return key;
}

bool root_guard(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid &&
         std::all_of(kernel.trail.begin(), kernel.trail.end(),
                     [&](Lit literal) {
                       return kernel.assigned[var_of(literal)].level == 0;
                     }) &&
         !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

bool common_limits_valid(const NaryCspPrefixLiveRequest &request) {
  return request.maximum_source_clauses &&
         request.maximum_source_literals &&
         request.maximum_recognition_work && request.maximum_domains >= 3 &&
         request.maximum_domain_values >= 2 &&
         request.maximum_forbidden_tuples &&
         request.maximum_prefix_arity >= 2 &&
         request.maximum_definition_clause_literals &&
         request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences &&
         request.maximum_external_extent;
}

bool apply_limits_valid(const NaryCspPrefixLiveRequest &request) {
  return common_limits_valid(request) && request.maximum_proof_work &&
         request.maximum_journal_bytes && request.maximum_extent_growth &&
         request.action_id && request.semantic_id && request.records;
}

enum class DomainClauseKind : uint8_t { Alo, Amo };

struct DomainDescriptor {
  DomainClauseKind kind = DomainClauseKind::Alo;
  uint32_t domain = 0;
  uint32_t first = 0;
  uint32_t second = 0;
};

struct ExpectedImage {
  std::map<ClauseKey, DomainDescriptor> domain_clauses;
  std::map<int32_t, prefix::DomainValue> tuple_literals;
};

bool build_expected(const std::vector<std::vector<int32_t>> &domains,
                    const NaryCspPrefixLiveRequest &request,
                    ExpectedImage &output, Meter &meter) {
  output = {};
  if (domains.size() < 3 || domains.size() > request.maximum_domains)
    return false;
  std::set<uint32_t> variables;
  for (size_t domain = 0; domain < domains.size(); ++domain) {
    const auto &values = domains[domain];
    if (values.size() < 2 || values.size() > request.maximum_domain_values)
      return false;
    for (size_t value = 0; value < values.size(); ++value) {
      const int32_t literal = values[value];
      if (!literal || literal == INT32_MIN ||
          (value && !literal_less(values[value - 1], literal)))
        return false;
      // Charge before allocating the expected literal and clause maps. This
      // quadratic image is part of recognition, including on a failed capture.
      if (!meter.charge(1) ||
          !variables.insert(variable(literal)).second ||
          !output.tuple_literals
               .emplace(-literal,
                        prefix::DomainValue{static_cast<uint32_t>(domain),
                                            static_cast<uint32_t>(value)})
               .second)
        return false;
    }
    // Clause construction uses the same one-plus-literals charge as scanning.
    if (!meter.charge(uint64_t{1} + values.size()) ||
        !output.domain_clauses
             .emplace(normalized(values),
                      DomainDescriptor{DomainClauseKind::Alo,
                                       static_cast<uint32_t>(domain), 0, 0})
             .second)
      return false;
    for (size_t first = 0; first < values.size(); ++first)
      for (size_t second = first + 1; second < values.size(); ++second)
        if (!meter.charge(3) ||
            !output.domain_clauses
                 .emplace(normalized({-values[first], -values[second]}),
                          DomainDescriptor{
                              DomainClauseKind::Amo,
                              static_cast<uint32_t>(domain),
                              static_cast<uint32_t>(first),
                              static_cast<uint32_t>(second)})
                 .second)
          return false;
  }
  return true;
}

enum class CaptureStatus : uint8_t {
  Complete,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  Invalid,
};

CaptureStatus stopped_capture(const Meter &meter) {
  if (meter.interrupted) return CaptureStatus::Interrupted;
  if (meter.exhausted) return CaptureStatus::ResourceExhausted;
  return CaptureStatus::Invalid;
}

NaryCspPrefixLiveStatus live_status(CaptureStatus status) {
  switch (status) {
  case CaptureStatus::Complete:
    return NaryCspPrefixLiveStatus::Applied;
  case CaptureStatus::NoStructure:
    return NaryCspPrefixLiveStatus::NoStructure;
  case CaptureStatus::ResourceExhausted:
    return NaryCspPrefixLiveStatus::ResourceExhausted;
  case CaptureStatus::Interrupted:
    return NaryCspPrefixLiveStatus::Interrupted;
  case CaptureStatus::Invalid:
    return NaryCspPrefixLiveStatus::Invalid;
  }
  std::abort();
}

struct FoundClause {
  ClauseKey literals;
  uint64_t native_id = 0;
  bool tuple = false;
  DomainDescriptor domain;
  std::vector<uint32_t> coordinates;
};

CaptureStatus capture_slice(
    const Kernel &kernel, const std::vector<std::vector<int32_t>> &domains,
    const NaryCspPrefixLiveRequest &request, Meter &meter,
    NaryCspPrefixOwnerSnapshot &output) {
  ExpectedImage expected;
  if (!build_expected(domains, request, expected, meter))
    return stopped_capture(meter);
  for (const auto &domain : domains)
    for (int32_t external : domain) {
      if (!meter.charge(1)) return stopped_capture(meter);
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)) ||
          kernel.value(literal))
        return CaptureStatus::NoStructure;
    }

  std::map<ClauseKey, FoundClause> found_domains;
  std::map<std::vector<uint32_t>, FoundClause> found_tuples;
  uint64_t source_literals = 0;
  auto retain = [&](FoundClause clause) {
    const uint64_t retained = found_domains.size() + found_tuples.size();
    if (retained >= request.maximum_source_clauses ||
        clause.literals.size() >
            request.maximum_source_literals - source_literals)
      return CaptureStatus::ResourceExhausted;
    source_literals += clause.literals.size();
    if (clause.tuple) {
      if (!found_tuples.emplace(clause.coordinates, std::move(clause)).second)
        return CaptureStatus::Invalid;
    } else if (!found_domains.emplace(clause.literals, std::move(clause)).second) {
      return CaptureStatus::Invalid;
    }
    return CaptureStatus::Complete;
  };
  CaptureStatus failure = CaptureStatus::Complete;
  auto observe = [&](const Lit *literals, uint32_t size, uint64_t native_id) {
    if (!meter.charge(uint64_t{1} + size)) {
      failure = stopped_capture(meter);
      return false;
    }
    ClauseKey external;
    external.reserve(size);
    bool assigned = false;
    for (uint32_t index = 0; index < size; ++index) {
      const Var var = var_of(literals[index]);
      if (kernel_var_retired(kernel, var)) {
        failure = CaptureStatus::Invalid;
        return false;
      }
      assigned |= kernel.value(literals[index]) != 0;
      external.push_back(kernel_externalize(kernel, literals[index]));
    }
    // Expected ALO, AMO, and tuple clauses contain only selected domain
    // variables, which discovery requires to be unassigned.  An assigned
    // literal therefore proves that this whole clause is outside the exact
    // source image.  Never residualize it: literal/native-proof identities
    // remain exact across capture and revalidation.
    if (assigned) return true;
    external = normalized(std::move(external));
    const auto domain_clause = expected.domain_clauses.find(external);
    if (domain_clause != expected.domain_clauses.end()) {
      failure = retain(
          FoundClause{external, native_id, false, domain_clause->second, {}});
      return failure == CaptureStatus::Complete;
    }
    if (size != domains.size()) return true;
    std::vector<uint32_t> coordinates(domains.size(), UINT32_MAX);
    for (int32_t literal : external) {
      const auto found = expected.tuple_literals.find(literal);
      if (found == expected.tuple_literals.end()) return true;
      const auto [domain, value] = found->second;
      if (coordinates[domain] != UINT32_MAX) return true;
      coordinates[domain] = value;
    }
    if (std::find(coordinates.begin(), coordinates.end(), UINT32_MAX) !=
        coordinates.end())
      return true;
    failure = retain(FoundClause{external, native_id, true, {}, coordinates});
    return failure == CaptureStatus::Complete;
  };

  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1)) return stopped_capture(meter);
    if (!clause->garbage() &&
        !observe(clause->lits, clause->size, clause->id()))
      return failure;
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return stopped_capture(meter);
      if (!watch.binary() || first >= watch.other()) continue;
      const Lit pair[] = {first, watch.other()};
      const uint64_t native_id =
          kernel.binary_ids.tracks_values()
              ? kernel_binary_id(kernel, first, watch.other())
              : 0;
      if (!observe(pair, 2, native_id)) return failure;
    }

  if (found_domains.size() != expected.domain_clauses.size())
    return CaptureStatus::NoStructure;

  std::vector<FoundClause> ordered;
  ordered.reserve(found_domains.size() + found_tuples.size());
  for (auto &[key, clause] : found_domains) {
    (void)key;
    ordered.push_back(clause);
  }
  for (auto &[coordinates, clause] : found_tuples) {
    (void)coordinates;
    ordered.push_back(clause);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const FoundClause &a, const FoundClause &b) {
              if (a.literals != b.literals) return a.literals < b.literals;
              return a.native_id < b.native_id;
            });

  NaryCspPrefixOwnerSnapshot snapshot;
  snapshot.external_extent = kernel.external_to_internal.size();
  snapshot.source_literals = source_literals;
  std::map<ClauseKey, uint64_t> occurrences;
  for (size_t index = 0; index < ordered.size(); ++index) {
    const uint64_t occurrence = index + 1;
    if (!occurrences.emplace(ordered[index].literals, occurrence).second)
      return CaptureStatus::Invalid;
    snapshot.source.push_back(
        {occurrence, ordered[index].literals, ordered[index].native_id});
  }
  snapshot.relation.domains.resize(domains.size());
  for (size_t domain = 0; domain < domains.size(); ++domain) {
    auto &destination = snapshot.relation.domains[domain];
    destination.values = domains[domain];
    const size_t n = domains[domain].size();
    destination.at_most_one.assign(n * n, 0);
  }
  for (const auto &[key, descriptor] : expected.domain_clauses) {
    const auto occurrence = occurrences.find(key);
    if (occurrence == occurrences.end()) return CaptureStatus::NoStructure;
    auto &domain = snapshot.relation.domains[descriptor.domain];
    if (descriptor.kind == DomainClauseKind::Alo) {
      domain.at_least_one = occurrence->second;
    } else {
      const size_t n = domain.values.size();
      domain.at_most_one[size_t(descriptor.first) * n + descriptor.second] =
          occurrence->second;
      domain.at_most_one[size_t(descriptor.second) * n + descriptor.first] =
          occurrence->second;
    }
  }
  for (const auto &[coordinates, clause] : found_tuples) {
    const auto occurrence = occurrences.find(clause.literals);
    if (occurrence == occurrences.end()) return CaptureStatus::Invalid;
    snapshot.relation.forbidden.push_back({coordinates, occurrence->second});
  }
  output = std::move(snapshot);
  return CaptureStatus::Complete;
}

bool owner_payload_valid(const NaryCspPrefixOwnerSnapshot &owner,
                         const NaryCspPrefixLiveRequest &request,
                         Meter &meter) {
  if (!owner.external_extent || owner.external_extent > INT32_MAX ||
      owner.source.empty() ||
      owner.source.size() > request.maximum_source_clauses ||
      owner.source_literals > request.maximum_source_literals ||
      owner.relation.domains.size() < 3 ||
      owner.relation.domains.size() > request.maximum_domains ||
      owner.relation.forbidden.size() > request.maximum_forbidden_tuples)
    return false;
  uint64_t literals = 0;
  for (size_t index = 0; index < owner.source.size(); ++index) {
    const auto &premise = owner.source[index];
    if (!meter.charge(uint64_t{1} + premise.literals.size()) ||
        premise.occurrence != index + 1 || premise.literals.empty() ||
        premise.literals.size() > request.maximum_source_literals - literals)
      return false;
    literals += premise.literals.size();
  }
  return literals == owner.source_literals;
}

struct Journal {
  uint64_t maximum = 0;
  uint64_t work = 0;
  std::string bytes;

  bool append(const std::string &text) {
    if (text.size() > UINT64_MAX - work) return false;
    work += text.size();
    if (bytes.size() > maximum || text.size() > maximum - bytes.size())
      return false;
    bytes += text;
    return true;
  }
  bool integers(const std::vector<int32_t> &values) {
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i]))) return false;
    return true;
  }
  bool unsigneds(const std::vector<uint64_t> &values) {
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i]))) return false;
    return true;
  }
};

bool render_journal(const NaryCspPrefixLiveRequest &request,
                    const NaryCspPrefixOwnerSnapshot &owner,
                    const prefix::Plan &plan, uint64_t first_id,
                    uint64_t proof_work, Journal &journal) {
  if (!journal.append("{\"kind\":\"nary-csp-prefix-begin\",\"action\":" +
                      std::to_string(request.action_id) +
                      ",\"semantic\":" +
                      std::to_string(request.semantic_id) +
                      ",\"residual_domain\":" +
                      std::to_string(plan.selection.residual_domain) +
                      ",\"recognition\":\"exact-live-domain-tuple-slice\","
                      "\"maximum_source_clauses\":" +
                      std::to_string(request.maximum_source_clauses) +
                      ",\"maximum_source_literals\":" +
                      std::to_string(request.maximum_source_literals) +
                      ",\"maximum_recognition_work\":" +
                      std::to_string(request.maximum_recognition_work) +
                      ",\"maximum_domains\":" +
                      std::to_string(request.maximum_domains) +
                      ",\"maximum_domain_values\":" +
                      std::to_string(request.maximum_domain_values) +
                      ",\"maximum_forbidden_tuples\":" +
                      std::to_string(request.maximum_forbidden_tuples) +
                      ",\"maximum_prefix_arity\":" +
                      std::to_string(request.maximum_prefix_arity) +
                      ",\"maximum_proof_work\":" +
                      std::to_string(request.maximum_proof_work) +
                      ",\"maximum_proof_additions\":" +
                      std::to_string(request.maximum_proof_additions) +
                      ",\"maximum_proof_literal_occurrences\":" +
                      std::to_string(
                          request.maximum_proof_literal_occurrences) +
                      "}\n"))
    return false;
  for (const auto &premise : owner.source) {
    if (!journal.append("{\"kind\":\"nary-csp-prefix-source\",\"action\":" +
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
    if (!journal.append("{\"kind\":\"nary-csp-prefix-domain\",\"action\":" +
                        std::to_string(request.action_id) +
                        ",\"domain\":" + std::to_string(domain) +
                        ",\"values\":[") ||
        !journal.integers(owner.relation.domains[domain].values) ||
        !journal.append("]}\n"))
      return false;
  }
  for (const auto &tuple : owner.relation.forbidden) {
    std::vector<uint64_t> coordinates(tuple.value_indices.begin(),
                                      tuple.value_indices.end());
    if (!journal.append("{\"kind\":\"nary-csp-prefix-tuple\",\"action\":" +
                        std::to_string(request.action_id) +
                        ",\"occurrence\":" +
                        std::to_string(tuple.occurrence) +
                        ",\"coordinates\":[") ||
        !journal.unsigneds(coordinates) || !journal.append("]}\n"))
      return false;
  }
  if (!journal.append(
          "{\"kind\":\"nary-csp-prefix-selection\",\"action\":" +
          std::to_string(request.action_id) + ",\"prefix\":["))
    return false;
  for (size_t index = 0; index < plan.selection.prefix.size(); ++index) {
    const auto selected = plan.selection.prefix[index];
    if (!journal.append((index ? "," : "") +
                        std::string("{\"domain\":") +
                        std::to_string(selected.domain) + ",\"value\":" +
                        std::to_string(selected.value) + "}"))
      return false;
  }
  if (!journal.append("],\"prefix_literals\":[") ||
      !journal.integers(plan.prefix_literals) ||
      !journal.append("],\"matching_forbidden_premises\":[") ||
      !journal.unsigneds(plan.matching_forbidden_premises) ||
      !journal.append("],\"excluded_residual_values\":[") ||
      !journal.integers(plan.excluded_residual_values) ||
      !journal.append("]}\n"))
    return false;
  for (size_t index = 0; index < plan.additions.size(); ++index) {
    const auto &addition = plan.additions[index];
    if (!journal.append("{\"kind\":\"nary-csp-prefix-addition\",\"action\":" +
                        std::to_string(request.action_id) + ",\"id\":" +
                        std::to_string(first_id + index) + ",\"proof\":" +
                        std::to_string(static_cast<unsigned>(addition.proof)) +
                        ",\"pivot\":" + std::to_string(addition.pivot) +
                        ",\"clause\":[") ||
        !journal.integers(addition.clause) ||
        !journal.append("],\"antecedents\":["))
      return false;
    for (size_t antecedent = 0; antecedent < addition.antecedents.size();
         ++antecedent) {
      const auto &reference = addition.antecedents[antecedent];
      if (!journal.append(
              (antecedent ? "," : "") + std::string("{\"kind\":\"") +
              (reference.kind == prefix::ReferenceKind::SourceOccurrence
                   ? "source"
                   : "planned-addition") +
              "\",\"id\":" + std::to_string(reference.id) +
              ",\"native_proof_id\":" +
              std::to_string(reference.native_proof_id) + "}"))
        return false;
    }
    if (!journal.append("]}\n")) return false;
  }
  return journal.append(
             "{\"kind\":\"nary-csp-prefix-complete\",\"action\":" +
             std::to_string(request.action_id) + ",\"additions\":" +
             std::to_string(plan.additions.size()) + ",\"proof_work\":" +
             std::to_string(proof_work) +
             ",\"projection\":\"omit-exact-and-extension\","
             "\"extension_variable\":" +
             std::to_string(plan.projection.extension_variable) +
             ",\"definition_inputs\":[") &&
         journal.integers(plan.projection.conjuncts) &&
         journal.append(
             "],\"reconstruction_witness_required\":false,"
             "\"source_lifetime\":\"retained\","
             "\"addition_lifetime\":\"ordinary-live-clauses\","
             "\"planner_work\":" +
             std::to_string(plan.consumed_work) +
             ",\"source_clauses\":" +
             std::to_string(plan.consumed_source_clauses) +
             ",\"source_literals\":" +
             std::to_string(plan.consumed_source_literals) +
             ","
             "\"first_published_clause_id\":" +
             std::to_string(first_id) + ",\"last_published_clause_id\":" +
             std::to_string(first_id + plan.additions.size() - 1) + "}\n");
}

NaryCspPrefixLiveStatus planning_status(const prefix::Plan &plan,
                                        const Meter &meter) {
  if (meter.interrupted) return NaryCspPrefixLiveStatus::Interrupted;
  if (meter.exhausted || plan.status == prefix::Status::EffortExhausted ||
      plan.status == prefix::Status::BoundExceeded)
    return NaryCspPrefixLiveStatus::ResourceExhausted;
  switch (plan.status) {
  case prefix::Status::Planned:
    return NaryCspPrefixLiveStatus::Applied;
  case prefix::Status::NoConsequence:
    return NaryCspPrefixLiveStatus::NoConsequence;
  case prefix::Status::IncompleteTheory:
    return NaryCspPrefixLiveStatus::NoStructure;
  case prefix::Status::InvalidRequest:
    return NaryCspPrefixLiveStatus::Invalid;
  case prefix::Status::BoundExceeded:
  case prefix::Status::EffortExhausted:
    return NaryCspPrefixLiveStatus::ResourceExhausted;
  }
  return NaryCspPrefixLiveStatus::Invalid;
}

struct Prepared {
  NaryCspPrefixLiveResult result;
  NaryCspPrefixOwnerSnapshot owned;
  const NaryCspPrefixOwnerSnapshot *owner = nullptr;
  prefix::Plan plan;
  Journal journal;
  uint64_t first_id = 0;
};

std::vector<std::vector<int32_t>> owner_domains(
    const NaryCspPrefixOwnerSnapshot &owner) {
  std::vector<std::vector<int32_t>> result;
  result.reserve(owner.relation.domains.size());
  for (const auto &domain : owner.relation.domains)
    result.push_back(domain.values);
  return result;
}

void prepare(Kernel &kernel, const NaryCspPrefixLiveRequest &request,
             Prepared &prepared) {
  auto &result = prepared.result;
  if (!apply_limits_valid(request)) return;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = NaryCspPrefixLiveStatus::UnsupportedProof;
    return;
  }
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? NaryCspPrefixLiveStatus::Interrupted
                        : NaryCspPrefixLiveStatus::Inapplicable;
    return;
  }

  Meter meter{kernel, request.maximum_recognition_work};
  prepared.owner = request.retained_owner;
  const uint64_t scan_begin = meter.used;
  if (!prepared.owner) {
    const CaptureStatus captured =
        request.domains.empty()
            ? CaptureStatus::Invalid
            : capture_slice(kernel, request.domains, request, meter,
                            prepared.owned);
    if (captured != CaptureStatus::Complete) {
      result.source_scan_work = meter.used - scan_begin;
      result.recognition_work = meter.used;
      result.recognition_limit_exhausted = meter.exhausted;
      result.status = live_status(captured);
      return;
    }
    prepared.owner = &prepared.owned;
  } else {
    const auto retained_domains = owner_domains(*prepared.owner);
    if ((!request.domains.empty() && request.domains != retained_domains) ||
        prepared.owner->external_extent > kernel.external_to_internal.size() ||
        !owner_payload_valid(*prepared.owner, request, meter)) {
      result.source_scan_work = meter.used - scan_begin;
      result.recognition_work = meter.used;
      result.recognition_limit_exhausted = meter.exhausted;
      result.status = meter.interrupted
                          ? NaryCspPrefixLiveStatus::Interrupted
                          : meter.exhausted
                                ? NaryCspPrefixLiveStatus::ResourceExhausted
                                : NaryCspPrefixLiveStatus::Invalid;
      return;
    }
  }
  result.source_scan_work = meter.used - scan_begin;
  result.source_clauses = prepared.owner->source.size();
  result.source_literals = prepared.owner->source_literals;

  const uint64_t extent = kernel.external_to_internal.size();
  if (extent >= INT32_MAX) {
    result.status = NaryCspPrefixLiveStatus::ResourceExhausted;
    result.recognition_work = meter.used;
    return;
  }
  const prefix::Bounds bounds{
      request.maximum_source_clauses,
      request.maximum_source_literals,
      request.maximum_domains,
      request.maximum_domain_values,
      request.maximum_forbidden_tuples,
      request.maximum_prefix_arity,
      request.maximum_definition_clause_literals,
      request.maximum_proof_additions,
      request.maximum_proof_literal_occurrences,
      static_cast<int32_t>(extent + 1)};
  const uint64_t planning_begin = meter.used;
  prepared.plan = prefix::plan_prefix(
      prepared.owner->source, prepared.owner->relation, request.selection,
      bounds, [&](uint64_t amount) { return meter.charge(amount); });
  result.planning_work = meter.used - planning_begin;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  if (prepared.plan.status != prefix::Status::Planned) {
    result.status = planning_status(prepared.plan, meter);
    return;
  }

  uint64_t proof_work = 0, literal_occurrences = 0, arena_words = 0;
  for (const auto &addition : prepared.plan.additions) {
    const uint64_t amount = uint64_t{1} + addition.clause.size() +
                            addition.antecedents.size();
    if (proof_work > request.maximum_proof_work ||
        amount > request.maximum_proof_work - proof_work ||
        literal_occurrences > request.maximum_proof_literal_occurrences ||
        addition.clause.size() >
            request.maximum_proof_literal_occurrences - literal_occurrences) {
      result.status = NaryCspPrefixLiveStatus::ResourceExhausted;
      return;
    }
    proof_work += amount;
    literal_occurrences += addition.clause.size();
    if (addition.clause.size() >= 3) {
      const uint64_t words = addition.clause.size() + CLAUSE_HEADER_WORDS;
      if (words > UINT64_MAX - arena_words) {
        result.status = NaryCspPrefixLiveStatus::ResourceExhausted;
        return;
      }
      arena_words += words;
    }
  }
  result.proof_work = proof_work;
  result.planned_additions = prepared.plan.additions.size();
  result.proof_literal_occurrences = literal_occurrences;
  result.extent_growth = 1;
  if (prepared.plan.additions.size() > request.maximum_proof_additions ||
      result.extent_growth > request.maximum_extent_growth ||
      extent + result.extent_growth > request.maximum_external_extent ||
      result.extent_growth > UINT32_MAX - kernel.nvars ||
      kernel.next_clause_id >
          uint64_t(INT64_MAX) - prepared.plan.additions.size() ||
      kernel.arena.size() >= INVALID_REF ||
      arena_words >= INVALID_REF - kernel.arena.size()) {
    result.status = NaryCspPrefixLiveStatus::ResourceExhausted;
    return;
  }

  prepared.first_id = kernel.next_clause_id + 1;
  prepared.journal = Journal{request.maximum_journal_bytes, 0, {}};
  if (!render_journal(request, *prepared.owner, prepared.plan,
                      prepared.first_id, proof_work, prepared.journal)) {
    result.journal_work = prepared.journal.work;
    result.status = NaryCspPrefixLiveStatus::ResourceExhausted;
    return;
  }
  result.journal_work = prepared.journal.work;
  result.journal_bytes = prepared.journal.bytes.size();

  const uint64_t revalidation_begin = meter.used;
  NaryCspPrefixOwnerSnapshot current;
  const auto domains = owner_domains(*prepared.owner);
  const CaptureStatus recaptured =
      capture_slice(kernel, domains, request, meter, current);
  result.revalidation_work = meter.used - revalidation_begin;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  if (recaptured != CaptureStatus::Complete) {
    result.status = recaptured == CaptureStatus::ResourceExhausted ||
                            recaptured == CaptureStatus::Interrupted
                        ? live_status(recaptured)
                        : NaryCspPrefixLiveStatus::Stale;
    return;
  }
  current.capture_work = prepared.owner->capture_work;
  // Namespace growth is not a source change.  Retained owners remain valid
  // across earlier conservative extensions as long as their exact clauses and
  // external identities recapture unchanged.
  current.external_extent = prepared.owner->external_extent;
  if (current != *prepared.owner) {
    result.status = NaryCspPrefixLiveStatus::Stale;
    return;
  }
  if (kernel_terminating(kernel)) {
    result.status = NaryCspPrefixLiveStatus::Interrupted;
    return;
  }

  result.status = NaryCspPrefixLiveStatus::Applied;
  for (int32_t literal : prepared.plan.prefix_literals)
    result.semantic_variables.push_back(
        {literal, NaryCspSemanticRole::PrefixDomainValue, false, {}});
  for (int32_t literal : prepared.plan.excluded_residual_values)
    result.semantic_variables.push_back(
        {literal, NaryCspSemanticRole::ExcludedResidualValue, false, {}});
  result.semantic_variables.push_back(
      {prepared.plan.projection.extension_variable,
       NaryCspSemanticRole::PrefixConjunction, true,
       prepared.plan.projection.conjuncts});
  for (const auto &addition : prepared.plan.additions)
    if (addition.proof == prefix::ProofStep::Rup)
      result.published_consequences.push_back(addition.clause);
}

} // namespace

NaryCspPrefixOwnerCaptureResult nary_csp_prefix_live_capture_owner(
    Kernel &kernel, const NaryCspPrefixLiveRequest &request,
    NaryCspPrefixOwnerSnapshot &output) {
  NaryCspPrefixOwnerCaptureResult result;
  if (!common_limits_valid(request) || request.domains.empty()) return result;
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? NaryCspPrefixLiveStatus::Interrupted
                        : NaryCspPrefixLiveStatus::Inapplicable;
    return result;
  }
  Meter meter{kernel, request.maximum_recognition_work};
  NaryCspPrefixOwnerSnapshot captured;
  const CaptureStatus status =
      capture_slice(kernel, request.domains, request, meter, captured);
  if (status != CaptureStatus::Complete) {
    result.status = live_status(status);
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    return result;
  }
  const uint64_t extent = kernel.external_to_internal.size();
  const prefix::Bounds bounds{
      request.maximum_source_clauses,
      request.maximum_source_literals,
      request.maximum_domains,
      request.maximum_domain_values,
      request.maximum_forbidden_tuples,
      request.maximum_prefix_arity,
      request.maximum_definition_clause_literals,
      request.maximum_proof_additions,
      request.maximum_proof_literal_occurrences,
      extent < INT32_MAX ? static_cast<int32_t>(extent + 1) : 0};
  const prefix::Plan checked = prefix::plan_prefix(
      captured.source, captured.relation, request.selection, bounds,
      [&](uint64_t amount) { return meter.charge(amount); });
  if (checked.status != prefix::Status::Planned &&
      checked.status != prefix::Status::NoConsequence) {
    result.status = planning_status(checked, meter);
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    return result;
  }
  captured.capture_work = meter.used;
  result.status = NaryCspPrefixLiveStatus::Applied;
  result.source_clauses = captured.source.size();
  result.source_literals = captured.source_literals;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  output = std::move(captured);
  return result;
}

NaryCspPrefixLiveResult nary_csp_prefix_live_preflight(
    Kernel &kernel, const NaryCspPrefixLiveRequest &request) {
  Prepared prepared;
  prepare(kernel, request, prepared);
  return std::move(prepared.result);
}

NaryCspPrefixLiveResult nary_csp_prefix_live_apply(
    Kernel &kernel, const NaryCspPrefixLiveRequest &request) {
  Prepared prepared;
  prepare(kernel, request, prepared);
  NaryCspPrefixLiveResult result = std::move(prepared.result);
  if (result.status != NaryCspPrefixLiveStatus::Applied) return result;

  kernel.close_startup_certificate();
  const Var fresh = kernel_new_extension_vars(kernel, 1);
  if (kernel_externalize(kernel, make_lit(fresh, false)) !=
      prepared.plan.projection.extension_variable)
    std::abort();
  std::vector<uint64_t> ids;
  ids.reserve(prepared.plan.additions.size());
  for (size_t index = 0; index < prepared.plan.additions.size(); ++index) {
    const auto &addition = prepared.plan.additions[index];
    std::vector<Lit> literals;
    literals.reserve(addition.clause.size());
    for (int32_t external : addition.clause) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
        std::abort();
      literals.push_back(literal);
    }
    const uint64_t id = ++kernel.next_clause_id;
    if (id != prepared.first_id + index) std::abort();
    if (kernel.proof) {
      if (addition.proof == prefix::ProofStep::Rup) {
        kernel.proof->add_rup(id, literals.data(), literals.size(), nullptr, 0);
      } else {
        std::vector<int64_t> hints;
        if (addition.proof == prefix::ProofStep::DefinitionReverseRat)
          for (const auto &reference : addition.antecedents) {
            if (reference.kind != prefix::ReferenceKind::PlannedAddition ||
                reference.id >= ids.size())
              std::abort();
            hints.push_back(-static_cast<int64_t>(ids[reference.id]));
          }
        kernel.proof->add_rat(id, literals.data(), literals.size(),
                              hints.empty() ? nullptr : hints.data(),
                              hints.size());
      }
    }
    ids.push_back(id);
    if (literals.size() == 1) {
      if (kernel.value(literals[0])) std::abort();
      kernel.unit_ids[var_of(literals[0])] = id;
      kernel_assign_unit(kernel, literals[0]);
    } else if (literals.size() == 2) {
      kernel_add_binary_clause(kernel, literals[0], literals[1], id);
    } else {
      const bool redundant = addition.proof == prefix::ProofStep::Rup;
      kernel_add_large_clause(kernel, literals.data(), literals.size(),
                              redundant,
                              redundant ? literals.size() : 0, id);
    }
  }
  if (std::fwrite(prepared.journal.bytes.data(), 1,
                  prepared.journal.bytes.size(), request.records) !=
          prepared.journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();

  result.result = ApplyResult::Ok;
  result.emitted_additions = prepared.plan.additions.size();
  result.first_published_clause_id = ids.front();
  result.last_published_clause_id = ids.back();
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  return result;
}

} // namespace tactix

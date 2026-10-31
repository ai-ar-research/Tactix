#include "rules/semantic_extension/nary_csp_relation_live.hpp"

#include "rules/semantic_extension/csp_physical_scan.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix {
namespace {

namespace relation = nary_csp_relation;
using ClauseKey = std::vector<int32_t>;

using Meter = InterruptibleWork;

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
         !kernel.dense && !kernel.probing.recording &&
         kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
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

NaryCspRelationCaptureStatus public_status(CaptureStatus status) {
  switch (status) {
  case CaptureStatus::Complete:
    return NaryCspRelationCaptureStatus::Applied;
  case CaptureStatus::NoStructure:
    return NaryCspRelationCaptureStatus::NoStructure;
  case CaptureStatus::ResourceExhausted:
    return NaryCspRelationCaptureStatus::ResourceExhausted;
  case CaptureStatus::Interrupted:
    return NaryCspRelationCaptureStatus::Interrupted;
  case CaptureStatus::Invalid:
    return NaryCspRelationCaptureStatus::Invalid;
  }
  std::abort();
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
  std::map<int32_t, relation::DomainValue> tuple_literals;
};

CaptureStatus build_expected(
    const std::vector<std::vector<int32_t>> &domains,
    const NaryCspRelationCaptureRequest &request, ExpectedImage &output,
    Meter &meter) {
  output = {};
  if (domains.size() < 3 || domains.size() > request.maximum_domains)
    return CaptureStatus::Invalid;
  std::set<uint32_t> variables;
  for (size_t domain = 0; domain < domains.size(); ++domain) {
    const auto &values = domains[domain];
    if (values.size() < 2 || values.size() > request.maximum_domain_values ||
        values.size() > std::numeric_limits<size_t>::max() / values.size())
      return CaptureStatus::Invalid;
    for (size_t value = 0; value < values.size(); ++value) {
      const int32_t literal = values[value];
      if (!literal || literal == INT32_MIN ||
          (value && !external_literal_less(values[value - 1], literal)))
        return CaptureStatus::Invalid;
      if (!meter.charge(1)) return stopped_capture(meter);
      if (!variables.insert(external_variable_of(literal)).second ||
          !output.tuple_literals
               .emplace(-literal,
                        relation::DomainValue{static_cast<uint32_t>(domain),
                                              static_cast<uint32_t>(value)})
               .second)
        return CaptureStatus::Invalid;
    }
    if (!meter.charge(uint64_t{1} + values.size()))
      return stopped_capture(meter);
    if (!output.domain_clauses
             .emplace(normalized(values),
                      DomainDescriptor{DomainClauseKind::Alo,
                                       static_cast<uint32_t>(domain), 0, 0})
             .second)
      return CaptureStatus::Invalid;
    for (size_t first = 0; first < values.size(); ++first)
      for (size_t second = first + 1; second < values.size(); ++second) {
        if (!meter.charge(3)) return stopped_capture(meter);
        if (!output.domain_clauses
                 .emplace(normalized({-values[first], -values[second]}),
                          DomainDescriptor{
                              DomainClauseKind::Amo,
                              static_cast<uint32_t>(domain),
                              static_cast<uint32_t>(first),
                              static_cast<uint32_t>(second)})
                 .second)
          return CaptureStatus::Invalid;
      }
  }
  return CaptureStatus::Complete;
}

struct FoundClause {
  ClauseKey literals;
  uint64_t native_id = 0;
  bool tuple = false;
  DomainDescriptor domain;
  std::vector<uint32_t> coordinates;
};

CaptureStatus capture_slice(
    const Kernel &kernel, const NaryCspRelationCaptureRequest &request,
    Meter &meter, NaryCspRelationOwnerSnapshot &output) {
  if (kernel.external_to_internal.size() > request.maximum_external_extent)
    return CaptureStatus::ResourceExhausted;

  ExpectedImage expected;
  CaptureStatus status =
      build_expected(request.domains, request, expected, meter);
  if (status != CaptureStatus::Complete) return status;

  for (const auto &domain : request.domains)
    for (int32_t external : domain) {
      if (!meter.charge(1)) return stopped_capture(meter);
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT ||
          kernel_var_retired(kernel, var_of(literal)) || kernel.value(literal))
        return CaptureStatus::NoStructure;
    }

  std::map<ClauseKey, FoundClause> found_domains;
  std::map<std::vector<uint32_t>, FoundClause> found_tuples;
  uint64_t source_literals = 0;
  auto retain = [&](FoundClause clause) {
    const uint64_t retained = found_domains.size() + found_tuples.size();
    if (retained >= request.maximum_source_clauses ||
        source_literals > request.maximum_source_literals ||
        clause.literals.size() >
            request.maximum_source_literals - source_literals)
      return CaptureStatus::ResourceExhausted;
    if (clause.tuple &&
        found_tuples.size() >= request.maximum_forbidden_tuples)
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
    // Selected variables are required unassigned. Assigned clauses cannot be
    // exact physical premises and are ignored rather than residualized.
    if (assigned) return true;
    external = normalized(std::move(external));
    const auto domain_clause = expected.domain_clauses.find(external);
    if (domain_clause != expected.domain_clauses.end()) {
      failure = retain(
          FoundClause{external, native_id, false, domain_clause->second, {}});
      return failure == CaptureStatus::Complete;
    }
    if (size != request.domains.size()) return true;
    std::vector<uint32_t> coordinates(request.domains.size(), UINT32_MAX);
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

  const csp_physical_scan::Status scan =
      csp_physical_scan::run(kernel, meter, observe);
  if (scan != csp_physical_scan::Status::Complete)
    return scan == csp_physical_scan::Status::Stopped ? stopped_capture(meter)
                                                       : failure;

  if (found_domains.size() != expected.domain_clauses.size())
    return CaptureStatus::NoStructure;

  std::vector<FoundClause> ordered;
  ordered.reserve(found_domains.size() + found_tuples.size());
  for (const auto &[key, clause] : found_domains) {
    (void)key;
    ordered.push_back(clause);
  }
  for (const auto &[coordinates, clause] : found_tuples) {
    (void)coordinates;
    ordered.push_back(clause);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const FoundClause &first, const FoundClause &second) {
              if (first.literals != second.literals)
                return first.literals < second.literals;
              return first.native_id < second.native_id;
            });

  NaryCspRelationOwnerSnapshot snapshot;
  snapshot.external_extent =
      static_cast<uint32_t>(kernel.external_to_internal.size());
  snapshot.source_literals = source_literals;
  std::map<ClauseKey, uint64_t> occurrences;
  for (size_t index = 0; index < ordered.size(); ++index) {
    const uint64_t occurrence = index + 1;
    if (!occurrences.emplace(ordered[index].literals, occurrence).second)
      return CaptureStatus::Invalid;
    snapshot.source.push_back(
        {occurrence, ordered[index].literals, ordered[index].native_id});
  }

  snapshot.relation.domains.resize(request.domains.size());
  for (size_t domain = 0; domain < request.domains.size(); ++domain) {
    auto &destination = snapshot.relation.domains[domain];
    destination.values = request.domains[domain];
    const size_t count = destination.values.size();
    destination.at_most_one.assign(count * count, 0);
  }
  for (const auto &[key, descriptor] : expected.domain_clauses) {
    const auto occurrence = occurrences.find(key);
    if (occurrence == occurrences.end()) return CaptureStatus::NoStructure;
    auto &domain = snapshot.relation.domains[descriptor.domain];
    if (descriptor.kind == DomainClauseKind::Alo) {
      domain.at_least_one = occurrence->second;
    } else {
      const size_t count = domain.values.size();
      domain.at_most_one[size_t(descriptor.first) * count +
                         descriptor.second] = occurrence->second;
      domain.at_most_one[size_t(descriptor.second) * count +
                         descriptor.first] = occurrence->second;
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

struct FormulaMark {
  KernelChangeMark change;
  uint64_t semantic_namespace = 0;
  uint64_t arena_generation = 0;
  uint64_t next_clause_id = 0;
  uint32_t variables = 0;
  size_t external_extent = 0;

  bool operator==(const FormulaMark &) const = default;
};

FormulaMark formula_mark(const Kernel &kernel) {
  return {kernel_change_mark(kernel), kernel.affine_semantic_namespace_epoch,
          kernel.arena_generation, kernel.next_clause_id, kernel.nvars,
          kernel.external_to_internal.size()};
}

} // namespace

bool NaryCspRelationCaptureRequest::valid() const {
  return maximum_source_clauses && maximum_source_literals &&
         maximum_recognition_work && maximum_domains >= 3 &&
         maximum_domain_values >= 2 && maximum_forbidden_tuples &&
         maximum_external_extent;
}

NaryCspRelationCaptureResult nary_csp_relation_capture_owner(
    Kernel &kernel, const NaryCspRelationCaptureRequest &request,
    NaryCspRelationOwnerSnapshot &output) {
  NaryCspRelationCaptureResult result;
  if (!request.valid() || request.domains.empty()) return result;
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? NaryCspRelationCaptureStatus::Interrupted
                        : NaryCspRelationCaptureStatus::Inapplicable;
    return result;
  }

  const FormulaMark before = formula_mark(kernel);
  Meter meter{kernel, request.maximum_recognition_work};
  NaryCspRelationOwnerSnapshot captured;
  const CaptureStatus status = capture_slice(kernel, request, meter, captured);
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  if (status != CaptureStatus::Complete) {
    result.status = public_status(status);
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = NaryCspRelationCaptureStatus::Interrupted;
    return result;
  }
  if (formula_mark(kernel) != before) {
    result.status = NaryCspRelationCaptureStatus::Stale;
    return result;
  }

  captured.capture_work = meter.used;
  result.status = NaryCspRelationCaptureStatus::Applied;
  result.source_clauses = captured.source.size();
  result.source_literals = captured.source_literals;
  output = std::move(captured);
  return result;
}

RuleEligibility nary_csp_relation_owner_guard(const Kernel &kernel) {
  return RuleEligibility{}.fact("relation-owner-root-fixpoint",
                                root_guard(kernel));
}

} // namespace tactix

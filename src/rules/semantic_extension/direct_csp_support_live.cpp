#include "rules/semantic_extension/direct_csp_support_live.hpp"

#include "proof/proof.hpp"
#include "rules/semantic_extension/bounded_journal.hpp"
#include "rules/semantic_extension/csp_physical_scan.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

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

namespace support = direct_csp_support;

using Meter = InterruptibleWork;

DirectCspSupportLiveStatus stopped_status(
    const Meter &meter, DirectCspSupportLiveStatus otherwise) {
  if (meter.interrupted) return DirectCspSupportLiveStatus::Interrupted;
  if (meter.exhausted) return DirectCspSupportLiveStatus::ResourceExhausted;
  return otherwise;
}

uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

std::vector<int32_t> normalized(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool root_guard(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

bool request_limits_valid(const DirectCspSupportLiveRequest &request) {
  return request.maximum_source_clauses &&
         request.maximum_source_literals &&
         request.maximum_recognition_work &&
         request.maximum_domain_values >= 2 &&
         request.maximum_direct_clause_literals &&
         request.maximum_definition_clause_literals &&
         request.maximum_proof_work && request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences &&
         request.maximum_journal_bytes && request.maximum_external_extent &&
         request.action_id && request.semantic_id && request.records;
}

bool capture_limits_valid(const DirectCspSupportLiveRequest &request) {
  return request.maximum_source_clauses &&
         request.maximum_source_literals &&
         request.maximum_recognition_work &&
         request.maximum_domain_values >= 2 &&
         request.maximum_direct_clause_literals &&
         request.maximum_definition_clause_literals &&
         request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences;
}

struct Expected {
  enum class Kind : uint8_t { LeftAlo, RightAlo, LeftAmo, RightAmo, Cross };
  Kind kind = Kind::LeftAlo;
  size_t first = 0;
  size_t second = 0;
};

using ClauseKey = std::vector<int32_t>;

bool insert_expected(std::map<ClauseKey, Expected> &expected,
                     std::vector<int32_t> clause, Expected value) {
  clause = normalized(std::move(clause));
  return expected.emplace(std::move(clause), value).second;
}

bool build_expected(const std::vector<int32_t> &left,
                    const std::vector<int32_t> &right,
                    std::map<ClauseKey, Expected> &expected) {
  expected.clear();
  if (left.size() < 2 || right.size() < 2) return false;
  if (!insert_expected(expected, left, {Expected::Kind::LeftAlo, 0, 0}) ||
      !insert_expected(expected, right, {Expected::Kind::RightAlo, 0, 0}))
    return false;
  std::set<uint32_t> variables;
  for (int32_t literal : left)
    if (!literal || literal == INT32_MIN ||
        !variables.insert(variable(literal)).second)
      return false;
  for (int32_t literal : right)
    if (!literal || literal == INT32_MIN ||
        !variables.insert(variable(literal)).second)
      return false;
  for (size_t i = 0; i < left.size(); ++i)
    for (size_t j = i + 1; j < left.size(); ++j)
      if (!insert_expected(expected, {-left[i], -left[j]},
                           {Expected::Kind::LeftAmo, i, j}))
        return false;
  for (size_t i = 0; i < right.size(); ++i)
    for (size_t j = i + 1; j < right.size(); ++j)
      if (!insert_expected(expected, {-right[i], -right[j]},
                           {Expected::Kind::RightAmo, i, j}))
        return false;
  for (size_t i = 0; i < left.size(); ++i)
    for (size_t j = 0; j < right.size(); ++j)
      if (!insert_expected(expected, {-left[i], -right[j]},
                           {Expected::Kind::Cross, i, j}))
        return false;
  return true;
}

struct FoundClause {
  uint64_t native_id = 0;
  ClauseKey literals;
  Expected expected;
};

enum class CaptureSliceStatus : uint8_t {
  Complete,
  NoStructure,
  ResourceExhausted,
  Interrupted,
  Invalid,
};

CaptureSliceStatus capture_stopped(const Meter &meter) {
  if (meter.interrupted) return CaptureSliceStatus::Interrupted;
  if (meter.exhausted) return CaptureSliceStatus::ResourceExhausted;
  return CaptureSliceStatus::Invalid;
}

DirectCspSupportLiveStatus live_capture_status(CaptureSliceStatus status) {
  switch (status) {
  case CaptureSliceStatus::Complete:
    return DirectCspSupportLiveStatus::Applied;
  case CaptureSliceStatus::NoStructure:
    return DirectCspSupportLiveStatus::NoStructure;
  case CaptureSliceStatus::ResourceExhausted:
    return DirectCspSupportLiveStatus::ResourceExhausted;
  case CaptureSliceStatus::Interrupted:
    return DirectCspSupportLiveStatus::Interrupted;
  case CaptureSliceStatus::Invalid:
    return DirectCspSupportLiveStatus::Invalid;
  }
  std::abort();
}

CaptureSliceStatus capture_slice(
    const Kernel &kernel, const std::vector<int32_t> &left,
    const std::vector<int32_t> &right,
    const DirectCspSupportLiveRequest &request, Meter &meter,
    DirectCspSupportOwnerSnapshot &output) {
  if (left.size() > request.maximum_domain_values ||
      right.size() > request.maximum_domain_values)
    return CaptureSliceStatus::ResourceExhausted;
  std::map<ClauseKey, Expected> expected;
  if (!build_expected(left, right, expected))
    return CaptureSliceStatus::Invalid;
  std::map<ClauseKey, FoundClause> found;
  uint64_t source_literals = 0;
  CaptureSliceStatus failure = CaptureSliceStatus::Complete;
  auto observe = [&](const Lit *literals, uint32_t size, uint64_t native_id) {
    if (!meter.charge(uint64_t{1} + size)) {
      failure = capture_stopped(meter);
      return false;
    }
    ClauseKey external;
    external.reserve(size);
    for (uint32_t index = 0; index < size; ++index) {
      const Var var = var_of(literals[index]);
      if (kernel_var_retired(kernel, var) || kernel.value(literals[index])) {
        failure = CaptureSliceStatus::Invalid;
        return false;
      }
      external.push_back(kernel_externalize(kernel, literals[index]));
    }
    external = normalized(std::move(external));
    const auto wanted = expected.find(external);
    if (wanted == expected.end()) return true;
    if (found.contains(external)) {
      failure = CaptureSliceStatus::Invalid;
      return false;
    }
    if (found.size() == request.maximum_source_clauses ||
        size > request.maximum_source_literals - source_literals) {
      failure = CaptureSliceStatus::ResourceExhausted;
      return false;
    }
    source_literals += size;
    found.emplace(external,
                  FoundClause{native_id, external, wanted->second});
    return true;
  };
  const csp_physical_scan::Status scan =
      csp_physical_scan::run(kernel, meter, observe);
  if (scan != csp_physical_scan::Status::Complete)
    return scan == csp_physical_scan::Status::Stopped ? capture_stopped(meter)
                                                       : failure;

  DirectCspSupportOwnerSnapshot snapshot;
  snapshot.external_extent = kernel.external_to_internal.size();
  snapshot.source_literals = source_literals;
  snapshot.relation.left.values = left;
  snapshot.relation.right.values = right;
  const size_t left_size = left.size(), right_size = right.size();
  snapshot.relation.left.at_most_one.assign(left_size * left_size, 0);
  snapshot.relation.right.at_most_one.assign(right_size * right_size, 0);
  snapshot.relation.forbidden.assign(left_size * right_size, 0);

  // Both ALOs and every within-domain AMO are mandatory. Cross entries are a
  // complete classification: present means forbidden, absent means compatible.
  for (const auto &[key, descriptor] : expected) {
    const auto present = found.find(key);
    if (descriptor.kind != Expected::Kind::Cross && present == found.end())
      return CaptureSliceStatus::NoStructure;
  }
  std::vector<FoundClause> ordered;
  ordered.reserve(found.size());
  for (auto &[key, clause] : found) {
    (void)key;
    ordered.push_back(clause);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const FoundClause &a, const FoundClause &b) {
              if (a.literals != b.literals) return a.literals < b.literals;
              return a.native_id < b.native_id;
            });
  std::map<ClauseKey, uint64_t> occurrences;
  for (size_t index = 0; index < ordered.size(); ++index) {
    const uint64_t occurrence = index + 1;
    occurrences.emplace(ordered[index].literals, occurrence);
    snapshot.source.push_back(
        {occurrence, ordered[index].literals, ordered[index].native_id});
  }
  for (const auto &[key, descriptor] : expected) {
    const auto occurrence = occurrences.find(key);
    const uint64_t id =
        occurrence == occurrences.end() ? 0 : occurrence->second;
    switch (descriptor.kind) {
    case Expected::Kind::LeftAlo:
      snapshot.relation.left.at_least_one = id;
      break;
    case Expected::Kind::RightAlo:
      snapshot.relation.right.at_least_one = id;
      break;
    case Expected::Kind::LeftAmo:
      snapshot.relation.left.at_most_one[descriptor.first * left_size +
                                         descriptor.second] = id;
      snapshot.relation.left.at_most_one[descriptor.second * left_size +
                                         descriptor.first] = id;
      break;
    case Expected::Kind::RightAmo:
      snapshot.relation.right.at_most_one[descriptor.first * right_size +
                                          descriptor.second] = id;
      snapshot.relation.right.at_most_one[descriptor.second * right_size +
                                          descriptor.first] = id;
      break;
    case Expected::Kind::Cross:
      snapshot.relation.forbidden[descriptor.first * right_size +
                                  descriptor.second] = id;
      break;
    }
  }
  output = std::move(snapshot);
  return CaptureSliceStatus::Complete;
}

bool owner_payload_valid(const DirectCspSupportOwnerSnapshot &owner,
                         const DirectCspSupportLiveRequest &request,
                         Meter &meter) {
  if (!owner.external_extent || owner.external_extent > INT32_MAX ||
      owner.source.empty() ||
      owner.source.size() > request.maximum_source_clauses ||
      owner.source_literals > request.maximum_source_literals ||
      owner.relation.left.values.size() > request.maximum_domain_values ||
      owner.relation.right.values.size() > request.maximum_domain_values)
    return false;
  uint64_t literals = 0;
  for (size_t index = 0; index < owner.source.size(); ++index) {
    const auto &premise = owner.source[index];
    if (!meter.charge(uint64_t{1} + premise.literals.size())) return false;
    if (premise.occurrence != index + 1 || premise.literals.empty() ||
        premise.literals.size() > request.maximum_source_literals - literals)
      return false;
    literals += premise.literals.size();
  }
  return literals == owner.source_literals;
}

bool owner_equal(const DirectCspSupportOwnerSnapshot &first,
                 const DirectCspSupportOwnerSnapshot &second) {
  if (first.external_extent != second.external_extent ||
      first.source_literals != second.source_literals ||
      first.capture_work != second.capture_work ||
      first.source.size() != second.source.size())
    return false;
  for (size_t index = 0; index < first.source.size(); ++index) {
    const auto &a = first.source[index];
    const auto &b = second.source[index];
    if (a.occurrence != b.occurrence ||
        a.native_proof_id != b.native_proof_id || a.literals != b.literals)
      return false;
  }
  const auto domain_equal = [](const support::Domain &a,
                               const support::Domain &b) {
    return a.values == b.values && a.at_least_one == b.at_least_one &&
           a.at_most_one == b.at_most_one;
  };
  return domain_equal(first.relation.left, second.relation.left) &&
         domain_equal(first.relation.right, second.relation.right) &&
         first.relation.forbidden == second.relation.forbidden;
}

bool metered_sort(ClauseKey &key, Meter &meter) {
  for (size_t index = 1; index < key.size(); ++index) {
    const int32_t value = key[index];
    size_t position = index;
    while (position) {
      if (!meter.charge(1)) return false;
      if (!(value < key[position - 1])) break;
      if (!meter.charge(1)) return false;
      key[position] = key[position - 1];
      --position;
    }
    if (position != index) {
      if (!meter.charge(1)) return false;
      key[position] = value;
    }
  }
  return true;
}

bool metered_key(const std::vector<int32_t> &input, Meter &meter,
                 ClauseKey &output) {
  output.clear();
  output.reserve(input.size());
  for (int32_t literal : input) {
    if (!meter.charge(1)) return false;
    output.push_back(literal);
  }
  return metered_sort(output, meter);
}

bool metered_external_key(const Kernel &kernel, const Lit *literals,
                          uint32_t size, Meter &meter, ClauseKey &output) {
  output.clear();
  output.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    if (!meter.charge(1)) return false;
    output.push_back(kernel_externalize(kernel, literals[index]));
  }
  return metered_sort(output, meter);
}

bool metered_equal(const ClauseKey &first, const ClauseKey &second,
                   Meter &meter, bool &equal) {
  if (!meter.charge(1)) return false;
  if (first.size() != second.size()) {
    equal = false;
    return true;
  }
  for (size_t index = 0; index < first.size(); ++index) {
    if (!meter.charge(1)) return false;
    if (first[index] != second[index]) {
      equal = false;
      return true;
    }
  }
  equal = true;
  return true;
}

bool binary_live_metered(const Kernel &kernel, Lit first, Lit second,
                         Meter &meter) {
  if (!meter.charge(1)) return false;
  if (kernel.watches.binary_size(second) <
      kernel.watches.binary_size(first))
    std::swap(first, second);
  const bool stop_at_large = kernel.watches.heads[first].binaries_first();
  for (const WatchEntry watch : kernel.watches.entries(first)) {
    if (!meter.charge(1)) return false;
    if (!watch.binary()) {
      if (stop_at_large) break;
      continue;
    }
    if (watch.other() == second) return true;
  }
  return false;
}

bool clause_live(const Kernel &kernel, const std::vector<int32_t> &wanted,
                 Meter &meter) {
  ClauseKey key;
  if (!metered_key(wanted, meter, key)) return false;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1)) return false;
    if (clause->garbage() || clause->size != key.size()) continue;
    ClauseKey external;
    if (!metered_external_key(kernel, clause->lits, clause->size, meter,
                              external))
      return false;
    bool equal = false;
    if (!metered_equal(external, key, meter, equal)) return false;
    if (equal) return true;
  }
  if (key.size() == 2) {
    if (!meter.charge(2)) return false;
    const Lit first = kernel_internalize(kernel, key[0]);
    const Lit second = kernel_internalize(kernel, key[1]);
    return first != INVALID_LIT && second != INVALID_LIT &&
           binary_live_metered(kernel, first, second, meter);
  }
  return false;
}

using Journal = semantic_extension::BoundedJournal<true>;

bool render_journal(const DirectCspSupportLiveRequest &request,
                    const DirectCspSupportOwnerSnapshot &owner,
                    const support::Plan &plan, uint64_t first_id,
                    uint64_t proof_work, Journal &journal) {
  if (!journal.append("{\"kind\":\"direct-csp-support-begin\",\"action\":" +
                      std::to_string(request.action_id) +
                      ",\"semantic\":" +
                      std::to_string(request.semantic_id) +
                      ",\"selected_left_value\":" +
                      std::to_string(request.selected_left_value) +
                      ",\"recognition\":\"exact-retained-two-domain-slice\"}\n"))
    return false;
  for (const auto &premise : owner.source) {
    if (!journal.append(
            "{\"kind\":\"direct-csp-support-source\",\"action\":" +
            std::to_string(request.action_id) + ",\"occurrence\":" +
            std::to_string(premise.occurrence) +
            ",\"native_proof_id\":" +
            std::to_string(premise.native_proof_id) + ",\"clause\":[") ||
        !journal.integers(premise.literals) || !journal.append("]}\n"))
      return false;
  }
  if (!journal.append(
          "{\"kind\":\"direct-csp-support-theory\",\"action\":" +
          std::to_string(request.action_id) +
          ",\"meaning\":\"complete exactly-one domains with binary "
          "forbidden pairs\",\"left\":[") ||
      !journal.integers(owner.relation.left.values) ||
      !journal.append("],\"right\":[") ||
      !journal.integers(owner.relation.right.values) ||
      !journal.append("],\"compatible\":[") ||
      !journal.integers(plan.compatible_right_values) ||
      !journal.append("],\"forbidden_premises\":[") ||
      !journal.integers(plan.forbidden_premises) ||
      !journal.append("]}\n"))
    return false;
  for (size_t index = 0; index < plan.additions.size(); ++index) {
    const auto &addition = plan.additions[index];
    if (!journal.append(
            "{\"kind\":\"direct-csp-support-addition\",\"action\":" +
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
              (reference.kind == support::ReferenceKind::SourceOccurrence
                   ? "source"
                   : "planned-addition") +
              "\",\"id\":" + std::to_string(reference.id) +
              ",\"native_proof_id\":" +
              std::to_string(reference.native_proof_id) + "}"))
        return false;
    }
    if (!journal.append("]}\n")) return false;
  }
  if (!journal.append(
          "{\"kind\":\"direct-csp-support-complete\",\"action\":" +
          std::to_string(request.action_id) + ",\"additions\":" +
          std::to_string(plan.additions.size()) +
          ",\"proof_work\":" + std::to_string(proof_work) +
          ",\"projection\":\"" +
          (plan.projection.extension_variable ? "omit-exact-or-extension"
                                              : "original-variables-only") +
          "\",\"extension_variable\":" +
          std::to_string(plan.projection.extension_variable) +
          ",\"definition_inputs\":[") ||
      !journal.integers(plan.projection.disjuncts) ||
      !journal.append(
          "],\"reconstruction_witness_required\":" +
          std::string(plan.projection.reconstruction_witness_required
                          ? "true"
                          : "false") +
          ",\"published_clause_id\":" +
          std::to_string(first_id + plan.additions.size() - 1) + "}\n"))
    return false;
  return true;
}

DirectCspSupportLiveStatus planning_status(const support::Plan &plan,
                                           const Meter &meter) {
  if (meter.interrupted) return DirectCspSupportLiveStatus::Interrupted;
  if (meter.exhausted || plan.status == support::Status::EffortExhausted ||
      plan.status == support::Status::BoundExceeded)
    return DirectCspSupportLiveStatus::ResourceExhausted;
  switch (plan.status) {
  case support::Status::NoConsequence:
    return DirectCspSupportLiveStatus::NoConsequence;
  case support::Status::IncompleteTheory:
    return DirectCspSupportLiveStatus::NoStructure;
  case support::Status::InvalidRequest:
    return DirectCspSupportLiveStatus::Invalid;
  case support::Status::Planned:
    return DirectCspSupportLiveStatus::Applied;
  case support::Status::BoundExceeded:
  case support::Status::EffortExhausted:
    return DirectCspSupportLiveStatus::ResourceExhausted;
  }
  return DirectCspSupportLiveStatus::Invalid;
}

struct PreparedLiveAction {
  DirectCspSupportLiveResult result;
  DirectCspSupportOwnerSnapshot owned;
  const DirectCspSupportOwnerSnapshot *owner = nullptr;
  support::Plan plan;
  Journal journal;
  uint64_t first_id = 0;
};

void prepare_live_action(Kernel &kernel,
                         const DirectCspSupportLiveRequest &request,
                         PreparedLiveAction &prepared) {
  DirectCspSupportLiveResult &result = prepared.result;
  if (!request_limits_valid(request)) return;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = DirectCspSupportLiveStatus::UnsupportedProof;
    return;
  }
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? DirectCspSupportLiveStatus::Interrupted
                        : DirectCspSupportLiveStatus::Inapplicable;
    return;
  }

  Meter meter{kernel, request.maximum_recognition_work};
  prepared.owner = request.retained_owner;
  const uint64_t scan_begin = meter.used;
  if (!prepared.owner) {
    const CaptureSliceStatus captured =
        request.left_values.empty() || request.right_values.empty()
            ? CaptureSliceStatus::Invalid
            : capture_slice(kernel, request.left_values,
                            request.right_values, request, meter,
                            prepared.owned);
    if (captured != CaptureSliceStatus::Complete) {
      result.source_scan_work = meter.used - scan_begin;
      result.recognition_work = meter.used;
      result.recognition_limit_exhausted = meter.exhausted;
      result.status = live_capture_status(captured);
      return;
    }
    prepared.owner = &prepared.owned;
  } else if ((!request.left_values.empty() &&
              request.left_values != prepared.owner->relation.left.values) ||
             (!request.right_values.empty() &&
              request.right_values != prepared.owner->relation.right.values) ||
             prepared.owner->external_extent >
                 kernel.external_to_internal.size() ||
             !owner_payload_valid(*prepared.owner, request, meter)) {
    result.source_scan_work = meter.used - scan_begin;
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    result.status = stopped_status(meter, DirectCspSupportLiveStatus::Invalid);
    return;
  }
  result.source_scan_work = meter.used - scan_begin;
  result.source_clauses = prepared.owner->source.size();
  result.source_literals = prepared.owner->source_literals;

  const uint64_t extent = kernel.external_to_internal.size();
  if (extent >= INT32_MAX) {
    result.status = DirectCspSupportLiveStatus::ResourceExhausted;
    result.recognition_work = meter.used;
    return;
  }
  support::Bounds bounds{request.maximum_source_clauses,
                         request.maximum_source_literals,
                         request.maximum_domain_values,
                         request.maximum_direct_clause_literals,
                         request.maximum_definition_clause_literals,
                         request.maximum_proof_additions,
                         request.maximum_proof_literal_occurrences,
                         request.allow_extension,
                         static_cast<int32_t>(extent + 1)};
  const uint64_t planning_begin = meter.used;
  const auto charge = [&](uint64_t amount) { return meter.charge(amount); };
  prepared.plan = support::plan_support(
      prepared.owner->source, prepared.owner->relation,
      request.selected_left_value, bounds, charge);
  result.planning_work = meter.used - planning_begin;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  if (prepared.plan.status != support::Status::Planned) {
    result.status = planning_status(prepared.plan, meter);
    return;
  }

  uint64_t proof_work = 0, literal_occurrences = 0, arena_words = 0;
  for (const support::Addition &addition : prepared.plan.additions) {
    const uint64_t amount = uint64_t{1} + addition.clause.size() +
                            addition.antecedents.size();
    if (amount > request.maximum_proof_work - proof_work ||
        addition.clause.size() >
            request.maximum_proof_literal_occurrences - literal_occurrences) {
      result.status = DirectCspSupportLiveStatus::ResourceExhausted;
      return;
    }
    proof_work += amount;
    literal_occurrences += addition.clause.size();
    if (addition.clause.size() >= 3) {
      const uint64_t words = addition.clause.size() + CLAUSE_HEADER_WORDS;
      if (words > UINT64_MAX - arena_words) {
        result.status = DirectCspSupportLiveStatus::ResourceExhausted;
        return;
      }
      arena_words += words;
    }
  }
  result.proof_work = proof_work;
  result.planned_additions = prepared.plan.additions.size();
  result.proof_literal_occurrences = literal_occurrences;
  result.extent_growth = prepared.plan.projection.extension_variable ? 1 : 0;
  if (prepared.plan.additions.size() > request.maximum_proof_additions ||
      result.extent_growth > request.maximum_extent_growth ||
      extent + result.extent_growth > request.maximum_external_extent ||
      result.extent_growth > UINT32_MAX - kernel.nvars ||
      kernel.next_clause_id >
          uint64_t(INT64_MAX) - prepared.plan.additions.size() ||
      kernel.arena.size() >= INVALID_REF ||
      arena_words >= INVALID_REF - kernel.arena.size()) {
    result.status = DirectCspSupportLiveStatus::ResourceExhausted;
    return;
  }

  prepared.journal = Journal{request.maximum_journal_bytes};
  prepared.first_id = kernel.next_clause_id + 1;
  if (!render_journal(request, *prepared.owner, prepared.plan,
                      prepared.first_id, proof_work, prepared.journal)) {
    result.journal_work = prepared.journal.work;
    result.status = DirectCspSupportLiveStatus::ResourceExhausted;
    return;
  }
  result.journal_work = prepared.journal.work;
  result.journal_bytes = prepared.journal.bytes.size();

  const uint64_t revalidation_begin = meter.used;
  DirectCspSupportOwnerSnapshot current;
  const CaptureSliceStatus recaptured = capture_slice(
      kernel, prepared.owner->relation.left.values,
      prepared.owner->relation.right.values, request, meter, current);
  if (recaptured != CaptureSliceStatus::Complete) {
    result.revalidation_work = meter.used - revalidation_begin;
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    result.status = recaptured == CaptureSliceStatus::ResourceExhausted ||
                            recaptured == CaptureSliceStatus::Interrupted
                        ? live_capture_status(recaptured)
                        : DirectCspSupportLiveStatus::Stale;
    return;
  }
  result.revalidation_work = meter.used - revalidation_begin;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  current.external_extent = prepared.owner->external_extent;
  current.capture_work = prepared.owner->capture_work;
  if (!owner_equal(current, *prepared.owner)) {
    result.status = DirectCspSupportLiveStatus::Stale;
    return;
  }
  if (kernel_terminating(kernel)) {
    result.status = DirectCspSupportLiveStatus::Interrupted;
    return;
  }
  const std::vector<int32_t> &conclusion =
      prepared.plan.additions.back().clause;
  if (!prepared.plan.projection.extension_variable &&
      clause_live(kernel, conclusion, meter)) {
    result.revalidation_work = meter.used - revalidation_begin;
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    result.status = stopped_status(meter, DirectCspSupportLiveStatus::Duplicate);
    return;
  }
  result.revalidation_work = meter.used - revalidation_begin;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  if (meter.interrupted || meter.exhausted) {
    result.status = stopped_status(meter, DirectCspSupportLiveStatus::Stale);
    return;
  }

  result.status = DirectCspSupportLiveStatus::Applied;
  result.published_support = conclusion;
  result.semantic_variables.push_back(
      {prepared.owner->relation.left.values[request.selected_left_value],
       DirectCspSemanticRole::SelectedDomainValue, false, {}});
  for (int32_t value : prepared.plan.compatible_right_values)
    result.semantic_variables.push_back(
        {value, DirectCspSemanticRole::CompatibleDomainValue, false, {}});
  if (prepared.plan.projection.extension_variable)
    result.semantic_variables.push_back(
        {prepared.plan.projection.extension_variable,
         DirectCspSemanticRole::SupportDisjunction, true,
         prepared.plan.projection.disjuncts});
}

} // namespace

DirectCspSupportOwnerCaptureResult direct_csp_support_live_capture_owner(
    Kernel &kernel, const DirectCspSupportLiveRequest &request,
    DirectCspSupportOwnerSnapshot &output) {
  DirectCspSupportOwnerCaptureResult result;
  if (!capture_limits_valid(request) || request.left_values.empty() ||
      request.right_values.empty())
    return result;
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? DirectCspSupportLiveStatus::Interrupted
                        : DirectCspSupportLiveStatus::Inapplicable;
    return result;
  }
  Meter meter{kernel, request.maximum_recognition_work};
  DirectCspSupportOwnerSnapshot captured;
  const CaptureSliceStatus capture = capture_slice(
      kernel, request.left_values, request.right_values, request, meter,
      captured);
  if (capture != CaptureSliceStatus::Complete) {
    result.status = live_capture_status(capture);
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    return result;
  }
  // Run the pure planner once to prove that the captured ownership tables are
  // internally complete. The selected row may legitimately be nonproductive.
  const uint64_t extent = kernel.external_to_internal.size();
  const int32_t prospective_extension =
      extent < INT32_MAX ? static_cast<int32_t>(extent + 1) : 0;
  support::Bounds bounds{request.maximum_source_clauses,
                         request.maximum_source_literals,
                         request.maximum_domain_values,
                         request.maximum_direct_clause_literals,
                         request.maximum_definition_clause_literals,
                         request.maximum_proof_additions,
                         request.maximum_proof_literal_occurrences,
                         request.allow_extension, prospective_extension};
  const auto charge = [&](uint64_t amount) { return meter.charge(amount); };
  const support::Plan checked = support::plan_support(
      captured.source, captured.relation, request.selected_left_value, bounds,
      charge);
  if (checked.status != support::Status::Planned &&
      checked.status != support::Status::NoConsequence) {
    result.status = planning_status(checked, meter);
    result.recognition_work = meter.used;
    result.recognition_limit_exhausted = meter.exhausted;
    return result;
  }
  captured.capture_work = meter.used;
  result.status = DirectCspSupportLiveStatus::Applied;
  result.source_clauses = captured.source.size();
  result.source_literals = captured.source_literals;
  result.recognition_work = meter.used;
  result.recognition_limit_exhausted = meter.exhausted;
  output = std::move(captured);
  return result;
}

DirectCspSupportLiveResult direct_csp_support_live_preflight(
    Kernel &kernel, const DirectCspSupportLiveRequest &request) {
  PreparedLiveAction prepared;
  prepare_live_action(kernel, request, prepared);
  return std::move(prepared.result);
}

DirectCspSupportLiveResult direct_csp_support_live_apply(
    Kernel &kernel, const DirectCspSupportLiveRequest &request) {
  PreparedLiveAction prepared;
  prepare_live_action(kernel, request, prepared);
  DirectCspSupportLiveResult result = std::move(prepared.result);
  if (result.status != DirectCspSupportLiveStatus::Applied) return result;
  support::Plan &plan = prepared.plan;
  Journal &journal = prepared.journal;
  const uint64_t first_id = prepared.first_id;
  const std::vector<int32_t> &conclusion = plan.additions.back().clause;

  // Commit starts here. All recoverable validation, capacity, proof, journal,
  // and liveness failures have already been discharged.
  kernel.close_startup_certificate();
  if (result.extent_growth) {
    const Var fresh = kernel_new_extension_vars(kernel, 1);
    if (kernel_externalize(kernel, make_lit(fresh, false)) !=
        plan.projection.extension_variable)
      std::abort();
  }
  std::vector<uint64_t> ids;
  ids.reserve(plan.additions.size());
  for (size_t index = 0; index < plan.additions.size(); ++index) {
    const support::Addition &addition = plan.additions[index];
    std::vector<Lit> literals;
    literals.reserve(addition.clause.size());
    for (int32_t external : addition.clause) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
        std::abort();
      literals.push_back(literal);
    }
    const uint64_t id = ++kernel.next_clause_id;
    if (id != first_id + index) std::abort();
    if (kernel.proof) {
      if (addition.proof == support::ProofStep::Rup) {
        kernel.proof->add_rup(id, literals.data(), literals.size(), nullptr, 0);
      } else {
        std::vector<int64_t> hints;
        if (addition.proof == support::ProofStep::DefinitionReverseRat)
          for (const support::Reference &reference : addition.antecedents) {
            if (reference.kind != support::ReferenceKind::PlannedAddition ||
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
      const bool redundant = addition.proof == support::ProofStep::Rup;
      kernel_add_large_clause(kernel, literals.data(), literals.size(),
                              redundant,
                              redundant ? literals.size() : 0, id);
    }
  }
  if (std::fwrite(journal.bytes.data(), 1, journal.bytes.size(),
                  request.records) != journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();

  result.status = DirectCspSupportLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.emitted_additions = plan.additions.size();
  result.journal_bytes = journal.bytes.size();
  result.published_clause_id = ids.back();
  result.published_support = conclusion;
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  return result;
}

} // namespace tactix

#include "rules/semantic_extension/cardinality_search_live.hpp"

#include "proof/proof.hpp"
#include "rules/semantic_extension/bounded_journal.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace tactix {
namespace {

using ClauseKey = cardinality_search::Clause;
using semantic_extension::append_bounded;

struct Source {
  uint64_t id = 0;
  Ref ref = INVALID_REF;
  Lit first = INVALID_LIT;
  Lit second = INVALID_LIT;
  bool operator==(const Source &) const = default;
};

struct Prepared {
  CardinalitySearchLiveResult result;
  cardinality_search::ResolutionPlan plan;
  std::map<ClauseKey, Source> sources;
  std::set<ClauseKey> live_clauses;
  std::vector<Lit> physical_reason;
  std::vector<Lit> trail;
  std::string journal;
  uint64_t clause_id = 0;
  uint64_t arena_generation = 0;
  uint32_t level = 0;
  Proof *proof = nullptr;
  ProofFormat proof_format = ProofFormat::DsrAscii;
};

ClauseKey canonical(ClauseKey clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool charge(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

bool collect_sources(const Kernel &kernel, uint64_t maximum,
                     uint64_t &work, std::map<ClauseKey, Source> &sources,
                     std::set<ClauseKey> *all_live = nullptr) {
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Ref current = ref;
    const Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - ref) return false;
    ref += words;
    if (!charge(uint64_t{1} + clause->size, maximum, work)) return false;
    if (clause->garbage()) continue;
    ClauseKey key;
    key.reserve(clause->size);
    for (Lit literal : *clause) {
      if (kernel_var_retired(kernel, var_of(literal))) return false;
      key.push_back(kernel_externalize(kernel, literal));
    }
    key = canonical(std::move(key));
    if (all_live) all_live->insert(key);
    if (!clause->redundant())
      sources.emplace(std::move(key),
                      Source{clause->id(), current, INVALID_LIT, INVALID_LIT});
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!charge(1, maximum, work)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      const Lit second = watch.other();
      if (kernel_var_retired(kernel, var_of(first)) ||
          kernel_var_retired(kernel, var_of(second)))
        return false;
      const uint64_t id = kernel.binary_ids.tracks_values()
                              ? kernel_binary_id(kernel, first, second)
                              : 0;
      ClauseKey key = canonical({kernel_externalize(kernel, first),
                                 kernel_externalize(kernel, second)});
      if (all_live) all_live->insert(key);
      sources.emplace(std::move(key), Source{id, INVALID_REF, first, second});
    }
  // Root units are physical solver premises even when the current decision
  // trail extends them. Require an exact native proof ID for this first path.
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(1, maximum, work)) return false;
    if (!kernel.unit_ids[var] || kernel_var_retired(kernel, var) ||
        !kernel.values[2 * var])
      continue;
    const Lit literal = kernel.values[2 * var] > 0 ? 2 * var : 2 * var + 1;
    if (kernel.assigned[var].level != 0) continue;
    ClauseKey key{kernel_externalize(kernel, literal)};
    if (all_live) all_live->insert(key);
    sources.emplace(std::move(key),
                    Source{kernel.unit_ids[var], INVALID_REF, literal,
                           INVALID_LIT});
  }
  return true;
}

std::string literals(const ClauseKey &clause) {
  std::string result;
  for (size_t at = 0; at < clause.size(); ++at)
    result += (at ? "," : "") + std::to_string(clause[at]);
  return result;
}

bool make_journal(const CardinalitySearchLiveRequest &request,
                  const CardinalitySearchLiveResult &result,
                  const cardinality_search::ResolutionPlan &plan,
                  std::string &journal) {
  const std::string prefix = "{\"action\":" +
                             std::to_string(request.action_id) +
                             ",\"semantic\":" +
                             std::to_string(request.semantic_id);
  if (!append_bounded(journal, request.maximum_journal_bytes,
                      prefix + ",\"kind\":\"cardinality-search-begin\","
                               "\"reason\":[" + literals(result.reason) +
                               "]}\n"))
    return false;
  size_t source = 0;
  for (const auto &constraint : request.constraints)
    for (const ClauseKey &premise : constraint.premises) {
      if (!append_bounded(journal, request.maximum_journal_bytes,
                          prefix + ",\"kind\":\"cardinality-source\","
                                   "\"id\":" +
                                   std::to_string(result.source_ids[source++]) +
                                   ",\"clause\":[" + literals(premise) +
                                   "]}\n"))
        return false;
    }
  for (size_t at = 0; at < plan.additions.size(); ++at)
    if (!append_bounded(journal, request.maximum_journal_bytes,
                        prefix + ",\"kind\":\"cardinality-rup\","
                                 "\"id\":" +
                                 std::to_string(result.proof_ids[at]) +
                                 ",\"role\":\"" +
                                 (at + 1 == plan.additions.size()
                                      ? "consequence"
                                      : "auxiliary") +
                                 "\",\"clause\":[" +
                                 literals(plan.additions[at]) + "]}\n"))
      return false;
  return append_bounded(
      journal, request.maximum_journal_bytes,
      prefix + ",\"kind\":\"cardinality-search-complete\"}\n");
}

Prepared preflight(Kernel &kernel,
                   const CardinalitySearchLiveRequest &request) {
  Prepared output;
  auto &result = output.result;
  if (!request.action_id || !request.semantic_id || !request.records ||
      !request.maximum_source_scan_work || !request.maximum_arena_words ||
      !request.maximum_journal_bytes || request.constraints.empty())
    return output;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = CardinalitySearchLiveStatus::UnsupportedProof;
    return output;
  }
  if (kernel.inconsistent || kernel.conflict_valid || kernel.pending.valid ||
      kernel.dense || kernel.probing.recording ||
      !kernel.large_watches_connected ||
      !kernel_propagation_fixpoint(kernel) ||
      kernel.external_to_internal.size() > INT32_MAX) {
    result.status = CardinalitySearchLiveStatus::Inapplicable;
    return output;
  }
  if (kernel_terminating(kernel)) {
    result.status = CardinalitySearchLiveStatus::Interrupted;
    return output;
  }
  if (!collect_sources(kernel, request.maximum_source_scan_work,
                       result.source_scan_work, output.sources,
                       &output.live_clauses)) {
    result.status = kernel_terminating(kernel)
                        ? CardinalitySearchLiveStatus::Interrupted
                        : CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  for (const auto &constraint : request.constraints)
    for (const ClauseKey &premise : constraint.premises) {
      const auto found = output.sources.find(canonical(premise));
      if (found == output.sources.end()) {
        result.status = CardinalitySearchLiveStatus::NoSource;
        return output;
      }
      result.source_ids.push_back(found->second.id);
    }
  output.trail.assign(kernel.trail.begin(), kernel.trail.end());
  std::vector<int32_t> external_trail;
  external_trail.reserve(output.trail.size());
  for (Lit literal : output.trail)
    external_trail.push_back(kernel_externalize(kernel, literal));
  auto inspected = cardinality_search::inspect(
      request.constraints, external_trail, request.inspection);
  result.inspection_work = inspected.work;
  if (inspected.status == cardinality_search::Status::Invalid) return output;
  if (inspected.status == cardinality_search::Status::ResourceExhausted) {
    result.status = CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  if (inspected.status == cardinality_search::Status::NoEvent) {
    result.status = CardinalitySearchLiveStatus::NoEvent;
    return output;
  }
  if (inspected.status != cardinality_search::Status::Conflict &&
      inspected.status != cardinality_search::Status::Implication)
    return output;
  result.reason = std::move(inspected.reason);
  const ClauseKey wanted = canonical(result.reason);
  for (const ClauseKey &live : output.live_clauses)
    if (std::includes(wanted.begin(), wanted.end(),
                      live.begin(), live.end())) {
      result.status = CardinalitySearchLiveStatus::NoEvent;
      return output;
    }
  if (kernel.level && result.reason.size() < 2) {
    result.status = CardinalitySearchLiveStatus::Inapplicable;
    return output;
  }
  output.plan = cardinality_search::plan_resolution(
      request.constraints, result.reason, request.resolution);
  result.resolution_work = output.plan.work;
  result.added_literals = output.plan.added_literals;
  if (output.plan.status == cardinality_search::Status::ResourceExhausted) {
    result.status = CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  if (output.plan.status != cardinality_search::Status::Conflict &&
      output.plan.status != cardinality_search::Status::Implication)
    return output;
  if (output.plan.additions.empty() ||
      output.plan.additions.back() != canonical(result.reason) ||
      kernel.next_clause_id >
          uint64_t(INT64_MAX) - output.plan.additions.size()) {
    result.status = CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  for (int32_t external : result.reason) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal))) {
      result.status = CardinalitySearchLiveStatus::Inapplicable;
      return output;
    }
    output.physical_reason.push_back(literal);
  }
  if (result.reason.size() >= 3 &&
      (clause_words(result.reason.size()) > request.maximum_arena_words ||
       kernel.arena.size() >= INVALID_REF ||
       clause_words(result.reason.size()) >=
           INVALID_REF - kernel.arena.size())) {
    result.status = CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  for (size_t at = 0; at < output.plan.additions.size(); ++at)
    result.proof_ids.push_back(kernel.next_clause_id + at + 1);
  if (!make_journal(request, result, output.plan, output.journal)) {
    result.status = CardinalitySearchLiveStatus::ResourceExhausted;
    return output;
  }
  output.clause_id = kernel.next_clause_id;
  output.arena_generation = kernel.arena_generation;
  output.level = kernel.level;
  output.proof = kernel.proof;
  if (kernel.proof) output.proof_format = kernel.proof->format();
  result.status = CardinalitySearchLiveStatus::Applied;
  return output;
}

bool unchanged(Kernel &kernel, const CardinalitySearchLiveRequest &request,
               const Prepared &prepared) {
  if (kernel_terminating(kernel) || kernel.next_clause_id != prepared.clause_id ||
      kernel.arena_generation != prepared.arena_generation ||
      kernel.level != prepared.level ||
      kernel.proof != prepared.proof ||
      (kernel.proof && kernel.proof->format() != prepared.proof_format) ||
      kernel.trail.size() != prepared.trail.size() ||
      !std::equal(kernel.trail.begin(), kernel.trail.end(),
                  prepared.trail.begin()) ||
      !kernel_propagation_fixpoint(kernel))
    return false;
  uint64_t work = 0;
  std::map<ClauseKey, Source> current;
  if (!collect_sources(kernel, request.maximum_source_scan_work,
                       work, current))
    return false;
  for (const auto &constraint : request.constraints)
    for (const ClauseKey &premise : constraint.premises) {
      const ClauseKey key = canonical(premise);
      const auto old = prepared.sources.find(key);
      const auto now = current.find(key);
      if (old == prepared.sources.end() || now == current.end() ||
          old->second != now->second)
        return false;
    }
  return true;
}

} // namespace

CardinalitySearchLiveResult cardinality_search_live_inspect(
    Kernel &kernel, const CardinalitySearchLiveRequest &request) {
  return preflight(kernel, request).result;
}

CardinalitySearchLiveResult cardinality_search_live_apply(
    Kernel &kernel, const CardinalitySearchLiveRequest &request) {
  Prepared prepared = preflight(kernel, request);
  auto &result = prepared.result;
  if (result.status != CardinalitySearchLiveStatus::Applied) return result;
  if (!unchanged(kernel, request, prepared)) {
    result.status = kernel_terminating(kernel)
                        ? CardinalitySearchLiveStatus::Interrupted
                        : CardinalitySearchLiveStatus::NoSource;
    return result;
  }
  kernel.close_startup_certificate();
  for (size_t at = 0; at < prepared.plan.additions.size(); ++at) {
    const ClauseKey &clause = prepared.plan.additions[at];
    const uint64_t id = ++kernel.next_clause_id;
    if (id != result.proof_ids[at]) std::abort();
    if (kernel.proof &&
        !kernel.proof->add_external_rup(id, clause.data(), clause.size()))
      std::abort();
  }
  const uint64_t final_id = result.proof_ids.back();
  if (prepared.physical_reason.empty())
    kernel.inconsistent = true;
  else if (prepared.physical_reason.size() == 1) {
    const Lit literal = prepared.physical_reason.front();
    if (kernel.level || kernel.value(literal)) std::abort();
    kernel.unit_ids[var_of(literal)] = final_id;
    kernel_assign_unit(kernel, literal);
  } else {
    if (prepared.physical_reason.size() == 2)
      kernel_add_binary_clause(kernel, prepared.physical_reason[0],
                               prepared.physical_reason[1], final_id);
    else
      kernel_add_large_clause(kernel, prepared.physical_reason.data(),
                              prepared.physical_reason.size(), true,
                              prepared.physical_reason.size(), final_id);
    if (kernel.level)
      kernel_rearm_theory_clause(kernel, prepared.physical_reason.data(),
                                 prepared.physical_reason.size());
  }
  if (std::fwrite(prepared.journal.data(), 1, prepared.journal.size(),
                  request.records) != prepared.journal.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  result.installed_clause_id = final_id;
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  result.result = prepared.physical_reason.empty()
                      ? ApplyResult::Unsat
                      : ApplyResult::Ok;
  return result;
}

} // namespace tactix

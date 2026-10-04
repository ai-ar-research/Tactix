#include "rules/semantic_extension/exact_cover_live.hpp"

#include "rules/semantic_extension/exact_cover_batch.hpp"

#include <algorithm>
#include <limits>

namespace tactix {
namespace {

using exact_cover_semantic::BatchPlan;
using exact_cover_semantic::BatchPlanningRequest;
using exact_cover_semantic::Premise;

struct SourceClause {
  uint64_t native_proof_id = 0;
  std::vector<int32_t> literals;
};

bool capture_source(Kernel &k, const ExactCoverLiveRequest &request,
                    uint64_t &work, std::vector<Premise> &source,
                    uint64_t &clause_count, uint64_t &literal_count,
                    ExactCoverLiveStatus &failure) {
  std::vector<SourceClause> captured;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k)) {
      failure = ExactCoverLiveStatus::Interrupted;
      return false;
    }
    if (amount > request.maximum_recognition_work - work) {
      failure = ExactCoverLiveStatus::ResourceExhausted;
      return false;
    }
    work += amount;
    return true;
  };
  const auto take = [&](const Lit *literals, uint32_t size, uint64_t id) {
    if (captured.size() == request.maximum_source_clauses ||
        size > request.maximum_source_literals - literal_count) {
      failure = ExactCoverLiveStatus::ResourceExhausted;
      return false;
    }
    if (!charge(uint64_t{1} + size))
      return false;
    SourceClause clause;
    clause.native_proof_id = id;
    clause.literals.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      const Var variable = var_of(literals[i]);
      if (kernel_var_retired(k, variable) || k.value(literals[i])) {
        failure = ExactCoverLiveStatus::NoStructure;
        return false;
      }
      clause.literals.push_back(kernel_externalize(k, literals[i]));
    }
    std::sort(clause.literals.begin(), clause.literals.end());
    literal_count += size;
    ++clause_count;
    captured.push_back(std::move(clause));
    return true;
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (!charge(1)) return false;
    if (!clause->garbage() && !take(clause->lits, clause->size, clause->id()))
      return false;
  }
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      const Lit pair[] = {first, watch.other()};
      const uint64_t proof_id = k.binary_ids.tracks_values()
                                    ? kernel_binary_id(k, first, watch.other())
                                    : 0;
      if (!take(pair, 2, proof_id))
        return false;
    }
  std::sort(captured.begin(), captured.end(),
            [](const SourceClause &a, const SourceClause &b) {
              if (a.literals != b.literals) return a.literals < b.literals;
              return a.native_proof_id < b.native_proof_id;
            });
  std::vector<Premise> planned;
  planned.reserve(captured.size());
  for (size_t i = 0; i < captured.size(); ++i)
    planned.push_back({i + 1, std::move(captured[i].literals),
                       captured[i].native_proof_id});
  source = std::move(planned);
  return true;
}

} // namespace

ExactCoverLiveResult exact_cover_live_apply(
    Kernel &k, const ExactCoverLiveRequest &request) {
  ExactCoverLiveResult result;
  if (request.arity < 3 || !request.maximum_items ||
      !request.maximum_source_clauses || !request.maximum_source_literals ||
      !request.maximum_recognition_work ||
      !request.maximum_premise_references || !request.maximum_definitions ||
      !request.maximum_additions || !request.maximum_literal_occurrences ||
      !request.maximum_journal_bytes ||
      !request.maximum_extent_growth || !request.maximum_external_extent ||
      !request.action_id || !request.records)
    return result;
  if (k.proof && k.proof->pb()) {
    result.status = ExactCoverLiveStatus::UnsupportedProof;
    return result;
  }
  if (kernel_terminating(k)) {
    result.status = ExactCoverLiveStatus::Interrupted;
    return result;
  }
  if (k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
      !k.trail.empty() || k.dense || k.probing.recording ||
      !k.large_watches_connected || !kernel_propagation_fixpoint(k) ||
      k.external_to_internal.size() > INT32_MAX) {
    result.status = ExactCoverLiveStatus::Inapplicable;
    return result;
  }

  uint64_t work = 0, source_clauses = 0, source_literals = 0;
  std::vector<Premise> source;
  ExactCoverLiveStatus failure = ExactCoverLiveStatus::NoStructure;
  if (!capture_source(k, request, work, source, source_clauses,
                      source_literals, failure) || source.empty()) {
    result.status = failure;
    result.source_clauses = source_clauses;
    result.source_literals = source_literals;
    result.recognition_work = work;
    return result;
  }
  result.source_clauses = source.size();
  result.source_literals = source_literals;

  BatchPlanningRequest planning;
  planning.arity = request.arity;
  planning.maximum_items = request.maximum_items;
  planning.maximum_source_clauses = request.maximum_source_clauses;
  planning.maximum_source_literals = request.maximum_source_literals;
  planning.maximum_premise_references = request.maximum_premise_references;
  planning.maximum_definitions = request.maximum_definitions;
  planning.maximum_additions = request.maximum_additions;
  planning.maximum_literal_occurrences = request.maximum_literal_occurrences;
  planning.maximum_extent_growth = request.maximum_extent_growth;
  planning.first_output_external =
      static_cast<uint32_t>(k.external_to_internal.size() + 1);
  planning.maximum_external_extent = request.maximum_external_extent;
  planning.action_id = request.action_id;
  BatchPlan plan;
  exact_cover_semantic::BatchPlanningStatus planning_status =
      exact_cover_semantic::BatchPlanningStatus::Invalid;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k)) {
      failure = ExactCoverLiveStatus::Interrupted;
      return false;
    }
    if (amount > request.maximum_recognition_work - work) {
      failure = ExactCoverLiveStatus::ResourceExhausted;
      return false;
    }
    work += amount;
    return true;
  };
  if (!exact_cover_semantic::plan_batch(source, planning, charge, plan,
                                        &planning_status)) {
    if (failure == ExactCoverLiveStatus::Interrupted ||
        failure == ExactCoverLiveStatus::ResourceExhausted) {
      result.status = failure;
    } else {
      using enum exact_cover_semantic::BatchPlanningStatus;
      result.status = planning_status == ResourceExhausted
                          ? ExactCoverLiveStatus::ResourceExhausted
                      : planning_status == Invalid
                          ? ExactCoverLiveStatus::Invalid
                          : ExactCoverLiveStatus::NoStructure;
    }
    result.recognition_work = work;
    return result;
  }
  result.recognition_work = work;

  const auto published = exact_cover_semantic::publish_batch(
      k, plan, {request.action_id, request.maximum_journal_bytes,
                request.records});
  if (published.result != ApplyResult::Ok) {
    using enum exact_cover_semantic::BatchPublicationStatus;
    switch (published.status) {
    case Invalid: result.status = ExactCoverLiveStatus::Invalid; break;
    case Inapplicable: result.status = ExactCoverLiveStatus::Inapplicable; break;
    case Stale: result.status = ExactCoverLiveStatus::Stale; break;
    case UnsupportedProof:
      result.status = ExactCoverLiveStatus::UnsupportedProof;
      break;
    case ResourceExhausted:
      result.status = ExactCoverLiveStatus::ResourceExhausted;
      break;
    case Interrupted: result.status = ExactCoverLiveStatus::Interrupted; break;
    case Published: result.status = ExactCoverLiveStatus::Invalid; break;
    }
    return result;
  }
  result.status = ExactCoverLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.definitions = published.definitions;
  result.additions = published.additions;
  result.literal_occurrences = published.literal_occurrences;
  result.journal_bytes = published.journal_bytes;
  result.extent_growth = published.extent_growth;
  result.introduced_theory_variables.reserve(plan.definitions().size());
  for (const auto &definition : plan.definitions())
    result.introduced_theory_variables.push_back(
        static_cast<uint32_t>(definition.output));
  return result;
}

} // namespace tactix

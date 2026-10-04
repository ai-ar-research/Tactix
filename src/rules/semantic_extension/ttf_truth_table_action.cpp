#include "rules/semantic_extension/ttf_truth_table_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <set>
#include <utility>

namespace tactix {

struct TtfTruthTableActionSessionAccess {
  static void configure(TtfTruthTableActionSession &session,
                        const Kernel &kernel,
                        TtfTruthTableLiveSnapshot snapshot,
                        TtfTruthTableAmoWitness amo,
                        const TtfTruthTableActionParameters &parameters) {
    session.owner_ = &kernel;
    session.snapshot_ = std::move(snapshot);
    session.amo_ = std::move(amo);
    session.parameters_ = parameters;
  }

  static TtfTruthTableLiveSession &live(
      TtfTruthTableActionSession &session) {
    return session.live_;
  }

  static void commit_identity(
      TtfTruthTableActionSession &session,
      std::vector<TtfTruthTableActionIdentity> identities) {
    session.identities_.swap(identities);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

enum class MeasureStatus : uint8_t { Complete, Invalid, Exhausted };

MeasureStatus measure_source(const TtfTruthTableLiveSnapshot &snapshot,
                             const TtfTruthTableActionLimits &limits,
                             TtfTruthTableSessionResult &result) {
  if (!snapshot.generation || !snapshot.external_variables ||
      snapshot.external_variables > INT32_MAX || snapshot.clauses.empty())
    return MeasureStatus::Invalid;
  if (snapshot.clauses.size() > limits.maximum_source_clauses)
    return MeasureStatus::Exhausted;
  std::set<uint64_t> ids;
  std::set<TtfTruthTableLiveSource> identities;
  for (const auto &source : snapshot.clauses) {
    if (!add_work(uint64_t{1} + source.literals.size(),
                  limits.maximum_session_work, result.session_work))
      return MeasureStatus::Exhausted;
    if (!source.native_proof_id || source.literals.empty() ||
        !ids.insert(source.native_proof_id).second)
      return MeasureStatus::Invalid;
    if (source.literals.size() >
        limits.maximum_source_literals - result.source_literals)
      return MeasureStatus::Exhausted;
    result.source_literals += source.literals.size();
    TtfTruthTableLiveSource canonical = source;
    std::sort(canonical.literals.begin(), canonical.literals.end());
    if (std::adjacent_find(canonical.literals.begin(),
                           canonical.literals.end()) !=
            canonical.literals.end() ||
        !identities.insert(canonical).second)
      return MeasureStatus::Invalid;
    for (size_t i = 0; i < canonical.literals.size(); ++i) {
      const int32_t literal = canonical.literals[i];
      if (!literal || literal == INT32_MIN ||
          variable(literal) > snapshot.external_variables)
        return MeasureStatus::Invalid;
      if (i && variable(canonical.literals[i - 1]) == variable(literal))
        return MeasureStatus::Invalid;
    }
  }
  result.source_clauses = snapshot.clauses.size();
  return MeasureStatus::Complete;
}

MeasureStatus measure_amo(const TtfTruthTableAmoWitness &amo,
                          const TtfTruthTableLiveSnapshot &snapshot,
                          const TtfTruthTableActionLimits &limits,
                          TtfTruthTableSessionResult &result) {
  if (amo.components.empty()) return MeasureStatus::Invalid;
  if (amo.components.size() > limits.maximum_amo_components)
    return MeasureStatus::Exhausted;
  std::set<TtfTruthTableLiveSource> retained_sources;
  for (auto source : snapshot.clauses) {
    std::sort(source.literals.begin(), source.literals.end());
    retained_sources.insert(std::move(source));
  }
  std::set<std::vector<int32_t>> leaf_sets;
  std::set<TtfTruthTableLiveSource> source_owners;
  for (const auto &component : amo.components) {
    if (!add_work(uint64_t{1} + component.leaves.size(),
                  limits.maximum_session_work, result.session_work))
      return MeasureStatus::Exhausted;
    if (component.leaves.size() < 2 ||
        component.leaves.size() > limits.amo.maximum_leaves ||
        component.sources.empty())
      return MeasureStatus::Invalid;
    std::vector<int32_t> leaves = component.leaves;
    std::sort(leaves.begin(), leaves.end());
    if (leaves.front() <= 0 ||
        static_cast<uint32_t>(leaves.back()) > snapshot.external_variables ||
        std::adjacent_find(leaves.begin(), leaves.end()) != leaves.end() ||
        !leaf_sets.insert(std::move(leaves)).second)
      return MeasureStatus::Invalid;
    for (const auto &raw : component.sources) {
      if (result.amo_sources == limits.maximum_amo_sources)
        return MeasureStatus::Exhausted;
      ++result.amo_sources;
      if (raw.literals.size() >
          limits.maximum_amo_source_literals -
              result.amo_source_literals)
        return MeasureStatus::Exhausted;
      result.amo_source_literals += raw.literals.size();
      if (!add_work(uint64_t{1} + raw.literals.size(),
                    limits.maximum_session_work, result.session_work))
        return MeasureStatus::Exhausted;
      TtfTruthTableLiveSource identity = raw;
      std::sort(identity.literals.begin(), identity.literals.end());
      if (!retained_sources.count(identity) ||
          !source_owners.insert(std::move(identity)).second)
        return MeasureStatus::Invalid;
    }
  }
  result.amo_components = amo.components.size();
  return MeasureStatus::Complete;
}

TtfTruthTableActionStatus action_status(TtfTruthTableLiveStatus status) {
  switch (status) {
  case TtfTruthTableLiveStatus::Applied:
    return TtfTruthTableActionStatus::Applied;
  case TtfTruthTableLiveStatus::Invalid:
    return TtfTruthTableActionStatus::Invalid;
  case TtfTruthTableLiveStatus::Inapplicable:
    return TtfTruthTableActionStatus::Inapplicable;
  case TtfTruthTableLiveStatus::NoStructure:
    return TtfTruthTableActionStatus::NoStructure;
  case TtfTruthTableLiveStatus::NoParityFrontier:
    return TtfTruthTableActionStatus::NoParityFrontier;
  case TtfTruthTableLiveStatus::MissingAmoWitness:
    return TtfTruthTableActionStatus::MissingAmoWitness;
  case TtfTruthTableLiveStatus::AlreadyPublished:
    return TtfTruthTableActionStatus::AlreadyPublished;
  case TtfTruthTableLiveStatus::ResourceExhausted:
    return TtfTruthTableActionStatus::ResourceExhausted;
  case TtfTruthTableLiveStatus::Interrupted:
    return TtfTruthTableActionStatus::Interrupted;
  case TtfTruthTableLiveStatus::UnsupportedProof:
    return TtfTruthTableActionStatus::UnsupportedProof;
  case TtfTruthTableLiveStatus::Stale:
    return TtfTruthTableActionStatus::Stale;
  }
  std::abort();
}

bool refresh_owned(void *context, TtfTruthTableLiveSnapshot &current) {
  const auto &session =
      *static_cast<const TtfTruthTableActionSession *>(context);
  if (!session.snapshot()) return false;
  current = *session.snapshot();
  return true;
}

TtfTruthTableLiveRequest make_request(
    Kernel &kernel, TtfTruthTableActionSession &session, uint64_t action_id) {
  const auto &parameters = *session.parameters();
  const auto &limits = parameters.limits;
  TtfTruthTableLiveRequest request;
  request.owner = session.snapshot();
  request.refresh = refresh_owned;
  request.refresh_context = &session;
  request.amo = session.amo();
  request.session = &TtfTruthTableActionSessionAccess::live(session);
  request.theory_limits = limits.theory;
  request.amo_limits = limits.amo;
  request.maximum_source_clauses = limits.maximum_source_clauses;
  request.maximum_source_literals = limits.maximum_source_literals;
  request.maximum_snapshot_work = limits.maximum_snapshot_work;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_proof_work = limits.maximum_proof_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_literal_occurrences =
      limits.maximum_proof_literal_occurrences;
  request.maximum_formula_additions = limits.maximum_formula_additions;
  request.maximum_formula_literal_occurrences =
      limits.maximum_formula_literal_occurrences;
  request.maximum_arena_words = limits.maximum_arena_words;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

std::vector<strategy::SemanticVariable> existing_advice(
    const std::vector<int32_t> &literals) {
  std::vector<uint32_t> variables;
  variables.reserve(literals.size());
  for (const int32_t literal : literals) {
    if (!literal || literal == INT32_MIN) std::abort();
    variables.push_back(variable(literal));
  }
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()),
                  variables.end());
  std::vector<strategy::SemanticVariable> result;
  result.reserve(variables.size());
  for (const uint32_t external : variables)
    result.push_back({external,
                      strategy::SemanticVariableRole::ExistingTheory, 1});
  return result;
}

} // namespace

bool TtfTruthTableActionLimits::valid() const {
  return theory.maximum_clauses && theory.maximum_literals &&
         theory.maximum_work && theory.maximum_plan_literals &&
         theory.maximum_stars && theory.maximum_domains &&
         theory.maximum_domain_leaves && theory.maximum_plan_additions &&
         amo.maximum_clauses && amo.maximum_literals && amo.maximum_work &&
         amo.maximum_leaves && amo.maximum_gates && maximum_source_clauses &&
         maximum_source_literals && maximum_snapshot_work &&
         maximum_recognition_work && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_formula_additions &&
         maximum_formula_literal_occurrences && maximum_arena_words &&
         maximum_journal_bytes && maximum_amo_components &&
         maximum_amo_sources && maximum_amo_source_literals &&
         maximum_session_commits && maximum_session_work;
}

bool TtfTruthTableActionLimits::operator==(
    const TtfTruthTableActionLimits &other) const {
  return theory.maximum_clauses == other.theory.maximum_clauses &&
         theory.maximum_literals == other.theory.maximum_literals &&
         theory.maximum_work == other.theory.maximum_work &&
         theory.maximum_plan_literals == other.theory.maximum_plan_literals &&
         theory.maximum_stars == other.theory.maximum_stars &&
         theory.maximum_domains == other.theory.maximum_domains &&
         theory.maximum_domain_leaves == other.theory.maximum_domain_leaves &&
         theory.maximum_plan_additions ==
             other.theory.maximum_plan_additions &&
         amo.maximum_clauses == other.amo.maximum_clauses &&
         amo.maximum_literals == other.amo.maximum_literals &&
         amo.maximum_work == other.amo.maximum_work &&
         amo.maximum_leaves == other.amo.maximum_leaves &&
         amo.maximum_gates == other.amo.maximum_gates &&
         maximum_source_clauses == other.maximum_source_clauses &&
         maximum_source_literals == other.maximum_source_literals &&
         maximum_snapshot_work == other.maximum_snapshot_work &&
         maximum_recognition_work == other.maximum_recognition_work &&
         maximum_proof_work == other.maximum_proof_work &&
         maximum_proof_additions == other.maximum_proof_additions &&
         maximum_proof_literal_occurrences ==
             other.maximum_proof_literal_occurrences &&
         maximum_formula_additions == other.maximum_formula_additions &&
         maximum_formula_literal_occurrences ==
             other.maximum_formula_literal_occurrences &&
         maximum_arena_words == other.maximum_arena_words &&
         maximum_journal_bytes == other.maximum_journal_bytes &&
         maximum_amo_components == other.maximum_amo_components &&
         maximum_amo_sources == other.maximum_amo_sources &&
         maximum_amo_source_literals == other.maximum_amo_source_literals &&
         maximum_session_commits == other.maximum_session_commits &&
         maximum_session_work == other.maximum_session_work;
}

bool TtfTruthTableActionParameters::valid() const {
  return selection == TtfTruthTableFrontierSelection::FirstEligible &&
         limits.valid();
}

TtfTruthTableSessionResult ttf_truth_table_action_configure(
    const Kernel &kernel, TtfTruthTableActionSession &session,
    const TtfTruthTableLiveSnapshot &snapshot,
    const TtfTruthTableAmoWitness &amo,
    const TtfTruthTableActionParameters &parameters) {
  TtfTruthTableSessionResult result;
  if (session.configured()) {
    result.status = TtfTruthTableSessionStatus::AlreadyConfigured;
    return result;
  }
  if (!parameters.valid()) return result;
  const MeasureStatus source_status =
      measure_source(snapshot, parameters.limits, result);
  if (source_status != MeasureStatus::Complete) {
    result.status = source_status == MeasureStatus::Exhausted
                        ? TtfTruthTableSessionStatus::ResourceExhausted
                        : TtfTruthTableSessionStatus::Invalid;
    return result;
  }
  const MeasureStatus amo_status =
      measure_amo(amo, snapshot, parameters.limits, result);
  if (amo_status != MeasureStatus::Complete) {
    result.status = amo_status == MeasureStatus::Exhausted
                        ? TtfTruthTableSessionStatus::ResourceExhausted
                        : TtfTruthTableSessionStatus::Invalid;
    return result;
  }

  // Construct every fallible owned copy before changing the public session.
  TtfTruthTableLiveSnapshot retained_snapshot = snapshot;
  TtfTruthTableAmoWitness retained_amo = amo;
  TtfTruthTableActionSessionAccess::configure(
      session, kernel, std::move(retained_snapshot), std::move(retained_amo),
      parameters);
  result.status = TtfTruthTableSessionStatus::Configured;
  return result;
}

RuleEligibility ttf_truth_table_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("assignment-free", kernel.trail.empty())
      .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("provenance-bound", kernel.structural_provenance != nullptr)
      .fact("action-id-available",
            kernel.structural_action_sequence != UINT64_MAX)
      .fact("candidate-slot-empty",
            !kernel.semantic_variable_candidate_pending)
      .fact("candidate-id-available",
            kernel.semantic_variable_publication != (UINT64_MAX >> 8))
      .fact("clausal-certificate",
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->pb()));
}

TtfTruthTableActionResult ttf_truth_table_action_apply(
    Kernel &kernel, TtfTruthTableActionSession &session) {
  TtfTruthTableActionResult output;
  output.committed_keys = session.committed_keys();
  output.committed_identities = session.committed_identities();
  if (!ttf_truth_table_action_guard(kernel)) {
    output.status = TtfTruthTableActionStatus::Inapplicable;
    output.live.status = TtfTruthTableLiveStatus::Inapplicable;
    return output;
  }
  if (!session.configured()) return output;
  if (!session.belongs_to(kernel)) {
    output.status = TtfTruthTableActionStatus::Stale;
    output.live.status = TtfTruthTableLiveStatus::Stale;
    return output;
  }
  const auto &parameters = *session.parameters();
  if (!parameters.valid()) return output;
  if (session.committed_identities() != session.committed_keys()) return output;
  // FirstEligible has exactly one retained semantic identity.  Once committed,
  // the immutable source, witness and selector can only reproduce that key;
  // suppress it before limits or recognition consume another action attempt.
  if (session.committed_identities()) {
    if (session.committed_identities() != 1) return output;
    output.status = TtfTruthTableActionStatus::AlreadyPublished;
    output.live.status = TtfTruthTableLiveStatus::AlreadyPublished;
    return output;
  }
  if (session.committed_identities() >=
      parameters.limits.maximum_session_commits) {
    output.status = TtfTruthTableActionStatus::ResourceExhausted;
    output.live.status = TtfTruthTableLiveStatus::ResourceExhausted;
    return output;
  }
  if (!add_work(uint64_t{1} + session.committed_identities(),
                parameters.limits.maximum_session_work,
                output.session_work)) {
    output.status = TtfTruthTableActionStatus::ResourceExhausted;
    output.live.status = TtfTruthTableLiveStatus::ResourceExhausted;
    return output;
  }

  std::vector<TtfTruthTableActionIdentity> identities = session.identities();
  identities.reserve(identities.size() + 1);
  identities.push_back({});
  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;
  TtfTruthTableLiveRequest request =
      make_request(kernel, session, output.proposed_action_id);
  output.live = ttf_truth_table_live_apply(kernel, request);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  output.propagation_pending = output.live.propagation_pending;
  output.complete_refutation = output.live.complete_refutation;
  if (output.live.status != TtfTruthTableLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok ||
      output.live.complete_refutation || !output.live.propagation_pending)
    std::abort();

  identities.back() = {output.proposed_action_id, output.semantic_id,
                       session.snapshot()->generation,
                       output.live.coordinate};
  TtfTruthTableActionSessionAccess::commit_identity(session,
                                                     std::move(identities));
  output.committed_action_id = output.proposed_action_id;
  output.committed_keys = session.committed_keys();
  output.committed_identities = session.committed_identities();
  if (output.committed_keys != output.committed_identities) std::abort();
  output.existing_variable_advice =
      existing_advice(output.live.semantic_candidate_variables);
  if (output.existing_variable_advice.empty()) std::abort();
  output.advice_ready = true;
  kernel.structural_action_sequence = output.proposed_action_id;
  return output;
}

} // namespace tactix

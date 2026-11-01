#include "rules/semantic_extension/gf3_affine_action.hpp"

#include "strategy/action_status_mapping.hpp"

#include "kernel/types.hpp"
#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/gf3_live_common.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tactix {

struct Gf3AffineActionSessionAccess {
  static void commit(Gf3AffineActionSession &session, const Kernel &kernel,
                     Gf3AffineOwnerSnapshot snapshot,
                     std::vector<Gf3AffineActionKey> keys) {
    session.owner_ = &kernel;
    if (session.snapshot_.sources.empty())
      session.snapshot_ = std::move(snapshot);
    session.keys_.swap(keys);
  }
};

namespace {

using gf3_live_common::add_work;

Gf3AffineActionKey canonical_key(uint32_t first, uint32_t second) {
  if (second < first) std::swap(first, second);
  return {first, second};
}

enum class KeyStatus : uint8_t { Novel, Present, Invalid, Exhausted };

enum class OwnerLimitStatus : uint8_t { Within, Invalid, Exhausted };

OwnerLimitStatus retained_owner_limit_status(
    const Gf3AffineOwnerSnapshot &owner,
    const Gf3AffineActionLimits &limits, uint64_t &work) {
  bool exceeded =
      owner.sources.size() > limits.recognition.maximum_source_clauses ||
      owner.source_literals > limits.recognition.maximum_source_literals ||
      owner.theory.domains.size() > limits.recognition.maximum_domains ||
      owner.theory.equations.size() > limits.recognition.maximum_equations;
  uint32_t maximum_arity = 0;
  uint64_t maximum_tuples = 0;
  for (const auto &equation : owner.theory.equations) {
    // Reading the three vector extents is constant work.  Their contents are
    // charged only on the exceptional deep-validation path below.
    if (!add_work(1, limits.maximum_session_work, work))
      return OwnerLimitStatus::Exhausted;
    if (equation.domains.size() > UINT32_MAX)
      return OwnerLimitStatus::Invalid;
    maximum_arity =
        std::max(maximum_arity,
                 static_cast<uint32_t>(equation.domains.size()));
    maximum_tuples =
        std::max(maximum_tuples,
                 static_cast<uint64_t>(equation.forbidden_by_tuple.size()));
  }
  exceeded = exceeded ||
             maximum_arity > limits.recognition.maximum_equation_arity ||
             maximum_tuples >
                 limits.recognition.maximum_tuples_per_equation;
  if (!exceeded) return OwnerLimitStatus::Within;

  // A retained owner was admitted under an earlier configuration.  Validate
  // it independently of the newly lowered recognition bounds before
  // classifying the mismatch: malformed retained identity is Invalid, while
  // an intrinsically valid owner that merely no longer fits is Exhausted.
  if (!owner.external_extent || owner.external_extent > INT32_MAX ||
      owner.sources.empty() ||
      owner.sources.size() != owner.theory.source.size() ||
      owner.theory.domains.empty() || owner.theory.equations.empty() ||
      owner.theory.domains.size() > UINT32_MAX ||
      owner.theory.equations.size() > UINT32_MAX)
    return OwnerLimitStatus::Invalid;
  std::vector<gf3_affine::Premise> premises;
  premises.reserve(owner.sources.size());
  uint64_t source_literals = 0;
  for (size_t index = 0; index < owner.sources.size(); ++index) {
    const Gf3AffineLiveSource &source = owner.sources[index];
    if (!add_work(uint64_t{1} + source.clause.size(),
                  limits.maximum_session_work, work))
      return OwnerLimitStatus::Exhausted;
    const bool invalid_identity =
        source.kind == Gf3AffineSourceKind::Large
            ? source.ref == INVALID_REF || source.first != INVALID_LIT ||
                  source.second != INVALID_LIT
            : source.kind == Gf3AffineSourceKind::Binary
                  ? source.ref != INVALID_REF || source.first == INVALID_LIT ||
                        source.second == INVALID_LIT ||
                        source.first == source.second ||
                        source.clause.size() != 2
                  : true;
    if (invalid_identity || source.clause.empty() ||
        source.occurrence != index + 1 ||
        source.clause.size() > UINT64_MAX - source_literals)
      return OwnerLimitStatus::Invalid;
    source_literals += source.clause.size();
    premises.push_back(
        {source.occurrence, source.clause, source.native_proof_id});
  }
  if (source_literals != owner.source_literals)
    return OwnerLimitStatus::Invalid;

  gf3_affine::RecognitionBounds relaxed;
  relaxed.maximum_source_clauses = owner.sources.size();
  relaxed.maximum_source_literals = source_literals;
  relaxed.maximum_domains = owner.theory.domains.size();
  relaxed.maximum_equations = owner.theory.equations.size();
  relaxed.maximum_equation_arity = maximum_arity;
  relaxed.maximum_tuples_per_equation = maximum_tuples;
  const auto charge = [&](uint64_t amount) {
    return add_work(amount, limits.maximum_session_work, work);
  };
  const gf3_affine::RecognitionResult recognized =
      gf3_affine::recognize(premises, relaxed, charge);
  if (recognized.status == gf3_affine::RecognitionStatus::EffortExhausted)
    return OwnerLimitStatus::Exhausted;
  if (recognized.status != gf3_affine::RecognitionStatus::Recognized ||
      owner.theory != recognized.theory ||
      recognized.theory.maximum_variable > owner.external_extent)
    return OwnerLimitStatus::Invalid;
  return OwnerLimitStatus::Exhausted;
}

KeyStatus prepare_keys(const Gf3AffineActionSession &session,
                       const Gf3AffineActionKey &key,
                       const Gf3AffineActionLimits &limits, uint64_t &work,
                       std::vector<Gf3AffineActionKey> &merged) {
  const auto &old = session.keys();
  for (size_t index = 0; index < old.size(); ++index) {
    if (!add_work(1, limits.maximum_session_work, work))
      return KeyStatus::Exhausted;
    if (old[index].first_equation >= old[index].second_equation ||
        (index && !(old[index - 1] < old[index])))
      return KeyStatus::Invalid;
  }
  if (old.size() > limits.maximum_session_keys)
    return KeyStatus::Exhausted;
  const auto position = std::lower_bound(old.begin(), old.end(), key);
  if (position != old.end() && *position == key) return KeyStatus::Present;
  if (old.size() == limits.maximum_session_keys)
    return KeyStatus::Exhausted;

  // Preallocate all fallible session storage before the live adapter commits.
  merged.reserve(old.size() + 1);
  merged.insert(merged.end(), old.begin(), position);
  merged.push_back(key);
  merged.insert(merged.end(), position, old.end());
  if (!add_work(uint64_t{1} + merged.size(), limits.maximum_session_work,
                work))
    return KeyStatus::Exhausted;
  return KeyStatus::Novel;
}

Gf3AffineActionStatus action_status(Gf3AffineLiveStatus status) {
  if (const auto failure =
          strategy::map_standard_action_failure<Gf3AffineActionStatus>(status))
    return *failure;
  switch (status) {
  case Gf3AffineLiveStatus::Applied:
    return Gf3AffineActionStatus::Applied;
  case Gf3AffineLiveStatus::NoStructure:
    return Gf3AffineActionStatus::NoStructure;
  case Gf3AffineLiveStatus::NoConsequence:
    return Gf3AffineActionStatus::NoConsequence;
  case Gf3AffineLiveStatus::Duplicate:
    return Gf3AffineActionStatus::AlreadyPublished;
  default: std::abort();
  }
}

Gf3AffineLiveRequest make_request(const Kernel &kernel,
                                  const Gf3AffineActionLimits &limits,
                                  uint32_t first_equation,
                                  uint32_t second_equation,
                                  uint64_t action_id) {
  Gf3AffineLiveRequest request;
  request.recognition = limits.recognition;
  request.elimination = limits.elimination;
  request.first_equation = first_equation;
  request.second_equation = second_equation;
  request.maximum_recognition_work = limits.maximum_recognition_work;
  request.maximum_planning_work = limits.maximum_planning_work;
  request.maximum_duplicate_scan_work = limits.maximum_duplicate_scan_work;
  request.maximum_revalidation_work = limits.maximum_revalidation_work;
  request.maximum_proof_work = limits.maximum_proof_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_literal_occurrences =
      limits.maximum_proof_literal_occurrences;
  request.maximum_arena_words = limits.maximum_arena_words;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.maximum_external_extent = limits.maximum_external_extent;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

void copy_capture_effort(Gf3AffineActionResult &output,
                         const Gf3AffineOwnerCaptureResult &capture) {
  output.effort.source_clauses = capture.source_clauses;
  output.effort.source_literals = capture.source_literals;
  output.effort.equations = capture.equations;
  output.effort.owner_capture_work = capture.recognition_work;
}

void copy_live_effort(Gf3AffineActionResult &output,
                      const Gf3AffineLiveResult &live) {
  output.effort.source_clauses = live.source_clauses;
  output.effort.source_literals = live.source_literals;
  output.effort.equations = live.equations;
  output.effort.recognition_work = live.recognition_work;
  output.effort.planning_work = live.planning_work;
  output.effort.duplicate_scan_work = live.duplicate_scan_work;
  output.effort.revalidation_work = live.revalidation_work;
  output.effort.proof_work = live.proof_work;
  output.effort.planned_additions = live.planned_additions;
  output.effort.duplicate_additions = live.duplicate_additions;
  output.effort.emitted_additions = live.emitted_additions;
  output.effort.proof_literal_occurrences = live.proof_literal_occurrences;
  output.effort.arena_words = live.arena_words;
  output.effort.journal_work = live.journal_work;
  output.effort.journal_bytes = live.journal_bytes;
}

std::vector<strategy::SemanticVariable>
semantic_advice(const std::vector<int32_t> &literals) {
  std::vector<uint32_t> variables;
  variables.reserve(literals.size());
  for (const int32_t literal : literals) {
    if (!literal || literal == std::numeric_limits<int32_t>::min())
      std::abort();
    variables.push_back(external_variable_of(literal));
  }
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()),
                  variables.end());
  std::vector<strategy::SemanticVariable> result;
  result.reserve(variables.size());
  for (uint32_t external : variables)
    result.push_back({external,
                      strategy::SemanticVariableRole::ExistingTheory, 1});
  return result;
}

} // namespace

bool Gf3AffineActionLimits::valid() const {
  return recognition.valid() && elimination.maximum_result_arity &&
         elimination.maximum_result_tuples && elimination.maximum_additions &&
         elimination.maximum_addition_literals && maximum_recognition_work &&
         maximum_planning_work && maximum_duplicate_scan_work &&
         maximum_revalidation_work && maximum_proof_work &&
         maximum_proof_additions && maximum_proof_literal_occurrences &&
         maximum_arena_words && maximum_journal_bytes &&
         maximum_external_extent && maximum_session_keys &&
         maximum_session_work;
}

RuleEligibility gf3_affine_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, true)
      .identity()
      .semantic_candidate()
      .clausal_certificate()
      .finish();
}

Gf3AffineActionResult gf3_affine_action_apply(
    Kernel &kernel, Gf3AffineActionSession &session,
    const Gf3AffineActionLimits &limits, uint32_t first_equation,
    uint32_t second_equation) {
  Gf3AffineActionResult output;
  // Every decline reports the live status and its action-status image.
  const auto decline = [&](Gf3AffineLiveStatus live) {
    output.status = action_status(live);
    output.live.status = live;
    return output;
  };
  output.owner_retained = session.retained();
  output.committed_keys = session.committed_keys();
  output.key = canonical_key(first_equation, second_equation);
  if (!gf3_affine_action_guard(kernel))
    return decline(Gf3AffineLiveStatus::Inapplicable);
  if (!limits.valid() || first_equation == second_equation) return output;
  if (session.retained() && !session.belongs_to(kernel))
    return decline(Gf3AffineLiveStatus::Stale);

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;

  std::vector<Gf3AffineActionKey> merged;
  const KeyStatus key_status = prepare_keys(
      session, output.key, limits, output.effort.session_work, merged);
  if (key_status == KeyStatus::Invalid) return output;
  if (key_status == KeyStatus::Exhausted)
    return decline(Gf3AffineLiveStatus::ResourceExhausted);
  if (session.retained()) {
    const OwnerLimitStatus owner_status = retained_owner_limit_status(
        *session.snapshot(), limits, output.effort.session_work);
    if (owner_status == OwnerLimitStatus::Invalid) return output;
    if (owner_status == OwnerLimitStatus::Exhausted)
      return decline(Gf3AffineLiveStatus::ResourceExhausted);
  }
  if (key_status == KeyStatus::Present)
    return decline(Gf3AffineLiveStatus::Duplicate);

  Gf3AffineLiveRequest request =
      make_request(kernel, limits, output.key.first_equation,
                   output.key.second_equation,
                   output.proposed_action_id);
  Gf3AffineOwnerSnapshot captured;
  const bool first = !session.retained();
  if (first) {
    const Gf3AffineOwnerCaptureResult capture =
        gf3_affine_live_capture_owner(kernel, request, captured);
    copy_capture_effort(output, capture);
    if (capture.status != Gf3AffineLiveStatus::Applied)
      return decline(capture.status);
    request.retained_owner = &captured;
  } else {
    request.retained_owner = session.snapshot();
  }

  output.live = gf3_affine_live_apply(kernel, request);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  copy_live_effort(output, output.live);
  if (output.live.status != Gf3AffineLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok) std::abort();

  Gf3AffineActionSessionAccess::commit(session, kernel, std::move(captured),
                                       std::move(merged));
  output.owner_retained = true;
  output.committed_keys = session.committed_keys();
  output.committed_action_id = output.proposed_action_id;

  output.semantic_candidates =
      semantic_advice(output.live.semantic_variables);
  if (output.semantic_candidates.empty()) std::abort();
  output.semantic_candidate_ready = true;
  kernel.structural_action_sequence = output.proposed_action_id;
  return output;
}
} // namespace tactix

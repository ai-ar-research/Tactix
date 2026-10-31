#include "rules/semantic_extension/gf3_affine_action.hpp"

#include "kernel/types.hpp"
#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

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

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

Gf3AffineActionKey canonical_key(uint32_t first, uint32_t second) {
  if (second < first) std::swap(first, second);
  return {first, second};
}

enum class KeyStatus : uint8_t { Novel, Present, Invalid, Exhausted };

enum class SessionKeysStatus : uint8_t { Valid, Invalid, Exhausted };

SessionKeysStatus validate_session_keys(
    const Gf3AffineActionSession &session,
    const Gf3AffineActionLimits &limits, uint64_t &work) {
  if (!session.retained() && !session.keys().empty())
    return SessionKeysStatus::Invalid;
  if (session.keys().size() > limits.maximum_session_keys)
    return SessionKeysStatus::Exhausted;
  for (size_t index = 0; index < session.keys().size(); ++index) {
    if (!add_work(1, limits.maximum_session_work, work))
      return SessionKeysStatus::Exhausted;
    const Gf3AffineActionKey &key = session.keys()[index];
    if (key.first_equation >= key.second_equation ||
        (index && !(session.keys()[index - 1] < key)))
      return SessionKeysStatus::Invalid;
  }
  return SessionKeysStatus::Valid;
}

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
      !gf3_affine::same_theory(owner.theory, recognized.theory) ||
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
  switch (status) {
  case Gf3AffineLiveStatus::Applied:
    return Gf3AffineActionStatus::Applied;
  case Gf3AffineLiveStatus::Invalid:
    return Gf3AffineActionStatus::Invalid;
  case Gf3AffineLiveStatus::Inapplicable:
    return Gf3AffineActionStatus::Inapplicable;
  case Gf3AffineLiveStatus::NoStructure:
    return Gf3AffineActionStatus::NoStructure;
  case Gf3AffineLiveStatus::NoConsequence:
    return Gf3AffineActionStatus::NoConsequence;
  case Gf3AffineLiveStatus::Duplicate:
    return Gf3AffineActionStatus::AlreadyPublished;
  case Gf3AffineLiveStatus::ResourceExhausted:
    return Gf3AffineActionStatus::ResourceExhausted;
  case Gf3AffineLiveStatus::Interrupted:
    return Gf3AffineActionStatus::Interrupted;
  case Gf3AffineLiveStatus::UnsupportedProof:
    return Gf3AffineActionStatus::UnsupportedProof;
  case Gf3AffineLiveStatus::Stale:
    return Gf3AffineActionStatus::Stale;
  }
  std::abort();
}

Gf3AffineActionOfferStatus offer_status(Gf3AffineLiveStatus status) {
  switch (status) {
  case Gf3AffineLiveStatus::Applied:
    return Gf3AffineActionOfferStatus::Ready;
  case Gf3AffineLiveStatus::Inapplicable:
  case Gf3AffineLiveStatus::UnsupportedProof:
    return Gf3AffineActionOfferStatus::Inapplicable;
  case Gf3AffineLiveStatus::NoStructure:
  case Gf3AffineLiveStatus::NoConsequence:
  case Gf3AffineLiveStatus::Duplicate:
    return Gf3AffineActionOfferStatus::NoStructure;
  case Gf3AffineLiveStatus::ResourceExhausted:
    return Gf3AffineActionOfferStatus::ResourceExhausted;
  case Gf3AffineLiveStatus::Interrupted:
    return Gf3AffineActionOfferStatus::Interrupted;
  case Gf3AffineLiveStatus::Stale:
    return Gf3AffineActionOfferStatus::Stale;
  case Gf3AffineLiveStatus::Invalid:
    return Gf3AffineActionOfferStatus::Invalid;
  }
  std::abort();
}

using OfferMeter = InterruptibleWork;

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
  return recognition.maximum_source_clauses &&
         recognition.maximum_source_literals && recognition.maximum_domains &&
         recognition.maximum_equations &&
         recognition.maximum_equation_arity >= 2 &&
         recognition.maximum_tuples_per_equation &&
         elimination.maximum_result_arity &&
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
  output.owner_retained = session.retained();
  output.committed_keys = session.committed_keys();
  output.key = canonical_key(first_equation, second_equation);
  if (!gf3_affine_action_guard(kernel)) {
    output.status = Gf3AffineActionStatus::Inapplicable;
    output.live.status = Gf3AffineLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || first_equation == second_equation) return output;
  if (session.retained() && !session.belongs_to(kernel)) {
    output.status = Gf3AffineActionStatus::Stale;
    output.live.status = Gf3AffineLiveStatus::Stale;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;

  std::vector<Gf3AffineActionKey> merged;
  const KeyStatus key_status = prepare_keys(
      session, output.key, limits, output.effort.session_work, merged);
  if (key_status == KeyStatus::Invalid) return output;
  if (key_status == KeyStatus::Exhausted) {
    output.status = Gf3AffineActionStatus::ResourceExhausted;
    output.live.status = Gf3AffineLiveStatus::ResourceExhausted;
    return output;
  }
  if (session.retained()) {
    const OwnerLimitStatus owner_status = retained_owner_limit_status(
        *session.snapshot(), limits, output.effort.session_work);
    if (owner_status == OwnerLimitStatus::Invalid) return output;
    if (owner_status == OwnerLimitStatus::Exhausted) {
      output.status = Gf3AffineActionStatus::ResourceExhausted;
      output.live.status = Gf3AffineLiveStatus::ResourceExhausted;
      return output;
    }
  }
  if (key_status == KeyStatus::Present) {
    output.status = Gf3AffineActionStatus::AlreadyPublished;
    output.live.status = Gf3AffineLiveStatus::Duplicate;
    return output;
  }

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
    if (capture.status != Gf3AffineLiveStatus::Applied) {
      output.status = action_status(capture.status);
      output.live.status = capture.status;
      return output;
    }
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

Gf3AffineActionOfferResult gf3_affine_action_offer(
    Kernel &kernel, const Gf3AffineActionSession &session,
    const Gf3AffineActionLimits &limits,
    const Gf3AffineActionOfferWindow &window) {
  Gf3AffineActionOfferResult output;
  output.owner_retained = session.retained();
  const auto decline = [&](Gf3AffineActionOfferStatus status) {
    output.status = status;
    output.complete = false;
    output.have_resume_after = false;
    output.offers.clear();
    return output;
  };
  if (!gf3_affine_action_guard(kernel))
    return decline(Gf3AffineActionOfferStatus::Inapplicable);
  if (!limits.valid() || !window.valid()) return output;
  if (kernel.external_to_internal.size() > limits.maximum_external_extent)
    return decline(Gf3AffineActionOfferStatus::ResourceExhausted);
  if (session.retained() && !session.belongs_to(kernel))
    return decline(Gf3AffineActionOfferStatus::Stale);

  const SessionKeysStatus keys_status =
      validate_session_keys(session, limits, output.effort.session_work);
  if (keys_status == SessionKeysStatus::Invalid) return output;
  if (keys_status == SessionKeysStatus::Exhausted ||
      session.committed_keys() >= limits.maximum_session_keys)
    return decline(Gf3AffineActionOfferStatus::ResourceExhausted);
  if (session.retained()) {
    const OwnerLimitStatus owner_status = retained_owner_limit_status(
        *session.snapshot(), limits, output.effort.session_work);
    if (owner_status == OwnerLimitStatus::Invalid) return output;
    if (owner_status == OwnerLimitStatus::Exhausted)
      return decline(Gf3AffineActionOfferStatus::ResourceExhausted);
  }

  Gf3AffineLiveRequest request = make_request(
      kernel, limits, 0, 1, kernel.structural_action_sequence + 1);
  Gf3AffineOwnerSnapshot captured;
  const Gf3AffineOwnerSnapshot *owner = session.snapshot();
  if (!owner) {
    const Gf3AffineOwnerCaptureResult capture =
        gf3_affine_live_capture_owner(kernel, request, captured);
    output.effort.source_clauses = capture.source_clauses;
    output.effort.source_literals = capture.source_literals;
    output.effort.equations = capture.equations;
    output.effort.source_work = capture.source_work;
    output.effort.recognition_work =
        capture.recognition_work - capture.source_work;
    if (capture.status != Gf3AffineLiveStatus::Applied)
      return decline(offer_status(capture.status));
    owner = &captured;
  }
  {
    const Gf3AffineOwnerValidationResult validation =
        gf3_affine_live_validate_owner(kernel, request, *owner);
    output.effort.source_clauses = validation.source_clauses;
    output.effort.source_literals = validation.source_literals;
    output.effort.equations = validation.equations;
    if (!add_work(validation.recognition_work, UINT64_MAX,
                  output.effort.recognition_work))
      return output;
    output.effort.revalidation_work = validation.revalidation_work;
    if (validation.status != Gf3AffineLiveStatus::Applied)
      return decline(offer_status(validation.status));
  }

  const size_t equation_count = owner->theory.equations.size();
  if (equation_count > UINT32_MAX || equation_count < 2)
    return decline(Gf3AffineActionOfferStatus::NoStructure);
  const uint32_t equations = static_cast<uint32_t>(equation_count);
  uint32_t start_first = 0, start_second = 1;
  if (window.have_after) {
    if (window.after.second_equation >= equations) return output;
    if (window.after.second_equation + 1 < equations) {
      start_first = window.after.first_equation;
      start_second = window.after.second_equation + 1;
    } else if (window.after.first_equation + 2 < equations) {
      start_first = window.after.first_equation + 1;
      start_second = start_first + 1;
    } else {
      output.status = Gf3AffineActionOfferStatus::Ready;
      output.complete = true;
      return output;
    }
  }

  OfferMeter meter{kernel, window.maximum_enumeration_work};
  if (!meter.charge(0)) {
    output.effort.enumeration_work = meter.used;
    return decline(meter.interrupted
                       ? Gf3AffineActionOfferStatus::Interrupted
                       : Gf3AffineActionOfferStatus::ResourceExhausted);
  }
  size_t committed = 0;
  Gf3AffineActionKey last_completed;
  bool have_completed = false;
  const auto complete_candidate = [&](const Gf3AffineActionKey &key) {
    last_completed = key;
    have_completed = true;
  };
  const auto finish_page = [&] {
    if (!have_completed) std::abort();
    output.status = Gf3AffineActionOfferStatus::Ready;
    output.complete = false;
    output.have_resume_after = true;
    output.resume_after = last_completed;
    output.effort.enumeration_work = meter.used;
    return output;
  };
  const auto terminal_decline = [&](Gf3AffineActionOfferStatus status) {
    output.effort.enumeration_work = meter.used;
    return decline(status);
  };
  const auto stopped = [&] {
    if (meter.interrupted)
      return terminal_decline(Gf3AffineActionOfferStatus::Interrupted);
    if (have_completed) return finish_page();
    return terminal_decline(Gf3AffineActionOfferStatus::ResourceExhausted);
  };

  for (uint32_t first = start_first; first < equations - 1; ++first) {
    const uint32_t initial_second =
        first == start_first ? std::max(first + 1, start_second) : first + 1;
    for (uint32_t second = initial_second; second < equations; ++second) {
      if (output.effort.pair_candidates == window.maximum_pair_candidates) {
        return finish_page();
      }
      const Gf3AffineActionKey key{first, second};
      if (!meter.charge(1)) return stopped();
      ++output.effort.pair_candidates;

      while (committed < session.keys().size() &&
             session.keys()[committed] < key) {
        if (!meter.charge(1)) return stopped();
        ++committed;
      }
      if (committed < session.keys().size()) {
        if (!meter.charge(1)) return stopped();
        if (session.keys()[committed] == key) {
          ++output.effort.committed_pairs_skipped;
          ++committed;
          complete_candidate(key);
          continue;
        }
      }

      uint64_t candidate_session_work = 0;
      std::vector<Gf3AffineActionKey> candidate_keys;
      const KeyStatus candidate_key_status =
          prepare_keys(session, key, limits, candidate_session_work,
                       candidate_keys);
      OwnerLimitStatus candidate_owner_status = OwnerLimitStatus::Within;
      if (candidate_key_status == KeyStatus::Novel && session.retained())
        candidate_owner_status = retained_owner_limit_status(
            *session.snapshot(), limits, candidate_session_work);
      if (!add_work(candidate_session_work, UINT64_MAX,
                    output.effort.session_work))
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      if (candidate_key_status == KeyStatus::Invalid ||
          candidate_owner_status == OwnerLimitStatus::Invalid)
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      if (candidate_key_status == KeyStatus::Exhausted ||
          candidate_owner_status == OwnerLimitStatus::Exhausted) {
        ++output.effort.resource_rejected_pairs;
        complete_candidate(key);
        continue;
      }
      if (candidate_key_status != KeyStatus::Novel)
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);

      uint64_t pair_work = 0;
      const auto charge = [&](uint64_t amount) {
        if (pair_work > limits.maximum_planning_work ||
            amount > limits.maximum_planning_work - pair_work)
          return false;
        if (!meter.charge(amount)) return false;
        pair_work += amount;
        return true;
      };
      const gf3_affine::PairEliminationPlan plan =
          gf3_affine::plan_pair_elimination(owner->theory, first, second,
                                            limits.elimination, charge);
      if (plan.status == gf3_affine::PlanStatus::EffortExhausted) {
        if (meter.interrupted || meter.exhausted) return stopped();
        ++output.effort.resource_rejected_pairs;
        complete_candidate(key);
        continue;
      }
      if (plan.status == gf3_affine::PlanStatus::BoundExceeded) {
        ++output.effort.resource_rejected_pairs;
        complete_candidate(key);
        continue;
      }
      if (plan.status == gf3_affine::PlanStatus::InvalidRequest ||
          plan.status == gf3_affine::PlanStatus::IncompleteTheory) {
        output.effort.enumeration_work = meter.used;
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      }
      if (plan.status == gf3_affine::PlanStatus::UnsupportedOverlap) {
        ++output.effort.structurally_ineligible_pairs;
        complete_candidate(key);
        continue;
      }
      if (plan.status == gf3_affine::PlanStatus::NoConsequence) {
        ++output.effort.consequence_free_pairs;
        complete_candidate(key);
        continue;
      }
      if (plan.status != gf3_affine::PlanStatus::Planned) {
        output.effort.enumeration_work = meter.used;
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      }
      const uint64_t remaining =
          meter.used <= meter.maximum ? meter.maximum - meter.used : 0;
      if (!remaining) {
        meter.exhausted = true;
        return stopped();
      }
      Gf3AffineLiveRequest pair_request = make_request(
          kernel, limits, first, second,
          kernel.structural_action_sequence + 1);
      const bool duplicate_page_limited =
          remaining < pair_request.maximum_duplicate_scan_work;
      pair_request.maximum_duplicate_scan_work = std::min(
          pair_request.maximum_duplicate_scan_work, remaining);
      const Gf3AffinePreflightResult preflight = gf3_affine_live_preflight(
          kernel, pair_request, *owner, plan);
      if (!meter.charge(preflight.duplicate_scan_work)) return stopped();
      if (!add_work(preflight.duplicate_scan_work, UINT64_MAX,
                    output.effort.duplicate_scan_work) ||
          !add_work(preflight.proof_work, UINT64_MAX,
                    output.effort.proof_preflight_work) ||
          !add_work(preflight.proof_literal_occurrences, UINT64_MAX,
                    output.effort.proof_literal_occurrences) ||
          !add_work(preflight.arena_words, UINT64_MAX,
                    output.effort.arena_words) ||
          !add_work(preflight.journal_work, UINT64_MAX,
                    output.effort.journal_work) ||
          !add_work(preflight.journal.size(), UINT64_MAX,
                    output.effort.journal_bytes))
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      if (preflight.status == Gf3AffinePreflightStatus::Interrupted)
        return terminal_decline(Gf3AffineActionOfferStatus::Interrupted);
      if (preflight.status == Gf3AffinePreflightStatus::ResourceExhausted) {
        if (preflight.duplicate_scan_exhausted && duplicate_page_limited) {
          meter.exhausted = true;
          return stopped();
        }
        ++output.effort.resource_rejected_pairs;
        complete_candidate(key);
        continue;
      }
      if (preflight.status == Gf3AffinePreflightStatus::Invalid)
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      if (preflight.status == Gf3AffinePreflightStatus::Duplicate) {
        ++output.effort.duplicate_pairs_skipped;
        complete_candidate(key);
        continue;
      }
      if (preflight.status != Gf3AffinePreflightStatus::Ready)
        return terminal_decline(Gf3AffineActionOfferStatus::Invalid);
      output.offers.push_back(key);
      complete_candidate(key);
      if (output.offers.size() == window.maximum_offers) {
        const bool final_pair =
            first == equations - 2 && second == equations - 1;
        if (!final_pair) return finish_page();
      }
    }
  }
  output.status = Gf3AffineActionOfferStatus::Ready;
  output.complete = true;
  output.have_resume_after = false;
  output.effort.enumeration_work = meter.used;
  if (output.offers.empty() && output.effort.resource_rejected_pairs) {
    output.status = Gf3AffineActionOfferStatus::ResourceExhausted;
    output.complete = false;
  }
  return output;
}

} // namespace tactix

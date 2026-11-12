#include "rules/semantic_extension/affine_basis_action.hpp"

#include "strategy/action_status_mapping.hpp"

#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/action_epilogue.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

#include "kernel/types.hpp"
#include "proof/proof.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tactix {

struct AffineBasisActionSessionAccess {
  static void commit(AffineBasisActionSession &session, const Kernel &kernel,
                     uint64_t owner_version,
                     const AffineLiveSnapshot *owner,
                     std::vector<AffineBasisConsumedKey> keys) {
    if (!session.kernel_) {
      if (!owner || !owner_version) std::abort();
      session.kernel_ = &kernel;
      session.owner_version_ = owner_version;
      session.owner_snapshot_ = *owner;
    } else if (session.kernel_ != &kernel || owner ||
               session.owner_version_ != owner_version) {
      std::abort();
    }
    session.keys_.swap(keys);
  }
};

namespace {

bool add_work(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (amount > maximum - work) return false;
  work += amount;
  return true;
}

enum class MeasureStatus : uint8_t { Complete, Invalid, Exhausted };

MeasureStatus measure_keys(const std::vector<AffineBasisConsumedKey> &keys,
                           uint64_t maximum, uint64_t &work) {
  for (size_t key_index = 0; key_index < keys.size(); ++key_index) {
    if (key_index) {
      if (!add_work(1, maximum, work)) return MeasureStatus::Exhausted;
      if (!(keys[key_index - 1] < keys[key_index]))
        return MeasureStatus::Invalid;
    }
    if (!affine_basis_key_work(keys[key_index], maximum, work))
      return MeasureStatus::Exhausted;
  }
  return MeasureStatus::Complete;
}

enum class MergeStatus : uint8_t { Complete, Invalid, Exhausted, Overlap };

MergeStatus merge_new_keys(const AffineBasisActionSession &session,
                           const std::vector<AffineBasisConsumedKey> &incoming,
                           const AffineBasisActionLimits &limits,
                           uint64_t &work,
                           std::vector<AffineBasisConsumedKey> &merged) {
  const auto &old = session.keys();
  if (incoming.empty()) return MergeStatus::Invalid;
  const MeasureStatus old_status =
      measure_keys(old, limits.maximum_session_work, work);
  if (old_status != MeasureStatus::Complete)
    return old_status == MeasureStatus::Invalid ? MergeStatus::Invalid
                                                : MergeStatus::Exhausted;
  const MeasureStatus incoming_status =
      measure_keys(incoming, limits.maximum_session_work, work);
  if (incoming_status != MeasureStatus::Complete)
    return incoming_status == MeasureStatus::Invalid ? MergeStatus::Invalid
                                                     : MergeStatus::Exhausted;

  size_t i = 0, j = 0;
  while (i < old.size() && j < incoming.size()) {
    if (!add_work(1, limits.maximum_session_work, work))
      return MergeStatus::Exhausted;
    if (old[i] < incoming[j]) {
      ++i;
    } else if (incoming[j] < old[i]) {
      ++j;
    } else {
      return MergeStatus::Overlap;
    }
  }

  if (old.size() > limits.maximum_session_keys ||
      incoming.size() > limits.maximum_session_keys - old.size() ||
      incoming.size() > std::numeric_limits<size_t>::max() - old.size())
    return MergeStatus::Exhausted;

  // Build the post-commit session before the live transaction mutates.
  merged.reserve(old.size() + incoming.size());
  i = j = 0;
  while (i < old.size() || j < incoming.size()) {
    if (!add_work(1, limits.maximum_session_work, work))
      return MergeStatus::Exhausted;
    if (j == incoming.size() ||
        (i < old.size() && old[i] < incoming[j]))
      merged.push_back(old[i++]);
    else
      merged.push_back(incoming[j++]);
  }
  return MergeStatus::Complete;
}

AffineBasisActionStatus action_status(AffineBasisLiveStatus status) {
  if (const auto failure =
          strategy::map_standard_action_failure<AffineBasisActionStatus>(
              status))
    return *failure;
  switch (status) {
  case AffineBasisLiveStatus::NoStructure:
    return AffineBasisActionStatus::NoStructure;
  case AffineBasisLiveStatus::Prepared:
    return AffineBasisActionStatus::Invalid;
  case AffineBasisLiveStatus::Applied:
    return AffineBasisActionStatus::Applied;
  default: std::abort();
  }
}

AffineBasisLiveRequest make_request(const Kernel &kernel,
                                    const AffineBasisActionSession &session,
                                    const AffineBasisActionLimits &limits,
                                    uint64_t action_id,
                                    const AffineLiveSnapshot &owner,
                                    uint64_t owner_version,
                                    const AffineBasisRowSelection *selection) {
  AffineBasisLiveRequest request;
  request.image = limits.image;
  request.basis = limits.basis;
  request.maximum_snapshot_literals = limits.maximum_snapshot_literals;
  request.maximum_snapshot_work = limits.maximum_snapshot_work;
  request.maximum_proof_additions = limits.maximum_proof_additions;
  request.maximum_proof_deletions = limits.maximum_proof_deletions;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.maximum_external_extent = limits.maximum_external_extent;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  request.owner = &owner;
  request.owner_version = owner_version;
  request.selection = selection;
  if (session.committed_keys()) {
    request.excluded_keys = &session.keys();
    request.maximum_exclusion_work = limits.maximum_session_work;
  }
  return request;
}

AffineLiveRequest snapshot_request(const Kernel &kernel,
                                   const AffineBasisActionLimits &limits) {
  AffineLiveRequest request;
  request.image = limits.image;
  request.maximum_snapshot_literals = limits.maximum_snapshot_literals;
  request.maximum_snapshot_work = limits.maximum_snapshot_work;
  request.records = kernel.structural_provenance;
  request.action_id = kernel.structural_action_sequence + 1;
  request.semantic_id = request.action_id;
  return request;
}

bool duplicate_key(const AffineBasisActionSession &session,
                   const affine_semantic_extension::Image &image,
                   const AffineLiveSnapshot &snapshot,
                   const affine_semantic_extension::BasisRow &row) {
  AffineBasisConsumedKey key;
  if (!affine_basis_consumed_key(image, snapshot, row, key)) return false;
  return std::binary_search(session.keys().begin(), session.keys().end(), key);
}

enum class OfferStatus : uint8_t {
  Complete, NoStructure, Inapplicable, ResourceExhausted, Interrupted, Stale,
  Invalid
};

struct Offer {
  OfferStatus status = OfferStatus::Invalid;
  uint64_t enumeration_work = 0, preflight_recognition_work = 0,
           preflight_basis_work = 0, preflight_additions = 0,
           preflight_deletions = 0, duplicates = 0;
  bool offered = false;
  AffineBasisRowSelection selection;
};

// The first preparable row, ordered contradiction, unit, binary, increasing
// width, then canonical payload; mutation-free.
Offer first_offer(Kernel &kernel, const AffineBasisActionSession &session,
                  const AffineBasisActionLimits &limits,
                  const AffineLiveSnapshot &owner, uint64_t owner_version,
                  AffineBasisLiveTransaction &selected_transaction,
                  AffineBasisLiveResult &selected_result) {
  Offer page;
  if (!limits.valid() || !owner_version) return page;
  if (!affine_basis_action_guard(kernel)) {
    page.status = OfferStatus::Inapplicable;
    return page;
  }
  if (session.bound() && (!session.belongs_to(kernel) ||
                          session.owner_version() != owner_version)) {
    page.status = OfferStatus::Stale;
    return page;
  }

  namespace affine = affine_semantic_extension;
  affine::Image image;
  uint64_t recognition_work = 0;
  bool interrupted = false, exhausted = false;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (amount > limits.image.maximum_elimination_work - recognition_work) {
      exhausted = true;
      return false;
    }
    recognition_work += amount;
    return true;
  };
  const affine::Status recognized =
      affine::recognize(owner.premises, limits.image, charge, image);
  page.enumeration_work = recognition_work;
  if (recognized != affine::Status::Complete) {
    page.status = interrupted ? OfferStatus::Interrupted
                  : exhausted || recognized == affine::Status::Exhausted
                      ? OfferStatus::ResourceExhausted
                  : recognized == affine::Status::NoStructure
                      ? OfferStatus::NoStructure
                      : OfferStatus::Invalid;
    return page;
  }
  affine::BasisPlan plan;
  const affine::BasisPlanStatus planned = affine::build_selectable_basis_plan(
      image.equations, limits.basis,
      [&](uint64_t amount) {
        return add_work(amount, UINT64_MAX, page.enumeration_work) &&
               !kernel_terminating(kernel);
      },
      plan);
  if (planned != affine::BasisPlanStatus::Complete) {
    page.status = planned == affine::BasisPlanStatus::Interrupted
                      ? OfferStatus::Interrupted
                  : planned == affine::BasisPlanStatus::Exhausted
                      ? OfferStatus::ResourceExhausted
                  : planned == affine::BasisPlanStatus::NoStructure
                      ? OfferStatus::NoStructure
                      : OfferStatus::Invalid;
    return page;
  }

  struct Candidate {
    AffineBasisRowSelection selection;
    size_t row = 0;
  };
  std::vector<Candidate> candidates;
  for (size_t i = 0; i < plan.rows.size(); ++i) {
    const affine::BasisRow &row = plan.rows[i];
    if (row.source_equations.size() <
        (row.kind == affine::BasisRowKind::Extended ? 1u : 2u))
      continue;
    Candidate candidate;
    candidate.row = i;
    if (!affine_basis_row_selection(owner_version, image, owner, row,
                                    candidate.selection)) {
      page.status = OfferStatus::Invalid;
      return page;
    }
    candidates.push_back(std::move(candidate));
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return a.selection < b.selection;
            });
  for (size_t i = 1; i < candidates.size(); ++i)
    if (!(candidates[i - 1].selection < candidates[i].selection)) {
      page.status = OfferStatus::Invalid;
      return page;
    }

  page.status = OfferStatus::NoStructure;
  for (const Candidate &candidate : candidates) {
    const uint64_t enumeration =
        1 + candidate.selection.variables.size() +
        candidate.selection.source_equation_ordinals.size();
    if (!add_work(enumeration, UINT64_MAX, page.enumeration_work)) {
      page.status = OfferStatus::ResourceExhausted;
      return page;
    }
    if (duplicate_key(session, image, owner, plan.rows[candidate.row])) {
      ++page.duplicates;
      continue;
    }
    const AffineBasisLiveRequest request = make_request(
        kernel, session, limits, kernel.structural_action_sequence + 1, owner,
        owner_version, &candidate.selection);
    AffineBasisLiveTransaction transaction;
    const AffineBasisLiveResult prepared =
        prepare_affine_basis_live(kernel, request, transaction);
    if (!add_work(prepared.recognition_work, UINT64_MAX,
                  page.preflight_recognition_work) ||
        !add_work(prepared.basis_work, UINT64_MAX,
                  page.preflight_basis_work) ||
        !add_work(prepared.preflight_additions, UINT64_MAX,
                  page.preflight_additions) ||
        !add_work(prepared.preflight_deletions, UINT64_MAX,
                  page.preflight_deletions)) {
      page.status = OfferStatus::ResourceExhausted;
      return page;
    }
    // A per-row proof/resource rejection does not hide later rows that may
    // fit the same configured allowance.
    if (prepared.status == AffineBasisLiveStatus::Prepared) {
      page.status = OfferStatus::Complete;
      selected_transaction = std::move(transaction);
      selected_result = prepared;
      page.offered = true;
      page.selection = candidate.selection;
      return page;
    } else if (prepared.status == AffineBasisLiveStatus::Stale) {
      page.status = OfferStatus::Stale;
    } else if (prepared.status == AffineBasisLiveStatus::ResourceExhausted ||
               prepared.status == AffineBasisLiveStatus::UnsupportedProof) {
      page.status = OfferStatus::ResourceExhausted;
    } else if (prepared.status == AffineBasisLiveStatus::Interrupted) {
      page.status = OfferStatus::Interrupted;
      return page;
    } else if (prepared.status == AffineBasisLiveStatus::Invalid) {
      page.status = OfferStatus::Invalid;
      return page;
    }
  }
  return page;
}

} // namespace

bool AffineBasisActionLimits::valid() const {
  return image.maximum_clauses && image.maximum_groups &&
         image.maximum_patterns && image.maximum_elimination_work &&
         image.maximum_arity && image.maximum_arity <= 8 &&
         image.maximum_intermediate_width && image.maximum_sources &&
         image.maximum_output_width <= 4 &&
         basis.maximum_work && basis.maximum_intermediate_width &&
         basis.maximum_sources && basis.maximum_premises &&
         basis.maximum_nodes && basis.maximum_direct_width >= 2 &&
         basis.maximum_direct_width <= 8 && maximum_snapshot_literals &&
         maximum_snapshot_work && maximum_proof_additions &&
         maximum_journal_bytes && maximum_external_extent &&
         maximum_session_keys && maximum_session_work;
}

bool affine_basis_row_selection_within_limits(
    const AffineBasisRowSelection &selection,
    const AffineBasisActionLimits &limits) {
  namespace affine = affine_semantic_extension;
  const bool extended = selection.kind == affine::BasisRowKind::Extended;
  const size_t maximum_width = extended
                                   ? limits.basis.maximum_intermediate_width
                                   : limits.basis.maximum_direct_width;
  const size_t minimum_sources = extended ? 1 : 2;
  if (!limits.valid() || !selection.owner_version ||
      selection.elimination_profile != 1 ||
      selection.kind > affine::BasisRowKind::Extended ||
      selection.variables.size() > maximum_width ||
      selection.source_equation_ordinals.size() < minimum_sources ||
      selection.source_equation_ordinals.size() >
          limits.basis.maximum_sources ||
      selection.source_equations.size() !=
          selection.source_equation_ordinals.size())
    return false;
  const size_t width = selection.variables.size();
  if ((selection.kind == affine::BasisRowKind::DirectContradiction &&
       (width || !selection.rhs)) ||
      (selection.kind == affine::BasisRowKind::DirectUnit && width != 1) ||
      (selection.kind == affine::BasisRowKind::DirectBinary && width != 2) ||
      (selection.kind == affine::BasisRowKind::DirectSmall &&
       (width < 3 || width > limits.basis.maximum_direct_width)) ||
      (selection.kind == affine::BasisRowKind::Extended &&
       width <= limits.basis.maximum_direct_width))
    return false;
  if (!affine::positive_scope(selection.variables)) return false;
  const uint64_t maximum_equations =
      limits.image.maximum_groups > UINT64_MAX / 2
          ? UINT64_MAX
          : 2 * limits.image.maximum_groups;
  for (size_t i = 0; i < selection.source_equation_ordinals.size(); ++i)
    if (selection.source_equation_ordinals[i] >= maximum_equations ||
        (i && selection.source_equation_ordinals[i - 1] >=
                  selection.source_equation_ordinals[i]))
      return false;

  uint64_t clauses = 0, literals = 0, snapshot_work = 0;
  for (const auto &equation : selection.source_equations) {
    const size_t arity = equation.variables.size();
    if (!arity || arity > limits.image.maximum_arity || arity >= 64 ||
        equation.premises.size() != (uint64_t{1} << (arity - 1)))
      return false;
    if (!affine::positive_scope(equation.variables)) return false;
    for (size_t premise_index = 0;
         premise_index < equation.premises.size(); ++premise_index) {
      const auto &premise = equation.premises[premise_index];
      if (!premise.native_clause_id || premise.clause.size() != arity ||
          clauses == limits.image.maximum_clauses ||
          clauses == limits.image.maximum_patterns ||
          clauses == limits.basis.maximum_premises ||
          premise.clause.size() >
              limits.maximum_snapshot_literals - literals ||
          premise.clause.size() + 1 >
              limits.maximum_snapshot_work - snapshot_work)
        return false;
      if (premise_index &&
          !(equation.premises[premise_index - 1] < premise))
        return false;
      ++clauses;
      literals += premise.clause.size();
      snapshot_work += premise.clause.size() + 1;
      unsigned negative_parity = 0;
      for (size_t i = 0; i < arity; ++i) {
        const int32_t literal = premise.clause[i];
        if (!literal || literal == INT32_MIN ||
            (i && premise.clause[i - 1] >= literal))
          return false;
        negative_parity ^= literal < 0;
      }
      for (const int32_t variable : equation.variables)
        if (std::count_if(premise.clause.begin(), premise.clause.end(),
                          [variable](int32_t literal) {
                            return external_variable_of(literal) ==
                                   static_cast<uint32_t>(variable);
                          }) != 1)
          return false;
      if (negative_parity != static_cast<unsigned>(!equation.rhs))
        return false;
    }
  }
  return clauses <= limits.image.maximum_patterns;
}

RuleEligibility affine_basis_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, false)
      .identity()
      .semantic_candidate()
      .clausal_certificate()
      .finish();
}

static AffineBasisActionResult apply_basis_action(
    Kernel &kernel, AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits,
    const AffineBasisRowSelection *requested_selection) {
  AffineBasisActionResult output;
  // A decline reports the live status and its action-status image.
  const auto decline = [&](AffineBasisLiveStatus live) {
    output.status = action_status(live);
    output.live.status = live;
    return output;
  };
  output.committed_keys = session.committed_keys();
  if (!affine_basis_action_guard(kernel))
    return decline(AffineBasisLiveStatus::Inapplicable);
  if (!limits.valid()) return output;
  if (session.bound() && !session.belongs_to(kernel))
    return decline(AffineBasisLiveStatus::Stale);

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;

  AffineLiveSnapshot captured_owner;
  const AffineLiveSnapshot *owner = session.owner();
  uint64_t owner_version = session.owner_version();
  if (!owner) {
    if (!capture_affine_live_snapshot(kernel, snapshot_request(kernel, limits),
                                      captured_owner)) {
      output.status = terminating_status(
          kernel, AffineBasisActionStatus::ResourceExhausted);
      output.live.status =
          terminating_status(kernel, AffineBasisLiveStatus::ResourceExhausted);
      return output;
    }
    owner = &captured_owner;
    owner_version = 1;
  }
  Offer page;
  AffineBasisLiveTransaction offered_transaction;
  AffineBasisLiveResult offered_result;
  if (!requested_selection)
    page = first_offer(kernel, session, limits, *owner, owner_version,
                       offered_transaction, offered_result);
  if (!requested_selection && !page.offered) {
    switch (page.status) {
    case OfferStatus::NoStructure:
      output.status = page.duplicates ? AffineBasisActionStatus::AlreadyPublished
                                      : AffineBasisActionStatus::NoStructure;
      output.live.status = AffineBasisLiveStatus::NoStructure;
      output.live.excluded_rows = page.duplicates;
      break;
    case OfferStatus::Inapplicable:
      return decline(AffineBasisLiveStatus::Inapplicable);
    case OfferStatus::ResourceExhausted:
      return decline(AffineBasisLiveStatus::ResourceExhausted);
    case OfferStatus::Interrupted:
      return decline(AffineBasisLiveStatus::Interrupted);
    case OfferStatus::Stale:
      return decline(AffineBasisLiveStatus::Stale);
    case OfferStatus::Complete:
    case OfferStatus::Invalid:
      return decline(AffineBasisLiveStatus::Invalid);
    }
    return output;
  }
  output.selection = requested_selection ? *requested_selection
                                         : page.selection;
  const AffineBasisLiveRequest request = make_request(
      kernel, session, limits, output.proposed_action_id, *owner, owner_version,
      &output.selection);
  AffineBasisLiveTransaction transaction;
  if (requested_selection) {
    output.live = prepare_affine_basis_live(kernel, request, transaction);
  } else {
    transaction = std::move(offered_transaction);
    output.live = offered_result;
  }
  output.status = action_status(output.live.status);
  output.session_work = output.live.exclusion_work;
  if (output.live.status == AffineBasisLiveStatus::NoStructure &&
      output.live.excluded_rows) {
    output.status = AffineBasisActionStatus::AlreadyPublished;
    return output;
  }
  if (output.live.status != AffineBasisLiveStatus::Prepared) return output;

  output.transaction_keys = transaction.consumed_keys().size();
  std::vector<AffineBasisConsumedKey> merged;
  const MergeStatus merge =
      merge_new_keys(session, transaction.consumed_keys(), limits,
                     output.session_work, merged);
  if (merge == MergeStatus::Invalid)
    return decline(AffineBasisLiveStatus::Invalid);
  if (merge == MergeStatus::Exhausted)
    return decline(AffineBasisLiveStatus::ResourceExhausted);
  // Overlap with the exclusion set given to the live transaction is a bug.
  if (merge == MergeStatus::Overlap)
    return decline(AffineBasisLiveStatus::Invalid);

  output.live = publish_affine_basis_live(kernel, request, transaction);
  output.status = action_status(output.live.status);
  output.result = output.live.result;
  if (output.live.status != AffineBasisLiveStatus::Applied ||
      output.live.result == ApplyResult::NoOp)
    return output;
  if (output.live.result != ApplyResult::Ok &&
      output.live.result != ApplyResult::Unsat)
    std::abort();

  AffineBasisActionSessionAccess::commit(
      session, kernel, owner_version, session.bound() ? nullptr : owner,
      std::move(merged));
  output.committed_keys = session.committed_keys();
  if (output.live.result == ApplyResult::Ok) {
    stage_theory_variables_or_abort(
        kernel, strategy::SemanticVariableProducer::Affine,
        output.proposed_action_id, output.live.existing_theory_variables,
        output.live.introduced_theory_variables);
    output.candidate_staged = true;
  }
  kernel.structural_action_sequence = output.proposed_action_id;
  output.committed_action_id = output.proposed_action_id;
  return output;
}

AffineBasisActionResult affine_basis_action_apply(
    Kernel &kernel, AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits) {
  return apply_basis_action(kernel, session, limits, nullptr);
}

AffineBasisActionResult affine_basis_action_apply(
    Kernel &kernel, AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits,
    const AffineBasisRowSelection &selection) {
  return apply_basis_action(kernel, session, limits, &selection);
}

} // namespace tactix

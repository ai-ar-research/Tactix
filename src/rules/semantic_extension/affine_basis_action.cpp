#include "rules/semantic_extension/affine_basis_action.hpp"

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
    const auto &key = keys[key_index];
    if (!add_work(1, maximum, work) ||
        !add_work(key.variables.size(), maximum, work))
      return MeasureStatus::Exhausted;
    for (const auto &equation : key.source_equations) {
      if (!add_work(1, maximum, work) ||
          !add_work(equation.variables.size(), maximum, work))
        return MeasureStatus::Exhausted;
      for (const auto &premise : equation.premises)
        if (!add_work(1, maximum, work) ||
            !add_work(premise.clause.size(), maximum, work))
          return MeasureStatus::Exhausted;
    }
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

  // Allocate and build the complete post-commit session before the live
  // transaction mutates the kernel.  The final session commit is a swap.
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
  switch (status) {
  case AffineBasisLiveStatus::Invalid:
    return AffineBasisActionStatus::Invalid;
  case AffineBasisLiveStatus::Inapplicable:
    return AffineBasisActionStatus::Inapplicable;
  case AffineBasisLiveStatus::NoStructure:
    return AffineBasisActionStatus::NoStructure;
  case AffineBasisLiveStatus::ResourceExhausted:
    return AffineBasisActionStatus::ResourceExhausted;
  case AffineBasisLiveStatus::Interrupted:
    return AffineBasisActionStatus::Interrupted;
  case AffineBasisLiveStatus::UnsupportedProof:
    return AffineBasisActionStatus::UnsupportedProof;
  case AffineBasisLiveStatus::Stale:
    return AffineBasisActionStatus::Stale;
  case AffineBasisLiveStatus::Prepared:
    return AffineBasisActionStatus::Invalid;
  case AffineBasisLiveStatus::Applied:
    return AffineBasisActionStatus::Applied;
  }
  std::abort();
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

AffineBasisOfferPage offer_page(
    Kernel &kernel, const AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits,
    const AffineBasisOfferCursor &cursor, uint32_t maximum_offers,
    const AffineLiveSnapshot &owner, uint64_t owner_version,
    AffineBasisLiveTransaction *selected_transaction = nullptr,
    AffineBasisLiveResult *selected_result = nullptr) {
  AffineBasisOfferPage page;
  page.owner_version = owner_version;
  if (!maximum_offers || !limits.valid() || !owner_version) return page;
  if (!affine_basis_action_guard(kernel)) {
    page.status = AffineBasisOfferStatus::Inapplicable;
    return page;
  }
  if (session.bound() && (!session.belongs_to(kernel) ||
                          session.owner_version() != owner_version)) {
    page.status = AffineBasisOfferStatus::Stale;
    return page;
  }
  if (cursor.present &&
      (cursor.last_examined.owner_version != owner_version ||
       cursor.last_examined.elimination_profile != 1))
    return page;

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
    page.status = interrupted ? AffineBasisOfferStatus::Interrupted
                  : exhausted || recognized == affine::Status::Exhausted
                      ? AffineBasisOfferStatus::ResourceExhausted
                  : recognized == affine::Status::NoStructure
                      ? AffineBasisOfferStatus::NoStructure
                      : AffineBasisOfferStatus::Invalid;
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
                      ? AffineBasisOfferStatus::Interrupted
                  : planned == affine::BasisPlanStatus::Exhausted
                      ? AffineBasisOfferStatus::ResourceExhausted
                  : planned == affine::BasisPlanStatus::NoStructure
                      ? AffineBasisOfferStatus::NoStructure
                      : AffineBasisOfferStatus::Invalid;
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
      page.status = AffineBasisOfferStatus::Invalid;
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
      page.status = AffineBasisOfferStatus::Invalid;
      return page;
    }
  if (cursor.present) {
    const auto found = std::lower_bound(
        candidates.begin(), candidates.end(), cursor.last_examined,
        [](const Candidate &candidate,
           const AffineBasisRowSelection &selection) {
          return candidate.selection < selection;
        });
    if (found == candidates.end() ||
        !(found->selection == cursor.last_examined)) {
      page.status = AffineBasisOfferStatus::Invalid;
      return page;
    }
  }

  page.status = AffineBasisOfferStatus::NoStructure;
  for (size_t i = 0; i < candidates.size(); ++i) {
    const Candidate &candidate = candidates[i];
    if (cursor.present &&
        !(cursor.last_examined < candidate.selection))
      continue;
    ++page.candidates_examined;
    const uint64_t enumeration =
        1 + candidate.selection.variables.size() +
        candidate.selection.source_equation_ordinals.size();
    if (!add_work(enumeration, UINT64_MAX, page.enumeration_work)) {
      page.status = AffineBasisOfferStatus::ResourceExhausted;
      return page;
    }
    page.continuation = {true, candidate.selection};
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
      page.status = AffineBasisOfferStatus::ResourceExhausted;
      return page;
    }
    if (prepared.status == AffineBasisLiveStatus::Prepared) {
      page.status = AffineBasisOfferStatus::Complete;
      if (page.offers.empty() && selected_transaction && selected_result) {
        *selected_transaction = std::move(transaction);
        *selected_result = prepared;
      }
      page.offers.push_back(candidate.selection);
      if (page.offers.size() == maximum_offers) {
        page.more = i + 1 < candidates.size();
        break;
      }
    } else if (prepared.status == AffineBasisLiveStatus::NoStructure) {
      ++page.covered;
    } else if (prepared.status == AffineBasisLiveStatus::Stale) {
      ++page.stale;
      if (page.offers.empty()) page.status = AffineBasisOfferStatus::Stale;
    } else if (prepared.status == AffineBasisLiveStatus::ResourceExhausted ||
               prepared.status == AffineBasisLiveStatus::UnsupportedProof) {
      if (page.offers.empty())
        page.status = AffineBasisOfferStatus::ResourceExhausted;
    } else if (prepared.status == AffineBasisLiveStatus::Interrupted) {
      page.status = AffineBasisOfferStatus::Interrupted;
      return page;
    } else if (prepared.status == AffineBasisLiveStatus::Invalid) {
      page.status = AffineBasisOfferStatus::Invalid;
      return page;
    }
    // Per-row proof/resource rejection advances the continuation but does not
    // hide later rows that may fit the same configured allowance.
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
  for (size_t i = 0; i < width; ++i)
    if (selection.variables[i] <= 0 ||
        (i && selection.variables[i - 1] >= selection.variables[i]))
      return false;
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
    for (size_t i = 0; i < arity; ++i)
      if (equation.variables[i] <= 0 ||
          (i && equation.variables[i - 1] >= equation.variables[i]))
        return false;
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
                            return (literal < 0 ? -literal : literal) ==
                                   variable;
                          }) != 1)
          return false;
      if (negative_parity != static_cast<unsigned>(!equation.rhs))
        return false;
    }
  }
  return clauses <= limits.image.maximum_patterns;
}

RuleEligibility affine_basis_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
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
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->terminal_pbp()));
}

AffineBasisOfferPage affine_basis_action_offers(
    Kernel &kernel, const AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits,
    const AffineBasisOfferCursor &cursor, uint32_t maximum_offers) {
  AffineBasisOfferPage page;
  if (!maximum_offers || !limits.valid()) return page;
  if (session.bound()) {
    if (!session.belongs_to(kernel) || !session.owner()) {
      page.status = AffineBasisOfferStatus::Stale;
      return page;
    }
    return offer_page(kernel, session, limits, cursor, maximum_offers,
                      *session.owner(), session.owner_version());
  }
  if (!affine_basis_action_guard(kernel)) {
    page.status = AffineBasisOfferStatus::Inapplicable;
    return page;
  }
  AffineLiveSnapshot owner;
  if (!capture_affine_live_snapshot(kernel, snapshot_request(kernel, limits),
                                    owner)) {
    page.status = kernel_terminating(kernel)
                      ? AffineBasisOfferStatus::Interrupted
                      : AffineBasisOfferStatus::ResourceExhausted;
    return page;
  }
  return offer_page(kernel, session, limits, cursor, maximum_offers, owner, 1);
}

static AffineBasisActionResult apply_basis_action(
    Kernel &kernel, AffineBasisActionSession &session,
    const AffineBasisActionLimits &limits,
    const AffineBasisRowSelection *requested_selection) {
  AffineBasisActionResult output;
  output.committed_keys = session.committed_keys();
  if (!affine_basis_action_guard(kernel)) {
    output.status = AffineBasisActionStatus::Inapplicable;
    output.live.status = AffineBasisLiveStatus::Inapplicable;
    return output;
  }
  if (!limits.valid()) return output;
  if (session.bound() && !session.belongs_to(kernel)) {
    output.status = AffineBasisActionStatus::Stale;
    output.live.status = AffineBasisLiveStatus::Stale;
    return output;
  }

  output.proposed_action_id = kernel.structural_action_sequence + 1;
  output.semantic_id = output.proposed_action_id;

  AffineLiveSnapshot captured_owner;
  const AffineLiveSnapshot *owner = session.owner();
  uint64_t owner_version = session.owner_version();
  if (!owner) {
    if (!capture_affine_live_snapshot(kernel, snapshot_request(kernel, limits),
                                      captured_owner)) {
      output.status = kernel_terminating(kernel)
                          ? AffineBasisActionStatus::Interrupted
                          : AffineBasisActionStatus::ResourceExhausted;
      output.live.status = kernel_terminating(kernel)
                               ? AffineBasisLiveStatus::Interrupted
                               : AffineBasisLiveStatus::ResourceExhausted;
      return output;
    }
    owner = &captured_owner;
    owner_version = 1;
  }
  AffineBasisOfferPage page;
  AffineBasisLiveTransaction offered_transaction;
  AffineBasisLiveResult offered_result;
  if (!requested_selection)
    page = offer_page(kernel, session, limits, {}, 1, *owner, owner_version,
                      &offered_transaction, &offered_result);
  if (!requested_selection && page.offers.empty()) {
    switch (page.status) {
    case AffineBasisOfferStatus::NoStructure:
      output.status = page.duplicates ? AffineBasisActionStatus::AlreadyPublished
                                      : AffineBasisActionStatus::NoStructure;
      output.live.status = AffineBasisLiveStatus::NoStructure;
      output.live.excluded_rows = page.duplicates;
      break;
    case AffineBasisOfferStatus::Inapplicable:
      output.status = AffineBasisActionStatus::Inapplicable;
      output.live.status = AffineBasisLiveStatus::Inapplicable;
      break;
    case AffineBasisOfferStatus::ResourceExhausted:
      output.status = AffineBasisActionStatus::ResourceExhausted;
      output.live.status = AffineBasisLiveStatus::ResourceExhausted;
      break;
    case AffineBasisOfferStatus::Interrupted:
      output.status = AffineBasisActionStatus::Interrupted;
      output.live.status = AffineBasisLiveStatus::Interrupted;
      break;
    case AffineBasisOfferStatus::Stale:
      output.status = AffineBasisActionStatus::Stale;
      output.live.status = AffineBasisLiveStatus::Stale;
      break;
    case AffineBasisOfferStatus::Complete:
    case AffineBasisOfferStatus::Invalid:
      output.status = AffineBasisActionStatus::Invalid;
      output.live.status = AffineBasisLiveStatus::Invalid;
      break;
    }
    return output;
  }
  output.selection = requested_selection ? *requested_selection
                                         : page.offers.front();
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
  if (merge == MergeStatus::Invalid) {
    output.status = AffineBasisActionStatus::Invalid;
    output.live.status = AffineBasisLiveStatus::Invalid;
    return output;
  }
  if (merge == MergeStatus::Exhausted) {
    output.status = AffineBasisActionStatus::ResourceExhausted;
    output.live.status = AffineBasisLiveStatus::ResourceExhausted;
    return output;
  }
  if (merge == MergeStatus::Overlap) {
    // The live transaction received this exact exclusion set before planning;
    // surviving overlap is an internal filtering/identity inconsistency.
    output.status = AffineBasisActionStatus::Invalid;
    output.live.status = AffineBasisLiveStatus::Invalid;
    return output;
  }

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
    std::vector<strategy::SemanticVariable> variables;
    variables.reserve(output.live.existing_theory_variables.size() +
                      output.live.introduced_theory_variables.size());
    for (int32_t external : output.live.existing_theory_variables) {
      if (external <= 0) std::abort();
      variables.push_back({static_cast<uint32_t>(external),
                           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
    for (int32_t external : output.live.introduced_theory_variables) {
      if (external <= 0) std::abort();
      variables.push_back(
          {static_cast<uint32_t>(external),
           strategy::SemanticVariableRole::IntroducedTheory, 1});
    }
    if (!strategy::stage_semantic_variable_candidate(
            kernel, strategy::SemanticVariableProducer::Affine,
            output.proposed_action_id, variables))
      std::abort();
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

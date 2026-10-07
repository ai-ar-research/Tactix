#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <sstream>

namespace tactix {
namespace {

using namespace affine_semantic_extension;

bool add(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
}

bool covers_complete_snapshot(const AffineLiveSnapshot &owner,
                              const Image &image, uint64_t expected_clauses,
                              uint64_t &covered) {
  covered = 0;
  std::vector<uint8_t> seen(owner.premises.size());
  for (const Equation &equation : image.equations)
    for (const uint64_t occurrence : equation.premises) {
      if (!occurrence || occurrence > seen.size() || seen[occurrence - 1])
        return false;
      seen[occurrence - 1] = 1;
      ++covered;
    }
  return owner.premises.size() == expected_clauses &&
         covered == owner.premises.size();
}

MatrixLimits matrix_limits(const strategy::AffineConsequenceParameters &p) {
  return {p.basis_maximum_work,
          p.basis_maximum_nodes,
          p.maximum_external_extent,
          p.basis_maximum_intermediate_width,
          p.basis_maximum_sources,
          p.maximum_external_extent};
}

MatrixTransactionLimits transaction_limits(
    const strategy::AffineConsequenceParameters &p) {
  return {p.basis_maximum_work,
          p.maximum_proof_additions,
          p.maximum_proof_deletions,
          p.maximum_external_extent,
          p.basis_maximum_work};
}

AffineLiveRequest capture_request(
    Kernel &kernel, const strategy::AffineConsequenceParameters &p) {
  AffineLiveRequest request;
  request.image = {p.maximum_clauses,
                   p.maximum_groups,
                   p.maximum_patterns,
                   p.maximum_elimination_work,
                   p.maximum_arity,
                   p.maximum_intermediate_width,
                   p.maximum_sources,
                   p.maximum_output_width};
  request.maximum_snapshot_literals = p.maximum_snapshot_literals;
  request.maximum_snapshot_work = p.maximum_snapshot_work;
  request.maximum_proof_additions = p.maximum_proof_additions;
  request.maximum_proof_deletions = p.maximum_proof_deletions;
  request.maximum_journal_bytes = p.maximum_journal_bytes;
  request.action_id = kernel.structural_action_sequence + 1;
  request.semantic_id = request.action_id;
  request.records = kernel.structural_provenance;
  return request;
}

enum class TrailStatus { Complete, Invalid, Exhausted, Interrupted };

TrailStatus exact_trail(const Kernel &kernel,
                        const std::vector<External> &support,
                        uint64_t maximum,
                        uint64_t &work,
                        std::vector<MatrixAssignment> &out) {
  out.clear();
  out.reserve(support.size());
  for (const External variable : support) {
    if (kernel_terminating(kernel)) return TrailStatus::Interrupted;
    if (!add(1, maximum, work)) return TrailStatus::Exhausted;
    if (variable <= 0 || variable == INT32_MIN)
      return TrailStatus::Invalid;
    const int8_t value = kernel_external_value(kernel, internalize(variable));
    if (value) out.push_back({variable, value > 0});
  }
  return TrailStatus::Complete;
}

std::string event_identity(uint64_t owner, const MatrixEvent &event) {
  std::ostringstream out;
  out << "am1:o=" << owner << ";k=" << unsigned(event.kind)
      << ";v=" << event.implied_variable << ':' << event.implied_value
      << ";s=";
  for (size_t i = 0; i < event.row.source_equations.size(); ++i) {
    if (i) out << ',';
    out << event.row.source_equations[i];
  }
  out << ";r=";
  for (size_t i = 0; i < event.reason.size(); ++i) {
    if (i) out << ',';
    out << event.reason[i];
  }
  return out.str();
}

bool make_initialization_journal(uint64_t owner, const AffineMatrix &matrix,
                                 uint64_t maximum, std::string &journal) {
  journal.clear();
  const auto append = [&](const std::string &text) {
    if (journal.size() > maximum || text.size() > maximum - journal.size())
      return false;
    journal += text;
    return true;
  };
  const auto list = [&](const auto &values) {
    if (!append("[")) return false;
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i]))) return false;
    return append("]");
  };
  if (!append("{\"kind\":\"affine-matrix-session\",\"owner\":" +
              std::to_string(owner) + ",\"admission\":\"" +
              (matrix.complete_admission() ? "complete" : "partial") +
              "\",\"work\":" + std::to_string(matrix.work()) +
              ",\"admitted_sources\":"))
    return false;
  if (!list(matrix.admitted_sources()) || !append(",\"rows\":["))
    return false;
  for (size_t i = 0; i < matrix.admitted_rows().size(); ++i) {
    const MatrixRow &row = matrix.admitted_rows()[i];
    if (!append((i ? "," : "") + std::string("{\"variables\":")) ||
        !list(row.variables) ||
        !append(",\"rhs\":" + std::to_string(row.rhs) +
                ",\"sources\":") ||
        !list(row.source_equations) || !append("}"))
      return false;
  }
  return append("]}\n");
}

AffineMatrixServiceStatus live_status(MatrixLiveStatus status) {
  switch (status) {
  case MatrixLiveStatus::Applied: return AffineMatrixServiceStatus::Applied;
  case MatrixLiveStatus::Inapplicable:
  case MatrixLiveStatus::UnsupportedOutcome:
    return AffineMatrixServiceStatus::Inapplicable;
  case MatrixLiveStatus::ResourceExhausted:
    return AffineMatrixServiceStatus::ResourceExhausted;
  case MatrixLiveStatus::Interrupted:
    return AffineMatrixServiceStatus::Interrupted;
  case MatrixLiveStatus::UnsupportedProof:
    return AffineMatrixServiceStatus::UnsupportedProof;
  case MatrixLiveStatus::Stale: return AffineMatrixServiceStatus::Stale;
  case MatrixLiveStatus::Invalid:
  case MatrixLiveStatus::Prepared:
  case MatrixLiveStatus::AlreadyApplied:
    return AffineMatrixServiceStatus::Invalid;
  }
  return AffineMatrixServiceStatus::Invalid;
}

void record_init(Kernel &kernel, const AffineMatrixInitResult &result) {
  ++kernel.stats.affine_matrix_init_attempts;
  kernel.stats.affine_matrix_init_snapshot_work += result.snapshot_work;
  kernel.stats.affine_matrix_init_recognition_work += result.recognition_work;
  kernel.stats.affine_matrix_init_matrix_work += result.matrix_work;
  kernel.stats.affine_matrix_init_premise_retention_work += result.premise_retention_work;
  if (result.status == AffineMatrixInitStatus::Applied ||
      result.status == AffineMatrixInitStatus::AppliedPartial)
    ++kernel.stats.affine_matrix_init_applied;
  if (result.status == AffineMatrixInitStatus::AppliedPartial)
    ++kernel.stats.affine_matrix_init_applied_partial;
  else if (result.status == AffineMatrixInitStatus::IncompleteImage)
    ++kernel.stats.affine_matrix_init_incomplete_image;
  else if (result.status == AffineMatrixInitStatus::IncompleteMatrix)
    ++kernel.stats.affine_matrix_init_incomplete_matrix;
  else if (result.status == AffineMatrixInitStatus::ResourceExhausted)
    ++kernel.stats.affine_matrix_init_exhausted;
  else if (result.status == AffineMatrixInitStatus::Interrupted)
    ++kernel.stats.affine_matrix_init_interrupted;
}

void record_service(Kernel &kernel, const AffineMatrixServiceResult &result) {
  ++kernel.stats.affine_matrix_service_calls;
  kernel.stats.affine_matrix_service_lifecycle_work += result.lifecycle_work;
  kernel.stats.affine_matrix_service_trail_capture_work +=
      result.trail_capture_work;
  kernel.stats.affine_matrix_service_sync_work += result.synchronization_work;
  kernel.stats.affine_matrix_service_extension_hits +=
      result.synchronized && result.synchronization_incremental;
  kernel.stats.affine_matrix_service_rebuilds +=
      result.synchronized && !result.synchronization_incremental &&
      !result.synchronization_restored_predecessor;
  kernel.stats.affine_matrix_service_rebuild_backtrack +=
      result.synchronized && !result.synchronization_restored_predecessor &&
      result.synchronization_kind ==
                                 MatrixSynchronizationKind::Backtrack;
  kernel.stats.affine_matrix_service_rebuild_reassignment +=
      result.synchronized && result.synchronization_kind ==
                                 MatrixSynchronizationKind::Reassignment;
  kernel.stats.affine_matrix_service_rebuild_partial +=
      result.synchronized && result.synchronization_kind ==
                                 MatrixSynchronizationKind::PriorPartial;
  kernel.stats.affine_matrix_service_retained_assignments +=
      result.synchronization_prefix;
  kernel.stats.affine_matrix_service_conditioned_assignments +=
      result.synchronization_conditioned;
  kernel.stats.affine_matrix_service_session_work += result.session_work;
  kernel.stats.affine_matrix_service_planner_work += result.live.planner_work;
  kernel.stats.affine_matrix_service_revalidation_work +=
      result.live.revalidation_work;
  kernel.stats.affine_matrix_service_proof_additions += result.live.additions;
  kernel.stats.affine_matrix_service_proof_deletions += result.live.deletions;
  kernel.stats.affine_matrix_service_proof_cache_hits +=
      result.live.proof_cache_hit;
  kernel.stats.affine_matrix_service_proof_cache_inserts +=
      result.live.proof_cache_insert;
  kernel.stats.affine_matrix_service_helper_extent += result.live.helper_extent;
  kernel.stats.affine_matrix_service_journal_bytes += result.live.journal_bytes;
  if (result.status == AffineMatrixServiceStatus::Applied)
    ++kernel.stats.affine_matrix_service_applied;
  else if (result.status == AffineMatrixServiceStatus::NoEvent)
    ++kernel.stats.affine_matrix_service_no_event;
  else if (result.status == AffineMatrixServiceStatus::Duplicate)
    ++kernel.stats.affine_matrix_service_duplicate;
  else if (result.status == AffineMatrixServiceStatus::ResourceExhausted)
    ++kernel.stats.affine_matrix_service_exhausted;
  else if (result.status == AffineMatrixServiceStatus::Interrupted)
    ++kernel.stats.affine_matrix_service_interrupted;
  else if (result.status == AffineMatrixServiceStatus::Stale)
    ++kernel.stats.affine_matrix_service_stale;
}

} // namespace

void AffineMatrixSearchSession::remember_terminal_initialization_failure(
    const Kernel &kernel,
    const strategy::AffineConsequenceParameters &parameters) {
  failed_owner_ = &kernel;
  failed_arena_generation_ = kernel.arena_generation;
  failed_touch_epoch_ = kernel.touch_epoch;
  failed_touch_fixed_head_ = kernel.touch_fixed_head;
  failed_trail_size_ = kernel.trail.size();
  failed_next_clause_id_ = kernel.next_clause_id;
  failed_parameters_ = parameters;
  have_terminal_initialization_failure_ = true;
}

bool AffineMatrixSearchSession::initialization_retry_suppressed(
    const Kernel &kernel,
    const strategy::AffineConsequenceParameters &parameters) const {
  return have_terminal_initialization_failure_ && failed_owner_ == &kernel &&
         failed_arena_generation_ == kernel.arena_generation &&
         failed_touch_epoch_ == kernel.touch_epoch &&
         failed_touch_fixed_head_ == kernel.touch_fixed_head &&
         failed_trail_size_ == kernel.trail.size() &&
         failed_next_clause_id_ == kernel.next_clause_id &&
         failed_parameters_ == parameters;
}

void AffineMatrixSearchSession::disable(Kernel &kernel) {
  if (owner_ == &kernel && proof_ && kernel.proof == proof_)
    proof_->release_premise_groups(premise_leases_, kernel.next_clause_id);
  *this = AffineMatrixSearchSession{};
}

bool AffineMatrixSearchSession::attached_to(const Kernel &kernel) const {
  return enabled() && belongs_to(kernel) &&
         kernel.affine_matrix_search_session == this &&
         records_ == kernel.structural_provenance && proof_ == kernel.proof &&
         (!kernel.proof || proof_format_ == kernel.proof->format());
}

RuleEligibility affine_matrix_initialize_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("session-attached", kernel.affine_matrix_search_session != nullptr)
      .fact("provenance-bound", kernel.structural_provenance != nullptr)
      .fact("clausal-proof",
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->pb()));
}

AffineMatrixInitResult affine_matrix_search_initialize(
    Kernel &kernel, AffineMatrixSearchSession &session,
    const strategy::AffineConsequenceParameters &parameters,
    AffineMatrixAdmission admission,
    const AffineMatrixPersistenceLimits &persistence) {
  AffineMatrixInitResult result;
  if (!affine_matrix_initialize_guard(kernel)) {
    result.status = AffineMatrixInitStatus::Inapplicable;
    record_init(kernel, result);
    return result;
  }
  if (!parameters.valid() ||
      parameters.semantics !=
          strategy::AffineConsequenceSemantics::CertifiedBasisV2 ||
      (admission != AffineMatrixAdmission::AuthenticatedSubimage &&
       admission != AffineMatrixAdmission::CompleteSnapshot)) {
    record_init(kernel, result);
    return result;
  }
  if (persistence.maximum_retained_bytes && (!kernel.proof || kernel.model_extended)) {
    result.status = AffineMatrixInitStatus::Inapplicable;
    record_init(kernel, result);
    return result;
  }
  if (!persistence.maximum_retained_bytes &&
      session.initialization_retry_suppressed(kernel, parameters)) {
    result.status = AffineMatrixInitStatus::ResourceExhausted;
    record_init(kernel, result);
    return result;
  }
  if (session.enabled() && !session.belongs_to(kernel)) {
    result.status = AffineMatrixInitStatus::Stale;
    record_init(kernel, result);
    return result;
  }
  if (kernel.structural_action_sequence == UINT64_MAX ||
      session.owner_version_ == UINT64_MAX) {
    result.status = AffineMatrixInitStatus::ResourceExhausted;
    session.remember_terminal_initialization_failure(kernel, parameters);
    record_init(kernel, result);
    return result;
  }

  AffineLiveSnapshot owner;
  const auto capture = capture_request(kernel, parameters);
  if (!capture_affine_live_snapshot(kernel, capture, owner)) {
    result.status = kernel_terminating(kernel)
                        ? AffineMatrixInitStatus::Interrupted
                        : AffineMatrixInitStatus::ResourceExhausted;
    if (result.status == AffineMatrixInitStatus::ResourceExhausted)
      session.remember_terminal_initialization_failure(kernel, parameters);
    record_init(kernel, result);
    return result;
  }
  result.snapshot_clauses = owner.sources.size();
  result.snapshot_literals = owner.literals;
  result.snapshot_work = owner.work;

  Image image;
  uint64_t recognition = 0;
  bool recognition_exhausted = false;
  const Status recognized = recognize(
      owner.premises, capture.image,
      [&](uint64_t amount) {
        if (kernel_terminating(kernel)) return false;
        if (!add(amount, parameters.maximum_elimination_work, recognition)) {
          recognition_exhausted = true;
          return false;
        }
        return true;
      },
      image);
  result.recognition_work = recognition;
  if (recognized != Status::Complete) {
    result.status = recognized == Status::NoStructure
                        ? AffineMatrixInitStatus::NoStructure
                    : kernel_terminating(kernel)
                        ? AffineMatrixInitStatus::Interrupted
                    : recognized == Status::Exhausted || recognition_exhausted
                        ? AffineMatrixInitStatus::ResourceExhausted
                        : AffineMatrixInitStatus::Invalid;
    if (result.status == AffineMatrixInitStatus::NoStructure ||
        result.status == AffineMatrixInitStatus::ResourceExhausted)
      session.remember_terminal_initialization_failure(kernel, parameters);
    record_init(kernel, result);
    return result;
  }

  if (admission == AffineMatrixAdmission::CompleteSnapshot &&
      !covers_complete_snapshot(owner, image,
                                kernel_irredundant_clauses(kernel),
                                result.covered_clauses)) {
    result.status = AffineMatrixInitStatus::IncompleteImage;
    record_init(kernel, result);
    return result;
  }
  if (admission == AffineMatrixAdmission::AuthenticatedSubimage)
    (void)covers_complete_snapshot(owner, image,
                                   kernel_irredundant_clauses(kernel),
                                   result.covered_clauses);

  if (persistence.maximum_retained_bytes) image.consequences.clear();
  std::vector<uint64_t> retained_leases;
  size_t retained_equations = 0;
  if (persistence.maximum_retained_bytes && session.enabled()) {
    if (!session.attached_to(kernel) || !session.persistent()) {
      result.status = AffineMatrixInitStatus::Stale;
      record_init(kernel, result); return result;
    }
    Image combined;
    for (size_t i = 0; i < session.image_.equations.size(); ++i) {
      const auto &equation = session.image_.equations[i];
      uint64_t work = 1 + equation.variables.size() + equation.premises.size();
      for (const auto &clause : equation.premise_clauses) work += 1 + clause.size();
      if (!add(work, parameters.maximum_session_work, result.premise_retention_work)) {
        result.status = AffineMatrixInitStatus::ResourceExhausted;
        record_init(kernel, result); return result;
      }
      if (kernel.proof->premise_group_active(session.premise_leases_[i])) {
        combined.equations.push_back(equation);
        retained_leases.push_back(session.premise_leases_[i]);
      }
    }
    retained_equations = combined.equations.size();
    // Retained occurrence IDs remain stable. Newly captured IDs are remapped
    // once into the same durable source namespace.
    AffineLiveSnapshot combined_owner = session.source_owner_;
    for (auto equation : image.equations) {
      bool duplicate = false;
      for (const auto &existing : combined.equations) {
        if (!add(1 + existing.variables.size(), parameters.maximum_session_work,
                 result.premise_retention_work)) {
          result.status = AffineMatrixInitStatus::ResourceExhausted;
          record_init(kernel, result); return result;
        }
        if (existing.variables == equation.variables && existing.rhs == equation.rhs)
          duplicate = true;
      }
      if (duplicate) continue;
      for (auto &occurrence : equation.premises) {
        auto source = owner.sources[occurrence - 1];
        source.occurrence = combined_owner.sources.size() + 1;
        occurrence = source.occurrence;
        combined_owner.premises.push_back({occurrence, source.clause});
        combined_owner.literals += source.clause.size();
        combined_owner.sources.push_back(std::move(source));
      }
      combined.equations.push_back(std::move(equation));
    }
    image = std::move(combined); owner = std::move(combined_owner);
    if (image.equations.size() == retained_equations) {
      result.status = AffineMatrixInitStatus::NoStructure;
      result.unchanged = true;
      record_init(kernel, result); return result;
    }
  }
  AffineMatrix matrix;
  uint64_t matrix_work = 0;
  bool matrix_exhausted = false;
  const uint64_t owner_version = session.owner_version_ + 1;
  const MatrixStatus built = build_affine_matrix(
      image.equations, owner_version, matrix_limits(parameters),
      [&](uint64_t amount) {
        if (kernel_terminating(kernel)) return false;
        if (!add(amount, parameters.basis_maximum_work, matrix_work)) {
          matrix_exhausted = true;
          return false;
        }
        return true;
      },
      matrix, persistence.maximum_retained_bytes != 0);
  result.matrix_work = matrix_work;
  if (built != MatrixStatus::Complete && built != MatrixStatus::Partial) {
    result.status = kernel_terminating(kernel)
                        ? AffineMatrixInitStatus::Interrupted
                    : built == MatrixStatus::NoStructure
                        ? AffineMatrixInitStatus::NoStructure
                    : built == MatrixStatus::Exhausted || matrix_exhausted
                        ? AffineMatrixInitStatus::ResourceExhausted
                        : AffineMatrixInitStatus::Invalid;
    if (result.status == AffineMatrixInitStatus::NoStructure ||
        result.status == AffineMatrixInitStatus::ResourceExhausted)
      session.remember_terminal_initialization_failure(kernel, parameters);
    record_init(kernel, result);
    return result;
  }
  if (admission == AffineMatrixAdmission::CompleteSnapshot &&
      built != MatrixStatus::Complete) {
    result.status = AffineMatrixInitStatus::IncompleteMatrix;
    result.equations = image.equations.size();
    result.rows = matrix.rows().size();
    result.admitted_sources = matrix.admitted_sources().size();
    record_init(kernel, result);
    return result;
  }

  uint64_t retained_bytes = sizeof(AffineMatrixSearchSession);
  const auto retain = [&](uint64_t bytes) {
    if (bytes > UINT64_MAX - retained_bytes) return false;
    retained_bytes += bytes;
    return !persistence.maximum_retained_bytes ||
           retained_bytes <= persistence.maximum_retained_bytes;
  };
  bool fits = true;
  for (const auto &source : owner.sources)
    fits &= retain(sizeof(AffineLiveSource) + sizeof(Premise) +
                   2 * source.clause.size() * sizeof(int32_t));
  for (const auto &equation : image.equations) {
    fits &= retain(4 * (sizeof(Equation) + equation.variables.size() * sizeof(int32_t) +
                   equation.premises.size() * sizeof(uint64_t)));
    for (const auto &clause : equation.premise_clauses)
      fits &= retain(4 * (sizeof(std::vector<int32_t>) + clause.size() * sizeof(int32_t)));
  }
  // Symmetric difference reserves both input sizes, so each vector capacity
  // may reach twice its exact logical coordinate/provenance union bound.
  const __uint128_t row_reserve_wide = __uint128_t(4) * matrix.admitted_rows().size() *
      (sizeof(MatrixRow) + __uint128_t(2) * matrix.row_width_bound() * sizeof(int32_t) +
       __uint128_t(2) * matrix.row_source_bound() * sizeof(size_t));
  const uint64_t row_reserve = row_reserve_wide > UINT64_MAX ? UINT64_MAX : uint64_t(row_reserve_wide);
  fits &= row_reserve_wide <= UINT64_MAX && retain(row_reserve);
  fits &= retain(4 * uint64_t(matrix.admitted_variables().size()) * sizeof(MatrixAssignment));
  if (session.enabled() && session.attached_to(kernel)) fits &= retain(session.matrix_index_bytes_);
  if (kernel.proof) fits &= retain(kernel.proof->persistent_external_bytes());
  if (!fits) {
    result.status = AffineMatrixInitStatus::ResourceExhausted;
    record_init(kernel, result); return result;
  }

  std::string initialization_journal;
  if (!make_initialization_journal(owner_version, matrix,
                                   parameters.maximum_journal_bytes,
                                   initialization_journal)) {
    result.status = AffineMatrixInitStatus::ResourceExhausted;
    session.remember_terminal_initialization_failure(kernel, parameters);
    record_init(kernel, result);
    return result;
  }
  if (persistence.maximum_retained_bytes) {
    const auto append = [&](const std::string &part) {
      return add(part.size(), parameters.maximum_session_work, result.premise_retention_work) &&
          part.size() <= parameters.maximum_journal_bytes -
                            std::min<uint64_t>(initialization_journal.size(), parameters.maximum_journal_bytes) &&
          (initialization_journal += part, true);
    };
    bool journal_fits = append("{\"kind\":\"affine-matrix-retained-sources\",\"owner\":" +
        std::to_string(owner_version) + ",\"maximum_retained_bytes\":" +
        std::to_string(persistence.maximum_retained_bytes) +
        ",\"maximum_deferred_deletions\":" + std::to_string(persistence.maximum_deferred_deletions) +
        ",\"maximum_deferred_literal_work\":" + std::to_string(persistence.maximum_deferred_literal_work) +
        ",\"sources\":[");
    for (size_t i = 0; journal_fits && i < image.equations.size(); ++i) {
      const auto &equation = image.equations[i];
      journal_fits = append((i ? "," : "") + std::string("{\"ordinal\":") +
          std::to_string(i) + ",\"rhs\":" + std::to_string(equation.rhs) + ",\"premises\":[");
      for (size_t j = 0; journal_fits && j < equation.premises.size(); ++j) {
        const auto &source = owner.sources[equation.premises[j] - 1];
        journal_fits = append((j ? "," : "") + std::string("{\"occurrence\":") +
            std::to_string(source.occurrence) + ",\"clause_id\":" +
            std::to_string(source.clause_id) + ",\"clause\":[");
        for (size_t l = 0; journal_fits && l < source.clause.size(); ++l)
          journal_fits = append((l ? "," : "") + std::to_string(source.clause[l]));
        journal_fits &= append("]}");
      }
      journal_fits &= append("]}");
    }
    journal_fits &= append("]}\n");
    if (!journal_fits) {
      result.status = AffineMatrixInitStatus::ResourceExhausted;
      record_init(kernel, result); return result;
    }
  }
  const long journal_position = std::ftell(kernel.structural_provenance);
  if (journal_position < 0) {
    result.status = AffineMatrixInitStatus::Invalid;
    record_init(kernel, result);
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = AffineMatrixInitStatus::Interrupted;
    record_init(kernel, result);
    return result;
  }

  std::vector<uint64_t> leases;
  if (persistence.maximum_retained_bytes) {
    std::vector<Proof::PremiseGroup> groups;
    for (size_t i = retained_equations; i < image.equations.size(); ++i) {
      const auto &equation = image.equations[i];
      if (!add(1 + equation.premises.size(), parameters.maximum_session_work,
               result.premise_retention_work)) {
        result.status = AffineMatrixInitStatus::ResourceExhausted;
        record_init(kernel, result); return result;
      }
      groups.push_back(equation.premise_clauses);
    }
    if (!kernel.proof->retain_premise_groups(groups, parameters.maximum_groups,
        parameters.maximum_snapshot_literals,
        persistence.maximum_retained_bytes - retained_bytes,
        [&](uint64_t amount) {
          return !kernel_terminating(kernel) &&
              add(amount, parameters.maximum_session_work, result.premise_retention_work);
        }, leases, {persistence.maximum_deferred_deletions, persistence.maximum_deferred_literal_work})) {
      result.status = kernel_terminating(kernel) ? AffineMatrixInitStatus::Interrupted :
                                                 AffineMatrixInitStatus::ResourceExhausted;
      record_init(kernel, result); return result;
    }
  }
  AffineMatrixSearchSession candidate;
  candidate.persistence_ = persistence;
  candidate.matrix_row_bytes_ = row_reserve;
  candidate.retained_bytes_ = retained_bytes + (kernel.proof ? kernel.proof->premise_bytes() : 0);
  retained_leases.insert(retained_leases.end(), leases.begin(), leases.end());
  candidate.premise_leases_ = std::move(retained_leases);
  candidate.premise_epoch_ = kernel.proof ? kernel.proof->premise_epoch() : 0;

  candidate.owner_ = &kernel;
  candidate.owner_version_ = owner_version;
  candidate.proof_ = kernel.proof;
  if (kernel.proof) candidate.proof_format_ = kernel.proof->format();
  candidate.records_ = kernel.structural_provenance;
  candidate.parameters_ = parameters;
  candidate.source_owner_ = std::move(owner);
  candidate.image_ = std::move(image);
  candidate.matrix_ = std::move(matrix);
  if (std::fwrite(initialization_journal.data(), 1,
                  initialization_journal.size(),
                  kernel.structural_provenance) !=
          initialization_journal.size() ||
      std::fflush(kernel.structural_provenance))
    std::abort();
  if (persistence.maximum_retained_bytes && session.persistent())
    session.premise_leases_.clear();
  session.disable(kernel);
  session = std::move(candidate);
  session.premise_epoch_ = kernel.proof ? kernel.proof->premise_epoch() : 0;

  result.status = session.matrix_.complete_admission()
                      ? AffineMatrixInitStatus::Applied
                      : AffineMatrixInitStatus::AppliedPartial;
  result.result = ApplyResult::Ok;
  result.owner_version = session.owner_version_;
  result.equations = session.image_.equations.size();
  result.rows = session.matrix_.rows().size();
  result.admitted_sources = session.matrix_.admitted_sources().size();
  result.journal_bytes = initialization_journal.size();
  result.retained_bytes = session.retained_bytes_;
  record_init(kernel, result);
  return result;
}

ApplyResult affine_matrix_initialize_apply(Kernel &kernel) {
  if (!kernel.affine_matrix_search_session) return ApplyResult::NoOp;
  if (!kernel.affine_matrix_initialize_parameters_active)
    return affine_matrix_search_initialize(kernel, *kernel.affine_matrix_search_session,
             kernel.affine_consequence_parameters).result;
  const auto &profile = kernel.affine_matrix_initialize_parameters;
  if (!profile.valid()) return ApplyResult::NoOp;
  const AffineMatrixPersistenceLimits limits{profile.maximum_retained_bytes,
      profile.maximum_deferred_deletions, profile.maximum_deferred_literal_work};
  using Operation = strategy::AffineMatrixInitializeParameters::Operation;
  switch (profile.operation) {
    case Operation::Initialize:
      return affine_matrix_search_initialize(kernel, *kernel.affine_matrix_search_session,
          profile.basis, AffineMatrixAdmission::AuthenticatedSubimage, limits).result;
    case Operation::Refresh:
    case Operation::RootMaintenance:
      return affine_matrix_search_refresh(kernel, *kernel.affine_matrix_search_session,
          profile.basis, limits, profile.operation == Operation::Refresh).result;
    case Operation::SearchStep:
      if (kernel.affine_matrix_search_session->parameters_ != profile.basis ||
          kernel.affine_matrix_search_session->persistence_.maximum_retained_bytes != limits.maximum_retained_bytes ||
          kernel.affine_matrix_search_session->persistence_.maximum_deferred_deletions != limits.maximum_deferred_deletions ||
          kernel.affine_matrix_search_session->persistence_.maximum_deferred_literal_work != limits.maximum_deferred_literal_work)
        return ApplyResult::NoOp;
      return affine_matrix_search_step(kernel, *kernel.affine_matrix_search_session).result;
  }
  return ApplyResult::NoOp;
}

AffineMatrixInitResult affine_matrix_search_refresh(Kernel &kernel,
    AffineMatrixSearchSession &session,
    const strategy::AffineConsequenceParameters &parameters,
    const AffineMatrixPersistenceLimits &persistence, bool allow_additional_recognition) {
  if (!affine_matrix_initialize_guard(kernel) || !parameters.valid()) {
    AffineMatrixInitResult result; result.status = AffineMatrixInitStatus::Inapplicable;
    return result;
  }
  if (session.enabled() && session.persistent() && session.attached_to(kernel) &&
      session.parameters_ == parameters &&
      session.persistence_.maximum_retained_bytes == persistence.maximum_retained_bytes &&
      session.persistence_.maximum_deferred_deletions == persistence.maximum_deferred_deletions &&
      session.persistence_.maximum_deferred_literal_work == persistence.maximum_deferred_literal_work &&
      kernel.affine_capture_epochs_trusted &&
      session.refresh_namespace_epoch_ == kernel.affine_semantic_namespace_epoch &&
      session.refresh_arena_generation_ == kernel.arena_generation &&
      session.refresh_touch_epoch_ == kernel.touch_epoch &&
      session.refresh_fixed_head_ == kernel.touch_fixed_head &&
      session.refresh_root_assignments_ == kernel.stats.root_assignments &&
      session.refresh_retraction_epoch_ == kernel.affine_trail_retraction_epoch &&
      session.refresh_clause_id_ == kernel.next_clause_id &&
      session.premise_epoch_ == kernel.proof->premise_epoch()) {
    AffineMatrixInitResult result; result.status = AffineMatrixInitStatus::NoStructure;
    result.unchanged = true; return result;
  }
  if (!allow_additional_recognition && (!session.enabled() || !session.persistent() ||
      !session.attached_to(kernel) || session.parameters_ != parameters ||
      session.persistence_.maximum_retained_bytes != persistence.maximum_retained_bytes ||
      session.persistence_.maximum_deferred_deletions != persistence.maximum_deferred_deletions ||
      session.persistence_.maximum_deferred_literal_work != persistence.maximum_deferred_literal_work)) {
    AffineMatrixInitResult result; result.status = AffineMatrixInitStatus::Inapplicable;
    return result;
  }
  AffineMatrixAliasResult rooted;
  if (session.enabled() && session.persistent() && !kernel.level &&
      session.attached_to(kernel) && session.parameters_ == parameters &&
      session.persistence_.maximum_retained_bytes == persistence.maximum_retained_bytes &&
      session.persistence_.maximum_deferred_deletions == persistence.maximum_deferred_deletions &&
      session.persistence_.maximum_deferred_literal_work == persistence.maximum_deferred_literal_work) {
    rooted = affine_matrix_search_preserve_aliases(kernel, session, {});
    kernel.stats.affine_matrix_init_root_unit_work += rooted.work;
    if (rooted.status != AffineMatrixServiceStatus::Disabled &&
        rooted.status != AffineMatrixServiceStatus::NoEvent &&
        rooted.status != AffineMatrixServiceStatus::Applied) {
      AffineMatrixInitResult result;
      result.status = rooted.status == AffineMatrixServiceStatus::Interrupted ?
          AffineMatrixInitStatus::Interrupted : AffineMatrixInitStatus::ResourceExhausted;
      result.root_unit_work = rooted.work; return result;
    }
  }
  if (!allow_additional_recognition) {
    AffineMatrixInitResult result;
    result.status = rooted.status == AffineMatrixServiceStatus::Applied ?
          AffineMatrixInitStatus::AppliedPartial : AffineMatrixInitStatus::NoStructure;
    result.result = rooted.result; result.root_unit_additions = rooted.proof_additions;
    result.root_unit_work = rooted.work;
    if (session.enabled()) {
      result.owner_version = session.owner_version_; result.retained_bytes = session.retained_bytes_;
      result.equations = session.image_.equations.size(); result.rows = session.matrix_.rows().size();
    }
    return result;
  }
  auto result = affine_matrix_search_initialize(kernel, session, parameters,
      AffineMatrixAdmission::AuthenticatedSubimage, persistence);
  result.root_unit_additions = rooted.proof_additions;
  result.root_unit_work = rooted.work;
  if (rooted.status == AffineMatrixServiceStatus::Applied &&
      result.status != AffineMatrixInitStatus::Applied &&
      result.status != AffineMatrixInitStatus::AppliedPartial) {
    result.additional_recognition_status = result.status;
    result.status = AffineMatrixInitStatus::AppliedPartial;
    result.result = ApplyResult::Ok;
    result.owner_version = session.owner_version_;
    result.equations = session.image_.equations.size();
    result.rows = session.matrix_.rows().size();
    result.retained_bytes = session.retained_bytes_;
  }
  const bool census_complete = result.status == AffineMatrixInitStatus::Applied ||
      result.status == AffineMatrixInitStatus::NoStructure ||
      (result.status == AffineMatrixInitStatus::AppliedPartial &&
       (result.additional_recognition_status == AffineMatrixInitStatus::Applied ||
        result.additional_recognition_status == AffineMatrixInitStatus::NoStructure));
  if (census_complete && session.enabled() && session.persistent()) {
    session.refresh_namespace_epoch_ = kernel.affine_semantic_namespace_epoch;
    session.refresh_arena_generation_ = kernel.arena_generation;
    session.refresh_touch_epoch_ = kernel.touch_epoch;
    session.refresh_fixed_head_ = kernel.touch_fixed_head;
    session.refresh_root_assignments_ = kernel.stats.root_assignments;
    session.refresh_retraction_epoch_ = kernel.affine_trail_retraction_epoch;
    session.refresh_clause_id_ = kernel.next_clause_id;
  }
  return result;
}

AffineMatrixAliasResult affine_matrix_search_preserve_aliases(
    Kernel &kernel, AffineMatrixSearchSession &session,
    const std::vector<std::pair<Lit, Lit>> &aliases) {
  AffineMatrixAliasResult result;
  if (!session.enabled() || !session.persistent()) return result;
  const auto finish = [&](AffineMatrixServiceStatus status) {
    if (!aliases.empty()) {
      kernel.stats.affine_matrix_alias_work += result.work;
      kernel.stats.affine_matrix_alias_proof_additions += result.proof_additions;
    }
    result.status = status; return result;
  };
  if (!session.attached_to(kernel) || !kernel.proof)
    return finish(AffineMatrixServiceStatus::Stale);
  if (kernel.level || kernel.inconsistent || kernel.conflict_valid || kernel.model_extended ||
      !kernel_propagation_fixpoint(kernel))
    return finish(AffineMatrixServiceStatus::Inapplicable);
  bool allowance_exhausted = false;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) return false;
    if (!add(amount, session.parameters_.basis_maximum_work, result.work)) {
      allowance_exhausted = true; return false;
    }
    return true;
  };
  const auto exhausted = [&]() {
    return finish(kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                              AffineMatrixServiceStatus::ResourceExhausted);
  };
  std::vector<std::pair<Lit, Lit>> selected;
  for (auto [source, target] : aliases) {
    if (!charge(1)) return exhausted();
    if (source == INVALID_LIT || target == INVALID_LIT ||
        var_of(source) >= kernel.nvars || var_of(target) >= kernel.nvars ||
        kernel_var_retired(kernel, var_of(source)) || kernel_var_retired(kernel, var_of(target)))
      return finish(AffineMatrixServiceStatus::Invalid);
    if (negated(source)) {source = negate(source); target = negate(target);}
    if (var_of(source) == var_of(target)) {
      if (source != target) return finish(AffineMatrixServiceStatus::Invalid);
      continue;
    }
    const int32_t external = kernel_externalize(kernel, source);
    if (std::binary_search(session.matrix_.admitted_variables().begin(),
                           session.matrix_.admitted_variables().end(), external))
      selected.emplace_back(source, target);
  }
  std::vector<int32_t> fixed;
  if (aliases.empty()) {
    for (External external : session.matrix_.admitted_variables()) {
      if (!charge(1)) return exhausted();
      const Lit internal = kernel_internalize(kernel, external);
      const int8_t value = kernel_external_value(kernel, internalize(external));
      const Var ev = var_of(internalize(external));
      const bool archived_root = ev < kernel.fixed_external_values.size() &&
          kernel.fixed_external_values[ev] == value && value != 0;
      bool live_root = false;
      if (internal != INVALID_LIT && kernel_externalize(kernel, internal) == external && value) {
        const auto &assigned = kernel.assigned[var_of(internal)];
        const Lit true_literal = value > 0 ? internal : negate(internal);
        live_root = !assigned.level && assigned.trail < kernel.trail.size() &&
                    kernel.trail[assigned.trail] == true_literal;
      }
      if ((archived_root || live_root) &&
          (internal == INVALID_LIT || kernel_var_retired(kernel, var_of(internal))))
        fixed.push_back(value > 0 ? external : -external);
    }
  }
  if (selected.empty() && fixed.empty()) return finish(AffineMatrixServiceStatus::NoEvent);
  if (!charge(1 + uint64_t(kernel.nvars) * 2) ||
      uint64_t(kernel.nvars) * 2 * (sizeof(uint8_t) + sizeof(Lit)) >
           session.persistence_.maximum_retained_bytes -
             std::min(session.retained_bytes_, session.persistence_.maximum_retained_bytes))
    return exhausted();
  std::vector<uint8_t> seen(2 * size_t(kernel.nvars));
  std::vector<Lit> queue; queue.reserve(2 * size_t(kernel.nvars));
  const auto reachable = [&](Lit from, Lit to) {
    if (!charge(seen.size())) return false;
    std::fill(seen.begin(), seen.end(), 0); queue.clear();
    seen[from] = 1; queue.push_back(from);
    for (size_t i = 0; i < queue.size(); ++i) {
      for (const auto watch : kernel.watches.entries(negate(queue[i]))) {
        if (!charge(1)) return false;
        if (!watch.binary()) continue;
        const Lit next = watch.other();
        if (next == to) return true;
        if (!seen[next]) {seen[next] = 1; queue.push_back(next);}
      }
    }
    return false;
  };
  for (const auto &source : session.source_owner_.sources)
    if (!charge(2 + 2 * source.clause.size())) return exhausted();
  for (const auto &equation : session.image_.equations) {
    if (!charge(2 + equation.variables.size() + equation.premises.size())) return exhausted();
    for (const auto &clause : equation.premise_clauses)
      if (!charge(1 + clause.size())) return exhausted();
  }
  Image image = session.image_;
  AffineLiveSnapshot owner = session.source_owner_;
  std::vector<Proof::PremiseGroup> groups;
  uint64_t retained_bytes = session.retained_bytes_;
  uint64_t next_id = kernel.next_clause_id;
  std::ostringstream journal;
  journal << "{\"kind\":\"affine-matrix-aliases\",\"owner\":" << session.owner_version_ << ",\"aliases\":[";
  for (auto [source, target] : selected) {
    if (!reachable(source, target) || !reachable(negate(source), negate(target))) {
      if (kernel_terminating(kernel) || allowance_exhausted)
        return exhausted();
      return finish(AffineMatrixServiceStatus::Invalid);
    }
    const int32_t from = kernel_externalize(kernel, source);
    const int32_t to = kernel_externalize(kernel, target);
    Equation equation;
    equation.variables = {from, int32_t(to < 0 ? -int64_t(to) : to)};
    std::sort(equation.variables.begin(), equation.variables.end());
    equation.rhs = to < 0;
    bool duplicate = false;
    for (const auto &existing : image.equations) {
      if (!charge(1 + existing.variables.size())) return exhausted();
      if (existing.variables == equation.variables && existing.rhs == equation.rhs) duplicate = true;
    }
    if (duplicate) continue;
    if (next_id > UINT64_MAX - 2 || groups.size() >= session.parameters_.maximum_groups ||
        (groups.size() + 1) * 2 > session.parameters_.maximum_proof_additions)
      return exhausted();
    equation.premise_clauses = clausal_form(equation.variables, equation.rhs);
    for (const auto &clause : equation.premise_clauses) {
      AffineLiveSource premise;
      premise.occurrence = owner.sources.size() + 1;
      premise.kind = AffineSourceKind::CheckerPremise;
      premise.clause_id = ++next_id; premise.clause = clause;
      equation.premises.push_back(premise.occurrence);
      owner.premises.push_back({premise.occurrence, clause});
      owner.sources.push_back(std::move(premise)); owner.literals += clause.size();
    }
    if (!groups.empty()) journal << ',';
    journal << "{\"source\":" << from << ",\"representative\":" << to
            << ",\"first_clause_id\":" << next_id - 1 << '}';
    groups.push_back(equation.premise_clauses);
    image.equations.push_back(std::move(equation));
    const uint64_t bytes = 4 * (sizeof(Equation) + 2 * sizeof(External) +
          2 * sizeof(uint64_t) + 2 * sizeof(std::vector<External>) + 4 * sizeof(External)) +
        2 * (sizeof(AffineLiveSource) + sizeof(Premise) + 4 * sizeof(External)) +
        8 * sizeof(MatrixAssignment);
    if (bytes > session.persistence_.maximum_retained_bytes -
                    std::min(retained_bytes, session.persistence_.maximum_retained_bytes))
      return exhausted();
    retained_bytes += bytes;
  }
  for (int32_t literal : fixed) {
    const External variable = literal < 0 ? -literal : literal;
    bool duplicate = false;
    for (const auto &existing : image.equations) {
      if (!charge(1 + existing.variables.size())) return exhausted();
      if (existing.variables == std::vector<External>{variable} && existing.rhs == (literal > 0))
        duplicate = true;
    }
    if (duplicate) continue;
    if (next_id == UINT64_MAX || groups.size() >= session.parameters_.maximum_groups ||
        (groups.size() + 1) * 2 > session.parameters_.maximum_proof_additions)
      return exhausted();
    Equation equation; equation.variables = {variable}; equation.rhs = literal > 0;
    equation.premise_clauses = {{literal}};
    AffineLiveSource premise; premise.occurrence = owner.sources.size() + 1;
    premise.kind = AffineSourceKind::CheckerPremise; premise.clause_id = ++next_id;
    premise.clause = {literal}; equation.premises = {premise.occurrence};
    owner.premises.push_back({premise.occurrence, premise.clause});
    owner.sources.push_back(std::move(premise)); ++owner.literals;
    if (!groups.empty()) journal << ',';
    journal << "{\"root_unit\":" << literal << ",\"clause_id\":" << next_id << '}';
    groups.push_back(equation.premise_clauses); image.equations.push_back(std::move(equation));
    const uint64_t bytes = 4 * (sizeof(Equation) + sizeof(External) + sizeof(uint64_t) +
        sizeof(std::vector<External>) + sizeof(External)) +
        2 * (sizeof(AffineLiveSource) + sizeof(Premise) + 2 * sizeof(External)) +
        4 * sizeof(MatrixAssignment);
    if (bytes > session.persistence_.maximum_retained_bytes -
                    std::min(retained_bytes, session.persistence_.maximum_retained_bytes))
      return exhausted();
    retained_bytes += bytes;
  }
  if (groups.empty()) return finish(AffineMatrixServiceStatus::NoEvent);
  if (session.owner_version_ == UINT64_MAX) return exhausted();
  AffineMatrix matrix;
  const auto built = build_affine_matrix(image.equations, session.owner_version_ + 1,
                                          matrix_limits(session.parameters_), charge, matrix, true);
  // Losing rows here would erase obligations just before variable retirement.
  if (built != MatrixStatus::Complete) return exhausted();
  // Reprice every retained row, since each new authenticated alias/root source
  // can enlarge provenance cardinality and coordinate geometry of old rows.
  // Four new populations cover the old basis plus staged replacement/scratch;
  // retain any larger old reservation across rank/projection reductions.
  const __uint128_t new_row_wide = __uint128_t(4) * matrix.admitted_rows().size() *
      (sizeof(MatrixRow) + __uint128_t(2) * matrix.row_width_bound() * sizeof(External) +
       __uint128_t(2) * matrix.row_source_bound() * sizeof(size_t));
  if (new_row_wide > UINT64_MAX) return exhausted();
  const uint64_t row_reserve = std::max(session.matrix_row_bytes_, uint64_t(new_row_wide));
  const uint64_t row_growth = row_reserve - session.matrix_row_bytes_;
  if (row_growth > session.persistence_.maximum_retained_bytes -
      std::min(retained_bytes, session.persistence_.maximum_retained_bytes)) return exhausted();
  retained_bytes += row_growth;
  journal << "]}\n";const auto record = journal.str();
  if (!charge(record.size()) || record.size() > session.parameters_.maximum_journal_bytes ||
      std::ftell(session.records_) < 0) return exhausted();
  auto retained_handles = session.premise_leases_;
  retained_handles.reserve(retained_handles.size() + groups.size());
  if (session.matrix_index_bytes_ > session.persistence_.maximum_retained_bytes -
          std::min(retained_bytes, session.persistence_.maximum_retained_bytes)) return exhausted();
  // The old index remains live until the atomically staged matrix replacement.
  retained_bytes += session.matrix_index_bytes_;
  // Both proof-only alias clauses must remain leased after their own physical
  // deletion requests. Otherwise the second request can retire the just-staged
  // source and falsely advertise preservation before Substitute rewrites it.
  if (!kernel.proof->can_defer_premise_group_deletions(groups,
      {session.persistence_.maximum_deferred_deletions,
       session.persistence_.maximum_deferred_literal_work}, charge)) return exhausted();
  const uint64_t registry_bytes_before = kernel.proof->premise_bytes();
  std::vector<uint64_t> handles;
  if (!kernel.proof->retain_premise_groups(groups, session.parameters_.maximum_groups,
      session.parameters_.maximum_snapshot_literals,
      session.persistence_.maximum_retained_bytes - retained_bytes,
      charge, handles, {session.persistence_.maximum_deferred_deletions,
                       session.persistence_.maximum_deferred_literal_work})) return exhausted();
  const uint64_t committed_premise_epoch = kernel.proof->premise_epoch();
  // No fallible operation or interruption boundary remains after lease commit.
  const size_t start = session.image_.equations.size();
  for (size_t i = start; i < image.equations.size(); ++i)
    for (size_t j = 0; j < image.equations[i].premises.size(); ++j) {
      const auto &premise = owner.sources[image.equations[i].premises[j] - 1];
      if (!kernel.proof->add_external_rup(++kernel.next_clause_id,
            premise.clause.data(), premise.clause.size()) ||
          kernel.next_clause_id != premise.clause_id) std::abort();
      // This copy is proof-only. Queue its own eventual retirement even if
      // Substitute has no identical native/transient lemma to delete.
      if (!kernel.proof->del_external(kernel.next_clause_id, premise.clause_id,
              premise.clause.data(), premise.clause.size())) std::abort();
    }
  // The read-only reservoir reservation rules out retirement during helper
  // serialization; an unexpected serializer lifecycle change is fatal rather
  // than installing inactive certified sources after an irreversible commit.
  if (kernel.proof->premise_epoch() != committed_premise_epoch) std::abort();
  for (uint64_t handle : handles)
    if (!kernel.proof->premise_group_active(handle)) std::abort();
  if (std::fwrite(record.data(), 1, record.size(), session.records_) != record.size() ||
      std::fflush(session.records_)) std::abort();
  session.matrix_row_bytes_ = row_reserve;
  session.image_ = std::move(image); session.source_owner_ = std::move(owner);
  session.matrix_ = std::move(matrix); ++session.owner_version_;
  retained_handles.insert(retained_handles.end(), handles.begin(), handles.end());
  session.premise_leases_ = std::move(retained_handles);
  session.premise_epoch_ = kernel.proof->premise_epoch();
  const uint64_t registry_bytes_after = kernel.proof->premise_bytes();
  if (registry_bytes_after >= registry_bytes_before) {
    const uint64_t growth = registry_bytes_after - registry_bytes_before;
    if (growth > UINT64_MAX - retained_bytes) std::abort();
    session.retained_bytes_ = retained_bytes + growth;
  } else {
    const uint64_t shrink = registry_bytes_before - registry_bytes_after;
    if (shrink > retained_bytes) std::abort();
    session.retained_bytes_ = retained_bytes - shrink;
  }
  session.projected_variables_.clear();
  session.cached_event_ = {}; session.cached_event_bytes_ = 0; session.cached_event_valid_ = false;
  std::vector<std::pair<Var, uint32_t>>{}.swap(session.capture_index_);
  std::vector<MatrixAssignment>{}.swap(session.capture_trail_);
  session.capture_bytes_ = 0; session.capture_valid_ = false;
  session.matrix_index_bytes_ = 0;
  result.proof_additions = 0; for (const auto &group : groups) result.proof_additions += group.size();
  result.retained_aliases = groups.size();
  result.journal_bytes = record.size(); result.result = ApplyResult::Ok;
  return finish(AffineMatrixServiceStatus::Applied);
}

AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &kernel, AffineMatrixSearchSession &session) {
  AffineMatrixServiceResult result;
  if (!session.enabled()) return result;
  if (!session.attached_to(kernel)) {
    result.status = AffineMatrixServiceStatus::Stale;
    session.disable(kernel);
    record_service(kernel, result);
    return result;
  }
  if (kernel.inconsistent || kernel.conflict_valid || kernel.pending.valid ||
      (session.persistent() && kernel.model_extended) || kernel.dense || kernel.probing.recording ||
      !kernel.large_watches_connected || !kernel_propagation_fixpoint(kernel)) {
    result.status = AffineMatrixServiceStatus::Inapplicable;
    session.disable(kernel);
    record_service(kernel, result);
    return result;
  }
  if (kernel.structural_action_sequence == UINT64_MAX) {
    result.status = AffineMatrixServiceStatus::ResourceExhausted;
    record_service(kernel, result);
    return result;
  }
  if (session.persistent()) {
    auto charge = [&](uint64_t amount) {
      return !kernel_terminating(kernel) &&
             add(amount, session.parameters_.basis_maximum_work, result.lifecycle_work);
    };
    if (session.premise_epoch_ != kernel.proof->premise_epoch()) {
      Image surviving;
      std::vector<uint64_t> leases;
      for (size_t i = 0; i < session.image_.equations.size(); ++i) {
        const auto &equation = session.image_.equations[i];
        uint64_t size = 1 + equation.variables.size() + equation.premises.size();
        for (const auto &clause : equation.premise_clauses) size += 1 + clause.size();
        if (!charge(size)) {
          result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                       AffineMatrixServiceStatus::ResourceExhausted;
          record_service(kernel, result); return result;
        }
        if (kernel.proof->premise_group_active(session.premise_leases_[i])) {
          surviving.equations.push_back(equation);
          leases.push_back(session.premise_leases_[i]);
        } else ++result.retired_source_groups;
      }
      if (surviving.equations.empty()) {
        session.disable(kernel); result.status = AffineMatrixServiceStatus::NoEvent;
        record_service(kernel, result); return result;
      }
      if (session.owner_version_ == UINT64_MAX) {
        result.status = AffineMatrixServiceStatus::ResourceExhausted;
        record_service(kernel, result); return result;
      }
      AffineMatrix replacement;
      const auto status = build_affine_matrix(surviving.equations,
          session.owner_version_ + 1, matrix_limits(session.parameters_), charge, replacement, true);
      if (status != MatrixStatus::Complete && status != MatrixStatus::Partial) {
        result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                     AffineMatrixServiceStatus::ResourceExhausted;
        record_service(kernel, result); return result;
      }
      session.image_ = std::move(surviving); session.matrix_ = std::move(replacement);
      session.premise_leases_ = std::move(leases); ++session.owner_version_;
      session.premise_epoch_ = kernel.proof->premise_epoch();
      session.projected_variables_.clear();
    }
    std::vector<External> unavailable;
    const bool namespace_unchanged = kernel.affine_capture_epochs_trusted && session.capture_valid_ &&
        session.capture_owner_ == session.owner_version_ &&
        session.capture_namespace_epoch_ == kernel.affine_semantic_namespace_epoch;
    if (!namespace_unchanged) for (External external : session.matrix_.admitted_variables()) {
      if (!charge(1)) {
        result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                     AffineMatrixServiceStatus::ResourceExhausted;
        record_service(kernel, result); return result;
      }
      const Lit internal = kernel_internalize(kernel, external);
      if (internal == INVALID_LIT || kernel_var_retired(kernel, var_of(internal)) ||
          kernel_externalize(kernel, internal) != external)
        unavailable.push_back(external);
    }
    if (!unavailable.empty()) {
      const auto status = session.matrix_.project(unavailable, charge);
      if (status != MatrixStatus::Complete) {
        result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                     AffineMatrixServiceStatus::ResourceExhausted;
        record_service(kernel, result); return result;
      }
      session.cached_event_valid_ = false; session.capture_valid_ = false;
      result.projected_variables = unavailable.size();
      session.projected_variables_.insert(session.projected_variables_.end(),
                                         unavailable.begin(), unavailable.end());
    }
  }
  std::vector<MatrixAssignment> fresh_trail;
  const std::vector<MatrixAssignment> *trail_view = &fresh_trail;
  auto capture_charge = [&](uint64_t amount) {
    return !kernel_terminating(kernel) &&
        add(amount, session.parameters_.basis_maximum_work, result.trail_capture_work);
  };
  bool capture_ok = true;
  if (session.persistent() && kernel.affine_capture_epochs_trusted) {
    const auto &support = session.matrix_.admitted_variables();
    const bool incremental = session.capture_valid_ && session.capture_owner_ == session.owner_version_ &&
        session.capture_namespace_epoch_ == kernel.affine_semantic_namespace_epoch &&
        session.capture_retraction_epoch_ == kernel.affine_trail_retraction_epoch &&
        session.capture_cursor_ <= kernel.trail.size();
    if (incremental) {
      std::vector<std::pair<External, bool>> changes;
      const uint64_t comparisons = 2 + std::bit_width(uint64_t(support.size()));
      for (size_t i = session.capture_cursor_; i < kernel.trail.size(); ++i) {
        if (!capture_charge(1 + comparisons)) { capture_ok = false; break; }
        ++result.capture_delta_literals;
        const Lit literal = kernel.trail[i];
        const Var variable = var_of(literal);
        const auto found = std::lower_bound(session.capture_index_.begin(), session.capture_index_.end(), variable,
            [](const auto &entry, Var key) { return entry.first < key; });
        if (found == session.capture_index_.end() || found->first != variable) continue;
        if (changes.empty()) {
          if (!capture_charge(1 + support.size())) { capture_ok = false; break; }
          changes.reserve(support.size());
        }
        if (!capture_charge(1)) { capture_ok = false; break; }
        changes.push_back({support[found->second], !negated(literal)});
      }
      if (capture_ok && !changes.empty()) {
        if (!capture_charge(1 + support.size() + session.capture_trail_.size())) capture_ok = false;
        else {
          fresh_trail.reserve(support.size()); fresh_trail = session.capture_trail_;
          for (const auto &[variable, value] : changes) {
            if (!capture_charge(1 + fresh_trail.size() + comparisons)) { capture_ok = false; break; }
            auto found = std::lower_bound(fresh_trail.begin(), fresh_trail.end(), variable,
                [](const MatrixAssignment &assignment, External key) { return assignment.variable < key; });
            if (found == fresh_trail.end() || found->variable != variable)
              fresh_trail.insert(found, {variable, value});
            else found->value = value;
          }
          if (capture_ok) {
            session.cached_event_valid_ = false;
            session.capture_trail_ = std::move(fresh_trail);
          }
        }
      }
      if (capture_ok) {
        result.incremental_capture = true; result.unchanged_support = changes.empty();
        session.capture_cursor_ = kernel.trail.size(); trail_view = &session.capture_trail_;
      }
    } else {
      session.capture_valid_ = false;
      // Fixed capacity reserves include index sorting and capture/replacement
      // temporaries. No index allocation occurs until bytes/work are admitted.
      const __uint128_t required = __uint128_t(support.size()) *
          (2 * sizeof(std::pair<Var, uint32_t>) + 3 * sizeof(MatrixAssignment) +
           2 * sizeof(std::pair<External, bool>));
      const __uint128_t reserved = std::max<__uint128_t>(required, session.capture_bytes_);
      const uint64_t base = session.retained_bytes_ - session.capture_bytes_;
      if (reserved > UINT64_MAX || reserved > session.persistence_.maximum_retained_bytes -
          std::min(base, session.persistence_.maximum_retained_bytes) ||
          !capture_charge(1 + support.size())) capture_ok = false;
      std::vector<std::pair<Var, uint32_t>> index;
      if (capture_ok) {
        index.reserve(support.size()); fresh_trail.reserve(support.size());
        for (size_t i = 0; i < support.size(); ++i) {
          if (!capture_charge(2)) { capture_ok = false; break; }
          const Lit internal = kernel_internalize(kernel, support[i]);
          if (internal == INVALID_LIT || kernel_var_retired(kernel, var_of(internal)) ||
              kernel_externalize(kernel, internal) != support[i]) { capture_ok = false; break; }
          index.push_back({var_of(internal), uint32_t(i)});
          const int8_t value = kernel.values[internal];
          if (value) fresh_trail.push_back({support[i], value > 0});
        }
      }
      const __uint128_t sorting = __uint128_t(support.size()) *
          (2 * std::bit_width(uint64_t(support.size())) + 2);
      if (capture_ok && (sorting > UINT64_MAX || !capture_charge(uint64_t(sorting)))) capture_ok = false;
      if (capture_ok) {
        std::sort(index.begin(), index.end());
        session.cached_event_valid_ = false;
        session.capture_index_ = std::move(index); session.capture_trail_ = std::move(fresh_trail);
        session.capture_bytes_ = uint64_t(reserved); session.retained_bytes_ = base + session.capture_bytes_;
        session.capture_owner_ = session.owner_version_;
        session.capture_namespace_epoch_ = kernel.affine_semantic_namespace_epoch;
        session.capture_retraction_epoch_ = kernel.affine_trail_retraction_epoch;
        session.capture_cursor_ = kernel.trail.size(); session.capture_valid_ = true;
        trail_view = &session.capture_trail_;
      }
    }
  } else {
    session.capture_valid_ = false;
    const auto status = exact_trail(kernel, session.matrix_.admitted_variables(),
        session.parameters_.basis_maximum_work, result.trail_capture_work, fresh_trail);
    if (status != TrailStatus::Complete) {
      result.status = status == TrailStatus::Interrupted ? AffineMatrixServiceStatus::Interrupted :
          status == TrailStatus::Exhausted ? AffineMatrixServiceStatus::ResourceExhausted : AffineMatrixServiceStatus::Stale;
      record_service(kernel, result); return result;
    }
  }
  if (!capture_ok) {
    result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                               AffineMatrixServiceStatus::ResourceExhausted;
    record_service(kernel, result); return result;
  }
  const auto &trail = *trail_view;
  bool exhausted = false;
  MatrixEvent event;
  auto synchronization_charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) return false;
    if (!add(amount, session.parameters_.basis_maximum_work, result.synchronization_work)) {
      exhausted = true; return false;
    }
    return true;
  };
  bool cached = false;
  if (session.persistent()) {
    if (!synchronization_charge(1 + (result.unchanged_support ? 0 : trail.size()))) {
      result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                 AffineMatrixServiceStatus::ResourceExhausted;
      record_service(kernel, result); return result;
    }
    cached = session.cached_event_valid_ && session.cached_event_owner_ == session.owner_version_ &&
             (result.unchanged_support || trail == session.matrix_.trail());
    if (cached) {
      const auto &previous = session.cached_event_;
      if (!synchronization_charge(1 + previous.row.variables.size() +
          previous.row.source_equations.size() + previous.reason.size())) {
        result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                   AffineMatrixServiceStatus::ResourceExhausted;
        record_service(kernel, result); return result;
      }
      event = previous;
    } else {
      // A partial/declined reduction may still advance the matrix trail; the
      // previous event must never become associated with that new snapshot.
      session.cached_event_valid_ = false;
      // The persistent indexed matrix meters touched rows and selected-event
      // reconstruction itself; it does not scan all source equations here.
    }
  }
  const uint64_t index_base = session.retained_bytes_ - session.matrix_index_bytes_;
  const MatrixIncrementalLimits index_limits{session.persistence_.maximum_retained_bytes -
      std::min(index_base, session.persistence_.maximum_retained_bytes)};
  const MatrixStatus synchronized = cached ? MatrixStatus::Complete : session.matrix_.synchronize(
      trail, synchronization_charge, event, session.persistent(), session.persistent() ? &index_limits : nullptr);
  if (synchronized != MatrixStatus::Complete &&
      synchronized != MatrixStatus::Partial) {
    result.status = kernel_terminating(kernel)
                        ? AffineMatrixServiceStatus::Interrupted
                    : synchronized == MatrixStatus::Exhausted || exhausted
                        ? AffineMatrixServiceStatus::ResourceExhausted
                        : AffineMatrixServiceStatus::Invalid;
    record_service(kernel, result);
    return result;
  }
  if (session.persistent() && !cached && synchronized == MatrixStatus::Complete) {
    session.matrix_index_bytes_ = session.matrix_.incremental_index_bytes();
    session.retained_bytes_ = index_base + session.matrix_index_bytes_;
  }
  result.dirty_row_update = !cached && session.matrix_.last_dirty_update();
  result.dirty_rows = result.dirty_row_update ? session.matrix_.last_dirty_rows() : 0;
  if (session.persistent() && !cached && synchronized == MatrixStatus::Complete) {
    // Reserve both the cached representation and replacement/copy peak.
    const uint64_t bytes = std::max(session.cached_event_bytes_, 2 * (sizeof(MatrixEvent) +
        uint64_t(event.row.variables.size() + event.reason.size()) * sizeof(External) +
        uint64_t(event.row.source_equations.size()) * sizeof(size_t)));
    const uint64_t base = session.retained_bytes_ - session.cached_event_bytes_;
    if (bytes <= session.persistence_.maximum_retained_bytes -
                    std::min(base, session.persistence_.maximum_retained_bytes) &&
        synchronization_charge(1 + event.row.variables.size() + event.row.source_equations.size() + event.reason.size())) {
      session.cached_event_ = event;
      session.cached_event_bytes_ = bytes; session.retained_bytes_ = base + bytes;
      session.cached_event_owner_ = session.owner_version_; session.cached_event_valid_ = true;
    } else {
      session.cached_event_valid_ = false;
      result.status = kernel_terminating(kernel) ? AffineMatrixServiceStatus::Interrupted :
                                                 AffineMatrixServiceStatus::ResourceExhausted;
      record_service(kernel, result); return result;
    }
  }
  result.synchronized = true;
  result.cached_event = cached;
  result.synchronization_incremental =
      session.matrix_.last_synchronization_incremental();
  result.synchronization_restored_predecessor =
      session.matrix_.last_synchronization_restored_predecessor();
  result.synchronization_prefix =
      session.matrix_.last_synchronization_prefix();
  result.synchronization_conditioned =
      session.matrix_.last_synchronization_conditioned();
  result.synchronization_kind = session.matrix_.last_synchronization_kind();
  if (cached) {
    result.synchronization_incremental = true;
    result.synchronization_restored_predecessor = false;
    result.synchronization_prefix = trail.size();
    result.synchronization_conditioned = 0;
    result.synchronization_kind = MatrixSynchronizationKind::Extension;
  }
  if (event.kind == MatrixEventKind::None) {
    result.status = AffineMatrixServiceStatus::NoEvent;
    record_service(kernel, result);
    return result;
  }
  result.reason_width = event.reason.size();
  result.event_identity = event_identity(session.owner_version_, event);
  if (result.event_identity.size() > session.parameters_.maximum_journal_bytes) {
    result.status = AffineMatrixServiceStatus::ResourceExhausted;
    record_service(kernel, result);
    return result;
  }
  const auto duplicate =
      std::lower_bound(session.published_.begin(), session.published_.end(),
                       result.event_identity);
  for (const auto &key : session.published_)
    if (!add(key.size() + 1, session.parameters_.maximum_session_work,
             result.session_work)) {
      result.status = AffineMatrixServiceStatus::ResourceExhausted;
      record_service(kernel, result);
      return result;
    }
  if (duplicate != session.published_.end() &&
      *duplicate == result.event_identity) {
    result.status = AffineMatrixServiceStatus::Duplicate;
    record_service(kernel, result);
    return result;
  }
  if (session.published_.size() == session.parameters_.maximum_session_keys ||
      !add(result.event_identity.size() + 1,
           session.parameters_.maximum_session_work, result.session_work)) {
    result.status = AffineMatrixServiceStatus::ResourceExhausted;
    record_service(kernel, result);
    return result;
  }

  const uint64_t action_id = kernel.structural_action_sequence + 1;
  MatrixLiveRequest request;
  request.owner = &session.source_owner_;
  request.matrix = &session.matrix_;
  request.exact_sources = &session.image_.equations;
  request.event = &event;
  request.owner_identity = session.owner_version_;
  request.transaction = transaction_limits(session.parameters_);
  request.maximum_revalidation_work =
      session.parameters_.maximum_snapshot_work;
  request.maximum_journal_bytes = session.parameters_.maximum_journal_bytes;
  request.maximum_external_extent = session.parameters_.maximum_external_extent;
  request.action_id = action_id;
  request.semantic_id = session.owner_version_;
  request.records = session.records_;
  request.proof_cache = &session.proof_cache_;
  if (session.persistent()) {
    request.premise_leases = &session.premise_leases_;
    if (session.retained_bytes_ >= session.persistence_.maximum_retained_bytes) {
      result.status = AffineMatrixServiceStatus::ResourceExhausted;
      record_service(kernel, result); return result;
    }
    uint64_t key_bytes = 2 * session.published_.capacity() * sizeof(std::string) +
                         2 * (sizeof(std::string) + result.event_identity.size() + 1);
    for (const auto &key : session.published_) key_bytes += 2 * (key.capacity() + 1);
    if (key_bytes >= session.persistence_.maximum_retained_bytes - session.retained_bytes_) {
      result.status = AffineMatrixServiceStatus::ResourceExhausted;
      record_service(kernel, result); return result;
    }
    request.maximum_persistent_bytes = session.persistence_.maximum_retained_bytes -
                                      session.retained_bytes_ - key_bytes;
  }
  result.installed_reason = event.reason;
  result.live = apply_affine_matrix_live(kernel, request);
  result.status = live_status(result.live.status);
  result.result = result.live.result;
  result.bcp_pending = result.live.propagation_pending;
  // Formula-changing root actions can retire a captured premise or a reason
  // variable after initialization.  The rejected event has made no mutation;
  // release the optional service and let ordinary CDCL continue instead of
  // repeatedly returning a terminal UNKNOWN.
  if (result.status == AffineMatrixServiceStatus::Stale ||
      result.status == AffineMatrixServiceStatus::Inapplicable)
    session.disable(kernel);
  if (result.status == AffineMatrixServiceStatus::Applied) {
    std::vector<std::string> committed = session.published_;
    committed.insert(committed.begin() + (duplicate - session.published_.begin()),
                     result.event_identity);
    session.published_.swap(committed);
    kernel.structural_action_sequence = action_id;
    if (!result.bcp_pending && result.result != ApplyResult::Unsat)
      std::abort();
  }
  record_service(kernel, result);
  return result;
}

TACTIX_REGISTER_POLICY_OP(AffineMatrixInitialize, RuleTier::Inprocess,
                          [](const Kernel &kernel) {
                            if (kernel.affine_matrix_initialize_parameters_active &&
                                kernel.affine_matrix_initialize_parameters.operation ==
                                    strategy::AffineMatrixInitializeParameters::Operation::SearchStep)
                              return RuleEligibility{}
                                  .fact("consistent", !kernel.inconsistent)
                                  .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
                                  .fact("propagated", kernel_propagation_fixpoint(kernel))
                                  .fact("sparse", !kernel.dense && kernel.large_watches_connected)
                                  .fact("no-probe-transaction", !kernel.probing.recording)
                                  .fact("profile-valid", kernel.affine_matrix_initialize_parameters.valid())
                                  .fact("clausal-proof", kernel.proof && !kernel.proof->lrat() && !kernel.proof->pb())
                                  .fact("no-extended-model", !kernel.model_extended)
                                  .fact("session-profile", kernel.affine_matrix_search_session &&
                                      kernel.affine_matrix_search_session->enabled() &&
                                      kernel.affine_matrix_search_session->attached_to(kernel) &&
                                      kernel.affine_matrix_search_session->matches_profile(
                                          kernel.affine_matrix_initialize_parameters.basis,
                                          {kernel.affine_matrix_initialize_parameters.maximum_retained_bytes,
                                           kernel.affine_matrix_initialize_parameters.maximum_deferred_deletions,
                                           kernel.affine_matrix_initialize_parameters.maximum_deferred_literal_work}));
                            auto guard = affine_matrix_initialize_guard(kernel);
                            if (kernel.affine_matrix_initialize_parameters_active) {
                              const auto &profile = kernel.affine_matrix_initialize_parameters;
                              guard.fact("profile-valid", profile.valid());
                              guard.fact("persistent-proof", kernel.proof && !kernel.model_extended);
                              if (profile.operation == strategy::AffineMatrixInitializeParameters::Operation::RootMaintenance)
                                guard.fact("session-profile", kernel.affine_matrix_search_session &&
                                    kernel.affine_matrix_search_session->enabled() &&
                                    kernel.affine_matrix_search_session->attached_to(kernel) &&
                                    kernel.affine_matrix_search_session->matches_profile(profile.basis,
                                        {profile.maximum_retained_bytes, profile.maximum_deferred_deletions,
                                         profile.maximum_deferred_literal_work}));
                            }
                            return guard;
                          },
                          affine_matrix_initialize_apply);

} // namespace tactix

#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <climits>
#include <sstream>

namespace tactix {
namespace {

using namespace affine_semantic_extension;

bool add(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (work > maximum || amount > maximum - work) return false;
  work += amount;
  return true;
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

TrailStatus exact_trail(const Kernel &kernel, uint64_t maximum,
                        uint64_t &work,
                        std::vector<MatrixAssignment> &out) {
  out.clear();
  out.reserve(kernel.trail.size());
  for (Lit literal : kernel.trail) {
    if (kernel_terminating(kernel)) return TrailStatus::Interrupted;
    if (!add(1, maximum, work)) return TrailStatus::Exhausted;
    const int32_t external = kernel_externalize(kernel, literal);
    if (!external || external == INT32_MIN) return TrailStatus::Invalid;
    out.push_back({external < 0 ? -external : external, external > 0});
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
  if (result.status == AffineMatrixInitStatus::Applied)
    ++kernel.stats.affine_matrix_init_applied;
  else if (result.status == AffineMatrixInitStatus::ResourceExhausted)
    ++kernel.stats.affine_matrix_init_exhausted;
  else if (result.status == AffineMatrixInitStatus::Interrupted)
    ++kernel.stats.affine_matrix_init_interrupted;
}

void record_service(Kernel &kernel, const AffineMatrixServiceResult &result) {
  ++kernel.stats.affine_matrix_service_calls;
  kernel.stats.affine_matrix_service_sync_work += result.synchronization_work;
  kernel.stats.affine_matrix_service_session_work += result.session_work;
  kernel.stats.affine_matrix_service_planner_work += result.live.planner_work;
  kernel.stats.affine_matrix_service_revalidation_work +=
      result.live.revalidation_work;
  kernel.stats.affine_matrix_service_proof_additions += result.live.additions;
  kernel.stats.affine_matrix_service_proof_deletions += result.live.deletions;
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
    const strategy::AffineConsequenceParameters &parameters) {
  AffineMatrixInitResult result;
  if (!affine_matrix_initialize_guard(kernel)) {
    result.status = AffineMatrixInitStatus::Inapplicable;
    record_init(kernel, result);
    return result;
  }
  if (!parameters.valid() ||
      parameters.semantics !=
          strategy::AffineConsequenceSemantics::CertifiedBasisV2) {
    record_init(kernel, result);
    return result;
  }
  if (session.initialization_retry_suppressed(kernel, parameters)) {
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
      matrix);
  result.matrix_work = matrix_work;
  if (built != MatrixStatus::Complete) {
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

  AffineMatrixSearchSession candidate;
  candidate.owner_ = &kernel;
  candidate.owner_version_ = owner_version;
  candidate.proof_ = kernel.proof;
  if (kernel.proof) candidate.proof_format_ = kernel.proof->format();
  candidate.records_ = kernel.structural_provenance;
  candidate.parameters_ = parameters;
  candidate.source_owner_ = std::move(owner);
  candidate.image_ = std::move(image);
  candidate.matrix_ = std::move(matrix);
  session = std::move(candidate);

  result.status = AffineMatrixInitStatus::Applied;
  result.result = ApplyResult::Ok;
  result.owner_version = session.owner_version_;
  result.equations = session.image_.equations.size();
  result.rows = session.matrix_.rows().size();
  record_init(kernel, result);
  return result;
}

ApplyResult affine_matrix_initialize_apply(Kernel &kernel) {
  if (!kernel.affine_matrix_search_session) return ApplyResult::NoOp;
  return affine_matrix_search_initialize(
             kernel, *kernel.affine_matrix_search_session,
             kernel.affine_consequence_parameters)
      .result;
}

AffineMatrixServiceResult affine_matrix_search_step(
    Kernel &kernel, AffineMatrixSearchSession &session) {
  AffineMatrixServiceResult result;
  if (!session.enabled()) return result;
  if (!session.belongs_to(kernel) || kernel.affine_matrix_search_session != &session ||
      session.records_ != kernel.structural_provenance ||
      session.proof_ != kernel.proof ||
      (kernel.proof && session.proof_format_ != kernel.proof->format())) {
    result.status = AffineMatrixServiceStatus::Stale;
    record_service(kernel, result);
    return result;
  }
  if (kernel.inconsistent || kernel.conflict_valid || kernel.pending.valid ||
      kernel.dense || kernel.probing.recording ||
      !kernel.large_watches_connected || !kernel_propagation_fixpoint(kernel)) {
    result.status = AffineMatrixServiceStatus::Inapplicable;
    record_service(kernel, result);
    return result;
  }
  if (kernel.structural_action_sequence == UINT64_MAX) {
    result.status = AffineMatrixServiceStatus::ResourceExhausted;
    record_service(kernel, result);
    return result;
  }
  std::vector<MatrixAssignment> trail;
  const TrailStatus trail_status = exact_trail(
      kernel, session.parameters_.basis_maximum_work,
      result.synchronization_work, trail);
  if (trail_status != TrailStatus::Complete) {
    result.status = trail_status == TrailStatus::Interrupted
                        ? AffineMatrixServiceStatus::Interrupted
                    : trail_status == TrailStatus::Exhausted
                        ? AffineMatrixServiceStatus::ResourceExhausted
                        : AffineMatrixServiceStatus::Stale;
    record_service(kernel, result);
    return result;
  }
  bool exhausted = false;
  MatrixEvent event;
  const MatrixStatus synchronized = session.matrix_.synchronize(
      trail,
      [&](uint64_t amount) {
        if (kernel_terminating(kernel)) return false;
        if (!add(amount, session.parameters_.basis_maximum_work,
                 result.synchronization_work)) {
          exhausted = true;
          return false;
        }
        return true;
      },
      event);
  if (synchronized != MatrixStatus::Complete) {
    result.status = kernel_terminating(kernel)
                        ? AffineMatrixServiceStatus::Interrupted
                    : synchronized == MatrixStatus::Exhausted || exhausted
                        ? AffineMatrixServiceStatus::ResourceExhausted
                        : AffineMatrixServiceStatus::Invalid;
    record_service(kernel, result);
    return result;
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
  result.live = apply_affine_matrix_live(kernel, request);
  result.status = live_status(result.live.status);
  result.result = result.live.result;
  result.bcp_pending = result.live.propagation_pending;
  if (result.status == AffineMatrixServiceStatus::Applied) {
    std::vector<std::string> committed = session.published_;
    committed.insert(committed.begin() + (duplicate - session.published_.begin()),
                     result.event_identity);
    session.published_.swap(committed);
    kernel.structural_action_sequence = action_id;
    if (!result.bcp_pending) std::abort();
  }
  record_service(kernel, result);
  return result;
}

TACTIX_REGISTER_POLICY_OP(AffineMatrixInitialize, RuleTier::Inprocess,
                          affine_matrix_initialize_guard,
                          affine_matrix_initialize_apply);

} // namespace tactix

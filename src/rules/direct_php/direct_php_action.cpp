#include "rules/direct_php/direct_php_action.hpp"

#include "rules/structural_action_guard.hpp"
#include "strategy/action_status_mapping.hpp"
#include "strategy/action_status_statistics.hpp"

#include <algorithm>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace tactix {

struct DirectPhpActionSessionAccess {
  static void commit(DirectPhpActionSession &session, const Kernel &kernel,
                     uint64_t action_id, DirectPhpActionRecord &record) {
    static_assert(
        std::is_nothrow_move_assignable_v<DirectPhpActionRecord>,
        "terminal publication must not be followed by a fallible commit");
    if (!session.belongs_to(kernel) || session.committed()) std::abort();
    session.action_id_ = action_id;
    session.record_ = std::move(record);
    session.committed_ = true;
  }
};

namespace {

using LiveStatus = direct_php_live::Status;

DirectPhpActionStatus action_status(LiveStatus status) {
  if (const auto failure =
          strategy::map_standard_action_failure<DirectPhpActionStatus>(status))
    return *failure;
  switch (status) {
  case LiveStatus::NotRecognized:
    return DirectPhpActionStatus::NotRecognized;
  case LiveStatus::Malformed: return DirectPhpActionStatus::Malformed;
  case LiveStatus::Ambiguous: return DirectPhpActionStatus::Ambiguous;
  case LiveStatus::Admitted: return DirectPhpActionStatus::Invalid;
  case LiveStatus::Prepared: return DirectPhpActionStatus::Prepared;
  case LiveStatus::Applied: return DirectPhpActionStatus::Applied;
  default: std::abort();
  }
}

direct_php_live::Request live_request(const DirectPhpActionLimits &limits) {
  return {limits.snapshot, limits.recognition, limits.certificate};
}

void copy_effort(const direct_php_live::Result &live,
                 DirectPhpActionEffort &effort) {
  effort.snapshot_clauses = live.snapshot_clauses;
  effort.snapshot_literals = live.snapshot_literals;
  effort.snapshot_work = live.snapshot_work;
  effort.recognition_work = live.recognition_work;
}

struct ReplayMeter {
  Kernel &kernel;
  uint64_t maximum = 0;
  uint64_t work = 0;
  bool interrupted = false;
  bool exhausted = false;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (work > maximum || amount > maximum - work) {
      exhausted = true;
      return false;
    }
    work += amount;
    return true;
  }
  // The status of a failed metered step: `otherwise` unless this meter was
  // interrupted or ran out of budget.
  DirectPhpActionStatus failure(DirectPhpActionStatus otherwise) const {
    return interrupted ? DirectPhpActionStatus::Interrupted
           : exhausted ? DirectPhpActionStatus::ResourceExhausted
                       : otherwise;
  }
};

// Prepared, or the reason the replay record could not be built.
DirectPhpActionStatus build_record(
    const DirectPhpActionRequest &request,
    const direct_php_live::Admission &admission,
    const direct_php_live::Transaction &transaction, ReplayMeter &meter,
    DirectPhpActionRecord &output) {
  DirectPhpActionRecord local;
  local.version = request.version;
  local.action_id = request.action_id;
  local.limits = request.limits;

  const auto &snapshot = admission.snapshot();
  if (!meter.charge(1)) return meter.failure(DirectPhpActionStatus::Invalid);
  for (const auto &source : snapshot.sources)
    if (!meter.charge(uint64_t{1} + source.clause.size()))
      return meter.failure(DirectPhpActionStatus::Invalid);
  for (const auto &premise : snapshot.premises)
    if (!meter.charge(uint64_t{1} + premise.literals.size()))
      return meter.failure(DirectPhpActionStatus::Invalid);

  // Recount the replay events with the planner's own accounting rule; each
  // addition carries a fresh id and each deletion only the id it retires.
  direct_php_certificate::Accounting observed;
  for (const direct_php_live::ReplayEvent &event : transaction.events()) {
    if (!meter.charge(uint64_t{1} + event.clause.size()))
      return meter.failure(DirectPhpActionStatus::Invalid);
    if ((event.kind == direct_php_certificate::EventKind::Delete
             ? !event.deleted_id || event.id
             : !event.id || event.deleted_id || event.source_occurrence) ||
        !direct_php_certificate::detail::count_event(observed, event.kind,
                                                     event.clause.size()))
      return DirectPhpActionStatus::Invalid;
  }
  const auto accounting = transaction.accounting();
  observed.fresh_variables = accounting.fresh_variables;
  observed.next_fresh = accounting.next_fresh;
  if (!(observed == accounting)) return DirectPhpActionStatus::Invalid;
  if (transaction.events().empty() ||
      transaction.events().back().kind !=
          direct_php_certificate::EventKind::AddRup ||
      !transaction.events().back().clause.empty())
    return DirectPhpActionStatus::Invalid;

  local.snapshot = snapshot;
  local.events = transaction.events();
  local.accounting = accounting;
  local.first_fresh = transaction.first_fresh();
  output = std::move(local);
  return DirectPhpActionStatus::Prepared;
}

bool same_source(const direct_php_live::Source &a,
                 const direct_php_live::Source &b, ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.occurrence != b.occurrence ||
      a.native_proof_id != b.native_proof_id ||
      a.identity_kind != b.identity_kind || a.kind != b.kind ||
      a.ref != b.ref || a.first != b.first || a.second != b.second ||
      a.clause.size() != b.clause.size())
    return false;
  return meter.charge(a.clause.size()) && a.clause == b.clause;
}

bool same_premise(const direct_php_recognition::Premise &a,
                  const direct_php_recognition::Premise &b,
                  ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.occurrence != b.occurrence ||
      a.native_proof_id != b.native_proof_id ||
      a.literals.size() != b.literals.size())
    return false;
  return meter.charge(a.literals.size()) && a.literals == b.literals;
}

bool same_snapshot(const direct_php_live::Snapshot &a,
                   const direct_php_live::Snapshot &b, ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.arena_generation != b.arena_generation ||
      a.next_clause_id != b.next_clause_id || a.nvars != b.nvars ||
      a.external_extent != b.external_extent || a.literals != b.literals ||
      a.work != b.work || a.sources.size() != b.sources.size() ||
      a.premises.size() != b.premises.size())
    return false;
  for (size_t i = 0; i < a.sources.size(); ++i)
    if (!same_source(a.sources[i], b.sources[i], meter)) return false;
  for (size_t i = 0; i < a.premises.size(); ++i)
    if (!same_premise(a.premises[i], b.premises[i], meter)) return false;
  return true;
}

bool same_event(const direct_php_live::ReplayEvent &a,
                const direct_php_live::ReplayEvent &b, ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.kind != b.kind || a.role != b.role || a.layer != b.layer ||
      a.id != b.id || a.deleted_id != b.deleted_id ||
      a.deleted_id_is_capture_local != b.deleted_id_is_capture_local ||
      a.source_occurrence != b.source_occurrence ||
      a.clause.size() != b.clause.size())
    return false;
  return meter.charge(a.clause.size()) && a.clause == b.clause;
}

bool same_record(const DirectPhpActionRecord &a,
                 const DirectPhpActionRecord &b, ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.version != b.version || a.action_id != b.action_id ||
      !(a.limits == b.limits) || a.first_fresh != b.first_fresh ||
      !(a.accounting == b.accounting) ||
      a.events.size() != b.events.size())
    return false;
  if (!same_snapshot(a.snapshot, b.snapshot, meter)) return false;
  for (size_t i = 0; i < a.events.size(); ++i)
    if (!same_event(a.events[i], b.events[i], meter)) return false;
  return true;
}

// `proof`, when given, is the accounting of the transaction or retained
// record the result reports.
DirectPhpActionResult stopped_result(
    DirectPhpActionStatus status, const DirectPhpActionRequest &request,
    const DirectPhpActionEffort &effort, bool replay_checked = false,
    const direct_php_certificate::Accounting *proof = nullptr) {
  DirectPhpActionResult result;
  result.status = status;
  result.action_id = request.action_id;
  result.replay_checked = replay_checked;
  result.effort = effort;
  if (proof) result.proof = *proof;
  return result;
}

const direct_php_certificate::Accounting *
accounting_of(const DirectPhpActionRecord *record) {
  return record ? &record->accounting : nullptr;
}

} // namespace

struct DirectPhpPreparedActionAccess {
  static DirectPhpActionResult prepare_impl(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request,
    const DirectPhpActionRecord *expected, bool proof_atomic,
    DirectPhpPreparedAction &output);
};

DirectPhpActionResult DirectPhpPreparedActionAccess::prepare_impl(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request,
    const DirectPhpActionRecord *expected, bool proof_atomic,
    DirectPhpPreparedAction &output) {
  output = {};
  DirectPhpActionEffort effort;
  if (!request.limits.valid() || request.version != 1 || !request.action_id)
    return stopped_result(DirectPhpActionStatus::Invalid, request, effort);
  if (!session.belongs_to(kernel))
    return stopped_result(DirectPhpActionStatus::Stale, request, effort);
  if (expected &&
      (expected->version != request.version ||
       expected->action_id != request.action_id ||
       !(expected->limits == request.limits)))
    return stopped_result(DirectPhpActionStatus::ReplayMismatch, request,
                          effort, true, accounting_of(session.record()));

  if (session.committed()) {
    const DirectPhpActionRecord *retained = session.record();
    if (!retained || retained->version != request.version ||
        retained->action_id != request.action_id ||
        !(retained->limits == request.limits))
      return stopped_result(DirectPhpActionStatus::ReplayMismatch, request,
                            effort, expected != nullptr,
                            accounting_of(retained));
    DirectPhpActionStatus status = DirectPhpActionStatus::AlreadyApplied;
    if (expected) {
      ReplayMeter meter{kernel, request.limits.maximum_replay_work};
      if (!same_record(*retained, *expected, meter))
        status = meter.failure(DirectPhpActionStatus::ReplayMismatch);
      effort.replay_work = meter.work;
    }
    return stopped_result(status, request, effort, expected != nullptr,
                          &retained->accounting);
  }

  // Proof has mutable private state beyond the Kernel pointer and clause
  // namespace (buffered bytes, variable maps, and checker population).  The
  // public split API cannot fingerprint it exactly, so expose that gap only
  // for proofless kernels.  apply/replay set proof_atomic and publish in the
  // same exclusive call.
  if (!proof_atomic && kernel.proof) {
    const auto status = kernel.proof->closed_by_refutation()
                            ? DirectPhpActionStatus::UnsupportedProof
                            : DirectPhpActionStatus::UnsupportedProofSplit;
    return stopped_result(status, request, effort);
  }

  direct_php_live::Admission admission;
  direct_php_live::Result live =
      direct_php_live::admit(kernel, live_request(request.limits), admission);
  copy_effort(live, effort);
  if (live.status != LiveStatus::Admitted)
    return stopped_result(action_status(live.status), request, effort);

  direct_php_live::Transaction transaction;
  live = direct_php_live::prepare(kernel, admission, transaction);
  if (live.status != LiveStatus::Prepared)
    return stopped_result(action_status(live.status), request, effort);

  ReplayMeter meter{kernel, request.limits.maximum_replay_work};
  DirectPhpActionRecord record;
  const DirectPhpActionStatus built =
      build_record(request, admission, transaction, meter, record);
  effort.replay_work = meter.work;
  if (built != DirectPhpActionStatus::Prepared)
    return stopped_result(built, request, effort, false,
                          &transaction.accounting());

  const bool replayed = !expected || same_record(record, *expected, meter);
  effort.replay_work = meter.work;
  if (!replayed)
    return stopped_result(
        meter.failure(DirectPhpActionStatus::ReplayMismatch), request, effort,
        true, &record.accounting);

  DirectPhpPreparedAction prepared;
  prepared.ready_ = true;
  prepared.owner_ = &kernel;
  prepared.session_ = &session;
  prepared.admission_ = std::move(admission);
  prepared.transaction_ = std::move(transaction);
  prepared.record_ = std::move(record);
  prepared.effort_ = effort;
  output = std::move(prepared);

  return stopped_result(DirectPhpActionStatus::Prepared, request, effort,
                        expected != nullptr, &output.record_.accounting);
}

bool DirectPhpActionLimits::valid() const {
  return direct_php_live::detail::valid(live_request(*this)) &&
         certificate.maximum_additions && certificate.maximum_deletions &&
         certificate.maximum_literals && certificate.maximum_fresh_variables &&
         certificate.maximum_events && maximum_replay_work;
}

RuleEligibility direct_php_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, true).clausal_certificate().finish();
}

DirectPhpActionResult direct_php_action_prepare(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request, DirectPhpPreparedAction &output) {
  return DirectPhpPreparedActionAccess::prepare_impl(
      kernel, session, request, nullptr, false, output);
}

DirectPhpActionResult direct_php_action_prepare_replay(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request,
    const DirectPhpActionRecord &expected,
    DirectPhpPreparedAction &output) {
  return DirectPhpPreparedActionAccess::prepare_impl(
      kernel, session, request, &expected, false, output);
}

DirectPhpActionResult direct_php_action_publish(
    Kernel &kernel, DirectPhpActionSession &session,
    DirectPhpPreparedAction &prepared) {
  DirectPhpActionResult result;
  result.action_id = prepared.record_.action_id;
  result.effort = prepared.effort_;
  result.proof = prepared.record_.accounting;
  if (!prepared.ready_) return result;
  if (prepared.owner_ != &kernel || prepared.session_ != &session ||
      session.committed()) {
    result.status = DirectPhpActionStatus::Stale;
    return result;
  }
  const direct_php_live::Result live =
      direct_php_live::publish(kernel, prepared.transaction_);
  result.status = action_status(live.status);
  if (live.status != LiveStatus::Applied) return result;

  DirectPhpActionSessionAccess::commit(
      session, kernel, prepared.record_.action_id, prepared.record_);
  prepared.ready_ = false;
  result.status = DirectPhpActionStatus::Applied;
  result.result = ApplyResult::Unsat;
  return result;
}

DirectPhpActionResult direct_php_action_apply(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request) {
  DirectPhpPreparedAction prepared;
  DirectPhpActionResult result = DirectPhpPreparedActionAccess::prepare_impl(
      kernel, session, request, nullptr, true, prepared);
  if (result.status != DirectPhpActionStatus::Prepared) return result;
  return direct_php_action_publish(kernel, session, prepared);
}

DirectPhpActionResult direct_php_action_replay(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request,
    const DirectPhpActionRecord &expected) {
  DirectPhpPreparedAction prepared;
  DirectPhpActionResult result = DirectPhpPreparedActionAccess::prepare_impl(
      kernel, session, request, &expected, true, prepared);
  if (result.status != DirectPhpActionStatus::Prepared) return result;
  result = direct_php_action_publish(kernel, session, prepared);
  result.replay_checked = true;
  return result;
}

void record_direct_php_action_feedback(Kernel &kernel,
                                       const DirectPhpActionResult &result) {
  auto &s = kernel.stats;
  ++s.direct_php_attempts;
  if (!strategy::record_standard_action_status(
          result.status,
          TACTIX_STANDARD_ACTION_STATUS_COUNTERS(s, direct_php))) {
    switch (result.status) {
    case DirectPhpActionStatus::NotRecognized:
      ++s.direct_php_not_recognized; break;
    case DirectPhpActionStatus::Malformed: ++s.direct_php_malformed; break;
    case DirectPhpActionStatus::Ambiguous: ++s.direct_php_ambiguous; break;
    case DirectPhpActionStatus::UnsupportedProofSplit:
      ++s.direct_php_unsupported_proof_split; break;
    case DirectPhpActionStatus::AlreadyApplied:
      ++s.direct_php_already_applied; break;
    case DirectPhpActionStatus::ReplayMismatch:
      ++s.direct_php_replay_mismatch; break;
    case DirectPhpActionStatus::Prepared: ++s.direct_php_prepared; break;
    default: break;
    }
  }
  s.direct_php_snapshot_clauses += result.effort.snapshot_clauses;
  s.direct_php_snapshot_literals += result.effort.snapshot_literals;
  s.direct_php_snapshot_work += result.effort.snapshot_work;
  s.direct_php_recognition_work += result.effort.recognition_work;
  s.direct_php_replay_work += result.effort.replay_work;
  s.direct_php_proof_additions += result.proof.additions;
  s.direct_php_proof_rup_additions += result.proof.rup_additions;
  s.direct_php_proof_rat_additions += result.proof.rat_additions;
  s.direct_php_proof_deletions += result.proof.deletions;
  s.direct_php_proof_addition_literals += result.proof.addition_literals;
  s.direct_php_proof_deletion_literals += result.proof.deletion_literals;
  s.direct_php_proof_fresh_variables += result.proof.fresh_variables;
  s.direct_php_proof_events += result.proof.events;
  s.direct_php_proof_maximum_event_width = std::max(
      s.direct_php_proof_maximum_event_width, result.proof.maximum_event_width);
}

} // namespace tactix

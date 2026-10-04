#include "rules/direct_php/direct_php_action.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
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

bool same_snapshot_limits(const direct_php_live::SnapshotLimits &a,
                          const direct_php_live::SnapshotLimits &b) {
  return a.maximum_clauses == b.maximum_clauses &&
         a.maximum_literals == b.maximum_literals &&
         a.maximum_work == b.maximum_work;
}

bool same_recognition_limits(const direct_php_recognition::Limits &a,
                             const direct_php_recognition::Limits &b) {
  return a.maximum_clauses == b.maximum_clauses &&
         a.maximum_literals == b.maximum_literals &&
         a.maximum_work == b.maximum_work &&
         a.maximum_row_candidates == b.maximum_row_candidates &&
         a.maximum_row_combinations == b.maximum_row_combinations &&
         a.maximum_mapping_trials == b.maximum_mapping_trials &&
         a.maximum_holes == b.maximum_holes;
}

bool same_certificate_limits(const direct_php_certificate::Limits &a,
                             const direct_php_certificate::Limits &b) {
  return a.maximum_additions == b.maximum_additions &&
         a.maximum_deletions == b.maximum_deletions &&
         a.maximum_literals == b.maximum_literals &&
         a.maximum_fresh_variables == b.maximum_fresh_variables &&
         a.maximum_events == b.maximum_events;
}

DirectPhpActionStatus action_status(LiveStatus status) {
  switch (status) {
  case LiveStatus::Invalid: return DirectPhpActionStatus::Invalid;
  case LiveStatus::Inapplicable: return DirectPhpActionStatus::Inapplicable;
  case LiveStatus::NotRecognized:
    return DirectPhpActionStatus::NotRecognized;
  case LiveStatus::Malformed: return DirectPhpActionStatus::Malformed;
  case LiveStatus::Ambiguous: return DirectPhpActionStatus::Ambiguous;
  case LiveStatus::ResourceExhausted:
    return DirectPhpActionStatus::ResourceExhausted;
  case LiveStatus::Interrupted: return DirectPhpActionStatus::Interrupted;
  case LiveStatus::UnsupportedProof:
    return DirectPhpActionStatus::UnsupportedProof;
  case LiveStatus::Stale: return DirectPhpActionStatus::Stale;
  case LiveStatus::Admitted: return DirectPhpActionStatus::Invalid;
  case LiveStatus::Prepared: return DirectPhpActionStatus::Prepared;
  case LiveStatus::Applied: return DirectPhpActionStatus::Applied;
  }
  std::abort();
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
};

bool add(uint64_t &value, uint64_t amount) {
  if (amount > std::numeric_limits<uint64_t>::max() - value) return false;
  value += amount;
  return true;
}

enum class RecordStatus : uint8_t {
  Complete,
  Invalid,
  Exhausted,
  Interrupted,
};

RecordStatus meter_status(const ReplayMeter &meter) {
  return meter.interrupted ? RecordStatus::Interrupted
                           : meter.exhausted ? RecordStatus::Exhausted
                                             : RecordStatus::Invalid;
}

RecordStatus build_record(
    const DirectPhpActionRequest &request,
    const direct_php_live::Admission &admission,
    const direct_php_live::Transaction &transaction, ReplayMeter &meter,
    DirectPhpActionRecord &output) {
  DirectPhpActionRecord local;
  local.version = request.version;
  local.action_id = request.action_id;
  local.limits = request.limits;

  const auto &snapshot = admission.snapshot();
  if (!meter.charge(1)) return meter_status(meter);
  for (const auto &source : snapshot.sources)
    if (!meter.charge(uint64_t{1} + source.clause.size()))
      return meter_status(meter);
  for (const auto &premise : snapshot.premises)
    if (!meter.charge(uint64_t{1} + premise.literals.size()))
      return meter_status(meter);

  direct_php_certificate::Accounting observed;
  for (const direct_php_live::ReplayEvent &event : transaction.events()) {
    if (!meter.charge(uint64_t{1} + event.clause.size()))
      return meter_status(meter);
    if (!add(observed.events, 1)) return RecordStatus::Invalid;
    observed.maximum_event_width =
        std::max<uint64_t>(observed.maximum_event_width, event.clause.size());
    if (event.kind == direct_php_certificate::EventKind::Delete) {
      if (!event.deleted_id || event.id ||
          !add(observed.deletions, 1) ||
          !add(observed.deletion_literals, event.clause.size()))
        return RecordStatus::Invalid;
    } else {
      if (!event.id || event.deleted_id || event.source_occurrence ||
          !add(observed.additions, 1) ||
          !add(observed.addition_literals, event.clause.size()))
        return RecordStatus::Invalid;
      uint64_t &kind = event.kind == direct_php_certificate::EventKind::AddRup
                           ? observed.rup_additions
                           : observed.rat_additions;
      if (!add(kind, 1)) return RecordStatus::Invalid;
    }
  }
  const auto accounting = transaction.accounting();
  observed.fresh_variables = accounting.fresh_variables;
  observed.next_fresh = accounting.next_fresh;
  if (!(observed == accounting)) return RecordStatus::Invalid;
  if (transaction.events().empty() ||
      transaction.events().back().kind !=
          direct_php_certificate::EventKind::AddRup ||
      !transaction.events().back().clause.empty())
    return RecordStatus::Invalid;

  local.snapshot = snapshot;
  local.events = transaction.events();
  local.accounting = accounting;
  local.first_fresh = transaction.first_fresh();
  output = std::move(local);
  return RecordStatus::Complete;
}

bool same_source(const direct_php_live::Source &a,
                 const direct_php_live::Source &b, ReplayMeter &meter) {
  if (!meter.charge(1)) return false;
  if (a.occurrence != b.occurrence ||
      a.native_proof_id != b.native_proof_id || a.kind != b.kind ||
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

DirectPhpActionResult stopped_result(DirectPhpActionStatus status,
                                     const DirectPhpActionRequest &request,
                                     const DirectPhpActionEffort &effort,
                                     bool replay_checked = false) {
  DirectPhpActionResult result;
  result.status = status;
  result.action_id = request.action_id;
  result.replay_checked = replay_checked;
  result.effort = effort;
  return result;
}

DirectPhpActionStatus record_failure(RecordStatus status) {
  switch (status) {
  case RecordStatus::Complete: return DirectPhpActionStatus::Prepared;
  case RecordStatus::Invalid: return DirectPhpActionStatus::Invalid;
  case RecordStatus::Exhausted:
    return DirectPhpActionStatus::ResourceExhausted;
  case RecordStatus::Interrupted: return DirectPhpActionStatus::Interrupted;
  }
  std::abort();
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
       !(expected->limits == request.limits))) {
    DirectPhpActionResult result = stopped_result(
        DirectPhpActionStatus::ReplayMismatch, request, effort, true);
    if (const DirectPhpActionRecord *retained = session.record())
      result.proof = retained->accounting;
    return result;
  }

  if (session.committed()) {
    const DirectPhpActionRecord *retained = session.record();
    if (!retained || retained->version != request.version ||
        retained->action_id != request.action_id ||
        !(retained->limits == request.limits)) {
      DirectPhpActionResult result = stopped_result(
          DirectPhpActionStatus::ReplayMismatch, request, effort,
          expected != nullptr);
      if (retained) result.proof = retained->accounting;
      return result;
    }
    DirectPhpActionResult result;
    if (expected) {
      ReplayMeter meter{kernel, request.limits.maximum_replay_work};
      const bool exact = same_record(*retained, *expected, meter);
      effort.replay_work = meter.work;
      const DirectPhpActionStatus status =
          exact ? DirectPhpActionStatus::AlreadyApplied
          : meter.interrupted ? DirectPhpActionStatus::Interrupted
          : meter.exhausted ? DirectPhpActionStatus::ResourceExhausted
                            : DirectPhpActionStatus::ReplayMismatch;
      result = stopped_result(status, request, effort, true);
    } else {
      result = stopped_result(DirectPhpActionStatus::AlreadyApplied, request,
                              effort);
    }
    result.proof = retained->accounting;
    return result;
  }

  // Proof has mutable private state beyond the Kernel pointer and clause
  // namespace (buffered bytes, variable maps, and checker population).  The
  // public split API cannot fingerprint it exactly, so expose that gap only
  // for proofless kernels.  apply/replay set proof_atomic and publish in the
  // same exclusive call.
  if (!proof_atomic && kernel.proof) {
    const auto status = kernel.proof->lrat() || kernel.proof->pb()
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
  const RecordStatus built =
      build_record(request, admission, transaction, meter, record);
  effort.replay_work = meter.work;
  if (built != RecordStatus::Complete) {
    DirectPhpActionResult result =
        stopped_result(record_failure(built), request, effort);
    result.proof = transaction.accounting();
    return result;
  }

  if (expected && !same_record(record, *expected, meter)) {
    effort.replay_work = meter.work;
    const auto status = meter.interrupted
                            ? DirectPhpActionStatus::Interrupted
                        : meter.exhausted
                            ? DirectPhpActionStatus::ResourceExhausted
                            : DirectPhpActionStatus::ReplayMismatch;
    DirectPhpActionResult result =
        stopped_result(status, request, effort, true);
    result.proof = record.accounting;
    return result;
  }
  effort.replay_work = meter.work;

  DirectPhpPreparedAction prepared;
  prepared.ready_ = true;
  prepared.owner_ = &kernel;
  prepared.session_ = &session;
  prepared.admission_ = std::move(admission);
  prepared.transaction_ = std::move(transaction);
  prepared.record_ = std::move(record);
  prepared.effort_ = effort;
  output = std::move(prepared);

  DirectPhpActionResult result = stopped_result(
      DirectPhpActionStatus::Prepared, request, effort, expected != nullptr);
  result.proof = output.record_.accounting;
  return result;
}

bool operator==(const DirectPhpActionLimits &a,
                const DirectPhpActionLimits &b) {
  return same_snapshot_limits(a.snapshot, b.snapshot) &&
         same_recognition_limits(a.recognition, b.recognition) &&
         same_certificate_limits(a.certificate, b.certificate) &&
         a.maximum_replay_work == b.maximum_replay_work;
}

bool DirectPhpActionLimits::valid() const {
  return snapshot.maximum_clauses && snapshot.maximum_literals &&
         snapshot.maximum_work && recognition.maximum_clauses &&
         recognition.maximum_literals && recognition.maximum_work &&
         recognition.maximum_row_candidates &&
         recognition.maximum_row_combinations &&
         recognition.maximum_mapping_trials && recognition.maximum_holes &&
         certificate.maximum_additions && certificate.maximum_deletions &&
         certificate.maximum_literals && certificate.maximum_fresh_variables &&
         certificate.maximum_events && maximum_replay_work;
}

RuleEligibility direct_php_action_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", !kernel.level)
      .fact("assignment-free", kernel.trail.empty())
      .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("clausal-certificate",
            !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->pb()));
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

} // namespace tactix

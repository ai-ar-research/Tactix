#include "rules/direct_php/direct_php_action.hpp"

#include "rules/semantic_extension/interruptible_work.hpp"
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

// Prepared, or the reason the replay record could not be built.
DirectPhpActionStatus build_record(
    const DirectPhpActionRequest &request,
    const direct_php_live::Admission &admission,
    const direct_php_live::Transaction &transaction, InterruptibleWork &meter,
    DirectPhpActionRecord &output) {
  DirectPhpActionRecord local;
  local.version = request.version;
  local.action_id = request.action_id;
  local.limits = request.limits;

  const auto &snapshot = admission.snapshot();
  if (!meter.charge(1))
    return stopped_status(meter, DirectPhpActionStatus::Invalid);
  for (const auto &source : snapshot.sources)
    if (!meter.charge(uint64_t{1} + source.clause.size()))
      return stopped_status(meter, DirectPhpActionStatus::Invalid);
  for (const auto &premise : snapshot.premises)
    if (!meter.charge(uint64_t{1} + premise.literals.size()))
      return stopped_status(meter, DirectPhpActionStatus::Invalid);

  // Recount the replay events with the planner's own accounting rule; each
  // addition carries a fresh id and each deletion only the id it retires.
  direct_php_certificate::Accounting observed;
  for (const direct_php_live::ReplayEvent &event : transaction.events()) {
    if (!meter.charge(uint64_t{1} + event.clause.size()))
      return stopped_status(meter, DirectPhpActionStatus::Invalid);
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

// `proof`, when given, is the accounting of the transaction or retained
// record the result reports.
DirectPhpActionResult stopped_result(
    DirectPhpActionStatus status, const DirectPhpActionRequest &request,
    const DirectPhpActionEffort &effort,
    const direct_php_certificate::Accounting *proof = nullptr) {
  DirectPhpActionResult result;
  result.status = status;
  result.action_id = request.action_id;
  result.effort = effort;
  if (proof) result.proof = *proof;
  return result;
}

} // namespace

bool DirectPhpActionLimits::valid() const {
  return direct_php_live::detail::valid(live_request(*this)) &&
         certificate.maximum_additions && certificate.maximum_deletions &&
         certificate.maximum_literals && certificate.maximum_fresh_variables &&
         certificate.maximum_events && maximum_replay_work;
}

RuleEligibility direct_php_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, true).clausal_certificate().finish();
}

DirectPhpActionResult direct_php_action_apply(
    Kernel &kernel, DirectPhpActionSession &session,
    const DirectPhpActionRequest &request) {
  DirectPhpActionEffort effort;
  if (!request.limits.valid() || request.version != 1 || !request.action_id)
    return stopped_result(DirectPhpActionStatus::Invalid, request, effort);
  if (!session.belongs_to(kernel))
    return stopped_result(DirectPhpActionStatus::Stale, request, effort);
  if (const DirectPhpActionRecord *retained = session.record()) {
    const bool same = retained->version == request.version &&
                      retained->action_id == request.action_id &&
                      retained->limits == request.limits;
    return stopped_result(same ? DirectPhpActionStatus::AlreadyApplied
                               : DirectPhpActionStatus::ReplayMismatch,
                          request, effort, &retained->accounting);
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

  InterruptibleWork meter{kernel, request.limits.maximum_replay_work};
  DirectPhpActionRecord record;
  const DirectPhpActionStatus built =
      build_record(request, admission, transaction, meter, record);
  effort.replay_work = meter.used;
  if (built != DirectPhpActionStatus::Prepared)
    return stopped_result(built, request, effort, &transaction.accounting());

  DirectPhpActionResult result;
  result.action_id = record.action_id;
  result.effort = effort;
  result.proof = record.accounting;
  live = direct_php_live::publish(kernel, transaction);
  result.status = action_status(live.status);
  if (live.status != LiveStatus::Applied) return result;
  DirectPhpActionSessionAccess::commit(session, kernel, record.action_id,
                                       record);
  result.status = DirectPhpActionStatus::Applied;
  result.result = ApplyResult::Unsat;
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

#include "rules/lucky/tseitin_cycle_transaction.hpp"
#include "rules/lucky/callback_work_meter.hpp"

#include <set>

namespace tactix::tseitin_cycle {
namespace {

using Meter = CallbackWorkMeter<
    TransactionLimits, TransactionStatus, TransactionStatus::Ready,
    TransactionStatus::Interrupted, TransactionStatus::ResourceExhausted>;

TransactionResult stop(const Meter &meter, TransactionStatus status,
                       TransactionReason reason) {
  TransactionResult result;
  result.status = status;
  result.reason = reason;
  result.work = meter.used();
  return result;
}

TransactionResult metered_stop(const Meter &meter) {
  if (meter.stopped() == TransactionStatus::Interrupted)
    return stop(meter, TransactionStatus::Interrupted,
                TransactionReason::Interrupted);
  return stop(meter, TransactionStatus::ResourceExhausted,
              TransactionReason::WorkLimit);
}

bool valid_source_ledger(const AdmissionResult &admission, Meter &meter) {
  if (admission.provenance.size() != admission.input.equations.size())
    return false;
  std::set<uint64_t> groups, occurrences, proofs;
  for (size_t i = 0; i < admission.provenance.size(); ++i) {
    if (!meter.spend())
      return false;
    const EquationProvenance &source = admission.provenance[i];
    if (!source.group_identity ||
        source.vertex != admission.input.equations[i].vertex ||
        !groups.insert(source.group_identity).second ||
        source.premises.empty())
      return false;
    if (i && admission.provenance[i - 1].group_identity >=
                 source.group_identity)
      return false;
    for (const StablePremise &premise : source.premises) {
      if (!meter.spend())
        return false;
      if (!premise.occurrence || !premise.proof ||
          !occurrences.insert(premise.occurrence).second ||
          !proofs.insert(premise.proof).second)
        return false;
    }
  }
  return true;
}

} // namespace

TransactionResult build_transaction(const AdmissionResult &admission,
                                    const Result &plan,
                                    TransactionLimits limits) {
  Meter meter(limits);
  if (!admission.admitted())
    return stop(meter, TransactionStatus::Declined,
                TransactionReason::AdmissionNotCommitted);
  if (!plan.planned())
    return stop(meter, TransactionStatus::Declined,
                TransactionReason::PlanNotCommitted);
  uint64_t validation_work = 0;
  bool validation_exhausted = false;
  bool validation_interrupted = false;
  const bool candidate_valid =
      validate_candidate(admission.input, plan.candidate, limits.work,
                         limits.interrupted, limits.interrupt_state,
                         validation_work, validation_exhausted,
                         validation_interrupted);
  if (!meter.spend(validation_work))
    return metered_stop(meter);
  if (validation_interrupted)
    return stop(meter, TransactionStatus::Interrupted,
                TransactionReason::Interrupted);
  if (validation_exhausted)
    return stop(meter, TransactionStatus::ResourceExhausted,
                TransactionReason::WorkLimit);
  if (!candidate_valid)
    return stop(meter, TransactionStatus::InvalidInput,
                TransactionReason::InvalidCandidate);
  if (!valid_source_ledger(admission, meter)) {
    if (meter.stopped() != TransactionStatus::Ready)
      return metered_stop(meter);
    return stop(meter, TransactionStatus::InvalidInput,
                TransactionReason::InvalidSourceLedger);
  }

  ProofTransaction transaction;
  transaction.version = 1;
  transaction.identity.pivot = plan.candidate.pivot;
  transaction.identity.cycle_edges = plan.candidate.cycle_edges;
  for (const EquationProvenance &source : admission.provenance) {
    TransactionSource copied;
    copied.group_identity = source.group_identity;
    copied.vertex = source.vertex;
    for (const StablePremise premise : source.premises)
      copied.premises.push_back({premise.occurrence, premise.proof});
    transaction.identity.sources.push_back(std::move(copied));
  }

  if (!meter.spend())
    return metered_stop(meter);
  transaction.x7.clause.push_back(-plan.candidate.pivot);
  // The negative unit is the semantic pivot.  Its implicit X7 image is true,
  // which maps the pivot variable to false.  Every remaining cycle edge flips.
  for (size_t i = 1; i < plan.candidate.cycle_edges.size(); ++i) {
    if (!meter.spend())
      return metered_stop(meter);
    const Edge edge = plan.candidate.cycle_edges[i];
    transaction.x7.substitution.push_back({edge, -edge});
  }

  // Proof::add_sr uses the propagating-unit helper form for this direct DSR
  // witness: two temporary clauses, the RUP unit, then both deletions.
  transaction.accounting.preflight_additions = 3;
  transaction.accounting.preflight_deletions = 2;
  transaction.accounting.temporary_helpers = 2;
  transaction.accounting.proof_only_variables = 1;
  transaction.accounting.committed_solver_clauses = 1;
  transaction.accounting.witness_literals =
      1 + 2 * transaction.x7.substitution.size();
  if (transaction.accounting.preflight_additions > limits.additions)
    return stop(meter, TransactionStatus::ResourceExhausted,
                TransactionReason::AdditionLimit);
  if (transaction.accounting.preflight_deletions > limits.deletions)
    return stop(meter, TransactionStatus::ResourceExhausted,
                TransactionReason::DeletionLimit);
  if (transaction.accounting.temporary_helpers > limits.helpers)
    return stop(meter, TransactionStatus::ResourceExhausted,
                TransactionReason::HelperLimit);
  if (transaction.accounting.witness_literals > limits.witness_literals)
    return stop(meter, TransactionStatus::ResourceExhausted,
                TransactionReason::WitnessLimit);
  const ProofEventKind events[] = {
      ProofEventKind::AddNegativeHelper,
      ProofEventKind::AddPositiveHelper,
      ProofEventKind::AddCandidateRup,
      ProofEventKind::DeletePositiveHelper,
      ProofEventKind::DeleteNegativeHelper,
  };
  for (ProofEventKind event : events) {
    if (!meter.spend())
      return metered_stop(meter);
    transaction.events.push_back(event);
  }
  transaction.requires_unassigned_pivot = true;
  transaction.requires_live_x7_validation = true;
  transaction.requires_post_apply_replan = true;

  TransactionResult result;
  result.status = TransactionStatus::Ready;
  result.reason = TransactionReason::None;
  result.work = meter.used();
  result.transaction_work = result.work;
  result.transaction = std::move(transaction);
  return result;
}

TransactionResult replay_transaction(const AdmissionRequest &request,
                                     const ProofTransaction &recorded,
                                     ReplayLimits limits) {
  // Admission owns its own transactional callback and work accounting.
  AdmissionResult admission = admit(request, limits.admission);
  if (!admission.admitted()) {
    TransactionResult result;
    result.admission_work = admission.work;
    result.status = admission.status == AdmissionStatus::Interrupted
                        ? TransactionStatus::Interrupted
                    : admission.status == AdmissionStatus::ResourceExhausted
                        ? TransactionStatus::ResourceExhausted
                        : TransactionStatus::Declined;
    result.reason = admission.status == AdmissionStatus::Interrupted
                        ? TransactionReason::Interrupted
                        : TransactionReason::ReplayAdmissionFailed;
    return result;
  }
  const Result plan = plan_one(admission.input, limits.plan);
  if (!plan.planned()) {
    TransactionResult result;
    result.admission_work = admission.work;
    result.planning_work = plan.work;
    result.status = plan.decline == DeclineReason::Interrupted
                        ? TransactionStatus::Interrupted
                    : plan.decline == DeclineReason::WorkLimit
                        ? TransactionStatus::ResourceExhausted
                        : TransactionStatus::Declined;
    result.reason = plan.decline == DeclineReason::Interrupted
                        ? TransactionReason::Interrupted
                        : TransactionReason::ReplayPlanFailed;
    return result;
  }
  TransactionResult rebuilt =
      build_transaction(admission, plan, limits.transaction);
  rebuilt.admission_work = admission.work;
  rebuilt.planning_work = plan.work;
  rebuilt.transaction_work = rebuilt.work;
  if (!rebuilt.ready())
    return rebuilt;
  if (!(rebuilt.transaction == recorded)) {
    const uint64_t work = rebuilt.work;
    const uint64_t admission_work = rebuilt.admission_work;
    const uint64_t planning_work = rebuilt.planning_work;
    const uint64_t transaction_work = rebuilt.transaction_work;
    rebuilt = {};
    rebuilt.status = TransactionStatus::InvalidInput;
    rebuilt.reason = TransactionReason::ReplayMismatch;
    rebuilt.work = work;
    rebuilt.admission_work = admission_work;
    rebuilt.planning_work = planning_work;
    rebuilt.transaction_work = transaction_work;
  }
  return rebuilt;
}

const char *transaction_reason_name(TransactionReason reason) {
  switch (reason) {
  case TransactionReason::None: return "none";
  case TransactionReason::AdmissionNotCommitted: return "admission-not-committed";
  case TransactionReason::PlanNotCommitted: return "plan-not-committed";
  case TransactionReason::InvalidCandidate: return "invalid-candidate";
  case TransactionReason::InvalidSourceLedger: return "invalid-source-ledger";
  case TransactionReason::AdditionLimit: return "addition-limit";
  case TransactionReason::DeletionLimit: return "deletion-limit";
  case TransactionReason::HelperLimit: return "helper-limit";
  case TransactionReason::WitnessLimit: return "witness-limit";
  case TransactionReason::WorkLimit: return "work-limit";
  case TransactionReason::Interrupted: return "interrupted";
  case TransactionReason::ReplayAdmissionFailed: return "replay-admission-failed";
  case TransactionReason::ReplayPlanFailed: return "replay-plan-failed";
  case TransactionReason::ReplayMismatch: return "replay-mismatch";
  }
  return "unknown";
}

} // namespace tactix::tseitin_cycle

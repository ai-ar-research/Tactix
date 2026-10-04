#include "rules/lucky/tseitin_cycle_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <set>

namespace tactix::tseitin_cycle_live {
namespace {

enum class CaptureStop { Complete, Invalid, ClauseLimit, LiteralLimit,
                         WorkLimit, Interrupted };

bool applicable(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel);
}

bool valid(const SnapshotLimits &limits) {
  return limits.maximum_clauses && limits.maximum_literals &&
         limits.maximum_work;
}

bool valid(const Request &request) {
  return valid(request.snapshot) && request.admission.work &&
         request.planning.work && request.planning.witness_length &&
         request.planning.additions && request.transaction.work &&
         request.transaction.additions && request.transaction.deletions &&
         request.transaction.helpers && request.transaction.witness_literals;
}

bool supported_proof(const Kernel &kernel) {
  return !kernel.proof || kernel.proof->dsr();
}

Result stopped(CaptureStop stop, const Snapshot &partial) {
  Result result;
  result.snapshot_clauses = partial.sources.size();
  result.snapshot_literals = partial.literals;
  result.snapshot_work = partial.work;
  switch (stop) {
  case CaptureStop::Complete:
    result.status = Status::Captured;
    return result;
  case CaptureStop::Interrupted:
    result.status = Status::Interrupted;
    result.reason = Reason::Interrupted;
    return result;
  case CaptureStop::ClauseLimit:
    result.status = Status::ResourceExhausted;
    result.reason = Reason::SnapshotClauseLimit;
    return result;
  case CaptureStop::LiteralLimit:
    result.status = Status::ResourceExhausted;
    result.reason = Reason::SnapshotLiteralLimit;
    return result;
  case CaptureStop::WorkLimit:
    result.status = Status::ResourceExhausted;
    result.reason = Reason::SnapshotWorkLimit;
    return result;
  case CaptureStop::Invalid:
    result.status = Status::Invalid;
    result.reason = Reason::InvalidKernelPopulation;
    return result;
  }
  return result;
}

CaptureStop capture_impl(Kernel &kernel, const SnapshotLimits &limits,
                         Snapshot &output, Snapshot &partial) {
  partial.arena_generation = kernel.arena_generation;
  partial.touch_epoch = kernel.touch_epoch;
  partial.root_assignments = kernel.stats.root_assignments;
  partial.next_clause_id = kernel.next_clause_id;
  partial.nvars = kernel.nvars;
  partial.internal_to_external = kernel.internal_to_external;
  partial.external_to_internal = kernel.external_to_internal;

  CaptureStop failure = CaptureStop::Invalid;
  const auto spend = [&](uint64_t amount = 1) {
    if (kernel_terminating(kernel)) {
      failure = CaptureStop::Interrupted;
      return false;
    }
    if (partial.work > limits.maximum_work ||
        amount > limits.maximum_work - partial.work) {
      failure = CaptureStop::WorkLimit;
      return false;
    }
    partial.work += amount;
    return true;
  };
  const auto take = [&](SourceKind kind, Ref ref, Lit first, Lit second,
                        uint64_t proof, const Lit *literals, uint32_t size) {
    if (!proof || !size || !literals) {
      failure = CaptureStop::Invalid;
      return false;
    }
    if (partial.sources.size() == limits.maximum_clauses) {
      failure = CaptureStop::ClauseLimit;
      return false;
    }
    if (partial.literals > limits.maximum_literals ||
        size > limits.maximum_literals - partial.literals) {
      failure = CaptureStop::LiteralLimit;
      return false;
    }
    if (!spend(uint64_t(size) + 1)) return false;
    Source source;
    source.native_proof_id = proof;
    source.kind = kind;
    source.ref = ref;
    source.first = first;
    source.second = second;
    source.clause.reserve(size);
    std::set<int32_t> variables;
    for (uint32_t index = 0; index < size; ++index) {
      const Lit literal = literals[index];
      if (literal == INVALID_LIT || var_of(literal) >= kernel.nvars ||
          var_of(literal) >= kernel.internal_to_external.size() ||
          kernel.internal_to_external[var_of(literal)] >= uint32_t(INT32_MAX) ||
          kernel_var_retired(kernel, var_of(literal))) {
        failure = CaptureStop::Invalid;
        return false;
      }
      const int32_t external = kernel_externalize(kernel, literal);
      if (!external || external == INT32_MIN ||
          !variables.insert(external < 0 ? -external : external).second) {
        failure = CaptureStop::Invalid;
        return false;
      }
      source.clause.push_back(external);
    }
    partial.literals += size;
    partial.sources.push_back(std::move(source));
    return true;
  };

  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Ref current = ref;
    const Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - ref)
      return CaptureStop::Invalid;
    ref += words;
    if (!spend()) return failure;
    if (!clause->garbage() &&
        !take(SourceKind::Large, current, INVALID_LIT, INVALID_LIT,
              clause->id(), clause->lits, clause->size))
      return failure;
  }

  std::set<uint64_t> binary_keys;
  for (Lit first = 0; first < 2 * kernel.nvars; ++first) {
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!spend()) return failure;
      if (!watch.binary() || first >= watch.other()) continue;
      const uint64_t key = kernel_binary_key(first, watch.other());
      if (!binary_keys.insert(key).second) return CaptureStop::Invalid;
      const Lit pair[] = {first, watch.other()};
      if (!take(SourceKind::Binary, INVALID_REF, first, watch.other(),
                kernel_binary_id(kernel, first, watch.other()), pair, 2))
        return failure;
    }
  }

  std::sort(partial.sources.begin(), partial.sources.end(),
            [](const Source &left, const Source &right) {
              if (left.native_proof_id != right.native_proof_id)
                return left.native_proof_id < right.native_proof_id;
              if (left.clause != right.clause)
                return left.clause < right.clause;
              return left.kind < right.kind;
            });
  std::set<uint64_t> proof_ids;
  partial.clauses.reserve(partial.sources.size());
  for (size_t index = 0; index < partial.sources.size(); ++index) {
    Source &source = partial.sources[index];
    if (!proof_ids.insert(source.native_proof_id).second)
      return CaptureStop::Invalid;
    source.occurrence = index + 1;
    partial.clauses.push_back(
        {source.occurrence, source.native_proof_id, source.clause});
  }
  output = partial;
  return CaptureStop::Complete;
}

Result map_transaction(const Kernel &kernel,
                       const tseitin_cycle::ProofTransaction &transaction,
                       SubstitutionRedundancyInstance &instance) {
  Result result;
  if (transaction.version != 1 || transaction.x7.clause.size() != 1 ||
      !transaction.x7.witness_true.empty() ||
      transaction.x7.clause.front() != -transaction.identity.pivot ||
      !transaction.requires_unassigned_pivot ||
      !transaction.requires_live_x7_validation ||
      !transaction.requires_post_apply_replan) {
    result.reason = Reason::ExternalMapping;
    return result;
  }
  const auto map = [&](int32_t external, Lit &internal) {
    internal = kernel_internalize(kernel, external);
    return internal != INVALID_LIT && var_of(internal) < kernel.nvars &&
           !kernel_var_retired(kernel, var_of(internal)) &&
           kernel_externalize(kernel, internal) == external;
  };
  Lit clause = INVALID_LIT;
  if (!map(transaction.x7.clause.front(), clause)) {
    result.reason = Reason::ExternalMapping;
    return result;
  }
  instance = {};
  instance.clause.push_back(clause);
  for (const auto &[external_source, external_target] :
       transaction.x7.substitution) {
    Lit source = INVALID_LIT, target = INVALID_LIT;
    if (!map(external_source, source) || !map(external_target, target) ||
        var_of(source) != var_of(target) || target != negate(source)) {
      result.reason = Reason::ExternalMapping;
      instance = {};
      return result;
    }
    instance.substitution.push_back({source, target});
  }
  if (kernel.value(clause)) {
    result.status = Status::Stale;
    result.reason = Reason::AssignedPivot;
    instance = {};
    return result;
  }
  result.status = Status::Prepared;
  return result;
}

void detach_shadow(Kernel &shadow) {
  shadow.proof = nullptr;
  shadow.h = nullptr;
  shadow.population_observer = nullptr;
  shadow.analysis_conflict_hook = nullptr;
  shadow.analysis_conflict_hook_state = nullptr;
  shadow.bve_score_update_hook = nullptr;
  shadow.bve_score_update_state = nullptr;
  shadow.structural_provenance = nullptr;
}

Result semantic_preflight(Kernel &kernel,
                          const SubstitutionRedundancyInstance &instance,
                          uint64_t maximum_validation_ticks) {
  Kernel shadow = kernel;
  detach_shadow(shadow);
  SubstitutionRedundancyParameters parameters;
  parameters.maximum_validation_ticks = maximum_validation_ticks;
  const SubstitutionRedundancyApplicationResult applied =
      substitution_redundancy_apply_detailed(shadow, instance, parameters,
                                             shadow.stop);
  Result result;
  result.substitution_status = applied.status;
  result.validation_ticks = applied.validation_ticks;
  result.witness_literals = applied.witness_literals;
  if (applied.status == SubstitutionRedundancyApplicationStatus::Applied) {
    result.status = Status::Prepared;
    return result;
  }
  result.reason = Reason::SemanticPreflight;
  switch (applied.status) {
  case SubstitutionRedundancyApplicationStatus::Interrupted:
    result.status = Status::Interrupted;
    result.reason = Reason::Interrupted;
    break;
  case SubstitutionRedundancyApplicationStatus::ResourceExhausted:
    result.status = Status::ResourceExhausted;
    break;
  case SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat:
    result.status = Status::UnsupportedProof;
    result.reason = Reason::UnsupportedProof;
    break;
  case SubstitutionRedundancyApplicationStatus::Inapplicable:
  case SubstitutionRedundancyApplicationStatus::NoChange:
    result.status = Status::Declined;
    break;
  case SubstitutionRedundancyApplicationStatus::Invalid:
  case SubstitutionRedundancyApplicationStatus::Unsat:
  case SubstitutionRedundancyApplicationStatus::Applied:
    result.status = Status::Invalid;
    break;
  }
  return result;
}

} // namespace

Result capture(Kernel &kernel, const SnapshotLimits &limits, Snapshot &output) {
  Result result;
  if (!valid(limits)) {
    result.reason = Reason::InvalidRequest;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = Status::Inapplicable;
    result.reason = Reason::StateNotApplicable;
    return result;
  }
  Snapshot local, partial;
  const CaptureStop stop = capture_impl(kernel, limits, local, partial);
  result = stopped(stop, stop == CaptureStop::Complete ? local : partial);
  if (stop == CaptureStop::Complete) output = std::move(local);
  return result;
}

Result prepare(Kernel &kernel, const Snapshot &snapshot,
               const std::vector<tseitin_cycle::CertifiedGroup> &groups,
               const Request &request, Prepared &output) {
  output = {};
  Result result;
  if (!valid(request)) {
    result.reason = Reason::InvalidRequest;
    return result;
  }
  if (!supported_proof(kernel)) {
    result.status = Status::UnsupportedProof;
    result.reason = Reason::UnsupportedProof;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = Status::Inapplicable;
    result.reason = Reason::StateNotApplicable;
    return result;
  }
  Snapshot current;
  Result captured = capture(kernel, request.snapshot, current);
  if (captured.status != Status::Captured) return captured;
  result.snapshot_clauses = current.sources.size();
  result.snapshot_literals = current.literals;
  result.snapshot_work = current.work;
  if (!(current == snapshot)) {
    result.status = Status::Stale;
    result.reason = Reason::SnapshotMismatch;
    return result;
  }

  tseitin_cycle::AdmissionRequest admission_request;
  admission_request.groups = groups;
  admission_request.live_clauses = snapshot.clauses;
  const tseitin_cycle::AdmissionResult admission =
      tseitin_cycle::admit(admission_request, request.admission);
  result.admission_work = admission.work;
  result.admission_reason = admission.reason;
  if (!admission.admitted()) {
    result.status = admission.status == tseitin_cycle::AdmissionStatus::Interrupted
                        ? Status::Interrupted
                    : admission.status ==
                              tseitin_cycle::AdmissionStatus::ResourceExhausted
                        ? Status::ResourceExhausted
                    : admission.status == tseitin_cycle::AdmissionStatus::InvalidInput
                        ? Status::Invalid
                        : Status::Declined;
    result.reason = admission.status == tseitin_cycle::AdmissionStatus::Interrupted
                        ? Reason::Interrupted
                        : Reason::AdmissionDeclined;
    return result;
  }
  const tseitin_cycle::Result plan =
      tseitin_cycle::plan_one(admission.input, request.planning);
  result.planning_work = plan.work;
  result.planning_reason = plan.decline;
  if (!plan.planned()) {
    result.status = plan.decline == tseitin_cycle::DeclineReason::WorkLimit
                        ? Status::ResourceExhausted
                        : Status::Declined;
    result.reason = Reason::PlanningDeclined;
    return result;
  }
  const tseitin_cycle::TransactionResult transaction =
      tseitin_cycle::build_transaction(admission, plan, request.transaction);
  result.transaction_work = transaction.work;
  result.transaction_reason = transaction.reason;
  if (!transaction.ready()) {
    result.status = transaction.status == tseitin_cycle::TransactionStatus::Interrupted
                        ? Status::Interrupted
                    : transaction.status ==
                              tseitin_cycle::TransactionStatus::ResourceExhausted
                        ? Status::ResourceExhausted
                    : transaction.status == tseitin_cycle::TransactionStatus::InvalidInput
                        ? Status::Invalid
                        : Status::Declined;
    result.reason = transaction.status == tseitin_cycle::TransactionStatus::Interrupted
                        ? Reason::Interrupted
                        : Reason::TransactionDeclined;
    return result;
  }
  if (kernel.external_to_internal.size() >= uint32_t(INT32_MAX) ||
      kernel.next_clause_id == UINT64_MAX) {
    result.status = Status::ResourceExhausted;
    result.reason = Reason::ProofNamespace;
    return result;
  }
  SubstitutionRedundancyInstance instance;
  Result mapped = map_transaction(kernel, transaction.transaction, instance);
  if (mapped.status != Status::Prepared) {
    result.status = mapped.status;
    result.reason = mapped.reason;
    return result;
  }
  Result preflight = semantic_preflight(
      kernel, instance, request.maximum_validation_ticks);
  result.validation_ticks = preflight.validation_ticks;
  result.witness_literals = preflight.witness_literals;
  result.substitution_status = preflight.substitution_status;
  if (preflight.status != Status::Prepared) {
    result.status = preflight.status;
    result.reason = preflight.reason;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    result.reason = Reason::Interrupted;
    return result;
  }
  Snapshot revalidated;
  captured = capture(kernel, request.snapshot, revalidated);
  if (captured.status == Status::Interrupted) return captured;
  if (captured.status != Status::Captured || !(revalidated == snapshot)) {
    result.status = Status::Stale;
    result.reason = Reason::SnapshotMismatch;
    return result;
  }

  Prepared local;
  local.ready_ = true;
  local.snapshot_ = snapshot;
  local.request_ = request;
  local.transaction_ = transaction.transaction;
  local.instance_ = std::move(instance);
  local.proof_ = kernel.proof;
  output = std::move(local);
  result.status = Status::Prepared;
  result.reason = Reason::None;
  return result;
}

Result apply(Kernel &kernel, const Prepared &prepared) {
  Result result;
  if (!prepared.ready_) {
    result.reason = Reason::InvalidRequest;
    return result;
  }
  if (!supported_proof(kernel)) {
    result.status = Status::UnsupportedProof;
    result.reason = Reason::UnsupportedProof;
    return result;
  }
  if (kernel.proof != prepared.proof_) {
    result.status = Status::Stale;
    result.reason = Reason::SnapshotMismatch;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = Status::Inapplicable;
    result.reason = Reason::StateNotApplicable;
    return result;
  }
  Snapshot current;
  Result captured = capture(kernel, prepared.request_.snapshot, current);
  if (captured.status != Status::Captured) return captured;
  result.snapshot_clauses = current.sources.size();
  result.snapshot_literals = current.literals;
  result.snapshot_work = current.work;
  if (!(current == prepared.snapshot_)) {
    result.status = Status::Stale;
    result.reason = Reason::SnapshotMismatch;
    return result;
  }
  SubstitutionRedundancyInstance instance;
  Result mapped = map_transaction(kernel, prepared.transaction_, instance);
  if (mapped.status != Status::Prepared || !(instance == prepared.instance_)) {
    result.status = mapped.status == Status::Prepared ? Status::Stale
                                                      : mapped.status;
    result.reason = mapped.status == Status::Prepared ? Reason::SnapshotMismatch
                                                      : mapped.reason;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    result.reason = Reason::Interrupted;
    return result;
  }
  SubstitutionRedundancyParameters parameters;
  parameters.maximum_validation_ticks =
      prepared.request_.maximum_validation_ticks;
  const uint64_t before_id = kernel.next_clause_id;
  const SubstitutionRedundancyApplicationResult applied =
      substitution_redundancy_apply_detailed(kernel, instance, parameters,
                                             kernel.stop);
  result.substitution_status = applied.status;
  result.validation_ticks = applied.validation_ticks;
  result.witness_literals = applied.witness_literals;
  result.result = applied.rule_result;
  if (applied.status == SubstitutionRedundancyApplicationStatus::Applied &&
      kernel.next_clause_id == before_id + 1 &&
      kernel.value(instance.clause.front()) > 0 &&
      kernel.unit_ids[var_of(instance.clause.front())] == kernel.next_clause_id) {
    result.status = Status::Applied;
    return result;
  }
  if (applied.status == SubstitutionRedundancyApplicationStatus::Interrupted) {
    result.status = Status::Interrupted;
    result.reason = Reason::Interrupted;
    return result;
  }
  if (applied.status ==
      SubstitutionRedundancyApplicationStatus::ResourceExhausted) {
    result.status = Status::ResourceExhausted;
    result.reason = Reason::SemanticPreflight;
    return result;
  }
  result.status = Status::Invalid;
  result.reason = Reason::ApplicationMismatch;
  return result;
}

const char *reason_name(Reason reason) {
  switch (reason) {
  case Reason::None: return "none";
  case Reason::InvalidRequest: return "invalid-request";
  case Reason::InvalidKernelPopulation: return "invalid-kernel-population";
  case Reason::StateNotApplicable: return "state-not-applicable";
  case Reason::SnapshotClauseLimit: return "snapshot-clause-limit";
  case Reason::SnapshotLiteralLimit: return "snapshot-literal-limit";
  case Reason::SnapshotWorkLimit: return "snapshot-work-limit";
  case Reason::Interrupted: return "interrupted";
  case Reason::UnsupportedProof: return "unsupported-proof";
  case Reason::SnapshotMismatch: return "snapshot-mismatch";
  case Reason::AdmissionDeclined: return "admission-declined";
  case Reason::PlanningDeclined: return "planning-declined";
  case Reason::TransactionDeclined: return "transaction-declined";
  case Reason::ExternalMapping: return "external-mapping";
  case Reason::AssignedPivot: return "assigned-pivot";
  case Reason::ProofNamespace: return "proof-namespace";
  case Reason::SemanticPreflight: return "semantic-preflight";
  case Reason::ApplicationMismatch: return "application-mismatch";
  }
  return "unknown";
}

} // namespace tactix::tseitin_cycle_live

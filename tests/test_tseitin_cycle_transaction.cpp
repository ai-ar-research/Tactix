#undef NDEBUG
#include "rules/lucky/tseitin_cycle_transaction.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix::tseitin_cycle;
using tactix::parity_certificate::xor_equation;

static AdmissionRequest triangle() {
  AdmissionRequest request;
  const std::vector<VertexEquation> equations = {
      {1, true, {1, 3}}, {2, false, {1, 2}}, {3, false, {2, 3}}};
  uint64_t occurrence = 50, proof = 500, group_id = 900;
  for (const VertexEquation &source : equations) {
    CertifiedGroup group;
    group.identity = group_id++;
    group.certified.equation.size =
        static_cast<uint8_t>(source.edges.size());
    group.certified.equation.rhs = source.charge;
    for (size_t i = 0; i < source.edges.size(); ++i)
      group.certified.equation.variables[i] = source.edges[i];
    const auto clauses = xor_equation(source.edges.data(), source.edges.size(),
                                      source.charge);
    for (const auto &clause : clauses) {
      LiveClause live{occurrence++, proof++,
                      {clause.lits.begin(),
                       clause.lits.begin() + clause.size}};
      group.certified.premises.push_back(live.occurrence);
      group.premise_proofs.push_back(live.proof);
      request.live_clauses.push_back(std::move(live));
    }
    request.groups.push_back(std::move(group));
  }
  return request;
}

static TransactionLimits ample() {
  return {1000, 3, 2, 2, 100, nullptr, nullptr};
}

struct InterruptState { uint64_t calls = 0, stop = 0; };
static bool interrupt_after(void *opaque) {
  auto &state = *static_cast<InterruptState *>(opaque);
  return ++state.calls >= state.stop;
}

int main() {
  const AdmissionRequest request = triangle();
  const AdmissionResult admission = admit(request, {1000, nullptr, nullptr});
  assert(admission.admitted());
  const Result plan = plan_one(admission.input, {1000, 16, 1});
  assert(plan.planned());

  const TransactionResult built = build_transaction(admission, plan, ample());
  assert(built.ready());
  const ProofTransaction &transaction = built.transaction;
  assert(transaction.version == 1);
  assert(transaction.identity.pivot == 3);
  assert((transaction.identity.cycle_edges == std::vector<Edge>{3, 1, 2}));
  assert(transaction.identity.sources.size() == admission.provenance.size());
  for (size_t i = 0; i < admission.provenance.size(); ++i) {
    assert(transaction.identity.sources[i].group_identity ==
           admission.provenance[i].group_identity);
    assert(transaction.identity.sources[i].vertex ==
           admission.provenance[i].vertex);
    assert(transaction.identity.sources[i].premises.size() ==
           admission.provenance[i].premises.size());
  }
  assert((transaction.x7.clause == std::vector<int32_t>{-3}));
  assert(transaction.x7.witness_true.empty());
  assert((transaction.x7.substitution ==
          std::vector<std::pair<int32_t, int32_t>>{{1, -1}, {2, -2}}));
  assert(transaction.accounting.preflight_additions == 3);
  assert(transaction.accounting.preflight_deletions == 2);
  assert(transaction.accounting.temporary_helpers == 2);
  assert(transaction.accounting.proof_only_variables == 1);
  assert(transaction.accounting.committed_solver_clauses == 1);
  assert(transaction.accounting.witness_literals == 5);
  assert(transaction.events.size() == 5);
  assert(transaction.events.front() == ProofEventKind::AddNegativeHelper);
  assert(transaction.events.back() == ProofEventKind::DeleteNegativeHelper);
  assert(transaction.requires_unassigned_pivot);
  assert(transaction.requires_live_x7_validation);
  assert(transaction.requires_post_apply_replan);

  ReplayLimits replay{{1000, nullptr, nullptr}, {1000, 16, 1}, ample()};
  TransactionResult checked = replay_transaction(request, transaction, replay);
  assert(checked.ready());
  assert(checked.transaction == transaction);

  // Replay is independent of discovery and literal order while retaining IDs.
  AdmissionRequest reordered = request;
  std::reverse(reordered.groups.begin(), reordered.groups.end());
  std::reverse(reordered.live_clauses.begin(), reordered.live_clauses.end());
  for (LiveClause &clause : reordered.live_clauses)
    std::reverse(clause.literals.begin(), clause.literals.end());
  checked = replay_transaction(reordered, transaction, replay);
  assert(checked.ready());

  ProofTransaction corrupted = transaction;
  ++corrupted.identity.sources[0].premises[0].proof;
  checked = replay_transaction(request, corrupted, replay);
  assert(checked.status == TransactionStatus::InvalidInput);
  assert(checked.reason == TransactionReason::ReplayMismatch);
  assert(checked.transaction.version == 0);
  corrupted = transaction;
  corrupted.x7.substitution[0].second = corrupted.x7.substitution[0].first;
  checked = replay_transaction(request, corrupted, replay);
  assert(checked.reason == TransactionReason::ReplayMismatch);

  AdmissionRequest replay_bad_source = request;
  replay_bad_source.live_clauses[0].literals[0] *= -1;
  checked = replay_transaction(replay_bad_source, transaction, replay);
  assert(checked.status == TransactionStatus::Declined);
  assert(checked.reason == TransactionReason::ReplayAdmissionFailed);
  corrupted = transaction;
  std::swap(corrupted.events[0], corrupted.events[1]);
  checked = replay_transaction(request, corrupted, replay);
  assert(checked.reason == TransactionReason::ReplayMismatch);

  TransactionLimits limited = ample();
  limited.additions = 2;
  TransactionResult declined = build_transaction(admission, plan, limited);
  assert(declined.reason == TransactionReason::AdditionLimit);
  assert(declined.transaction.version == 0);
  limited = ample(); limited.deletions = 1;
  assert(build_transaction(admission, plan, limited).reason ==
         TransactionReason::DeletionLimit);
  limited = ample(); limited.helpers = 1;
  assert(build_transaction(admission, plan, limited).reason ==
         TransactionReason::HelperLimit);
  limited = ample(); limited.witness_literals = 4;
  assert(build_transaction(admission, plan, limited).reason ==
         TransactionReason::WitnessLimit);
  assert(built.work > 0);
  limited = ample(); limited.work = built.work - 1;
  declined = build_transaction(admission, plan, limited);
  assert(declined.status == TransactionStatus::ResourceExhausted);
  assert(declined.reason == TransactionReason::WorkLimit);
  assert(declined.transaction.version == 0);

  InterruptState interrupted{0, 4};
  limited = ample();
  limited.interrupted = interrupt_after;
  limited.interrupt_state = &interrupted;
  declined = build_transaction(admission, plan, limited);
  assert(declined.status == TransactionStatus::Interrupted);
  assert(declined.reason == TransactionReason::Interrupted);
  assert(declined.transaction.version == 0);

  AdmissionResult damaged_admission = admission;
  damaged_admission.provenance[0].premises[0].occurrence = 0;
  declined = build_transaction(damaged_admission, plan, ample());
  assert(declined.status == TransactionStatus::InvalidInput);
  assert(declined.reason == TransactionReason::InvalidSourceLedger);
  Result damaged_plan = plan;
  damaged_plan.candidate.substitution.back().kind = ImageKind::Constant;
  declined = build_transaction(admission, damaged_plan, ample());
  assert(declined.reason == TransactionReason::InvalidCandidate);

  std::printf("test_tseitin_cycle_transaction: all passed; work %llu\n",
              static_cast<unsigned long long>(built.work));
}

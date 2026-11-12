#undef NDEBUG
#include "tseitin_test_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "rules/lucky/tseitin_cycle_live.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

using namespace tactix;
using namespace tseitin_test;
namespace live = tactix::tseitin_cycle_live;
namespace cycle = tactix::tseitin_cycle;
using tactix::parity_certificate::xor_equation;

namespace {

Kernel triangle(bool residual = false) {
  Kernel kernel = formula(tseitin_test::triangle, residual ? 4 : 3);
  if (residual)
    kernel_add_binary_clause(kernel, internalize(1), internalize(4),
                             ++kernel.next_clause_id);
  return kernel;
}

live::Request request() {
  live::Request result;
  result.snapshot = {100, 1000, 10000, 100};
  result.admission = {10000, nullptr, nullptr};
  result.planning = {10000, 16, 1};
  result.transaction = {10000, 3, 2, 2, 100, nullptr, nullptr};
  result.maximum_validation_ticks = 100000;
  return result;
}

std::vector<int32_t> clause_key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

std::vector<cycle::CertifiedGroup> groups(
    const live::Snapshot &snapshot,
    const std::vector<Equation> &equations = tseitin_test::triangle) {
  std::vector<cycle::CertifiedGroup> result;
  uint64_t identity = 100;
  for (const Equation &equation : equations) {
    cycle::CertifiedGroup group;
    group.identity = identity++;
    group.certified.equation.size = equation.variables.size();
    group.certified.equation.rhs = equation.rhs;
    for (size_t index = 0; index < equation.variables.size(); ++index)
      group.certified.equation.variables[index] = equation.variables[index];
    const auto expected = xor_equation(equation.variables.data(),
                                       equation.variables.size(), equation.rhs);
    for (const auto &clause : expected) {
      const std::vector<int32_t> wanted = clause_key(
          {clause.lits.begin(), clause.lits.begin() + clause.size});
      const auto found = std::find_if(
          snapshot.sources.begin(), snapshot.sources.end(),
          [&](const live::Source &source) {
            return clause_key(source.clause) == wanted;
          });
      assert(found != snapshot.sources.end());
      group.certified.premises.push_back(found->occurrence);
      group.premise_proofs.push_back(found->native_proof_id);
    }
    result.push_back(std::move(group));
  }
  return result;
}

live::Snapshot snapshot(Kernel &);

void compacted_external_mapping() {
  const std::vector<Equation> equations = {
      {true, {2, 4}}, {false, {2, 3}}, {false, {3, 4}}};
  Kernel kernel = formula(equations, 4);
  kernel.var_status.assign(kernel.nvars, VarStatus::Active);
  kernel.var_status[0] = VarStatus::Eliminated;
  --kernel.unassigned_active;
  assert(kernel_compact_variables(kernel));
  assert((kernel.internal_to_external == std::vector<Var>{1, 2, 3}));

  const live::Snapshot captured = snapshot(kernel);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured, equations), request(),
                       prepared)
             .status == live::Status::Prepared);
  assert(prepared.transaction().identity.pivot == 4);
  assert(kernel_externalize(kernel, prepared.instance().clause.front()) == -4);
  assert(var_of(prepared.instance().clause.front()) == 2);
  for (const auto &[source, target] : prepared.instance().substitution) {
    assert(target == negate(source));
    assert(kernel_externalize(kernel, source) == 2 ||
           kernel_externalize(kernel, source) == 3);
  }
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(kernel_internalize(kernel, -4)) > 0);
  kernel_check_invariants(kernel);
}

void large_clause_population() {
  Kernel kernel = formula(width_three, 6);
  const live::Snapshot captured = snapshot(kernel);
  assert(captured.sources.size() == 16 && captured.literals == 48);
  for (const live::Source &source : captured.sources) {
    assert(source.kind == live::SourceKind::Large);
    assert(source.ref != INVALID_REF && source.clause.size() == 3);
  }
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured, width_three),
                       request(), prepared)
             .status == live::Status::Prepared);
  assert(prepared.transaction().identity.pivot == 4);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{4, 1, 2}));
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(internalize(-4)) > 0);
  kernel_check_invariants(kernel);
}

live::Snapshot snapshot(Kernel &kernel) {
  live::Snapshot result;
  const live::Result captured = live::capture(kernel, request().snapshot, result);
  assert(captured.status == live::Status::Captured);
  return result;
}

void exact_snapshot_prepare_apply() {
  Kernel kernel = triangle();
  struct ProjectionProbe final : QueryProjectionService {
    unsigned restrictions = 0;
    std::span<const int32_t> constraints() const override { return {}; }
    bool constrains(Var) const override { return false; }
    bool preserves_constant(Lit) const override { return true; }
    bool preserves_restriction(const Kernel &, std::span<const int8_t>,
                               std::span<const Lit>) const override { return true; }
    QueryModelAdmission admit_model(Kernel &) override { return QueryModelAdmission::Accepted; }
    void reconstruction_literal(Lit) override { assert(false); }
    void substitute(Kernel &, std::span<const std::pair<Lit, Lit>>) override { assert(false); }
    void restrict(Kernel &, std::span<const int8_t>, std::span<const Lit>) override {
      ++restrictions;
    }
    void restrict_constant(Lit) override { ++restrictions; }
  } projection;
  kernel.query_projection = &projection;
  const live::Snapshot captured = snapshot(kernel);
  assert(captured.sources.size() == 6 && captured.clauses.size() == 6);
  assert(captured.literals == 12);
  for (size_t index = 0; index < captured.sources.size(); ++index) {
    assert(captured.sources[index].occurrence == index + 1);
    assert(captured.sources[index].native_proof_id == index + 1);
    assert(captured.sources[index].kind == live::SourceKind::Binary);
    assert(captured.clauses[index].occurrence == index + 1);
    assert(captured.clauses[index].proof == index + 1);
  }

  live::Prepared prepared;
  const live::Result admitted =
      live::prepare(kernel, captured, groups(captured), request(), prepared);
  assert(admitted.status == live::Status::Prepared && prepared.ready());
  assert(projection.restrictions == 0); // speculative shadow cannot mutate the owner
  assert(admitted.admission_reason == cycle::AdmissionReason::None);
  assert(admitted.planning_reason == cycle::DeclineReason::None);
  assert(admitted.transaction_reason == cycle::TransactionReason::None);
  assert(admitted.substitution_status ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(prepared.transaction().identity.pivot == 3);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{3, 1, 2}));
  assert((prepared.instance().clause == std::vector<Lit>{internalize(-3)}));
  assert((prepared.instance().substitution ==
          std::vector<std::pair<Lit, Lit>>{{internalize(1), internalize(-1)},
                                          {internalize(2), internalize(-2)}}));
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result applied = live::apply(kernel, prepared);
  assert(applied.status == live::Status::Applied);
  assert(projection.restrictions == 1); // the committed rule alone notifies it
  assert(applied.result == ApplyResult::Ok);
  assert(applied.substitution_status ==
         SubstitutionRedundancyApplicationStatus::Applied);
  assert(kernel.next_clause_id == before_id + 1);
  assert(kernel.value(internalize(-3)) > 0);
  assert(kernel.unit_ids[var_of(internalize(-3))] == kernel.next_clause_id);
  kernel_check_invariants(kernel);

  // The immutable transaction cannot be replayed after its own unit changes
  // the exact formula/root snapshot.
  assert(live::apply(kernel, prepared).status == live::Status::Inapplicable);
}

void resource_and_interrupt_boundaries() {
  {
    Kernel kernel = triangle();
    const live::Snapshot baseline = snapshot(kernel);
    auto exact = request().snapshot;
    exact.maximum_clauses = baseline.sources.size();
    exact.maximum_literals = baseline.literals;
    exact.maximum_work = baseline.work;
    exact.maximum_variables = kernel.nvars;
    live::Snapshot output;
    const live::Result admitted = live::capture(kernel, exact, output);
    assert(admitted.status == live::Status::Captured);
    assert(output == baseline);
    --exact.maximum_work;
    const live::Result declined = live::capture(kernel, exact, output);
    assert(declined.status == live::Status::ResourceExhausted);
    assert(declined.reason == live::Reason::SnapshotWorkLimit);
    assert(output.sources.empty());
  }
  {
    Kernel kernel = triangle();
    const auto clause_limit = [](live::SnapshotLimits &l) { l.maximum_clauses = 5; };
    const auto variable_limit = [](live::SnapshotLimits &l) { l.maximum_variables = 2; };
    const auto literal_limit = [](live::SnapshotLimits &l) { l.maximum_literals = 11; };
    const std::pair<void (*)(live::SnapshotLimits &), live::Reason> cases[] = {
        {clause_limit, live::Reason::SnapshotClauseLimit},
        {variable_limit, live::Reason::SnapshotVariableLimit},
        {literal_limit, live::Reason::SnapshotLiteralLimit}};
    for (const auto &[limit, reason] : cases) {
      live::Snapshot output = snapshot(kernel);
      auto limits = request().snapshot;
      limit(limits);
      const live::Result result = live::capture(kernel, limits, output);
      assert(result.status == live::Status::ResourceExhausted);
      assert(result.reason == reason && output.sources.empty());
    }
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto limited = request();
    limited.transaction.additions = 2;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.transaction_reason == cycle::TransactionReason::AdditionLimit);
    assert(!prepared.ready());
    // Reusing an output object cannot retain an earlier authorized tuple.
    auto ample = request();
    assert(live::prepare(kernel, captured, groups(captured), ample, prepared)
               .status == live::Status::Prepared);
    assert(prepared.ready());
    assert(live::prepare(kernel, captured, groups(captured), limited, prepared)
               .status == live::Status::ResourceExhausted);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto limited = request();
    limited.maximum_validation_ticks = 0;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SemanticPreflight);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    kernel.next_clause_id = UINT64_MAX;
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::ProofNamespace);
    assert(kernel.next_clause_id == UINT64_MAX);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    live::Snapshot output;
    assert(live::capture(kernel, request().snapshot, output).status ==
           live::Status::Interrupted);
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    const uint64_t before_id = kernel.next_clause_id;
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == live::Status::Interrupted);
    assert(kernel.next_clause_id == before_id);
    assert(!kernel.value(internalize(-3)));
  }
  for (const bool admission : {true, false}) {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto interrupted = request();
    InterruptState state{0, 1};
    auto &phase = admission ? interrupted.admission.interrupted
                            : interrupted.transaction.interrupted;
    phase = interrupt_after;
    (admission ? interrupted.admission.interrupt_state
               : interrupted.transaction.interrupt_state) = &state;
    live::Prepared prepared;
    const live::Result result = live::prepare(
        kernel, captured, groups(captured), interrupted, prepared);
    assert(result.status == live::Status::Interrupted && !prepared.ready());
    if (admission)
      assert(result.admission_reason == cycle::AdmissionReason::Interrupted);
    else
      assert(result.transaction_reason == cycle::TransactionReason::Interrupted);
  }
  {
    // Prepared drops the caller's polling pointers, so apply may outlive them.
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    {
      auto bounded = request();
      InterruptState state;
      bounded.admission.interrupted = interrupt_after;
      bounded.admission.interrupt_state = &state;
      bounded.transaction.interrupted = interrupt_after;
      bounded.transaction.interrupt_state = &state;
      assert(live::prepare(kernel, captured, groups(captured), bounded,
                           prepared)
                 .status == live::Status::Prepared);
      assert(state.calls > 0);
    }
    assert(live::apply(kernel, prepared).status == live::Status::Applied);
  }
}

void stale_and_invalid_declines_are_atomic() {
  {
    Kernel kernel = triangle();
    live::Snapshot captured = snapshot(kernel);
    const uint64_t before_id = kernel.next_clause_id;
    ++captured.clauses.front().proof;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::Stale);
    assert(result.reason == live::Reason::SnapshotMismatch);
    assert(kernel.next_clause_id == before_id && !prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    auto selected = groups(captured);
    ++selected.front().premise_proofs.front();
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, selected, request(), prepared);
    assert(result.status == live::Status::Invalid);
    assert(result.admission_reason ==
           cycle::AdmissionReason::PremiseIdentityMismatch);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle(true);
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::Declined);
    assert(result.admission_reason == cycle::AdmissionReason::ResidualEdgeTouch);
    assert(!prepared.ready());
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    const uint64_t before_id = kernel.next_clause_id;
    ++kernel.next_clause_id;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == live::Status::Stale);
    assert(result.reason == live::Reason::SnapshotMismatch);
    assert(kernel.next_clause_id == before_id + 1);
    assert(!kernel.value(internalize(-3)));
  }
  {
    Kernel kernel = triangle();
    kernel_add_binary_clause(kernel, internalize(1), internalize(-2),
                             ++kernel.next_clause_id);
    // A second identical occurrence has no independently addressable live
    // binary proof identity in the kernel map, so exact capture must refuse it.
    kernel_add_binary_clause(kernel, internalize(1), internalize(-2),
                             ++kernel.next_clause_id);
    live::Snapshot output;
    const live::Result result = live::capture(kernel, request().snapshot, output);
    assert(result.status == live::Status::Invalid);
    assert(result.reason == live::Reason::InvalidKernelPopulation);
  }
  {
    Kernel kernel = triangle();
    kernel.external_to_internal[0] = INVALID_VAR;
    live::Snapshot output;
    const live::Result result = live::capture(kernel, request().snapshot, output);
    assert(result.status == live::Status::Invalid);
    assert(result.reason == live::Reason::InvalidKernelPopulation);
  }
  {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    const live::Source source = captured.sources.front();
    bool removed = false;
    kernel.watches.compact(source.first, [&](WatchEntry watch) {
      if (!removed && watch.binary() && watch.other() == source.second) {
        removed = true;
        return false;
      }
      return true;
    });
    assert(removed);
    live::Snapshot output;
    const live::Result result = live::capture(kernel, request().snapshot, output);
    assert(result.status == live::Status::Invalid);
    assert(result.reason == live::Reason::InvalidKernelPopulation);
  }
}

void corrupted_prepared_transactions_decline() {
  const auto run = [](live::Status expected_status,
                      live::Reason expected_reason,
                      cycle::TransactionReason expected_transaction_reason,
                      auto corrupt) {
    Kernel kernel = triangle();
    const live::Snapshot captured = snapshot(kernel);
    live::Prepared prepared;
    assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
               .status == live::Status::Prepared);
    corrupt(prepared);
    const uint64_t before_id = kernel.next_clause_id;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == expected_status);
    assert(result.reason == expected_reason);
    assert(result.transaction_reason == expected_transaction_reason);
    assert(kernel.next_clause_id == before_id);
    assert(!kernel.value(internalize(-3)));
  };
  run(live::Status::Invalid, live::Reason::TransactionDeclined,
      cycle::TransactionReason::ReplayMismatch, [](live::Prepared &prepared) {
    auto &transaction = const_cast<cycle::ProofTransaction &>(
        prepared.transaction());
    ++transaction.identity.sources.front().premises.front().proof;
  });
  run(live::Status::Invalid, live::Reason::TransactionDeclined,
      cycle::TransactionReason::ReplayMismatch, [](live::Prepared &prepared) {
    auto &transaction = const_cast<cycle::ProofTransaction &>(
        prepared.transaction());
    transaction.x7.substitution.front().second =
        transaction.x7.substitution.front().first;
  });
  run(live::Status::Invalid, live::Reason::TransactionDeclined,
      cycle::TransactionReason::ReplayMismatch, [](live::Prepared &prepared) {
    auto &transaction = const_cast<cycle::ProofTransaction &>(
        prepared.transaction());
    ++transaction.accounting.preflight_additions;
  });
  run(live::Status::Stale, live::Reason::SnapshotMismatch,
      cycle::TransactionReason::None, [](live::Prepared &prepared) {
    auto &instance = const_cast<SubstitutionRedundancyInstance &>(
        prepared.instance());
    instance.substitution.front().second =
        instance.substitution.front().first;
  });
}

void proof_writer_replacement_is_stale() {
  TemporaryFile first_file, second_file;
  Kernel kernel = triangle();
  Proof first, second;
  assert(first.open(first_file.path.c_str(), ProofFormat::DsrAscii));
  assert(second.open(second_file.path.c_str(), ProofFormat::DsrAscii));
  attach(second, kernel);
  attach(first, kernel);
  const live::Snapshot captured = snapshot(kernel);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
             .status == live::Status::Prepared);
  kernel.proof = &second;
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result result = live::apply(kernel, prepared);
  assert(result.status == live::Status::Stale);
  assert(result.reason == live::Reason::SnapshotMismatch);
  assert(kernel.next_clause_id == before_id);
  assert(!kernel.value(internalize(-3)));
  kernel.proof = nullptr;
  first.flush();
  second.flush();
}

void proof_route_transition_is_precommit() {
  TemporaryFile file;
  Kernel kernel = triangle();
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  attach(proof, kernel);
  const live::Snapshot captured = snapshot(kernel);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
             .status == live::Status::Prepared);
  proof.finish_input_transaction();
  assert(proof.startup_pb_available());
  assert(proof.begin_pb());
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result result = live::apply(kernel, prepared);
  assert(result.status == live::Status::UnsupportedProof);
  assert(result.reason == live::Reason::UnsupportedProof);
  assert(kernel.next_clause_id == before_id);
  assert(!kernel.value(internalize(-3)));
  assert(proof.finish_pb());
  kernel.proof = nullptr;
}

void unsupported_proof_is_precommit() {
  TemporaryFile file;
  Kernel kernel = triangle();
  const live::Snapshot captured = snapshot(kernel);
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  proof.bind_variable_map(&kernel.internal_to_external);
  kernel.proof = &proof;
  const uint64_t before_id = kernel.next_clause_id;
  live::Prepared prepared;
  const live::Result result =
      live::prepare(kernel, captured, groups(captured), request(), prepared);
  assert(result.status == live::Status::UnsupportedProof);
  assert(result.reason == live::Reason::UnsupportedProof);
  assert(kernel.next_clause_id == before_id && !prepared.ready());
  proof.flush();
  kernel.proof = nullptr;
  assert(read_file(file.path).empty());
}

} // namespace

int main() {
  exact_snapshot_prepare_apply();
  compacted_external_mapping();
  large_clause_population();
  resource_and_interrupt_boundaries();
  stale_and_invalid_declines_are_atomic();
  corrupted_prepared_transactions_decline();
  proof_writer_replacement_is_stale();
  proof_route_transition_is_precommit();
  unsupported_proof_is_precommit();
  std::puts("test_tseitin_cycle_live: passed");
}

#include "unit_test.hpp"
// Live Tseitin-cycle capture, component discovery, preparation and apply.
#include "tseitin_test_support.hpp"
#include "temporary_file.hpp"
#include "rules/lucky/tseitin_cycle_discovery.hpp"

using namespace tseitin_test;
namespace discovery = tactix::tseitin_cycle_discovery;
namespace live = tactix::tseitin_cycle_live;
namespace cycle = tactix::tseitin_cycle;
using tactix::parity_certificate::xor_equation;

namespace {

Kernel triangle() { return formula(tseitin_test::triangle, 3); }

live::Request request() {
  live::Request result;
  result.snapshot = {100, 1000, 10000, 100};
  result.admission = {10000, nullptr, nullptr};
  result.planning = {10000, 16, 1};
  result.transaction = {10000, 3, 2, 2, 100, nullptr, nullptr};
  result.maximum_validation_ticks = 100000;
  return result;
}

discovery::Limits discovery_limits() {
  return {100000, 1000, 100, 4, nullptr, nullptr};
}

live::Snapshot snapshot(Kernel &kernel) {
  live::Snapshot result;
  assert(live::capture(kernel, request().snapshot, result).status ==
         live::Status::Captured);
  return result;
}

std::vector<int32_t> clause_key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

// Certified groups citing the snapshot's identities for each equation.
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
    std::copy(equation.variables.begin(), equation.variables.end(),
              group.certified.equation.variables.begin());
    for (const auto &clause : xor_equation(equation.variables.data(),
                                           equation.variables.size(),
                                           equation.rhs)) {
      const std::vector<int32_t> wanted =
          clause_key({clause.lits.begin(), clause.lits.begin() + clause.size});
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

live::Prepared prepared_triangle(Kernel &kernel) {
  const live::Snapshot captured = snapshot(kernel);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured), request(), prepared)
             .status == live::Status::Prepared);
  return prepared;
}

// Asserts that a failed apply left the triangle's pivot unit unpublished.
void unchanged(const Kernel &kernel, uint64_t before_id) {
  assert(kernel.next_clause_id == before_id && !kernel.value(internalize(-3)));
}

TEST(exact_snapshot_prepare_apply) {
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
  const live::Prepared prepared = prepared_triangle(kernel);
  assert(prepared.ready());
  assert(projection.restrictions == 0); // speculative shadow cannot mutate the owner
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{3, 1, 2}));
  assert((prepared.instance().clause == std::vector<Lit>{internalize(-3)}));
  assert((prepared.instance().substitution ==
          std::vector<std::pair<Lit, Lit>>{{internalize(1), internalize(-1)},
                                          {internalize(2), internalize(-2)}}));
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result applied = live::apply(kernel, prepared);
  assert(applied.status == live::Status::Applied &&
         applied.result == ApplyResult::Ok);
  assert(projection.restrictions == 1); // the committed rule alone notifies it
  assert(kernel.next_clause_id == before_id + 1);
  assert(kernel.value(internalize(-3)) > 0);
  assert(kernel.unit_ids[var_of(internalize(-3))] == kernel.next_clause_id);
  kernel_check_invariants(kernel);

  // The immutable transaction cannot be replayed after its own unit changes
  // the exact formula/root snapshot.
  assert(live::apply(kernel, prepared).status == live::Status::Inapplicable);
}

TEST(compacted_external_mapping) {
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
  for (const auto &[source, target] : prepared.instance().substitution) {
    assert(target == negate(source));
    assert(kernel_externalize(kernel, source) == 2 ||
           kernel_externalize(kernel, source) == 3);
  }
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(kernel_internalize(kernel, -4)) > 0);
  kernel_check_invariants(kernel);
}

TEST(large_clause_population) {
  Kernel kernel = formula(width_three, 6);
  const live::Snapshot captured = snapshot(kernel);
  assert(captured.sources.size() == 16 && captured.literals == 48);
  for (const live::Source &source : captured.sources)
    assert(source.kind == live::SourceKind::Large && source.ref != INVALID_REF);
  live::Prepared prepared;
  assert(live::prepare(kernel, captured, groups(captured, width_three),
                       request(), prepared)
             .status == live::Status::Prepared);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{4, 1, 2}));
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  assert(kernel.value(internalize(-4)) > 0);
  kernel_check_invariants(kernel);
}

TEST(resource_and_interrupt_boundaries) {
  Kernel kernel = triangle();
  const live::Snapshot baseline = snapshot(kernel);
  {
    auto exact = request().snapshot;
    exact.maximum_clauses = baseline.sources.size();
    exact.maximum_literals = baseline.literals;
    exact.maximum_work = baseline.work;
    exact.maximum_variables = kernel.nvars;
    live::Snapshot output;
    assert(live::capture(kernel, exact, output).status == live::Status::Captured);
    assert(output == baseline);
    --exact.maximum_work;
    const live::Result declined = live::capture(kernel, exact, output);
    assert(declined.status == live::Status::ResourceExhausted);
    assert(declined.reason == live::Reason::SnapshotWorkLimit);
    assert(output.sources.empty());
  }
  {
    const auto clause_limit = [](live::SnapshotLimits &l) { l.maximum_clauses = 5; };
    const auto variable_limit = [](live::SnapshotLimits &l) { l.maximum_variables = 2; };
    const auto literal_limit = [](live::SnapshotLimits &l) { l.maximum_literals = 11; };
    const std::pair<void (*)(live::SnapshotLimits &), live::Reason> cases[] = {
        {clause_limit, live::Reason::SnapshotClauseLimit},
        {variable_limit, live::Reason::SnapshotVariableLimit},
        {literal_limit, live::Reason::SnapshotLiteralLimit}};
    for (const auto &[limit, reason] : cases) {
      live::Snapshot output = baseline;
      auto limits = request().snapshot;
      limit(limits);
      const live::Result result = live::capture(kernel, limits, output);
      assert(result.status == live::Status::ResourceExhausted);
      assert(result.reason == reason && output.sources.empty());
    }
  }
  {
    auto limited = request();
    limited.transaction.additions = 2;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, baseline, groups(baseline), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.transaction_reason == cycle::TransactionReason::AdditionLimit);
    assert(!prepared.ready());
    // Reusing an output object cannot retain an earlier authorized tuple.
    assert(live::prepare(kernel, baseline, groups(baseline), request(), prepared)
               .status == live::Status::Prepared);
    assert(live::prepare(kernel, baseline, groups(baseline), limited, prepared)
               .status == live::Status::ResourceExhausted);
    assert(!prepared.ready());
  }
  {
    auto limited = request();
    limited.maximum_validation_ticks = 0;
    live::Prepared prepared;
    const live::Result result =
        live::prepare(kernel, baseline, groups(baseline), limited, prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::SemanticPreflight && !prepared.ready());
  }
  {
    Kernel exhausted = triangle();
    exhausted.next_clause_id = UINT64_MAX;
    const live::Snapshot captured = snapshot(exhausted);
    live::Prepared prepared;
    const live::Result result =
        live::prepare(exhausted, captured, groups(captured), request(), prepared);
    assert(result.status == live::Status::ResourceExhausted);
    assert(result.reason == live::Reason::ProofNamespace);
    assert(exhausted.next_clause_id == UINT64_MAX && !prepared.ready());
  }
  {
    Kernel stopped = triangle();
    volatile sig_atomic_t stop = 1;
    stopped.stop = &stop;
    live::Snapshot output;
    assert(live::capture(stopped, request().snapshot, output).status ==
           live::Status::Interrupted);
  }
  {
    Kernel stopped = triangle();
    const live::Prepared prepared = prepared_triangle(stopped);
    const uint64_t before_id = stopped.next_clause_id;
    volatile sig_atomic_t stop = 1;
    stopped.stop = &stop;
    assert(live::apply(stopped, prepared).status == live::Status::Interrupted);
    unchanged(stopped, before_id);
  }
  {
    // Prepared drops the caller's polling pointers, so apply may outlive them.
    live::Prepared prepared;
    {
      auto bounded = request();
      InterruptState state;
      bounded.admission.interrupted = interrupt_after;
      bounded.admission.interrupt_state = &state;
      bounded.transaction.interrupted = interrupt_after;
      bounded.transaction.interrupt_state = &state;
      assert(live::prepare(kernel, baseline, groups(baseline), bounded, prepared)
                 .status == live::Status::Prepared);
      assert(state.calls > 0);
    }
    assert(live::apply(kernel, prepared).status == live::Status::Applied);
  }
}

TEST(stale_and_invalid_declines_are_atomic) {
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
    const live::Prepared prepared = prepared_triangle(kernel);
    const uint64_t before_id = ++kernel.next_clause_id;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == live::Status::Stale);
    assert(result.reason == live::Reason::SnapshotMismatch);
    unchanged(kernel, before_id);
  }
  {
    Kernel kernel = triangle();
    // A second identical occurrence has no independently addressable live
    // binary proof identity in the kernel map, so exact capture must refuse it.
    for (int copy = 0; copy < 2; ++copy)
      kernel_add_binary_clause(kernel, internalize(1), internalize(-2),
                               ++kernel.next_clause_id);
    live::Snapshot output;
    const live::Result result = live::capture(kernel, request().snapshot, output);
    assert(result.status == live::Status::Invalid);
    assert(result.reason == live::Reason::InvalidKernelPopulation);
  }
}

TEST(corrupted_prepared_transactions_decline) {
  const auto run = [](live::Status expected_status, live::Reason expected_reason,
                      cycle::TransactionReason expected_transaction_reason,
                      auto corrupt) {
    Kernel kernel = triangle();
    live::Prepared prepared = prepared_triangle(kernel);
    corrupt(prepared);
    const uint64_t before_id = kernel.next_clause_id;
    const live::Result result = live::apply(kernel, prepared);
    assert(result.status == expected_status);
    assert(result.reason == expected_reason);
    assert(result.transaction_reason == expected_transaction_reason);
    unchanged(kernel, before_id);
  };
  run(live::Status::Invalid, live::Reason::TransactionDeclined,
      cycle::TransactionReason::ReplayMismatch, [](live::Prepared &prepared) {
    auto &transaction = const_cast<cycle::ProofTransaction &>(
        prepared.transaction());
    ++transaction.identity.sources.front().premises.front().proof;
  });
  run(live::Status::Stale, live::Reason::SnapshotMismatch,
      cycle::TransactionReason::None, [](live::Prepared &prepared) {
    auto &instance = const_cast<SubstitutionRedundancyInstance &>(
        prepared.instance());
    instance.substitution.front().second = instance.substitution.front().first;
  });
}

TEST(proof_writer_replacement_is_stale) {
  TemporaryFile first_file, second_file;
  Kernel kernel = triangle();
  Proof first, second;
  assert(first.open(first_file.path.c_str(), ProofFormat::DsrAscii));
  assert(second.open(second_file.path.c_str(), ProofFormat::DsrAscii));
  attach(second, kernel);
  attach(first, kernel);
  const live::Prepared prepared = prepared_triangle(kernel);
  kernel.proof = &second;
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result result = live::apply(kernel, prepared);
  assert(result.status == live::Status::Stale);
  assert(result.reason == live::Reason::SnapshotMismatch);
  unchanged(kernel, before_id);
  kernel.proof = nullptr;
  first.flush();
  second.flush();
}

TEST(proof_route_transition_is_precommit) {
  TemporaryFile file;
  Kernel kernel = triangle();
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  attach(proof, kernel);
  const live::Prepared prepared = prepared_triangle(kernel);
  proof.finish_input_transaction();
  assert(proof.startup_pb_available() && proof.begin_pb());
  const uint64_t before_id = kernel.next_clause_id;
  const live::Result result = live::apply(kernel, prepared);
  assert(result.status == live::Status::UnsupportedProof);
  assert(result.reason == live::Reason::UnsupportedProof);
  unchanged(kernel, before_id);
  assert(proof.finish_pb());
  kernel.proof = nullptr;
}

// The single legal component of a snapshot; Declined/Disconnected otherwise.
discovery::Result discover(const live::Snapshot &snapshot,
                           const discovery::Limits &limits,
                           discovery::Plan &output) {
  output = {};
  std::vector<discovery::Plan> components;
  discovery::Result result = discovery::enumerate(snapshot, limits, components);
  if (!result.discovered()) return result;
  if (components.size() != 1) {
    result.status = discovery::Status::Declined;
    result.reason = discovery::Reason::Disconnected;
    return result;
  }
  output = std::move(components.front());
  return result;
}

TEST(discovery_is_read_only_and_prepares) {
  Kernel kernel = formula(tseitin_test::triangle, 8);
  const live::Snapshot before = snapshot(kernel);
  const uint64_t next_clause_id = kernel.next_clause_id;
  const uint64_t touch_epoch = kernel.touch_epoch;
  discovery::Plan plan;
  const discovery::Result result = discover(before, discovery_limits(), plan);
  assert(result.discovered() && plan.ready());
  assert(result.clauses == 6 && result.complete_groups == 3 &&
         result.selected_groups == 3 && result.work > 0);
  assert(kernel.next_clause_id == next_clause_id &&
         kernel.touch_epoch == touch_epoch && snapshot(kernel) == before);
  live::Prepared prepared;
  assert(discovery::prepare(kernel, plan, request(), prepared).status ==
         live::Status::Prepared);
  assert((prepared.transaction().identity.cycle_edges ==
          std::vector<cycle::Edge>{3, 1, 2}));

  // Discovery is independent of snapshot storage order.
  live::Snapshot permuted = before;
  std::reverse(permuted.sources.begin(), permuted.sources.end());
  std::reverse(permuted.clauses.begin(), permuted.clauses.end());
  discovery::Plan second;
  assert(discover(permuted, discovery_limits(), second).discovered());
  assert(second.groups().size() == plan.groups().size());
  for (size_t i = 0; i < plan.groups().size(); ++i) {
    const auto &a = plan.groups()[i], &b = second.groups()[i];
    assert(a.identity == b.identity && a.premise_proofs == b.premise_proofs &&
           a.certified.premises == b.certified.premises &&
           a.certified.equation.variables == b.certified.equation.variables);
  }
}

TEST(discovery_declines) {
  const auto declined = [](Kernel kernel, discovery::Status status,
                           discovery::Reason reason) {
    discovery::Plan plan;
    const auto result = discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == status && result.reason == reason && !plan.ready());
  };
  const auto formula = [](const std::vector<Equation> &equations) {
    return tseitin_test::formula(equations, 8);
  };
  const auto pairs = [](std::initializer_list<int> firsts) {
    Kernel kernel;
    kernel_reserve_vars(kernel, 2);
    for (int first : firsts)
      for (int second : {-2, 2})
        kernel_add_binary_clause(kernel, internalize(first), internalize(second),
                                 ++kernel.next_clause_id);
    return kernel;
  };
  using S = discovery::Status;
  using R = discovery::Reason;
  declined(formula({{false, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}}),
           S::Declined, R::EvenCharge);
  std::vector<Equation> two = tseitin_test::triangle;
  two.insert(two.end(), {{true, {4, 6}}, {false, {4, 5}}, {false, {5, 6}}});
  declined(formula(two), S::Declined, R::Disconnected);
  declined(pairs({-1, 1}), S::Declined, R::AmbiguousEquation);
  declined(pairs({1}), S::Inapplicable, R::NoCompleteEquations);
  declined(formula({{true, {1, 2}}, {false, {1, 2, 3}}}), S::Declined,
           R::EdgeNotInExactlyTwoEquations);
  Kernel residual = formula(tseitin_test::triangle);
  kernel_add_binary_clause(residual, internalize(1), internalize(4),
                           ++residual.next_clause_id);
  declined(std::move(residual), S::Declined, R::ResidualEdgeTouch);
}

TEST(discovery_quarantines_ambiguity_and_enumerates_atomically) {
  {
    Kernel kernel = formula(tseitin_test::triangle, 8);
    // Both XOR right-hand sides on this disjoint scope are complete.  This
    // region is contradictory/ambiguous, but it must not hide the clean
    // component on variables 1..3.
    for (int first : {-4, 4})
      for (int second : {-5, 5})
        kernel_add_binary_clause(kernel, internalize(first), internalize(second),
                                 ++kernel.next_clause_id);
    std::vector<discovery::Plan> plans;
    assert(discovery::enumerate(snapshot(kernel), discovery_limits(), plans)
               .discovered());
    assert(plans.size() == 1 && plans[0].groups().size() == 3);
    for (const auto &group : plans[0].groups())
      for (uint32_t i = 0; i < group.certified.equation.size; ++i)
        assert(group.certified.equation.variables[i] <= 3);
  }
  std::vector<Equation> equations = tseitin_test::triangle;
  equations.insert(equations.end(),
                   {{true, {4, 6}}, {false, {4, 5}}, {false, {5, 6}}});
  Kernel kernel = formula(equations, 8);
  const auto captured = snapshot(kernel);
  std::vector<discovery::Plan> complete_plans;
  const auto complete =
      discovery::enumerate(captured, discovery_limits(), complete_plans);
  assert(complete.discovered() && complete_plans.size() == 2);
  auto limits = discovery_limits();
  limits.work = complete.work - 1;
  std::vector<discovery::Plan> incomplete{{}};
  const auto exhausted = discovery::enumerate(captured, limits, incomplete);
  assert(exhausted.status == discovery::Status::ResourceExhausted);
  assert(exhausted.components >= 1 && incomplete.empty());
}

TEST(discovery_is_bounded_and_interruptible) {
  Kernel kernel = formula(tseitin_test::triangle, 8);
  const live::Snapshot captured = snapshot(kernel);
  discovery::Plan exact;
  const auto complete = discover(captured, discovery_limits(), exact);
  assert(complete.discovered() && complete.work > 0);
  for (int kind = 0; kind < 3; ++kind) {
    auto limits = discovery_limits();
    if (kind == 0) limits.work = complete.work - 1;
    if (kind == 1) limits.maximum_clauses = 5;
    if (kind == 2) limits.maximum_groups = 2;
    discovery::Plan plan;
    const auto result = discover(captured, limits, plan);
    assert(result.status == discovery::Status::ResourceExhausted && !plan.ready());
    assert(result.reason == (kind == 0   ? discovery::Reason::WorkLimit
                             : kind == 1 ? discovery::Reason::ClauseLimit
                                         : discovery::Reason::GroupLimit));
  }
  {
    InterruptState state{0, 1};
    auto limits = discovery_limits();
    limits.interrupted = interrupt_after;
    limits.interrupt_state = &state;
    discovery::Plan plan;
    const auto result = discover(captured, limits, plan);
    assert(result.status == discovery::Status::Interrupted);
    assert(result.reason == discovery::Reason::Interrupted && !plan.ready());
  }
  InterruptState state{0, 1};
  auto limits = request().snapshot;
  limits.interrupted = interrupt_after;
  limits.interrupt_state = &state;
  live::Snapshot output;
  assert(live::capture(kernel, limits, output).status == live::Status::Interrupted);
  assert(output.sources.empty());
}

} // namespace

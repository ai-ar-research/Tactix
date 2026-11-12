#undef NDEBUG
#include "tseitin_test_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/tseitin_cycle_discovery.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

using namespace tactix;
using namespace tseitin_test;
namespace discovery = tactix::tseitin_cycle_discovery;
namespace live = tactix::tseitin_cycle_live;
namespace cycle = tactix::tseitin_cycle;

namespace {

const std::vector<Equation> &odd_triangle = tseitin_test::triangle;

Kernel formula(const std::vector<Equation> &equations) {
  return tseitin_test::formula(equations, 8);
}

live::SnapshotLimits snapshot_limits() {
  return {1000, 10000, 100000, 1000, nullptr, nullptr};
}

discovery::Limits discovery_limits() {
  return {100000, 1000, 100, 4, nullptr, nullptr};
}

live::Request live_request() {
  live::Request result;
  result.snapshot = snapshot_limits();
  result.admission = {100000, nullptr, nullptr};
  result.planning = {100000, 64, 1};
  result.transaction = {100000, 3, 2, 2, 1000, nullptr, nullptr};
  result.maximum_validation_ticks = 100000;
  return result;
}

live::Snapshot snapshot(Kernel &kernel) {
  live::Snapshot result;
  assert(live::capture(kernel, snapshot_limits(), result).status ==
         live::Status::Captured);
  return result;
}

// The single connected component of a complete snapshot: enumerate, declined
// as disconnected unless exactly one component is legal.
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
    result.selected_groups = 0;
    return result;
  }
  output = std::move(components.front());
  return result;
}

bool same_groups(const std::vector<cycle::CertifiedGroup> &left,
                 const std::vector<cycle::CertifiedGroup> &right) {
  if (left.size() != right.size())
    return false;
  for (size_t i = 0; i < left.size(); ++i) {
    const auto &a = left[i], &b = right[i];
    if (a.identity != b.identity || a.premise_proofs != b.premise_proofs ||
        a.certified.premises != b.certified.premises ||
        a.certified.equation.size != b.certified.equation.size ||
        a.certified.equation.rhs != b.certified.equation.rhs ||
        a.certified.equation.variables != b.certified.equation.variables)
      return false;
  }
  return true;
}

void positive_and_no_mutation() {
  {
    Kernel kernel = formula(odd_triangle);
    const live::Snapshot before = snapshot(kernel);
    const uint64_t next_clause_id = kernel.next_clause_id;
    const uint64_t touch_epoch = kernel.touch_epoch;
    discovery::Plan plan;
    const discovery::Result result = discover(
        before, discovery_limits(), plan);
    assert(result.discovered() && plan.ready());
    assert(result.clauses == 6 && result.complete_groups == 3 &&
           result.selected_groups == 3 && result.work > 0);
    assert(kernel.next_clause_id == next_clause_id &&
           kernel.touch_epoch == touch_epoch && snapshot(kernel) == before);

    live::Prepared prepared;
    assert(discovery::prepare(kernel, plan, live_request(), prepared).status ==
           live::Status::Prepared);
    assert(prepared.transaction().identity.pivot == 3);
    assert((prepared.transaction().identity.cycle_edges ==
            std::vector<cycle::Edge>{3, 1, 2}));
  }
  {
    Kernel kernel = formula(width_three);
    discovery::Plan plan;
    assert(discover(snapshot(kernel), discovery_limits(), plan)
               .discovered());
    assert(plan.groups().size() == 4);
    live::Prepared prepared;
    assert(discovery::prepare(kernel, plan, live_request(), prepared).status ==
           live::Status::Prepared);
    assert(prepared.transaction().identity.pivot == 4);
  }
}

void rejects_even_disconnected_ambiguous_and_incomplete() {
  {
    const std::vector<Equation> even = {
        {false, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}};
    Kernel kernel = formula(even);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Declined);
    assert(result.reason == discovery::Reason::EvenCharge && !plan.ready());
  }
  {
    std::vector<Equation> disconnected = odd_triangle;
    disconnected.insert(disconnected.end(),
                        {{true, {4, 6}}, {false, {4, 5}},
                         {false, {5, 6}}});
    Kernel kernel = formula(disconnected);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Declined);
    assert(result.reason == discovery::Reason::Disconnected && !plan.ready());
  }
  {
    Kernel kernel;
    kernel_reserve_vars(kernel, 2);
    for (int first : {-1, 1})
      for (int second : {-2, 2})
        kernel_add_binary_clause(kernel, internalize(first),
                                 internalize(second),
                                 ++kernel.next_clause_id);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Declined);
    assert(result.reason == discovery::Reason::AmbiguousEquation &&
           !plan.ready());
  }
  {
    Kernel kernel;
    kernel_reserve_vars(kernel, 2);
    kernel_add_binary_clause(kernel, internalize(1), internalize(2),
                             ++kernel.next_clause_id);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Inapplicable);
    assert(result.reason == discovery::Reason::NoCompleteEquations &&
           !plan.ready());
  }
}

void quarantines_ambiguity_and_enumerates_atomically() {
  {
    Kernel kernel = formula(odd_triangle);
    // Both XOR right-hand sides on this disjoint scope are complete.  This
    // region is contradictory/ambiguous, but it must not hide the clean
    // component on variables 1..3.
    for (int first : {-4, 4})
      for (int second : {-5, 5})
        kernel_add_binary_clause(kernel, internalize(first),
                                 internalize(second),
                                 ++kernel.next_clause_id);
    std::vector<discovery::Plan> plans;
    const auto result = discovery::enumerate(snapshot(kernel),
                                             discovery_limits(), plans);
    assert(result.discovered());
    assert(plans.size() == 1 && plans[0].groups().size() == 3);
    for (const auto &group : plans[0].groups())
      for (uint32_t i = 0; i < group.certified.equation.size; ++i)
        assert(group.certified.equation.variables[i] <= 3);
  }
  {
    std::vector<Equation> equations = odd_triangle;
    equations.insert(equations.end(),
                     {{true, {4, 6}}, {false, {4, 5}},
                      {false, {5, 6}}});
    Kernel kernel = formula(equations);
    const auto captured = snapshot(kernel);
    std::vector<discovery::Plan> complete_plans;
    const auto complete = discovery::enumerate(
        captured, discovery_limits(), complete_plans);
    assert(complete.discovered() && complete_plans.size() == 2 &&
           complete.work > 1);
    auto limits = discovery_limits();
    limits.work = complete.work - 1;
    std::vector<discovery::Plan> incomplete{{}};
    const auto exhausted = discovery::enumerate(captured, limits, incomplete);
    assert(exhausted.status == discovery::Status::ResourceExhausted);
    assert(exhausted.components >= 1);
    assert(incomplete.empty());
  }
}

void rejects_residual_and_bad_degree() {
  {
    Kernel kernel = formula(odd_triangle);
    kernel_add_binary_clause(kernel, internalize(1), internalize(4),
                             ++kernel.next_clause_id);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Declined);
    assert(result.reason == discovery::Reason::ResidualEdgeTouch &&
           !plan.ready());
  }
  {
    const std::vector<Equation> degree_one = {
        {true, {1, 2}}, {false, {1, 2, 3}}};
    Kernel kernel = formula(degree_one);
    discovery::Plan plan;
    const auto result =
        discover(snapshot(kernel), discovery_limits(), plan);
    assert(result.status == discovery::Status::Declined);
    assert(result.reason ==
           discovery::Reason::EdgeNotInExactlyTwoEquations);
  }
}

void bounded_and_interruptible() {
  Kernel kernel = formula(odd_triangle);
  const live::Snapshot captured = snapshot(kernel);
  discovery::Plan exact;
  const auto complete =
      discover(captured, discovery_limits(), exact);
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
  {
    InterruptState state{0, 1};
    auto limits = snapshot_limits();
    limits.interrupted = interrupt_after;
    limits.interrupt_state = &state;
    live::Snapshot output;
    const auto result = live::capture(kernel, limits, output);
    assert(result.status == live::Status::Interrupted);
    assert(output.sources.empty());
  }
}

void deterministic_order_and_stale_plan() {
  Kernel kernel = formula(odd_triangle);
  live::Snapshot ordered = snapshot(kernel);
  live::Snapshot permuted = ordered;
  std::reverse(permuted.sources.begin(), permuted.sources.end());
  std::reverse(permuted.clauses.begin(), permuted.clauses.end());
  discovery::Plan first, second;
  assert(discover(ordered, discovery_limits(), first).discovered());
  assert(discover(permuted, discovery_limits(), second).discovered());
  assert(same_groups(first.groups(), second.groups()));

  kernel_add_binary_clause(kernel, internalize(1), internalize(4),
                           ++kernel.next_clause_id);
  live::Prepared prepared;
  const auto stale = discovery::prepare(kernel, first, live_request(), prepared);
  assert(stale.status == live::Status::Stale);
  assert(stale.reason == live::Reason::SnapshotMismatch && !prepared.ready());
}

void capture_prepare_apply_and_proof_replay() {
  Kernel kernel = formula(odd_triangle);
  discovery::Plan plan;
  const auto found = discover(snapshot(kernel), discovery_limits(), plan);
  assert(found.discovered() && plan.ready());

  live::Prepared prepared;
  assert(discovery::prepare(kernel, plan, live_request(), prepared).status ==
         live::Status::Prepared);
  cycle::AdmissionRequest replay_request;
  replay_request.groups = plan.groups();
  replay_request.live_clauses = plan.snapshot().clauses;
  const cycle::ReplayLimits replay_limits{
      live_request().admission, live_request().planning,
      live_request().transaction};
  const auto replayed = cycle::replay_transaction(
      replay_request, prepared.transaction(), replay_limits);
  assert(replayed.ready());
  assert(replayed.transaction == prepared.transaction());

  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  attach(proof, kernel);
  // The proof writer is part of the prepared identity: prepare again with it.
  assert(discovery::prepare(kernel, plan, live_request(), prepared).status ==
         live::Status::Prepared);
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  proof.flush();
  kernel.proof = nullptr;
  const std::string contents = read_file(file.path);
  assert(!contents.empty() && contents.find("-3 0") != std::string::npos);
  kernel_check_invariants(kernel);
}

} // namespace

int main() {
  positive_and_no_mutation();
  rejects_even_disconnected_ambiguous_and_incomplete();
  quarantines_ambiguity_and_enumerates_atomically();
  rejects_residual_and_bad_degree();
  bounded_and_interruptible();
  deterministic_order_and_stale_plan();
  capture_prepare_apply_and_proof_replay();
  std::puts("test_tseitin_cycle_discovery: passed");
}

#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "rules/lucky/tseitin_cycle_discovery.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>

using namespace tactix;
namespace discovery = tactix::tseitin_cycle_discovery;
namespace live = tactix::tseitin_cycle_live;
namespace cycle = tactix::tseitin_cycle;

namespace {

struct Equation {
  bool rhs = false;
  std::vector<int32_t> variables;
};

const std::vector<Equation> odd_triangle = {
    {true, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}};

void add_equations(Kernel &kernel, const std::vector<Equation> &equations,
                   bool reverse = false) {
  struct Clause {
    std::vector<int32_t> literals;
  };
  std::vector<Clause> clauses;
  for (const Equation &equation : equations) {
    const auto encoded = parity_certificate::xor_equation(
        equation.variables.data(), equation.variables.size(), equation.rhs);
    for (const auto &clause : encoded)
      clauses.push_back({{clause.lits.begin(),
                          clause.lits.begin() + clause.size}});
  }
  if (reverse)
    std::reverse(clauses.begin(), clauses.end());
  for (const Clause &clause : clauses) {
    if (clause.literals.size() == 2) {
      kernel_add_binary_clause(kernel, internalize(clause.literals[0]),
                               internalize(clause.literals[1]),
                               ++kernel.next_clause_id);
    } else {
      std::vector<Lit> internal;
      for (int32_t literal : clause.literals)
        internal.push_back(internalize(literal));
      kernel_add_large_clause(kernel, internal.data(), internal.size(), false,
                              0, ++kernel.next_clause_id);
    }
  }
}

Kernel formula(const std::vector<Equation> &equations,
               uint32_t variables = 8, bool reverse = false) {
  Kernel kernel;
  kernel_reserve_vars(kernel, variables);
  add_equations(kernel, equations, reverse);
  return kernel;
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

struct InterruptState {
  uint64_t calls = 0;
  uint64_t stop = UINT64_MAX;
};

bool interrupt_after(void *opaque) {
  auto &state = *static_cast<InterruptState *>(opaque);
  return ++state.calls >= state.stop;
}

std::string temporary_path() {
  char pattern[] = "/tmp/tseitin-discovery-XXXXXX";
  const int descriptor = mkstemp(pattern);
  assert(descriptor >= 0);
  close(descriptor);
  return pattern;
}

std::string read_file(const std::string &path) {
  FILE *file = std::fopen(path.c_str(), "rb");
  assert(file);
  std::string output;
  char bytes[4096];
  while (const size_t count = std::fread(bytes, 1, sizeof bytes, file))
    output.append(bytes, count);
  assert(!std::ferror(file));
  assert(!std::fclose(file));
  return output;
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
    const discovery::Result result = discovery::discover(
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
    const std::vector<Equation> width_three = {
        {true, {1, 2, 3}}, {false, {1, 4, 5}},
        {false, {2, 4, 6}}, {false, {3, 5, 6}}};
    Kernel kernel = formula(width_three);
    discovery::Plan plan;
    assert(discovery::discover(snapshot(kernel), discovery_limits(), plan)
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
        discovery::discover(snapshot(kernel), discovery_limits(), plan);
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
      discovery::discover(captured, discovery_limits(), exact);
  assert(complete.discovered() && complete.work > 0);
  {
    auto limits = discovery_limits();
    limits.work = complete.work - 1;
    discovery::Plan plan;
    const auto result = discovery::discover(captured, limits, plan);
    assert(result.status == discovery::Status::ResourceExhausted);
    assert(result.reason == discovery::Reason::WorkLimit && !plan.ready());
  }
  {
    auto limits = discovery_limits();
    limits.maximum_clauses = 5;
    discovery::Plan plan;
    const auto result = discovery::discover(captured, limits, plan);
    assert(result.status == discovery::Status::ResourceExhausted);
    assert(result.reason == discovery::Reason::ClauseLimit && !plan.ready());
  }
  {
    auto limits = discovery_limits();
    limits.maximum_groups = 2;
    discovery::Plan plan;
    const auto result = discovery::discover(captured, limits, plan);
    assert(result.status == discovery::Status::ResourceExhausted);
    assert(result.reason == discovery::Reason::GroupLimit && !plan.ready());
  }
  {
    InterruptState state{0, 1};
    auto limits = discovery_limits();
    limits.interrupted = interrupt_after;
    limits.interrupt_state = &state;
    discovery::Plan plan;
    const auto result = discovery::discover(captured, limits, plan);
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
  {
    InterruptState state{0, 1};
    auto limits = discovery_limits();
    limits.interrupted = interrupt_after;
    limits.interrupt_state = &state;
    discovery::Plan plan;
    const auto result =
        discovery::capture(kernel, snapshot_limits(), limits, plan);
    assert(result.status == discovery::Status::Interrupted);
    assert(result.reason == discovery::Reason::Interrupted && !plan.ready());
  }
}

void deterministic_order_and_stale_plan() {
  Kernel kernel = formula(odd_triangle);
  live::Snapshot ordered = snapshot(kernel);
  live::Snapshot permuted = ordered;
  std::reverse(permuted.sources.begin(), permuted.sources.end());
  std::reverse(permuted.clauses.begin(), permuted.clauses.end());
  discovery::Plan first, second;
  assert(discovery::discover(ordered, discovery_limits(), first).discovered());
  assert(discovery::discover(permuted, discovery_limits(), second).discovered());
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
  const auto found = discovery::capture(
      kernel, snapshot_limits(), discovery_limits(), plan);
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

  const std::string path = temporary_path();
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  // A proof writer is part of the prepared snapshot identity, so prepare once
  // more after attaching it and publish exactly that replayed transaction.
  assert(discovery::prepare(kernel, plan, live_request(), prepared).status ==
         live::Status::Prepared);
  assert(live::apply(kernel, prepared).status == live::Status::Applied);
  proof.flush();
  kernel.proof = nullptr;
  const std::string contents = read_file(path);
  assert(!contents.empty() && contents.find("-3 0") != std::string::npos);
  unlink(path.c_str());
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

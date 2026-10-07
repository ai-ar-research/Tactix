#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "rules/lucky/lucky.hpp"
#include "rules/registry.hpp"
#include "policies/heuristics.hpp"
#include "strategy/local_search.hpp"
#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace tactix;

int main() {
  static_assert(LUCKY_NOL_HYPERGRAPH == 25);
  static_assert(static_cast<uint8_t>(
                    strategy::LocalSearchKind::RetiredHypergraphColoring) == 6);
  assert(lucky_retired_constructor_instance(LUCKY_NOL_HYPERGRAPH));
  assert(!lucky_retired_constructor_instance(LUCKY_EXACT_ONE_PROOF));
  assert(!lucky_retired_constructor_instance(LUCKY_GRAPH_AUTHENTICATE));
  assert(!strategy::valid_local_search_kind(
      strategy::LocalSearchKind::RetiredHypergraphColoring));
  assert(!strategy::valid_local_search_candidate(
      (uint64_t{1} << 8) | static_cast<uint8_t>(
                                strategy::LocalSearchKind::RetiredHypergraphColoring)));

  Kernel kernel;
  ScopedHeuristics heuristics(kernel);
  kernel_reserve_vars(kernel, 2);
  kernel_add_binary_clause(kernel, internalize(1), internalize(2));
  const auto &lucky = tactix_rules()[tactix_rule_id("Lucky")];
  uint64_t instances[LUCKY_INSTANCE_COUNT]{};
  const uint32_t count = lucky.instances(
      kernel, instances, LUCKY_INSTANCE_COUNT);
  assert(count == LUCKY_ACTIVE_INSTANCE_COUNT);
  assert(std::find(instances, instances + count, LUCKY_NOL_HYPERGRAPH) ==
         instances + count);
  assert(lucky.apply_instance(kernel, LUCKY_NOL_HYPERGRAPH) ==
         ApplyResult::NoOp);

  auto &retired_hint = kernel.local_search_hints[static_cast<size_t>(
      strategy::LocalSearchKind::RetiredHypergraphColoring)];
  retired_hint.id = (uint64_t{1} << 8) | static_cast<uint8_t>(
      strategy::LocalSearchKind::RetiredHypergraphColoring);
  retired_hint.external_phases = {1};
  const auto &guidance =
      tactix_rules()[tactix_rule_id("LocalSearchGuidance")];
  uint64_t guidance_instances[8]{};
  assert(guidance.instances(kernel, guidance_instances, 8) == 0);
  assert(guidance.apply_instance(
             kernel, static_cast<uint8_t>(
                         strategy::LocalSearchKind::RetiredHypergraphColoring)) ==
         ApplyResult::NoOp);
  assert(guidance.apply(kernel) == ApplyResult::NoOp);

  strategy::OnlineInvocation invocation;
  invocation.id = 1;
  invocation.rule = "Lucky";
  invocation.granularity = strategy::InvocationGranularity::OneInstance;
  invocation.instances = {LUCKY_NOL_HYPERGRAPH};
  strategy::OnlineStrategyProgram program{{invocation}, {}};
  std::string error;
  assert(!strategy::validate_online_strategy_program(program, &error));
  assert(error == "Lucky constructor instance is retired");

  invocation.rule = "LocalSearchGuidance";
  invocation.instances = {static_cast<uint8_t>(
      strategy::LocalSearchKind::RetiredHypergraphColoring)};
  program.sequence = {invocation};
  assert(!strategy::validate_online_strategy_program(program, &error));
  assert(error == "LocalSearchGuidance kind is retired");
  invocation.instances = {retired_hint.id};
  program.sequence = {invocation};
  assert(!strategy::validate_online_strategy_program(program, &error));
  assert(error == "LocalSearchGuidance kind is retired");

  auto *solver = tactix_init();
  tactix_add(solver, 1);
  tactix_add(solver, 2);
  tactix_add(solver, 0);
  const int lucky_id = tactix_rule_id("Lucky");
  assert(lucky_id >= 0);
  uint64_t public_instances[LUCKY_INSTANCE_COUNT]{};
  const int public_count = tactix_rule_instances(
      solver, lucky_id, public_instances, LUCKY_INSTANCE_COUNT);
  assert(public_count == int(LUCKY_ACTIVE_INSTANCE_COUNT));
  assert(std::find(public_instances, public_instances + public_count,
                   LUCKY_NOL_HYPERGRAPH) ==
         public_instances + public_count);
  // Direct legacy calls retain the stable numeric payload as a no-op; the
  // enumerated and replayable surfaces never advertise or accept it.
  assert(tactix_apply_instance(solver, lucky_id, LUCKY_NOL_HYPERGRAPH) == 1);
  tactix_release(solver);
}

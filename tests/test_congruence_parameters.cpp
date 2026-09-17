#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/congruence/congruence.hpp"
#include "strategy/congruence_parameters.hpp"
#define tactix tactix_handle
#include "tactix.h"
#undef tactix

#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <set>
#include <vector>

using namespace tactix;

static void add(Kernel &k, std::initializer_list<int> lits) {
  std::vector<Lit> internal;
  for (int lit : lits)
    internal.push_back(internalize(lit));
  kernel_add_large_clause(k, internal.data(), internal.size(), false, 0);
}

static void and3(Kernel &k, int output, int a, int b, int c) {
  kernel_add_binary_clause(k, internalize(-output), internalize(a));
  kernel_add_binary_clause(k, internalize(-output), internalize(b));
  kernel_add_binary_clause(k, internalize(-output), internalize(c));
  add(k, {output, -a, -b, -c});
}

static void xor2(Kernel &k, int output, int a, int b) {
  add(k, {-output, a, b});
  add(k, {output, -a, b});
  add(k, {output, a, -b});
  add(k, {-output, -a, -b});
}

static void ite(Kernel &k, int output, int condition, int then_lit,
                int else_lit) {
  add(k, {condition, -else_lit, output});
  add(k, {condition, else_lit, -output});
  add(k, {-condition, -then_lit, output});
  add(k, {-condition, then_lit, -output});
}

static size_t kind_count(const Kernel &k, GateKind kind) {
  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, true);
  size_t result = 0;
  for (const CongGate &gate : gates)
    result += gate.kind == kind;
  return result;
}

static void discovery_limits_are_effective_and_unbounded() {
  {
    Kernel k;
    kernel_reserve_vars(k, 6);
    and3(k, 5, 1, 2, 3);
    and3(k, 6, 1, 2, 3);
    k.congruence_parameters.and_arity_limit = 2;
    assert(kind_count(k, GateKind::And) == 0);
    k.congruence_parameters.and_arity_limit = 0;
    assert(kind_count(k, GateKind::And) == 2);
    k.congruence_parameters.gate_count_limit = 1;
    assert(kind_count(k, GateKind::And) >= 1);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 3);
    xor2(k, 3, 1, 2);
    k.congruence_parameters.xor_arity_limit = 1;
    assert(kind_count(k, GateKind::Xor) == 0);
    k.congruence_parameters.xor_arity_limit = 2;
    assert(kind_count(k, GateKind::Xor) == 3);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    ite(k, 4, 1, 2, 3);
    assert(kind_count(k, GateKind::Ite) == 1);
  }
}

static void scope_and_c_api_validate_without_partial_update() {
  Kernel k;
  const strategy::CongruenceParameters original = k.congruence_parameters;
  const strategy::CongruenceParameters posed{7, 11, 3};
  {
    strategy::ScopedCongruenceParameters scope(k, posed);
    assert(k.congruence_parameters.and_arity_limit == 7);
    assert(k.congruence_parameters.gate_count_limit == 11);
  }
  assert(k.congruence_parameters.and_arity_limit == original.and_arity_limit);

  tactix_handle *solver = tactix_init();
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_AND_ARITY) == 1000000);
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_GATE_COUNT) == 20000000);
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_XOR_ARITY) == 4);
  assert(tactix_set_congruence_limits(solver, 7, 11, 3) == 0);
  assert(tactix_set_congruence_limits(solver, 1, 2, 5) == -1);
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_AND_ARITY) == 7);
  assert(tactix_set_congruence_watch_order(solver,
      TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER) == 0);
  assert(tactix_set_congruence_watch_order(solver, -1) == -1);
  assert(tactix_set_congruence_watch_order(solver, 256) == -1);
  assert(tactix_set_congruence_limits(solver, 8, 12, 4) == 0);
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_WATCH_ORDER) == 1);
  assert(tactix_set_congruence_watch_order(solver,
      TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER) == 0);
  for (int literal : {1, 2, 3, 0}) tactix_add(solver, literal);
  tactix_online_step step{};
  step.step_id = 70;
  step.rule_id = -1;
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (!std::strcmp(tactix_rule_name(id), "Congruence")) step.rule_id = id;
  assert(step.rule_id >= 0);
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_CONGRUENCE;
  step.parameters.value.congruence = {1000000, 4, 20000000,
      TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER};
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(solver, &program, 70,
      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  step.parameters.value.congruence.binary_watch_order = 256;
  assert(tactix_online_configure(solver, &program, 71,
      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.configuration_revision == 70 && result.applications == 1);
  assert(tactix_config(solver, TACTIX_CFG_CONGRUENCE_WATCH_ORDER) == 0);
  tactix_release(solver);
}

static void optional_entry_order_preserves_inference() {
  using Order = strategy::CongruenceBinaryWatchOrder;
  Kernel quiet;
  kernel_reserve_vars(quiet, 10);
  for (int partner : {8, 3, 6})
    kernel_add_binary_clause(quiet, internalize(1), internalize(partner));
  add(quiet, {1, 4, 5});
  add(quiet, {-2, 7, 8});
  const std::vector<uint32_t> before(quiet.watches.arena.begin(),
                                     quiet.watches.arena.end());
  auto preserved = quiet.congruence_parameters;
  preserved.binary_watch_order = Order::PreserveExisting;
  {
    strategy::ScopedCongruenceParameters scope(quiet, preserved);
    assert(congruence_apply(quiet) == ApplyResult::NoOp);
    assert(quiet.stats.congruence_binary_rebuilds == 0);
    assert(before == std::vector<uint32_t>(quiet.watches.arena.begin(),
                                           quiet.watches.arena.end()));
  }
  assert(quiet.congruence_parameters.binary_watch_order == Order::Canonical);
  assert(congruence_apply(quiet) == ApplyResult::NoOp);
  assert(quiet.stats.congruence_binary_rebuilds == 1);
  assert(before != std::vector<uint32_t>(quiet.watches.arena.begin(),
                                         quiet.watches.arena.end()));
  kernel_check_invariants(quiet);
  for (Order order : {Order::Canonical, Order::PreserveExisting}) {
    Kernel k;
    kernel_reserve_vars(k, 6);
    and3(k, 5, 1, 2, 3);
    and3(k, 6, 1, 2, 3);
    k.congruence_parameters.binary_watch_order = order;
    assert(congruence_apply(k) == ApplyResult::Ok);
    assert(k.stats.congruence_merges > 0);
    assert(kernel_binary_live(k, internalize(-5), internalize(6)));
    assert(kernel_binary_live(k, internalize(5), internalize(-6)));
    kernel_check_invariants(k);
  }
  auto invalid = preserved;
  invalid.binary_watch_order = static_cast<Order>(2);
  assert(!invalid.valid());
  DefaultStrategyConfig config;
  config.inprocess.congruence_parameters = preserved;
  assert(config.valid());
  assert(config.serialize().find("inprocess.congruence_binary_watch_order=1") !=
         std::string::npos);
  config.inprocess.congruence_parameters = invalid;
  assert(!config.valid());
}

static void sparse_and_membership_matches_physical_formula() {
  Kernel k;
  kernel_reserve_vars(k, 48);
  and3(k, 31, 1, 2, 3);
  and3(k, 32, 1, 2, 3);
  and3(k, 33, 4, 5, 6);
  add(k, {31, -1, -2, -3}); // Physical repeated base, same ordered output.
  add(k, {34, -1, -2, -3}); // A missing defining binary must reject the gate.
  kernel_add_binary_clause(k, internalize(-34), internalize(1));
  kernel_add_binary_clause(k, internalize(-34), internalize(2));
  for (int lit = 7; lit < 30; ++lit) {
    kernel_add_binary_clause(k, internalize(-31), internalize(lit));
    add(k, {lit, 40, 41}); // Mixed large-watch tails.
  }
  kernel_add_binary_clause(k, internalize(-31), internalize(1));
  BinaryLivenessIndex filter;
  filter.reserve(128);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary()) filter.add(kernel_binary_key(lit, watch.other()));
  const auto missing = kernel_binary_key(internalize(-33), internalize(6));
  assert(filter.maybe(missing));

  using Gate = std::pair<Lit, std::vector<Lit>>;
  const auto oracle = [&] {
    // Re-derive the mathematical gate predicate from physical clause pairs,
    // independently of the kernel query and both extraction traversals.
    std::set<std::pair<Lit, Lit>> binaries;
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary()) binaries.emplace(std::min(lit, watch.other()),
                                             std::max(lit, watch.other()));
    std::vector<Gate> expected;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      ref += clause_actual_words(clause);
      if (clause->garbage() || clause->redundant() || clause->size < 3 ||
          clause->size > k.congruence_parameters.and_arity_limit + 1) continue;
      if (k.congruence_parameters.gate_count_limit &&
          expected.size() >= k.congruence_parameters.gate_count_limit) break;
      for (Lit output : *clause) {
        bool gate = true;
        std::vector<Lit> inputs;
        for (Lit literal : *clause) if (literal != output) {
          const Lit first = negate(output), second = negate(literal);
          gate &= binaries.count({std::min(first, second), std::max(first, second)}) != 0;
          inputs.push_back(second);
        }
        if (gate) {
          std::sort(inputs.begin(), inputs.end());
          expected.emplace_back(output, std::move(inputs));
        }
      }
    }
    return expected;
  };
  for (bool deleted : {false, true}) {
    if (deleted) {
      assert(kernel_delete_binary_all_copies(k, internalize(-33), internalize(6)));
      assert(filter.maybe(missing)); // The stale positive is deliberately retained.
    }
    for (bool indexed : {false, true}) for (uint32_t cap : {0u, 1u, 2u}) {
      k.binary_liveness = indexed ? &filter : nullptr;
      k.congruence_parameters.and_arity_limit = 3;
      k.congruence_parameters.gate_count_limit = cap;
      const auto expected = oracle();
      for (bool dense : {false, true}) {
        OccIndex occurrences;
        if (dense) kernel_dense_connect(k, occurrences);
        const auto generation = k.arena_generation, epoch = k.strategy_epoch;
        const std::vector<uint32_t> watches(k.watches.arena.begin(), k.watches.arena.end());
        std::vector<CongGate> gates;
        congruence_extract(k, gates);
        std::vector<Gate> observed;
        for (const auto &gate : gates) {
          assert(gate.kind == GateKind::And);
          observed.emplace_back(gate.output, std::vector<Lit>(gate.inputs.begin(), gate.inputs.end()));
        }
        assert(observed == expected);
        assert(generation == k.arena_generation && epoch == k.strategy_epoch);
        assert(watches == std::vector<uint32_t>(k.watches.arena.begin(), k.watches.arena.end()));
        if (dense) kernel_dense_disconnect(k);
      }
      k.binary_liveness = nullptr;
    }
  }
  kernel_check_invariants(k);
}

int main() {
  optional_entry_order_preserves_inference();
  sparse_and_membership_matches_physical_formula();
  discovery_limits_are_effective_and_unbounded();
  scope_and_c_api_validate_without_partial_update();
  std::puts("Congruence typed discovery limits and unbounded paths PASS");
}

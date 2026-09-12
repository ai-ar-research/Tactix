#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "strategy/congruence_parameters.hpp"
#define tactix tactix_handle
#include "tactix.h"
#undef tactix

#include <cassert>
#include <cstdio>
#include <initializer_list>
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
  tactix_release(solver);
}

int main() {
  discovery_limits_are_effective_and_unbounded();
  scope_and_c_api_validate_without_partial_update();
  std::puts("Congruence typed discovery limits and unbounded paths PASS");
}

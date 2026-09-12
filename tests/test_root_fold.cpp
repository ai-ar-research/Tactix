#undef NDEBUG
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

static std::vector<uint64_t> fixture(bool fold, bool reverse, bool stable) {
  Kernel k;
  ScopedHeuristics owner(k);
  owner.h.stable = stable;
  constexpr Var roots = 1024, live = 8;
  kernel_reserve_vars(k, roots + live);
  std::vector<Var> order;
  for (Var v = 0; v < k.nvars; v++) order.push_back(v);
  if (reverse) std::reverse(order.begin(), order.end());
  for (Var v : order) kernel_record_import(k, v);
  heuristics_seed_activation_order(k, order);
  for (Var v = 0; v < roots; v++) {
    kernel_assign_unit(k, make_lit(v, v & 1));
    k.unit_ids[v] = 100 + v;
  }
  assert(propagate_apply(k) == ApplyResult::Ok);
  heuristics_deactivate_inactive_vars(k);
  heuristics_compact_random_order_fixed(k);
  const Var representative = owner.h.fixed_representative;
  owner.h.links[representative].stamp = 31337;
  for (unsigned repeat = 0; repeat < 3; repeat++) {
    assert(kernel_compact_variables(k, fold));
    heuristics_compact_random_order_fixed(k);
    assert(k.nvars == (fold ? live + 1 : roots + live));
    assert(k.fixed_external_count == (fold ? roots - 1 : 0));
    assert(kernel_root_fixed(k) + k.fixed_external_count == roots);
    assert(owner.h.links[owner.h.fixed_representative].stamp == 31337);
    for (Var ev = 0; ev < roots; ev++) {
      assert(kernel_external_value(k, make_lit(ev, ev & 1)) == 1);
      assert(kernel_external_value(k, make_lit(ev, !(ev & 1))) == -1);
      assert(kernel_external_unit_id(k, ev) == 100 + ev);
    }
    kernel_check_invariants(k);
  }
  std::vector<uint64_t> semantic;
  for (Var v : k.compact_import_order)
    semantic.push_back(k.internal_to_external[v]);
  semantic.push_back(UINT64_MAX);
  for (Var v = owner.h.queue_first; v != INVALID_VAR; v = owner.h.links[v].next) {
    semantic.push_back(k.internal_to_external[v]);
    semantic.push_back(owner.h.links[v].stamp);
  }
  for (const RuleVTable &rule : tactix_rules()) {
    if (std::strcmp(rule.name, "Decide")) continue;
    uint64_t actions[2 * live];
    assert(rule.instances(k, actions, 2 * live) == 2 * live);
    std::sort(actions, actions + 2 * live);
    semantic.insert(semantic.end(), actions, actions + 2 * live);
  }
  // New extension identities must never recycle a compacted external root.
  const Var extension = kernel_new_extension_vars(k, 1);
  assert(k.internal_to_external[extension] == roots + live);
  const Var newly_fixed = var_of(kernel_internalize(k, roots + 1));
  kernel_assign_unit(k, make_lit(newly_fixed, false));
  k.unit_ids[newly_fixed] = 9001;
  assert(propagate_apply(k) == ApplyResult::Ok);
  heuristics_deactivate_inactive_vars(k);
  assert(kernel_compact_variables(k, fold));
  heuristics_compact_random_order_fixed(k);
  assert(kernel_external_unit_id(k, roots) == 9001);
  assert(kernel_root_fixed(k) + k.fixed_external_count == roots + 1);
  kernel_check_invariants(k);
  kernel_extend_model(k);
  for (Var ev = 0; ev < roots; ev++)
    assert(k.model_values[make_lit(ev, ev & 1)] == 1);
  assert(k.model_values[make_lit(roots, false)] == 1);
  return semantic;
}

static void referenced_roots_stay_hot() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  for (Var v = 0; v < 3; v++) kernel_assign_unit(k, make_lit(v, false));
  // x2 is still a literal in a stored clause; x1 is the representative, and
  // only x3 can leave the hot arrays. No clause/proof mutation is requested.
  kernel_add_binary_clause(k, internalize(-2), internalize(4), 10);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(kernel_compact_variables(k, true));
  assert(kernel_internalize(k, 2) != INVALID_LIT);
  assert(kernel_internalize(k, 3) == INVALID_LIT);
  assert(kernel_external_value(k, internalize(3)) == 1);
  assert(kernel_binary_live(k, kernel_internalize(k, -2), kernel_internalize(k, 4)));
  kernel_check_invariants(k);
}

static int proof_fixture(const char *input, const char *output, const char *format, bool fold) {
  Kernel k;
  ScopedHeuristics owner(k);
  Proof proof;
  assert(proof.open(output, !std::strcmp(format, "lrat") ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  FILE *file = std::fopen(input, "r");
  assert(file && !parse_dimacs(file, k));
  std::fclose(file);
  assert(propagate_apply(k) == ApplyResult::Ok);
  heuristics_deactivate_inactive_vars(k);
  heuristics_compact_random_order_fixed(k);
  const uint32_t roots = kernel_root_fixed(k);
  assert(roots >= 3);
  assert(kernel_compact_variables(k, fold));
  heuristics_compact_random_order_fixed(k);
  assert(fold ? k.fixed_external_count >= 2 : k.fixed_external_count == 0);
  assert(kernel_root_fixed(k) + k.fixed_external_count == roots);
  const int result = default_solve(k);
  proof.flush();
  if (result == 10) {
    kernel_extend_model(k);
    std::puts("s SATISFIABLE");
    std::printf("v");
    for (Var ev = 0; ev < kernel_num_output_vars(k); ev++)
      std::printf(" %d", k.model_values[make_lit(ev, false)] > 0 ? int(ev+1) : -int(ev+1));
    std::puts(" 0");
  } else {
    assert(result == 20);
    std::puts("s UNSATISFIABLE");
  }
  std::printf("c fixed-root fixture archived %u\n", k.fixed_external_count);
  return result;
}

static void reconstruction_and_nonroot_reason() {
  {
    Kernel k;
    kernel_reserve_vars(k, 5);
    kernel_assign_unit(k, internalize(1));
    kernel_assign_unit(k, internalize(-2));
    kernel_assign_unit(k, internalize(3));
    const Lit true_rest = internalize(3), false_rest = internalize(2);
    kernel_push_witness(k, internalize(4), &true_rest, 1);
    kernel_push_witness(k, internalize(5), &false_rest, 1);
    k.var_status.assign(5, VarStatus::Active);
    k.var_status[3] = k.var_status[4] = VarStatus::Eliminated;
    k.unassigned_active -= 2;
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(kernel_compact_variables(k, true));
    assert(k.nvars == 1 && k.fixed_external_count == 2);
    kernel_extend_model(k);
    assert(k.model_values[internalize(-2)] > 0);
    assert(k.model_values[internalize(3)] > 0);
    assert(k.model_values[internalize(-4)] > 0);
    assert(k.model_values[internalize(5)] > 0);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 6);
    for (int v : {1, 2, 3}) kernel_assign_unit(k, internalize(v));
    const Lit clause[] = {internalize(-2), internalize(-4), internalize(5)};
    kernel_add_large_clause(k, clause, 3, false, 0, 10);
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(4));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.value(internalize(5)) > 0 && k.var_level(4) == 1);
    assert(kernel_compact_variables(k, true));
    assert(k.nvars == 5 && k.fixed_external_count == 1);
    assert(kernel_internalize(k, 2) != INVALID_LIT);
    assert(kernel_internalize(k, 3) == INVALID_LIT);
    const Var forced = var_of(kernel_internalize(k, 5));
    assert(k.assigned[forced].level == 1 && !k.assigned[forced].binary);
    const Clause *reason = clause_at(k.arena, k.assigned[forced].reason);
    assert(!reason->garbage());
    assert(k.frames[1] == 2 && k.propagate_head == k.trail.size());
    kernel_check_invariants(k);
  }
}

int main(int argc, char **argv) {
  if (argc == 5) return proof_fixture(argv[1], argv[2], argv[3], !std::strcmp(argv[4], "fold"));
  assert(argc == 1);
  for (bool reverse : {false, true})
    for (bool stable : {false, true})
      assert(fixture(false, reverse, stable) == fixture(true, reverse, stable));
  referenced_roots_stay_hot();
  reconstruction_and_nonroot_reason();
  std::puts("root-fold: physical shrink, values/units, live ordering/actions, repeated compaction, extensions and model PASS");
}

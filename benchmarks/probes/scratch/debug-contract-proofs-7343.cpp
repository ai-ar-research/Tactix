// Original-CNF proof witnesses for properly named Substitute and disconnected GC.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitute/substitute.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 6);
  Kernel k;
  Proof proof;
  assert(proof.open(argv[2], !std::strcmp(argv[3], "lrat") ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  FILE *input = std::fopen(argv[1], "r");
  assert(input && !parse_dimacs(input, k));
  std::fclose(input);
  assert(propagate_apply(k) == ApplyResult::Ok);
  unsigned reach = 0;
  if (!std::strcmp(argv[4], "substitute")) {
    const auto &rule = tactix_rules()[tactix_rule_id("Substitute")];
    assert(rule.guard(k));
    uint64_t instances[32];
    assert(rule.instances(k, instances, 32));
    const auto result = !std::strcmp(argv[5], "explicit")
        ? rule.apply_instance(k, instances[0]) : rule.apply(k);
    assert(result == ApplyResult::Ok || result == ApplyResult::Unsat);
    reach = 1;
  } else {
    assert(!std::strcmp(argv[4], "collect"));
    kernel_flush_large_watches(k);
    OccIndex occurrences;
    const bool indexed = !std::strcmp(argv[5], "indexed");
    if (indexed) kernel_dense_connect(k, occurrences, true);
    for (Ref ref = 0; ref < k.arena.size();) {
      Clause *clause = clause_at(k.arena, ref);
      bool satisfied = false;
      for (Lit literal : *clause) satisfied |= kernel_root_value(k, literal) > 0;
      if (satisfied) { kernel_delete_clause(k, clause); ++reach; }
      ref += clause_actual_words(clause);
    }
    assert(reach);
    kernel_collect_garbage(k);
    assert(!k.large_watches_connected);
    for (Lit literal = 0; literal < 2 * k.nvars; literal++)
      assert(!k.watches.heads[literal].has_large());
    kernel_check_invariants(k);
    if (indexed) kernel_dense_disconnect(k);
    kernel_rebuild_large_watches(k);
  }
  kernel_check_invariants(k);
  const int result = k.inconsistent ? 20 : default_solve(k);
  assert(result == 10 || result == 20);
  proof.flush();
  if (result == 10) {
    kernel_extend_model(k);
    std::puts("s SATISFIABLE"); std::printf("v");
    for (Var v = 0; v < kernel_num_output_vars(k); v++)
      std::printf(" %d", k.model_values[make_lit(v,false)] > 0 ? int(v+1) : -int(v+1));
    std::puts(" 0");
  } else std::puts("s UNSATISFIABLE");
  std::printf("c reached %s %u\n", argv[4], reach);
  return result;
}

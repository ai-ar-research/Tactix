// Valid original-formula witnesses supplement the arbitrary-state byte oracle.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 9);
  const int roots = std::atoi(argv[1]), order = std::atoi(argv[2]);
  const int copies = std::atoi(argv[3]), elevated = std::atoi(argv[4]);
  const bool unsat = std::atoi(argv[5]);
  const int mode = std::atoi(argv[6]);
  assert(roots >= 1 && roots <= 3 && order >= 0 && order <= 2);
  assert(copies >= 1 && copies <= 3 && mode >= 0 && mode <= 2);
  Proof proof;
  assert(proof.open(argv[8], mode == 2 ? ProofFormat::Lrat :
                   mode == 1 ? ProofFormat::DratBinary : ProofFormat::DratAscii));
  Kernel k;
  kernel_reserve_vars(k, 8);
  kernel_materialize_cold_state(k);
  ScopedHeuristics h(k);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  if (order != 2) {
    k.next_import_rank = 8;
    for (Var v = 0; v < 8; ++v) {
      k.internal_import_rank[v] = order ? 7-v : v;
      k.external_import_rank[v] = order ? 7-v : v;
      k.import_rank_to_external.push_back(order ? 7-v : v);
    }
  }
  // The full solver consumes both directions of the permanent permutation.
  // The standalone collector byte oracle only reads forward ranks; do not
  // transfer its deliberately partial diagnostic setup to a solve witness.
  assert(k.next_import_rank == k.import_rank_to_external.size());
  std::vector<Var> import_order;
  kernel_vars_in_import_order(k,import_order);
  assert(import_order.size() == 8);
  for (Var i = 0; i < 8; ++i)
    assert(import_order[i] == (order == 1 ? 7-i : i));
  std::vector<std::vector<int>> input;
  for (int i = 0; i < copies; ++i) input.push_back({1,2});
  // One satisfied large watch and one binary with only a decision-true end.
  input.push_back({roots == 2 ? 2 : 1,3,4});
  input.push_back({7,8});
  input.push_back({5,6}); input.push_back({5,-6}); input.push_back({-5,6});
  if (unsat) input.push_back({-5,-6});
  if (roots & 1) input.push_back({1});
  if (roots & 2) input.push_back({2});
  for (const auto &clause : input) {
    std::vector<Lit> lits;
    for (int lit : clause) lits.push_back(internalize(lit));
    const auto id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else if (lits.size() == 2) kernel_add_binary_clause(k,lits[0],lits[1],id);
    else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,id);
  }
  assert(propagate_apply(k) == ApplyResult::Ok);
  if (elevated) {
    kernel_assign_decision(k,internalize(7));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  const auto before = k.stats.binaries_deleted;
  kernel_sparse_collect_root(k);
  assert(k.stats.binaries_deleted - before == unsigned(copies));
  kernel_check_invariants(k);
  kernel_sparse_collect_root(k);
  assert(k.stats.binaries_deleted - before == unsigned(copies));
  assert(default_solve(k) == (unsat ? 20 : 10));
  if (!unsat) {
    kernel_extend_model(k);
    for (const auto &clause : input) {
      bool satisfied = false;
      for (int lit : clause) satisfied |= k.model_values[internalize(lit)] > 0;
      assert(satisfied);
    }
  }
  proof.flush();
  std::ofstream cnf(argv[7]);
  cnf << "p cnf 8 " << input.size() << '\n';
  for (const auto &clause : input) {
    for (int lit : clause) cnf << lit << ' ';
    cnf << "0\n";
  }
  std::printf("roots=%d order=%d copies=%d elevated=%d unsat=%d mode=%d deleted=%d PASS\n",
              roots,order,copies,elevated,unsat,mode,copies);
}

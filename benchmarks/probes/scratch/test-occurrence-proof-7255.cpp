// Original-formula proof/model witnesses for public connected rule consumers.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 8);
  const unsigned family = std::atoi(argv[1]), mixed = std::atoi(argv[2]);
  const unsigned mode = std::atoi(argv[3]), unsat = std::atoi(argv[4]);
  const unsigned variant = std::atoi(argv[5]);
  const char *names[] = {"Sweep", "Substitute", "Congruence", "HyperTernaryRes",
                         "SubsumeForward", "Strengthen", "BVE"};
  assert(family < 7 && mixed < 2 && mode < 3 && unsat < 2 && variant < 2);
  assert(family != 6 || mixed);
  Proof proof;
  assert(proof.open(argv[7], mode == 2 ? ProofFormat::Lrat :
                   mode == 1 ? ProofFormat::DratBinary : ProofFormat::DratAscii));
  Kernel k;
  kernel_reserve_vars(k, 12);
  kernel_materialize_cold_state(k);
  ScopedHeuristics h(k);
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  std::vector<std::vector<int>> input = {
    {-1,2}, {1,-2}, {-3,4}, {-3,5}, {3,-4,-5},
    {1,3,6}, {-1,3,6}, {1,4,5,6}, {1,4,6},
    {1,8}, {1,8}, {7,8,9}, {9}, {11,12}, {11,-12}, {-11,12}};
  if (unsat) input.push_back({-11,-12});
  if (variant)
    for (auto &clause : input) {
      for (int &lit : clause) lit = -lit;
      std::reverse(clause.begin(), clause.end());
    }
  for (const auto &clause : input) {
    std::vector<Lit> lits;
    for (int lit : clause) lits.push_back(internalize(lit));
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 1) {
      k.unit_ids[var_of(lits[0])] = id;
      kernel_assign_unit(k, lits[0]);
    } else if (lits.size() == 2) kernel_add_binary_clause(k, lits[0], lits[1], id);
    else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  assert(propagate_apply(k) == ApplyResult::Ok);
  OccIndex occ;
  kernel_flush_large_watches(k);
  kernel_dense_connect(k, occ, mixed);
  const int rule = tactix_rule_id(names[family]);
  assert(rule >= 0);
  const auto &vt = tactix_rules()[rule];
  assert(vt.guard(k));
  const auto result = vt.apply(k);
  assert(k.dense == &occ);
  kernel_collect_garbage(k);
  kernel_dense_disconnect(k);
  kernel_rebuild_large_watches(k, false);
  kernel_check_invariants(k);
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
  std::ofstream cnf(argv[6]);
  cnf << "p cnf 12 " << input.size() << '\n';
  for (const auto &clause : input) {
    for (int lit : clause) cnf << lit << ' ';
    cnf << "0\n";
  }
  std::printf("family=%s mixed=%u mode=%u unsat=%u variant=%u apply=%u PASS\n",
              names[family],mixed,mode,unsat,variant,unsigned(result));
}

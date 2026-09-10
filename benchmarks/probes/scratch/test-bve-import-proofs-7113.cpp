// Nontrivial one-sided definition proof: lifted learned clauses must justify x.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 8);
  const unsigned pad = std::atoi(argv[1]), copies = std::atoi(argv[2]);
  const bool reverse = std::atoi(argv[3]), dense = std::atoi(argv[4]), lrat = std::atoi(argv[5]);
  assert(pad <= 2 && copies >= 1 && copies <= 2);
  Kernel k; kernel_reserve_vars(k, 4 + pad);
  k.bve_gate_mask = BVE_GATE_DEFINITION; k.bve_backward = false;
  std::vector<std::vector<Lit>> rows;
  for (unsigned i = 0; i < pad; ++i) rows.push_back({make_lit(4+i, true)});
  for (unsigned copy = 0; copy < copies; ++copy) {
    for (int a : {2, -2}) for (int b : {3, -3}) {
      std::vector<Lit> clause{internalize(1), internalize(a), internalize(b)};
      if (reverse) std::reverse(clause.begin(), clause.end());
      rows.push_back(clause);
    }
    std::vector<Lit> negative{internalize(-1), internalize(4)};
    for (unsigned i = 0; i < pad; ++i) negative.push_back(make_lit(4+i, false));
    if (reverse) std::reverse(negative.begin(), negative.end());
    rows.push_back(negative);
  }
  // This original unit is applied after the BVE invocation. Even with it,
  // the original CNF has no unit-propagation refutation of the a,b gadget.
  rows.push_back({internalize(-4)});
  FILE *cnf = std::fopen(argv[6], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf %u %zu\n", k.nvars, rows.size());
  for (const auto &row : rows) {
    for (Lit lit : row) std::fprintf(cnf, "%d ", externalize(lit));
    std::fputs("0\n", cnf);
    const auto id = ++k.next_clause_id;
    if (row.size() == 1) k.unit_ids[var_of(row[0])] = id;
    else if (row.size() == 2) kernel_add_binary_clause(k, row[0], row[1], id);
    else kernel_add_large_clause(k, row.data(), row.size(), false, 0, id);
  }
  std::fclose(cnf);
  for (unsigned i = 0; i < pad; ++i) kernel_assign_unit(k, make_lit(4+i, true));
  Proof proof; assert(proof.open(argv[7], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  OccIndex occ;
  if (dense) kernel_dense_connect(k, occ, true);
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(k.value(internalize(1)) > 0 && k.stats.bve_definition_units == 1);
  assert(k.stats.bve_definitions_checked == 1 && k.stats.bve_eliminated == 0);
  if (dense) kernel_dense_disconnect(k);
  kernel_assign_unit(k, internalize(-4));
  assert(propagate_apply(k) == ApplyResult::Unsat);
  proof.flush(); k.proof = nullptr;
  std::printf("BVE definition pad=%u copies=%u reverse=%d dense=%d lrat=%d ticks=%llu units=%llu\n",
      pad, copies, reverse, dense, lrat, (unsigned long long)k.stats.bve_definition_ticks,
      (unsigned long long)k.stats.bve_definition_units);
}

// Fixed actual BVE proofs: arena growth before witness reads, filtered roots,
// physical duplicates, literal order and sparse/dense public application.
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
  Kernel k; kernel_reserve_vars(k, 6 + pad);
  k.bve_gate_mask = 0; k.bve_backward = false;
  std::vector<std::vector<Lit>> rows;
  for (unsigned i = 0; i < pad; ++i) rows.push_back({make_lit(6+i, true)});
  for (unsigned copy = 0; copy < copies; ++copy)
    for (auto clause : {std::vector<Lit>{internalize(1), internalize(2), internalize(3)},
                       std::vector<Lit>{internalize(-1), internalize(4), internalize(5)}}) {
      for (unsigned i = 0; i < pad; ++i) clause.push_back(make_lit(6+i, false));
      if (reverse) std::reverse(clause.begin(), clause.end());
      rows.push_back(clause);
    }
  uint64_t reasons[4][2] = {};
  // No input unit fixes an antecedent literal. The two clauses for each
  // negative literal imply it, but ordinary unit propagation cannot see that.
  for (int i = 2; i <= 5; ++i) {
    rows.push_back({internalize(6), internalize(-i)});
    rows.push_back({internalize(-6), internalize(-i)});
  }
  FILE *cnf = std::fopen(argv[6], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf %u %zu\n", k.nvars, rows.size());
  for (const auto &row : rows) {
    for (Lit lit : row) std::fprintf(cnf, "%d ", externalize(lit));
    std::fputs("0\n", cnf);
    const auto id = ++k.next_clause_id;
    if (row.size() == 1) k.unit_ids[var_of(row[0])] = id;
    else if (row.size() == 2) {
      kernel_add_binary_clause(k, row[0], row[1], id);
      reasons[var_of(row[1])-1][negated(row[0])] = id;
    } else kernel_add_large_clause(k, row.data(), row.size(), false, 0, id);
  }
  std::fclose(cnf);
  for (unsigned i = 0; i < pad; ++i) kernel_assign_unit(k, make_lit(6+i, true));
  Proof proof; assert(proof.open(argv[7], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  k.arena.shrink_to_fit();
  const auto capacity = k.arena.capacity();
  OccIndex occ;
  if (dense) kernel_dense_connect(k, occ, true);
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(k.arena.capacity() > capacity && k.stats.bve_eliminated == 1);
  assert(k.stats.bve_resolutions == copies * copies);
  if (dense) kernel_dense_disconnect(k);
  // Derive units through their real two-clause RUP chains after BVE.
  for (int i = 2; i <= 5; ++i) {
    const Lit unit = internalize(-i);
    const uint64_t id = ++k.next_clause_id;
    proof.add_rup(id, &unit, 1, reasons[i-2], 2);
    k.unit_ids[var_of(unit)] = id;
    kernel_assign_unit(k, unit);
  }
  assert(propagate_apply(k) == ApplyResult::Unsat);
  proof.flush(); k.proof = nullptr;
  std::printf("BVE views pad=%u copies=%u reverse=%d dense=%d lrat=%d resolutions=%llu eliminated=%llu\n",
      pad, copies, reverse, dense, lrat, (unsigned long long)k.stats.bve_resolutions,
      (unsigned long long)k.stats.bve_eliminated);
}

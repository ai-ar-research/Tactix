// UNSAT witnesses whose final root conflict uses the extracted XOR equivalence.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 8);
  const unsigned padding = std::atoi(argv[1]), parity = std::atoi(argv[2]);
  const bool reverse = std::atoi(argv[3]);
  const unsigned copies = std::atoi(argv[4]); const bool lrat = std::atoi(argv[5]);
  assert(padding <= 2 && parity <= 1 && copies >= 1 && copies <= 2);
  Kernel k; kernel_reserve_vars(k,4+padding);
  std::vector<std::vector<Lit>> input;
  for (unsigned v = 0; v < padding; v++) input.push_back({make_lit(4+v,true)});
  for (unsigned o = 0; o < 2; o++)
    for (unsigned i = 0; i < 8; i++) {
      const unsigned signs = reverse ? 7-i : i;
      if (((signs ^ (signs>>1) ^ (signs>>2)) & 1) != parity) continue;
      std::vector<Lit> row{make_lit(0,signs&1),make_lit(1,(signs>>1)&1),make_lit(2+o,(signs>>2)&1)};
      for (unsigned v = 0; v < padding; v++) row.push_back(make_lit(4+v,false));
      if (reverse) std::reverse(row.begin(),row.end());
      for (unsigned copy = 0; copy < copies; copy++) input.push_back(row);
    }
  input.push_back({make_lit(2,false)}); input.push_back({make_lit(3,true)});
  FILE *cnf = std::fopen(argv[6],"wx"); assert(cnf);
  std::fprintf(cnf,"p cnf %u %zu\n",k.nvars,input.size());
  for (const auto &row : input) {
    for (Lit lit : row) std::fprintf(cnf,"%d ",kernel_externalize(k,lit));
    std::fprintf(cnf,"0\n");
    const auto id = ++k.next_clause_id;
    if (row.size() == 1) k.unit_ids[var_of(row[0])] = id;
    else kernel_add_large_clause(k,row.data(),row.size(),false,0,id);
  }
  std::fclose(cnf);
  for (unsigned v = 0; v < padding; v++) kernel_assign_unit(k,make_lit(4+v,true));
  Proof proof; assert(proof.open(argv[7],lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instance = 0; assert(rule.instances(k,&instance,1) == 1);
  assert(rule.apply_instance(k,instance) == ApplyResult::Ok);
  assert(k.stats.congruence_gates_xor && k.stats.congruence_merges && !k.stats.congruence_unjustified);
  // These original units were deliberately unassigned during extraction.
  // The newly proved output equivalence now gives an immediate root conflict.
  kernel_assign_unit(k,make_lit(2,false)); kernel_assign_unit(k,make_lit(3,true));
  assert(propagate_apply(k) == ApplyResult::Unsat && k.inconsistent);
  proof.flush(); k.proof = nullptr;
  std::printf("xor-proof %u %u %d %u %d: gates=%llu merges=%llu ticks=%llu P=%llu\n",
      padding,parity,reverse,copies,lrat,(unsigned long long)k.stats.congruence_gates_xor,
      (unsigned long long)k.stats.congruence_merges,(unsigned long long)k.stats.congruence_ticks,
      (unsigned long long)k.stats.propagations);
}

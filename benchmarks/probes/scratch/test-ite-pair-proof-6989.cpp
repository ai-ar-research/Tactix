// Final root conflict requires equivalence discovered from repeated ITE pairs.
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
  const unsigned padding = std::atoi(argv[1]), signs = std::atoi(argv[2]);
  const bool reverse = std::atoi(argv[3]);
  const unsigned copies = std::atoi(argv[4]); const bool lrat = std::atoi(argv[5]);
  assert(padding <= 2 && signs < 8 && copies >= 1 && copies <= 2);
  constexpr unsigned groups = 8, a = 24, b = 25;
  Kernel k; kernel_reserve_vars(k, 26+padding);
  std::vector<std::vector<Lit>> input;
  for (unsigned v = 0; v < padding; v++) input.push_back({make_lit(26+v,true)});
  std::vector<std::vector<Lit>> gates;
  for (unsigned o : {a,b}) for (unsigned i = 0; i < groups; i++) {
    const Lit c = make_lit(3*i,signs&1), t = make_lit(3*i+1,signs&2), e = make_lit(3*i+2,signs&4);
    const Lit out = make_lit(o,false);
    std::vector<Lit> rows[] = {{c,negate(e),out},{c,e,negate(out)},
                              {negate(c),negate(t),out},{negate(c),t,negate(out)}};
    for (auto row : rows) {
      for (unsigned v = 0; v < padding; v++) row.push_back(make_lit(26+v,false));
      if (reverse) std::reverse(row.begin(),row.end());
      for (unsigned copy = 0; copy < copies; copy++) gates.push_back(row);
    }
  }
  if (reverse) std::reverse(gates.begin(),gates.end());
  input.insert(input.end(),gates.begin(),gates.end());
  input.push_back({make_lit(a,false)}); input.push_back({make_lit(b,true)});
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
  for (unsigned v = 0; v < padding; v++) kernel_assign_unit(k,make_lit(26+v,true));
  Proof proof; assert(proof.open(argv[7],lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instance = 0; assert(rule.instances(k,&instance,1) == 1);
  assert(rule.apply_instance(k,instance) == ApplyResult::Ok);
  assert(k.stats.congruence_gates_ite >= 16 && k.stats.congruence_merges && !k.stats.congruence_unjustified);
  // Opposite original output units were unassigned during extraction. With
  // no assigned condition/branch, original gate clauses cannot propagate a
  // conflict from these alone; the newly derived output equivalence can.
  kernel_assign_unit(k,make_lit(a,false)); kernel_assign_unit(k,make_lit(b,true));
  assert(propagate_apply(k) == ApplyResult::Unsat && k.inconsistent);
  proof.flush(); k.proof = nullptr;
  std::printf("ite-proof %u %u %d %u %d: gates=%llu merges=%llu ticks=%llu P=%llu\n",
      padding,signs,reverse,copies,lrat,(unsigned long long)k.stats.congruence_gates_ite,
      (unsigned long long)k.stats.congruence_merges,(unsigned long long)k.stats.congruence_ticks,
      (unsigned long long)k.stats.propagations);
}

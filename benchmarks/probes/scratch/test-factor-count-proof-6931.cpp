// Fixed BVA certificates whose final conflict traverses generated clauses.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/factor/factor.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tactix;

int main(int argc, char **argv) {
  assert(argc == 9);
  const unsigned rows = std::atoi(argv[1]), columns = std::atoi(argv[2]);
  const unsigned width = std::atoi(argv[3]);
  const bool reverse = std::atoi(argv[4]), limited = std::atoi(argv[5]);
  const bool lrat = std::atoi(argv[6]);
  assert(rows >= 3 && columns >= 3 && (width == 1 || width == 2 || width == 4));
  Kernel k;
  ScopedHeuristics heuristics(k);
  const unsigned original = rows + columns*width;
  kernel_reserve_vars(k, original);
  k.factor_size = width+1;
  k.work_budget = limited ? 18 : 0;
  FILE *cnf = std::fopen(argv[7], "wx"); assert(cnf);
  std::fprintf(cnf,"p cnf %u %u\n",original,rows*columns+rows+width);
  for (unsigned i = 0; i < rows; ++i)
    for (unsigned j = 0; j < columns; ++j) {
      const unsigned row = reverse ? rows-1-i : i;
      const unsigned col = reverse ? columns-1-j : j;
      std::vector<Lit> clause{make_lit(row,false)};
      for (unsigned r = 0; r < width; ++r)
        clause.push_back(make_lit(rows+col*width+r,r&1));
      if (reverse) std::reverse(clause.begin(),clause.end());
      for (Lit lit : clause) std::fprintf(cnf,"%d ",kernel_externalize(k,lit));
      std::fprintf(cnf,"0\n");
      const uint64_t id = ++k.next_clause_id;
      if (width == 1) kernel_add_binary_clause(k,clause[0],clause[1],id);
      else kernel_add_large_clause(k,clause.data(),clause.size(),false,0,id);
    }
  std::vector<Lit> units;
  for (unsigned row = 0; row < rows; ++row) units.push_back(make_lit(row,true));
  for (unsigned r = 0; r < width; ++r) units.push_back(make_lit(rows+r,!(r&1)));
  for (Lit lit : units) {
    std::fprintf(cnf,"%d 0\n",kernel_externalize(k,lit));
    k.unit_ids[var_of(lit)] = ++k.next_clause_id;
  }
  std::fclose(cnf);
  Proof proof; assert(proof.open(argv[8],lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  const auto &rule = tactix_rules()[tactix_rule_id("Factor")];
  const ApplyResult result = rule.apply_instance(k,lit_instance(k,make_lit(0,false)));
  assert(result == ApplyResult::Ok || result == ApplyResult::NoOp);
  if (!limited) assert(k.stats.factorings == 1 && k.nvars == original+1);
  // Input units exist in the original CNF, but are assigned only after BVA.
  // Dividers then force x, and its first quotient conflicts with these roots.
  for (Lit lit : units) kernel_assign_unit(k,lit);
  assert(propagate_apply(k) == ApplyResult::Unsat && k.inconsistent);
  assert(kernel_num_output_vars(k) == original);
  k.proof = nullptr; proof.flush();
  std::printf("factor-proof %u %u %u %d %d %d: factors=%llu ticks=%llu P=%llu nvars=%u\n",
      rows,columns,width,reverse,limited,lrat,
      (unsigned long long)k.stats.factorings,(unsigned long long)k.stats.factor_ticks,
      (unsigned long long)k.stats.propagations,k.nvars);
}

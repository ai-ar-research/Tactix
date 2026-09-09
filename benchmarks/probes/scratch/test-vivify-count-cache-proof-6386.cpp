// Fixed canonical schedule proof fixture; benchmark invocations use SLURM.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/vivify/vivify.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 7);
  const unsigned scope = std::atoi(argv[1]);
  const bool reuse = std::atoi(argv[2]), flush = std::atoi(argv[3]), lrat = std::atoi(argv[4]);
  std::vector<std::vector<int>> clauses = {
      {1,2,3}, {1,2,3,4}, {3,4,5}, {3,4,5}, {3,4,5,6},
      {4,5,6,7}, {5,6,7,8}, {2,4,6,8}, {1,3,5,7}, {2,3,5,8}, {1,2}};
  for (int p = 0; p < 4; ++p) clauses.push_back({9+3*p,10+3*p,11+3*p});
  for (int h = 0; h < 3; ++h) for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b)
    clauses.push_back({-(9+3*a+h),-(9+3*b+h)});
  FILE *cnf = std::fopen(argv[5], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf 20 %zu\n", clauses.size());
  for (const auto &clause : clauses) {
    for (int lit : clause) std::fprintf(cnf, "%d ", lit);
    std::fputs("0\n", cnf);
  }
  std::fclose(cnf);
  Kernel k;
  Proof proof; assert(proof.open(argv[6], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  FILE *input = std::fopen(argv[5], "r"); assert(input);
  assert(!parse_dimacs(input, k)); std::fclose(input);
  // Add proof-backed duplicates into every redundant band, so all six
  // requested scopes actually sort candidates before the tiny final solve.
  const Ref original_words = k.arena.size();
  for (Ref ref = 0; ref < original_words;) {
    const Clause *c = clause_at(k.arena, ref);
    const Ref next = ref + clause_actual_words(c);
    const uint64_t antecedent = c->id();
    const std::vector<Lit> lits(c->begin(), c->end());
    for (unsigned glue : {2u,5u,9u}) {
      const uint64_t id = ++k.next_clause_id;
      proof.add_rup(id,lits.data(),lits.size(),&antecedent,1);
      kernel_add_large_clause(k,lits.data(),lits.size(),true,glue,id);
    }
    ref = next;
  }
  k.vivify_scope = scope; k.vivify_reuse = reuse; k.vivify_flush = flush;
  unsigned index = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    c->set_vivify(index++ % 2);
    ref += clause_actual_words(c);
  }
  std::vector<Ref> order; vivify_candidates(k, order);
  const size_t scheduled = order.size();
  assert(scheduled > 1);
  for (uint64_t budget : {UINT64_C(1), UINT64_C(0)}) {
    k.work_budget = budget;
    const auto result = vivify_apply(k);
    assert(result == ApplyResult::Ok || result == ApplyResult::NoOp || result == ApplyResult::Unsat);
    if (result == ApplyResult::Unsat) break;
  }
  k.work_budget = 0;
  assert(default_solve(k) == 20);
  k.proof = nullptr; proof.flush();
  std::printf("PASS: canonical scope=%u reuse=%d flush=%d scheduled=%zu final UNSAT\n", scope,reuse,flush,scheduled);
}

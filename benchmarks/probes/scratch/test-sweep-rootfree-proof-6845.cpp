// Fixed proof-emission control: the final contradiction requires Sweep's
// non-RUP equivalence, not checker-only contradictory units.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/sweep/sweep.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 7);
  const unsigned roots = std::atoi(argv[1]), position = std::atoi(argv[2]);
  const bool strip = std::atoi(argv[3]), lrat = std::atoi(argv[4]);
  assert(roots < 3 && position < 5);
  std::vector<std::vector<int>> clauses;
  for (int z : {-3,3}) for (int w : {-4,4}) for (bool reverse : {false,true}) {
    std::vector<int> c = {reverse ? 1 : -1, reverse ? -2 : 2, z, w};
    if (roots == 1 || (roots == 2 && clauses.size() % 2)) c.insert(c.begin()+position, 5);
    clauses.push_back(c);
  }
  FILE *cnf = std::fopen(argv[5], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf 5 %u\n", 10u + bool(roots));
  for (const auto &clause : clauses) {
    for (int l : clause) std::fprintf(cnf, "%d ", l);
    std::fputs("0\n", cnf);
  }
  if (roots) std::fputs("-5 0\n", cnf);
  // Opposing assumptions are not themselves contradictory. The eight-clause
  // environment needs decisions to refute them before Sweep adds its lemma.
  std::fputs("1 0\n-2 0\n", cnf); std::fclose(cnf);
  Kernel k; kernel_reserve_vars(k, 5);
  for (const auto &clause : clauses) {
    std::vector<Lit> lits;
    for (int l : clause) lits.push_back(internalize(l));
    kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, ++k.next_clause_id);
  }
  if (roots) {
    k.unit_ids[4] = ++k.next_clause_id;
    kernel_assign_unit(k, internalize(-5));
  }
  const uint64_t positive = ++k.next_clause_id, negative = ++k.next_clause_id;
  Proof proof; assert(proof.open(argv[6], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  SweepLimits limits; limits.strip_false = strip;
  assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);
  assert(k.stats.sweep_general > 0);
  if (roots) assert(k.stats.sweep_env_false > 0);
  if (roots && strip) assert(k.stats.sweep_env_stripped > 0);
  const Lit a = internalize(-1), b = internalize(2);
  assert(kernel_binary_live(k, a, b));
  const uint64_t bridge = kernel_binary_id(k, a, b); assert(bridge > negative);
  const uint64_t chain[] = {positive, negative, bridge};
  proof.add_empty(++k.next_clause_id, chain, 3);
  k.proof = nullptr; proof.flush();
  std::printf("roots=%u position=%u strip=%d lrat=%d general=%llu false=%llu stripped=%llu bridge=%llu\n",
      roots, position, strip, lrat, (unsigned long long)k.stats.sweep_general,
      (unsigned long long)k.stats.sweep_env_false, (unsigned long long)k.stats.sweep_env_stripped,
      (unsigned long long)bridge);
  return 0;
}

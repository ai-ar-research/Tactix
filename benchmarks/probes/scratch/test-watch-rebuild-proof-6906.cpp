// The emitted contradiction must traverse a reconnected large clause.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;
int main(int argc, char **argv) {
  assert(argc == 8);
  const unsigned mode = std::atoi(argv[1]);
  const bool before = std::atoi(argv[2]), flush = std::atoi(argv[3]);
  const bool lrat = std::atoi(argv[4]), reversed = std::atoi(argv[5]);
  assert(mode < 4);
  FILE *cnf = std::fopen(argv[6],"wx"); assert(cnf);
  std::fprintf(cnf,"p cnf 6 6\n%s\n1 0\n-2 0\n-3 0\n4 5 0\n1 4 6 0\n",
               reversed ? "3 2 -1 0" : "-1 2 3 0");
  std::fclose(cnf);
  Kernel k; kernel_reserve_vars(k,6);
  const Lit a[] = {internalize(reversed ? 3 : -1),internalize(2),
                   internalize(reversed ? -1 : 3)};
  kernel_add_large_clause(k,a,3,false,0,1);
  kernel_add_binary_clause(k,internalize(4),internalize(5),5);
  const Lit satisfied[] = {internalize(1),internalize(4),internalize(6)};
  kernel_add_large_clause(k,satisfied,3,false,0,6);
  k.next_clause_id = 6;
  k.unit_ids[0] = 2; k.unit_ids[1] = 3; k.unit_ids[2] = 4;
  Proof proof; assert(proof.open(argv[7],lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  k.proof = &proof;
  const auto units = [&]() {
    kernel_assign_unit(k,internalize(1));
    kernel_assign_unit(k,internalize(-2));
    kernel_assign_unit(k,internalize(-3));
  };
  if (before) units();
  OccIndex dense;
  if (mode == 3) kernel_dense_connect(k,dense,true);
  if (mode == 1 || mode == 3) kernel_drop_large_watches(k);
  if (mode == 2) kernel_flush_large_watches(k);
  for (WatchHead &h : k.watches.heads)
    if (!h.has_large()) h.set_binaries_first(false);
  kernel_rebuild_large_watches(k,flush);
  assert(k.large_watches_connected);
  if (!before) units();
  assert(propagate_apply(k) == ApplyResult::Unsat);
  assert(k.conflict_ref != INVALID_REF && k.inconsistent);
  if (mode == 3) kernel_dense_disconnect(k);
  k.proof = nullptr; proof.flush();
  std::printf("rebuild mode=%u before=%d flush=%d lrat=%d reversed=%d C=%llu P=%llu ticks=%llu\n",
      mode,before,flush,lrat,reversed,(unsigned long long)k.stats.conflicts,
      (unsigned long long)k.stats.propagations,(unsigned long long)k.stats.ticks);
  return 0;
}

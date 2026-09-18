#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tactix;

static unsigned fixture(bool reverse,bool fixed,bool fold) {
  Kernel k;
  kernel_reserve_vars(k,8);
  std::vector<Var> activation={0,1,2,3,4,5,6,7};
  if(reverse)std::reverse(activation.begin(),activation.end());
  for(Var v:activation)kernel_record_import(k,v);
  {
    ScopedHeuristics h(k);
    heuristics_seed_activation_order(k,activation);
    if(fixed) {
      for(Var v:{1u,3u,5u})kernel_assign_unit(k,make_lit(v,false));
      heuristics_consume_root_assignments(k,0);
      assert(kernel_compact_variables(k,fold));
      heuristics_compact_random_order_fixed(k);
    }
  }
  const auto order=k.compact_import_order;
  const auto positions=k.compact_import_position;
  unsigned missing=0;
  for(unsigned repeat=0;repeat<3;repeat++) {
    ScopedHeuristics h(k);
    missing+=k.compact_import_order!=order;
    missing+=k.compact_import_position!=positions;
    // A new guidance owner rebuilds its rankings, but fixed identities must
    // remain inactive and the kernel's compact index image is not H-owned.
    for(Var v=0;v<k.nvars;v++)
      if(kernel_root_value(k,make_lit(v,false)))assert(!heuristics_queue_contains(k,v));
    if(!missing)kernel_check_invariants(k);
  }
  if(!missing) {
    ScopedHeuristics h(k);
    const Var extension=kernel_new_extension_vars(k,2);
    assert(k.compact_import_order.size()==order.size()+2);
    assert(k.compact_import_order[order.size()]==extension);
    assert(k.compact_import_order[order.size()+1]==extension+1);
    assert(k.compact_import_position[extension]==order.size());
    assert(k.compact_import_position[extension+1]==order.size()+1);
    kernel_check_invariants(k);
  }
  return missing;
}

static void residual_handoff_uses_live_unsatisfied_formula() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  for (Var v = 0; v < k.nvars; ++v)
    kernel_record_import(k, v);
  // Original activation order is intentionally unrelated to the residual.
  k.input_activation_order = {7, 6, 5, 4, 3, 2, 1, 0};

  const Lit satisfied[] = {make_lit(6, false), make_lit(5, false),
                           make_lit(4, false)};
  const Lit first[] = {make_lit(2, false), make_lit(0, false),
                       make_lit(3, false)};
  kernel_add_large_clause(k, satisfied, 3, false, 0, 10);
  kernel_add_large_clause(k, first, 3, false, 0, 20);
  // The mirrored binary has no stored direction. Prior activation ranks put
  // variable 7 before variable 1 inside its later clause.
  kernel_add_binary_clause(k, make_lit(1, false), make_lit(7, false), 30);
  kernel_assign_unit(k, make_lit(6, false));
  assert(propagate_apply(k) == ApplyResult::Ok);

  heuristics_prepare_residual_activation_order(k);
  const std::vector<Var> expected = {2, 0, 3, 7, 1, 6};
  assert(k.input_activation_order == expected);

  ScopedHeuristics h(k);
  heuristics_seed_residual_import_order(k, expected);
  assert(k.compact_import_order == expected);
  for (Var position = 0; position < expected.size(); ++position)
    assert(k.compact_import_position[expected[position]] == position);
  assert(!heuristics_queue_contains(k, 6));
  assert(heuristics_queue_contains(k, 2));
  assert(heuristics_queue_contains(k, 7));
  // Stable kernel identities absent from the residual stay at the least
  // preferred end; the next decision comes from the residual suffix.
  assert(heuristics_queue_next_unassigned(k) == 1);
  kernel_check_invariants(k);
}
int main(int argc,char **argv) {
  const bool negative=argc==2&&!std::strcmp(argv[1],"--expect-missing");
  assert(argc==1||negative);
  unsigned missing=0,cases=0;
  for(bool reverse:{false,true})for(bool fixed:{false,true})for(bool fold:{false,true}) {
    missing+=fixture(reverse,fixed,fold);++cases;
  }
  std::printf("guidance reattach: cases=%u changed-images=%u\n",cases,missing);
  assert(negative?missing==48:missing==0);
  if (!negative)
    residual_handoff_uses_live_unsatisfied_formula();
}

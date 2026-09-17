// Newly published representative binaries can re-arm a previously drained
// root. Forward cleanup must propagate them before dropping assigned pairs.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

static bool model(const Kernel &k, unsigned bits) {
  const auto value=[&](Lit l) {
    const bool v=(bits >> var_of(l)) & 1u;
    return (l & 1u) ? !v : v;
  };
  for(Var v=0;v<k.nvars;++v) {
    const Lit l=make_lit(v,false);
    if(k.value(l) && value(l)!=(k.value(l)>0)) return false;
  }
  for(Lit l=0;l<2*k.nvars;++l)
    for(const WatchEntry w:k.watches.entries(l))
      if(w.binary() && !value(l) && !value(w.other())) return false;
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
    if(c->garbage()) continue;
    bool satisfied=false;for(Lit l:*c) satisfied|=value(l);
    if(!satisfied) return false;
  }
  return true;
}

int main() {
  for(bool preserve:{false,true}) for(bool dense:{false,true}) {
    Kernel k;kernel_reserve_vars(k,13);
    const std::vector<std::vector<int>> clauses={
      {1,7,12},{1,-7,-12},{-1,-5,12},{-1,5,-12},
      {1,7,13},{1,-7,-13},{-1,-5,13},{-1,5,-13},
      {-12,4},{-4,-12}};
    for(const auto& c:clauses) {
      std::vector<Lit> lits;for(int l:c) lits.push_back(internalize(l));
      const uint64_t id=++k.next_clause_id;
      if(lits.size()==2) kernel_add_binary_clause(k,lits[0],lits[1],id);
      else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,id);
    }
    std::vector<bool> original(1u<<13);unsigned count=0;
    for(unsigned bits=0;bits<original.size();++bits) count+=(original[bits]=model(k,bits));
    assert(count);
    k.congruence_parameters.binary_watch_order=preserve
        ? strategy::CongruenceBinaryWatchOrder::PreserveExisting
        : strategy::CongruenceBinaryWatchOrder::Canonical;
    OccIndex occurrences;if(dense) kernel_dense_connect(k,occurrences);
    assert(congruence_guard(k));
    assert(congruence_apply(k)==ApplyResult::Ok);
    assert(k.stats.congruence_merges && k.stats.propagation_rearms);
    assert(kernel_propagation_fixpoint(k));
    assert(k.value(internalize(-12))>0 && k.value(internalize(-13))>0);
    for(unsigned bits=0;bits<original.size();++bits) assert(model(k,bits)==original[bits]);
    if(dense) kernel_dense_disconnect(k);
    kernel_check_invariants(k);
  }
  std::puts("Congruence forward cleanup preserves root closure and every model");
}

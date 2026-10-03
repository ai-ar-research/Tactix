#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix;
static void xor3(Kernel &k, int a, int b, int c) {
  const int scope[]={a,b,c};
  for(unsigned mask=0;mask<8;++mask) {
    if(__builtin_popcount(mask)%2!=1) continue;
    Lit lits[3];
    for(unsigned i=0;i<3;++i) lits[i]=internalize(mask&(1u<<i)?-scope[i]:scope[i]);
    kernel_add_large_clause(k,lits,3,false,0,++k.next_clause_id);
  }
}
static bool binary(const Kernel &k,int a,int b) {
  for(const WatchEntry w:k.watches.entries(internalize(a)))
    if(w.binary() && w.other()==internalize(b))return true;
  return false;
}
static void run(bool matched) {
  Kernel k; kernel_reserve_vars(k,7);
  if(matched) { xor3(k,1,3,4);xor3(k,2,3,4); }
  else { xor3(k,1,3,4);xor3(k,4,5,6);xor3(k,6,7,3);xor3(k,2,5,7); }
  assert(congruence_guard(k));
  const auto result=congruence_apply(k);
  assert(result==ApplyResult::Ok || result==ApplyResult::NoOp);
  const bool equivalent=binary(k,-1,2)&&binary(k,1,-2);
  std::printf("{\"case\":\"%s\",\"xor_gates\":%llu,\"merges\":%llu,\"equivalence_added\":%s,\"inconsistent\":%s}\n",matched?"identical_inputs":"four_equation_cancellation",(unsigned long long)k.stats.congruence_gates_xor,(unsigned long long)k.stats.congruence_merges,equivalent?"true":"false",k.inconsistent?"true":"false");
  assert(!k.inconsistent);
  if(matched) assert(equivalent);
}
int main(){run(true);run(false);}

// Fixed full-rule fixture linked with the exact old/new solver objects.
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include <cstdio>
using namespace tactix;
static Ref add3(Kernel &k,int a,int b,int c,bool red=false) {
  const Lit ls[3]={internalize(a),internalize(b),internalize(c)};
  return kernel_add_large_clause(k,ls,3,red,red?1:0,++k.next_clause_id);
}
static void gate(Kernel &k,int out) {
  kernel_add_binary_clause(k,internalize(-out),internalize(1),++k.next_clause_id);
  kernel_add_binary_clause(k,internalize(-out),internalize(2),++k.next_clause_id);
  add3(k,out,-1,-2);
}
int main() {
  Kernel k;kernel_reserve_vars(k,7);gate(k,3);gate(k,4);
  const Ref before=add3(k,3,-4,5,true);
  add3(k,5,6,7);
  const Ref after=add3(k,3,-4,6,true);
  const auto result=congruence_apply(k);
  const bool prefix=clause_at(k.arena,before)->garbage();
  const bool suffix=clause_at(k.arena,after)->garbage();
  std::printf("tactix prefix_deleted=%d suffix_deleted=%d\n",prefix,suffix);
  return result==ApplyResult::Ok && prefix && !suffix ? 0 : 1;
}

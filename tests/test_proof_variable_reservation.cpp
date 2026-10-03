#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix;
static std::vector<Var> exercise(bool writing) {
  Kernel k;kernel_reserve_vars(k,3);
  Proof proof;
  if(writing) {assert(proof.open("/tmp/tactix-proof-reservation.dsr",ProofFormat::DsrAscii));k.proof=&proof;}
  const auto original_map=k.internal_to_external;
  const auto original_values=k.values;
  Var first=INVALID_VAR;
  assert(kernel_reserve_proof_variables(k,5,first));assert(first==3);
  assert(k.nvars==3&&k.internal_to_external==original_map&&k.values==original_values);
  assert(kernel_num_output_vars(k)==3);
  for(Var v=3;v<8;++v)assert(k.external_to_internal[v]==INVALID_VAR);
  std::vector<Var> order;kernel_vars_in_external_order(k,order);assert((order==std::vector<Var>{0,1,2}));
  assert(kernel_new_extension_vars(k,2)==3);
  assert(k.internal_to_external[3]==8&&k.internal_to_external[4]==9);
  assert(kernel_num_output_vars(k)==3);
  kernel_assign_unit(k,make_lit(0,false));
  k.var_status.assign(k.nvars,VarStatus::Active);
  k.var_status[2]=VarStatus::Eliminated;
  assert(kernel_compact_variables(k,true));
  assert(kernel_num_output_vars(k)==3);
  assert(k.external_to_internal[2]==INVALID_VAR);
  assert(kernel_reserve_proof_variables(k,2,first));assert(first==10);
  const Var fresh=kernel_new_extension_vars(k,1);assert(k.internal_to_external[fresh]==12);
  kernel_vars_in_external_order(k,order);assert(order.size()==k.nvars);
  const auto mapping=k.external_to_internal;
  first=123;
  assert(!kernel_reserve_proof_variables(k,UINT32_MAX,first));assert(first==123);
  assert(k.external_to_internal==mapping&&kernel_num_output_vars(k)==3);
  kernel_extend_model(k);assert(kernel_num_output_vars(k)==3);
  assert(k.model_values[make_lit(0,false)]==1);
  k.proof=nullptr;
  return mapping;
}
int main() {
  { Kernel k;kernel_reserve_vars(k,3);Var first=INVALID_VAR;
    assert(kernel_reserve_proof_variables(k,0,first));assert(first==3);
    assert(k.original_external_vars==UINT32_MAX&&k.external_to_internal.size()==3); }
  assert(exercise(false)==exercise(true));
  std::puts("test_proof_variable_reservation: all passed");
}

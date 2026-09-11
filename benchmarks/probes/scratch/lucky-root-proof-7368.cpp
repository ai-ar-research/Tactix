#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
using namespace tactix;
int main(int argc,char **argv) {
  assert(argc==7);
  Kernel k;Proof proof;
  assert(proof.open(argv[2],!std::strcmp(argv[3],"lrat")?ProofFormat::Lrat:ProofFormat::DratAscii));
  k.proof=&proof;proof.bind_variable_map(&k.internal_to_external);
  FILE *input=std::fopen(argv[1],"r");assert(input&&!parse_dimacs(input,k));std::fclose(input);
  std::optional<ScopedHeuristics> owner;
  const bool eager=!std::strcmp(argv[4],"eager");
  if(eager)owner.emplace(k);
  volatile sig_atomic_t stop=1;
  int result=default_solve(k,&stop);
  unsigned fixed=0,stale=0;
  for(Var v=0;v<k.nvars;v++)if(kernel_root_value(k,make_lit(v,false))) {
    ++fixed;if(k.h)stale+=heuristics_queue_contains(k,v);
  }
  assert(fixed>=static_cast<unsigned>(std::atoi(argv[6])));
  if(std::strcmp(argv[5],"parent"))assert(!stale);
  std::printf("c Lucky reached fixed=%u stale=%u eager=%u\n",fixed,stale,eager);
  if(result==0)result=default_solve(k);
  assert(result==10||result==20);proof.flush();
  if(result==10) {
    kernel_extend_model(k);std::puts("s SATISFIABLE");std::printf("v");
    for(Var v=0;v<kernel_num_output_vars(k);v++)std::printf(" %d",k.model_values[make_lit(v,false)]>0?int(v+1):-int(v+1));
    std::puts(" 0");
  } else std::puts("s UNSATISFIABLE");
  return result;
}

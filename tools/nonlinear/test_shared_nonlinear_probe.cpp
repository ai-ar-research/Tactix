#undef NDEBUG
#include "diagnostics/shared_nonlinear_probe.hpp"
#include "frontend/parse.hpp"
#include <cassert>
using namespace tactix;
namespace probe=tactix::shared_nonlinear_probe;
static void gate(std::vector<int32_t> &f,bool is_and,int o,int a,int b) {
  std::vector<std::vector<int32_t>> clauses;
  if(is_and)clauses={{-o,a},{-o,b},{o,-a,-b}};
  else clauses={{o,a,-b},{o,-a,b},{-o,a,b},{-o,-a,-b}};
  for(const auto &c:clauses){f.insert(f.end(),c.begin(),c.end());f.push_back(0);}
}
static void check(const char *arm,unsigned assignment,bool corrupt) {
  setenv("TACTIX_NONLINEAR_ARM",arm,1);probe::no_proof_authorized=true;
  Kernel k;std::vector<int32_t> f;
  gate(f,false,5,1,3);gate(f,false,6,2,4);gate(f,true,7,5,6);
  gate(f,false,8,1,4);gate(f,false,9,2,3);gate(f,true,10,8,9);
  ingest_dimacs(k,f);ScopedHeuristics h(k);assert(propagate_apply(k)==ApplyResult::Ok);
  const auto extent=k.external_to_internal.size();const auto oldn=k.nvars;
  assert(probe::run(k));
  if(!std::strcmp(arm,"baseline"))assert(k.nvars==oldn);
  else if(!std::strcmp(arm,"control")) {
    assert(k.nvars==oldn&&probe::session.groups==1&&probe::session.priority_count==1&&probe::session.priority_sum==2);
  } else {
    assert(k.nvars>oldn&&kernel_num_output_vars(k)==extent);
    assert(probe::session.groups==1&&probe::session.links==2&&probe::session.summaries.size()==1);
    assert(probe::session.priority_count==unsigned(!std::strcmp(arm,"priority")));
    for(auto ext:probe::session.summaries)assert(!kernel_var_retired(k,var_of(kernel_internalize(k,int32_t(ext)))));
  }
  bool values[11]{};for(unsigned j=1;j<=4;++j)values[j]=(assignment>>(j-1))&1u;
  values[5]=values[1]!=values[3];values[6]=values[2]!=values[4];values[7]=values[5]&&values[6];
  values[8]=values[1]!=values[4];values[9]=values[2]!=values[3];values[10]=values[8]&&values[9];
  if(corrupt)kernel_assign_unit(k,kernel_internalize(k,values[7] ? -7:7));
  for(int j=1;j<=4;++j)kernel_assign_unit(k,kernel_internalize(k,values[j] ? j:-j));
  auto result=propagate_apply(k);assert((result==ApplyResult::Unsat)==corrupt);
  if(!corrupt)for(int j=1;j<=10;++j)assert((k.value(kernel_internalize(k,j))>0)==values[j]);
#ifndef NDEBUG
  kernel_check_invariants(k);heuristics_check(k);
#endif
}
int main() {
  for(const char *arm:{"baseline","definitions","priority","control"})
    for(unsigned assignment=0;assignment<16;++assignment)for(bool corrupt:{false,true})check(arm,assignment,corrupt);
  unsetenv("TACTIX_NONLINEAR_ARM");
}

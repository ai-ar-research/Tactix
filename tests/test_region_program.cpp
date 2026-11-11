#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "strategy/region_program.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;
static int rule(const char *name) { return tactix_rule_id(name); }
static tactix_c_handle *fixture() {
  auto *s=tactix_init();
  for(int l:{1,2,0})tactix_add(s,l);
  assert(!tactix_regions_begin(s));return s;
}
static int region(tactix_c_handle *s) {
  int variables[16];int n=tactix_fresh_variable_start(s)-1;assert(n<=16);
  for(int i=0;i<n;++i)variables[i]=i+1;
  return tactix_region_add(s,variables,n);
}
static void local_cdcl() {
  auto *s=fixture();
  int a=tactix_define_extension(s,1,1,2),b=tactix_define_extension(s,1,2,1);
  int z=tactix_define_extension(s,3,a,b),r=region(s);assert(r==0);
  // The commuted-gate miter is false but not UP-refutable under z alone.
  assert(tactix_region_apply(s,rule("Probe"),z,r,0)==1);
  assert(tactix_region_apply(s,rule("Sweep"),z,r,4096)==0);
  assert(tactix_stat(s,TACTIX_STAT_REGION_UNITS)==1);
  assert(tactix_stat(s,TACTIX_STAT_REGION_LEMMAS)>1);
  assert(tactix_solve(s)==10);
  assert(tactix_value(s,1)>0||tactix_value(s,2)>0);
  assert(!tactix_regions_clear(s));tactix_release(s);
}
// A nonterminal query must retire its conditioned proof-only prefix before
// ordinary solving (and potentially witnessed rewrites) resumes.
static void transient_proof_lifetime() {
  struct Observer : PopulationObserver {
    bool enabled = false;
    int goal = 0, additions = 0, deletions = 0;
    void observe(const CheckerPopulationEvent &e) noexcept override {
      if (!enabled) return;
      if (e.kind == CheckerPopulationEventKind::Add) {
        bool conditioned = false;
        for (uint32_t i = 0; i < e.clause.size(); ++i)
          conditioned |= e.clause.at(i) == goal;
        assert(conditioned);
        ++additions;
      } else if (e.kind == CheckerPopulationEventKind::DeleteRequested)
        ++deletions;
    }
  } observer;
  auto *s = tactix_init();
  assert(!tactix_proof(s, "/dev/null", TACTIX_PROOF_PBP));
  for (int l : {1,2,0}) tactix_add(s,l);
  assert(tactix_status(s) == 0);
  auto &k = api_detail::kernel_for_fixed_testing(s);
  {
    ScopedPopulationObserverBinding binding(k, observer);
    assert(!tactix_regions_begin(s));
    const int a = tactix_define_extension(s,1,1,2);
    const int b = tactix_define_extension(s,1,2,1);
    const int z = tactix_define_extension(s,3,a,b), r = region(s);
    observer.goal = -z; observer.enabled = true;
    assert(!tactix_region_apply(s,rule("Sweep"),z,r,4096));
    observer.enabled = false;
    assert(observer.additions > 1);
    assert(observer.deletions == observer.additions - 1);
    assert(tactix_value(s,z) < 0);
    assert(!tactix_regions_clear(s));
  }
  assert(tactix_solve(s) == 10);
  tactix_release(s);
}
static void source_deletion(bool large, bool detached = false) {
  auto *s=fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
  int a=tactix_define_extension(s,1,1,2),b=tactix_define_extension(s,1,2,1);
  int z=tactix_define_extension(s,1,a,-b),r=region(s);
  PopulationObserver silent;
  std::unique_ptr<ScopedPopulationObserverBinding> binding;
  if (detached) binding=std::make_unique<ScopedPopulationObserverBinding>(k,silent);
  if(large) {
    bool removed=false;
    for(auto w:k.watches.entries(kernel_internalize(k,b)))if(!w.binary()) {
      Clause *c=clause_at(k.arena,w.ref());
      if(!c->garbage()&&c->size==3){kernel_delete_clause(k,c);removed=true;break;}
    }
    assert(removed);
  } else assert(kernel_delete_binary(k,kernel_internalize(k,-a),internalize(1)));
  binding.reset();
  // The capture missed deletion while detached, but restoring its pointer
  // cannot restore trust. The binding epoch authenticates the whole interval.
  if (detached) assert(!tactix_stat(s,TACTIX_STAT_REGION_INVALIDATED));
  assert(tactix_region_apply(s,rule("Sweep"),z,r,4096)==1);
  assert(tactix_stat(s,TACTIX_STAT_REGION_INVALIDATED)==1);
  assert(!tactix_stat(s,TACTIX_STAT_REGION_UNITS));
  assert(!tactix_decide(s,z));assert(tactix_solve(s)==10);tactix_release(s);
}
static void queue_and_guards() {
  auto *s=tactix_init();
  for(int l:{-2,-1,0,-2,1,0,3,4,0})tactix_add(s,l);
  assert(!tactix_regions_begin(s));assert(tactix_regions_begin(s)==-1);
  const int variables[]={2,1,2};int r=tactix_region_add(s,variables,3);assert(r==0);
  assert(tactix_region_queue(s,rule("BVE"),2,r,0)==-1);
  assert(tactix_region_queue(s,rule("Probe"),3,r,0)==-1);
  assert(tactix_region_queue(s,rule("Probe"),2,r,1)==-1);
  assert(tactix_region_queue(s,rule("Probe"),2,r,0)==0);
  assert(tactix_solve(s)==10&&tactix_stat(s,TACTIX_STAT_REGION_QUERIES)==1);
  assert(tactix_stat(s,TACTIX_STAT_REGION_UNITS)==1&&tactix_value(s,2)<0);
  assert(!tactix_regions_clear(s));tactix_release(s);
}
static void interruption() {
  Kernel k;kernel_reserve_vars(k,4);kernel_materialize_cold_state(k);
  kernel_add_binary_clause(k,internalize(-2),internalize(-1));
  kernel_add_binary_clause(k,internalize(-2),internalize(1));
  kernel_add_binary_clause(k,internalize(3),internalize(4));
  strategy::RegionProgram program(k);const int vars[]={1,2};
  int r=program.add_region(vars);assert(r==0);
  assert(program.enqueue({rule("Probe"),2,0,0}));
  volatile sig_atomic_t stop=1;DefaultPolicy policy;
  assert(policy.solve(k,&stop,&program)==0&&program.pending()==1);
  stop=0;assert(policy.solve(k,&stop,&program)==10&&program.pending()==0);
  assert(k.stats.region_units==1);
}
static void callback_boundary() {
  Kernel k;kernel_reserve_vars(k,6);kernel_materialize_cold_state(k);
  for(int v:{2,4}) {
    kernel_add_binary_clause(k,internalize(-v),internalize(v-1));
    kernel_add_binary_clause(k,internalize(-v),internalize(1-v));
  }
  kernel_add_binary_clause(k,internalize(5),internalize(6));
  strategy::RegionProgram program(k);const int vars[]={1,2,3,4};
  assert(program.add_region(vars)==0);
  for(int v:{2,4})assert(program.enqueue({rule("Probe"),v,0,0}));
  unsigned polls=0;
  const auto stop=[](void *p){return ++*static_cast<unsigned *>(p)==2;};
  assert(program.run(stop,&polls)==ApplyResult::Ok);
  assert(program.pending()==1&&k.stats.region_units==1&&k.stats.region_queries==1);
  assert(program.run()==ApplyResult::Ok&&program.pending()==0);
  assert(k.stats.region_units==2&&k.stats.region_queries==2);
}
// Optional integration mode writes a fixed non-UP UNSAT input and its whole
// proof. External checkers validate the local solver's intermediate clauses.
static void proof_fixture(const char *cnf,const char *pbp) {
  const int clauses[][4]={{-3,1,0},{-3,2,0},{3,-1,-2,0},
      {-4,2,0},{-4,1,0},{4,-2,-1,0},{5,3,-4,0},
      {5,-3,4,0},{-5,3,4,0},{-5,-3,-4,0},{5,0}};
  FILE *f=std::fopen(cnf,"w");assert(f);std::fprintf(f,"p cnf 5 11\n");
  auto *s=tactix_init();assert(!tactix_proof(s,pbp,TACTIX_PROOF_PBP));
  for(const auto &row:clauses){for(int l:row){std::fprintf(f,"%d ",l);tactix_add(s,l);if(!l)break;}std::fputc('\n',f);}std::fclose(f);
  assert(!tactix_regions_begin(s));
  int a=tactix_define_extension(s,1,1,2),b=tactix_define_extension(s,1,2,1);
  int z=tactix_define_extension(s,3,a,b),r=region(s);
  assert(!tactix_region_queue(s,rule("Sweep"),z,r,4096));
  assert(tactix_solve(s)==20&&tactix_stat(s,TACTIX_STAT_REGION_QUERIES)==1);
  tactix_release(s);
}
int main(int argc,char **argv) {
  if(argc==3){proof_fixture(argv[1],argv[2]);return 0;}
  assert(argc==1);local_cdcl();transient_proof_lifetime();source_deletion(false);source_deletion(true);
  source_deletion(false,true);source_deletion(true,true);
  queue_and_guards();interruption();callback_boundary();std::puts("region program PASS");
}

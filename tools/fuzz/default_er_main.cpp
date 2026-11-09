// Targeted qualification: public ER advice followed by ordinary DefaultPolicy.
#include "tactix.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdlib>

int main(int argc,char **argv) {
  uint64_t seed=1;
  const char *input=nullptr,*proof=nullptr;
  for(int i=1;i<argc;++i) {
    if(!std::strcmp(argv[i],"--pbp"))continue;
    if(!std::strncmp(argv[i],"--er-seed=",10)) {
      const char *p=argv[i]+10,*end=p+std::strlen(p);
      auto parsed=std::from_chars(p,end,seed);
      if(parsed.ec!=std::errc{}||parsed.ptr!=end)return 1;
    } else if(!input)input=argv[i];else if(!proof)proof=argv[i];else return 1;
  }
  if(!input)return 1;
  std::unique_ptr<FILE,decltype(&std::fclose)> f(std::fopen(input,"r"),std::fclose);
  if(!f)return 1;
  std::unique_ptr<tactix,decltype(&tactix_release)> owner(tactix_init(),tactix_release);
  auto *s=owner.get();
  if((proof&&tactix_proof(s,proof,TACTIX_PROOF_PBP))||tactix_read_dimacs(s,f.get()))return 1;
  f.reset();
  const int original=tactix_fresh_variable_start(s)-1;
  const auto random=[&]() {
    seed+=UINT64_C(0x9e3779b97f4a7c15);uint64_t x=seed;
    x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);
    x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);return x^(x>>31);
  };
  int propagation=-1;
  for(int id=0;id<tactix_rule_count();++id)
    if(!std::strcmp(tactix_rule_name(id),"Propagate"))propagation=id;
  if(propagation<0)return 1;
  const bool regions=tactix_status(s)==0&&tactix_regions_begin(s)==0;
  unsigned requested=0,declined=0,added=0,bumped=0,released=0,composed=0;
  std::vector<int> outputs;
  for(unsigned i=0;i<8&&tactix_status(s)==0;++i) {
    // Definitions require a propagated root. New clauses may expose roots.
    if(tactix_apply(s,propagation)==20||tactix_status(s)!=0)break;
    int candidates[32],count=tactix_decide_candidates(s,candidates,32);
    if(count<=0)break;
    int a=candidates[random()%count],b=candidates[random()%count];
    const bool composing=!outputs.empty()&&random()%2;
    if(composing)a=outputs.back();
    if(random()%2)a=-a;
    if(random()%2)b=-b;
    ++requested;
    int z=tactix_define_extension(s,1+int(random()%3),a,b);
    // The decision surface can include retired identities under discarded
    // guidance. ER correctly rejects those operands; retain that outcome.
    if(z<0){++declined;continue;}
    outputs.push_back(z);++added;if(composing)++composed;
    if(tactix_variable_retained(s,z)!=1)return 1;
    if(random()%2&&!tactix_bump_variable(s,z,1+unsigned(random()%4)))++bumped;
    if(random()%3==0){if(tactix_set_variable_retention(s,z,0))return 1;++released;}
  }
  unsigned region_requests=0,region_queued=0,region_declined=0,miter_definitions=0;
  int probe=-1,sweep=-1;
  for(int id=0;id<tactix_rule_count();++id) {
    if(!std::strcmp(tactix_rule_name(id),"Probe"))probe=id;
    if(!std::strcmp(tactix_rule_name(id),"Sweep"))sweep=id;
  }
  int miter=0;
  if(regions&&tactix_status(s)==0&&tactix_apply(s,propagation)!=20) {
    int candidates[16],count=tactix_decide_candidates(s,candidates,16);
    if(count>1) {
      int a=candidates[0],b=candidates[1];
      int u=tactix_define_extension(s,TACTIX_EXTENSION_AND,a,b);
      int v=tactix_define_extension(s,TACTIX_EXTENSION_AND,b,a);
      if(u>0){outputs.push_back(u);++miter_definitions;}
      if(v>0){outputs.push_back(v);++miter_definitions;}
      if(u>0&&v>0){miter=tactix_define_extension(s,TACTIX_EXTENSION_XOR,u,v);if(miter>0){outputs.push_back(miter);++miter_definitions;}}
    }
    int candidates2[32],count2=tactix_decide_candidates(s,candidates2,32);
    std::vector<int> pool=outputs;
    for(int i=0;i<count2;++i)pool.push_back(std::abs(candidates2[i]));
    std::sort(pool.begin(),pool.end());pool.erase(std::unique(pool.begin(),pool.end()),pool.end());
    if(!pool.empty()) {
      int full=tactix_region_add(s,pool.data(),int(pool.size()));
      const auto queue=[&](int rule,int literal,int region,uint64_t ticks) {
        ++region_requests;
        if(region>=0&&!tactix_region_queue(s,rule,literal,region,ticks))++region_queued;
        else ++region_declined;
      };
      if(miter>0)queue(sweep,miter,full,4096);
      for(unsigned i=0;i<12;++i) {
        int literal=pool[random()%pool.size()];std::vector<int> vertices{literal};
        for(int x:pool)if(random()%2)vertices.push_back(x);
        int region=random()%3?full:tactix_region_add(s,vertices.data(),int(vertices.size()));
        bool probing=random()%2;
        if(random()%2)literal=-literal;
        queue(probing?probe:sweep,literal,region,probing?0:1+random()%4096);
      }
      // A public source-weakening action between configuration and execution.
      if(random()%4==0&&tactix_apply(s,propagation)!=20) {
        (void)tactix_support(s,TACTIX_SUPPORT_SPARSE_COLLECT,0);
        (void)tactix_apply(s,propagation);
      }
    }
  }
  const int defined_extent=tactix_fresh_variable_start(s);
  const int result=tactix_solve(s);
  const uint64_t extension_decisions=tactix_stat(s,TACTIX_STAT_DECISIONS_EXTENSION);
  // With no later fresh identities, every solver-extension decision was on
  // one of this client's definitions. Otherwise leave attribution unknown.
  const uint64_t er_decisions=tactix_fresh_variable_start(s)==defined_extent
      ? extension_decisions:0;
  std::printf("c default-er requests=%u declined=%u definitions=%u composed=%u bumped=%u released=%u decisions=%llu extension-decisions=%llu er-decisions=%llu\n",
      requested,declined,added,composed,bumped,released,(unsigned long long)tactix_stat(s,TACTIX_STAT_DECISIONS),
      (unsigned long long)extension_decisions,(unsigned long long)er_decisions);
  std::printf("c default-er region-requests=%u region-queued=%u region-declined=%u miter-definitions=%u region-queries=%llu region-units=%llu region-lemmas=%llu region-budget=%llu region-invalidated=%llu\n",
      region_requests,region_queued,region_declined,miter_definitions,
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_QUERIES),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_UNITS),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_LEMMAS),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_BUDGET),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_INVALIDATED));
  if(result==10) {
    std::puts("s SATISFIABLE");std::printf("v");
    for(int v=1;v<=original;++v)std::printf(" %d",tactix_value(s,v)>0?v:-v);
    std::puts(" 0");
  } else std::puts(result==20?"s UNSATISFIABLE":"s UNKNOWN");
  return result==10||result==20?result:1;
}

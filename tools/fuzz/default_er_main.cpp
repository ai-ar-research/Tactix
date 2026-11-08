// Targeted qualification: public ER advice followed by ordinary DefaultPolicy.
#include "tactix.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <vector>

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
  FILE *f=std::fopen(input,"r");if(!f)return 1;
  auto *s=tactix_init();
  if((proof&&tactix_proof(s,proof,TACTIX_PROOF_PBP))||tactix_read_dimacs(s,f))return 1;
  std::fclose(f);
  const int original=tactix_fresh_variable_start(s)-1;
  const auto random=[&]() {
    seed+=UINT64_C(0x9e3779b97f4a7c15);uint64_t x=seed;
    x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);
    x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);return x^(x>>31);
  };
  unsigned added=0,bumped=0,released=0,composed=0;
  std::vector<int> outputs;
  for(unsigned i=0;i<8&&tactix_status(s)==0;++i) {
    int candidates[32],count=tactix_decide_candidates(s,candidates,32);
    if(count<=0)break;
    int a=candidates[random()%count],b=candidates[random()%count];
    if(!outputs.empty()&&random()%2){a=outputs.back();++composed;}
    if(random()%2)a=-a;
    if(random()%2)b=-b;
    int z=tactix_define_extension(s,1+int(random()%3),a,b);
    if(z<0)return 1;
    outputs.push_back(z);++added;
    if(tactix_variable_retained(s,z)!=1)return 1;
    if(random()%2&&!tactix_bump_variable(s,z,1+unsigned(random()%4)))++bumped;
    if(random()%3==0){if(tactix_set_variable_retention(s,z,0))return 1;++released;}
  }
  const int defined_extent=tactix_fresh_variable_start(s);
  const int result=tactix_solve(s);
  const uint64_t extension_decisions=tactix_stat(s,TACTIX_STAT_DECISIONS_EXTENSION);
  // With no later fresh identities, every solver-extension decision was on
  // one of this client's definitions. Otherwise leave attribution unknown.
  const uint64_t er_decisions=tactix_fresh_variable_start(s)==defined_extent
      ? extension_decisions:0;
  std::printf("c default-er definitions=%u composed=%u bumped=%u released=%u decisions=%llu extension-decisions=%llu er-decisions=%llu\n",
      added,composed,bumped,released,(unsigned long long)tactix_stat(s,TACTIX_STAT_DECISIONS),
      (unsigned long long)extension_decisions,(unsigned long long)er_decisions);
  if(result==10) {
    std::puts("s SATISFIABLE");std::printf("v");
    for(int v=1;v<=original;++v)std::printf(" %d",tactix_value(s,v)>0?v:-v);
    std::puts(" 0");
  } else std::puts(result==20?"s UNSATISFIABLE":"s UNKNOWN");
  tactix_release(s);return result==10||result==20?result:1;
}

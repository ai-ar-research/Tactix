#undef NDEBUG
#include "proof/pigeonhole_certificate.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <set>

struct Sink {
  int32_t extent;
  FILE *out;
  uint64_t additions=0,helpers=0;
  bool fresh(int32_t &v){if(extent==INT32_MAX)return false;v=++extent;++helpers;return true;}
  bool add(const std::vector<int32_t> &c,bool,const char *) {
    std::set<int32_t> seen;
    for(int32_t l:c) {assert(l&&!seen.count(-l)&&seen.insert(l).second);if(out)std::fprintf(out,"%d ",l);}
    if(out)std::fputs("0\n",out);
    ++additions;return true;
  }
};
static void exercise(unsigned n,const char *cnf_path=nullptr,const char *proof_path=nullptr) {
  std::vector<std::vector<int32_t>> x(n,std::vector<int32_t>(n));
  for(unsigned i=0;i<n;++i)for(unsigned j=0;j<n;++j)x[i][j]=i*n+j+1;
  FILE *cnf=cnf_path?std::fopen(cnf_path,"w"):nullptr,*proof=proof_path?std::fopen(proof_path,"w"):nullptr;
  if(cnf_path)assert(cnf&&proof);
  const int32_t a=n*n+1,b=n*n+2;
  if(cnf) {
    std::fprintf(cnf,"p cnf %d %llu\n",b,(unsigned long long)(n+uint64_t(n)*n*(n-1)/2+4));
    for(const auto &row:x){for(int32_t l:row)std::fprintf(cnf,"%d ",l);std::fputs("0\n",cnf);}
    for(unsigned j=0;j<n;++j)for(unsigned i=0;i<n;++i)for(unsigned k=i+1;k<n;++k)
      std::fprintf(cnf,"%d %d 0\n",-x[i][j],-x[k][j]);
    for(int as:{1,-1})for(int bs:{1,-1})std::fprintf(cnf,"%d %d 0\n",as*a,bs*b);
    std::fclose(cnf);
  }
  Sink sink{b,proof};std::vector<int32_t> occupied;
  tactix::pigeonhole_certificate::Certificate<Sink> certificate(sink);
  assert(certificate.derive_occupancy(x,occupied)&&occupied.size()==n);
  // Publish every opposite-direction cover, not just helper root units.
  for(unsigned j=0;j<n;++j) {
    std::vector<int32_t> cover;for(unsigned i=0;i<n;++i)cover.push_back(x[i][j]);
    assert(sink.add(cover,false,"original-hole-cover"));
  }
  if(proof){std::fprintf(proof,"%d 0\n0\n",a);assert(!std::ferror(proof));std::fclose(proof);}
  if(cnf_path)std::printf("{\"width\":%u,\"proved_hole_covers\":%u,\"helpers\":%llu,\"additions\":%llu}\n",n,n,(unsigned long long)sink.helpers,(unsigned long long)sink.additions);
}
int main(int argc,char **argv) {
  if(argc==1){for(unsigned n=1;n<=6;++n)exercise(n);std::puts("test_permutation_certificate: passed");}
  else{assert(argc==4);unsigned n=std::strtoul(argv[1],nullptr,10);assert(n&&n<=1000);exercise(n,argv[2],argv[3]);}
}

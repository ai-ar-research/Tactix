#undef NDEBUG
#include "rules/semantic_extension/nary_csp_equality_transitivity.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
using namespace tactix::equality_transitivity;
namespace {
bool rup(const std::vector<Clause> &f,const Clause &c,size_t extent){
 std::vector<int> values(extent);
 for(auto l:c)values[std::abs(l)]=l>0?-1:1;
 bool changed=true;
 while(changed){changed=false;
   for(const auto &clause:f){size_t open=0;int unit=0;bool sat=false;
     for(auto l:clause){auto v=values[std::abs(l)];if(v*(l>0?1:-1)>0){sat=true;break;}if(!v){++open;unit=l;}}
     if(sat)continue;
     if(!open)return true;
     if(open==1){values[std::abs(unit)]=unit>0?1:-1;changed=true;}
   }
 }return false;
}
void schema(size_t n,bool signed_values){
 Domains d;for(size_t i=0;i<3;++i)for(size_t j=0;j<n;++j){int l=int(i*n+j+1);d[i].push_back(signed_values&&j%2?-l:l);}
 // Domain order is canonical lexicographic; first entries remain positive.
 std::array<uint32_t,3> e{uint32_t(3*n+1),uint32_t(3*n+2),uint32_t(3*n+3)};
 Plan p;assert(plan(d,e,1000000,p));
 assert(p.additions==3*n+3&&p.deletions==3*n&&p.literal_occurrences==24*n+9);
 std::vector<Clause> f;
 for(const auto &domain:d){f.push_back(domain);for(size_t a=0;a<n;++a)for(size_t b=a+1;b<n;++b)f.push_back({-domain[a],-domain[b]});}
 const std::array<std::pair<size_t,size_t>,3> pairs{{{0,1},{0,2},{1,2}}};
 for(size_t i=0;i<3;++i)for(size_t j=0;j<n;++j){
   auto a=d[pairs[i].first][j],b=d[pairs[i].second][j];
   f.push_back({int(e[i]),-a,-b});f.push_back({-int(e[i]),-a,b});
 }
 for(size_t i=0;i<3;++i){
   assert(!rup(f,p.triangles[i],3*n+4));
   for(const auto &bridge:p.bridges[i]){assert(rup(f,bridge,3*n+4));f.push_back(bridge);}
   assert(rup(f,p.triangles[i],3*n+4));f.push_back(p.triangles[i]);
   for(const auto &bridge:p.bridges[i]){auto it=std::find(f.begin(),f.end(),bridge);assert(it!=f.end());f.erase(it);}
 }
 // Equality truth for every color choice makes every installed triangle true.
 for(size_t a=0;a<n;++a)for(size_t b=0;b<n;++b)for(size_t c=0;c<n;++c){
   const std::array<bool,3> same{a==b,a==c,b==c};
   for(const auto &triangle:p.triangles){bool sat=false;for(auto l:triangle){auto index=size_t(std::abs(l))-3*n-1;sat|=(l>0)==same[index];}assert(sat);}
 }
 Plan rejected;assert(!plan(d,e,p.planning_work-1,rejected)&&rejected.additions==0);
 auto bad=e;bad[1]=bad[0];assert(!plan(d,bad,1000000,rejected));
 bad=e;bad[2]=1;assert(!plan(d,bad,1000000,rejected));
}
}
int main(){for(size_t n:{2u,3u,4u}){schema(n,false);schema(n,true);}return 0;}

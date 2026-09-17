#undef NDEBUG
#include "engine/affine/projection.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <set>
using namespace tactix::strategy::affine;
template<class Stop>
ProjectionStatus checked_project(size_t n,const std::vector<Equation>&e,
    const std::vector<Coordinate>&c,uint64_t bytes,uint64_t work,Stop stop,Projection&p) {
  Work meter(work,stop);return project(n,e,c,bytes,meter,p);
}
template<class Stop>
bool checked_reconstruct(const Projection&p,const Bits&b,std::vector<uint8_t>&a,
    uint64_t limit,Stop stop,uint64_t&work) {
  Work meter(limit,stop);return reconstruct(p,b,a,meter,work);
}
int main() {
  std::mt19937 gen(19016);
  size_t comparisons=0;
  for(size_t n=0;n<=8;++n) for(size_t seed=0;seed<80;++seed) {
    std::vector<Equation> equations;
    for(size_t j=0,jend=gen()%12;j<jend;++j) {
      Equation e{{},bool(gen()%2)};
      for(size_t v=0;v<n;++v) if(gen()%2)e.variables.push_back(v);
      equations.push_back(e);
    }
    std::vector<size_t> order(n);std::iota(order.begin(),order.end(),0);
    std::shuffle(order.begin(),order.end(),gen);
    std::vector<Coordinate> coords;
    for(size_t i=0,end=gen()%(n+1);i<end;++i)coords.push_back({order[i],bool(gen()%2)});
    std::set<size_t> expected;
    for(size_t model=0;model<(size_t{1}<<n);++model) {
      bool good=true;
      for(const auto&e:equations) {bool x=e.rhs;for(size_t v:e.variables)x^=(model>>v)&1;good&=!x;}
      if(!good)continue;
      size_t mask=0;for(size_t i=0;i<coords.size();++i)mask|=(((model>>coords[i].variable)&1)^coords[i].complemented)<<i;
      expected.insert(mask);
    }
    Projection p;
    const auto status=checked_project(n,equations,coords,10000000,10000000,[]{return false;},p);
    assert(status==ProjectionStatus::Ready||status==ProjectionStatus::Inconsistent);
    assert((status==ProjectionStatus::Inconsistent)==expected.empty());
    if(status==ProjectionStatus::Inconsistent)continue;
    std::set<size_t> actual;
    for(size_t mask=0;mask<(size_t{1}<<coords.size());++mask) {
      Bits syndrome(p.target.size(),0);
      for(size_t i=0;i<coords.size();++i)if((mask>>i)&1)for(size_t w=0;w<syndrome.size();++w)syndrome[w]^=p.columns[i][w];
      if(syndrome!=p.target)continue;
      actual.insert(mask);
      std::vector<uint8_t> assignment(n);for(auto&v:assignment)v=gen()%2;
      Bits support((coords.size()+63)/64,mask);uint64_t work;
      assert(checked_reconstruct(p,support,assignment,10000000,[]{return false;},work));
      for(const auto&e:equations) {bool x=e.rhs;for(size_t v:e.variables)x^=assignment[v];assert(!x);}
      ++comparisons;
    }
    assert(actual==expected);
  }
  // Root equations, complemented fixed coordinates, and >64 check/support bits.
  std::vector<Equation> equations;std::vector<Coordinate> coords;
  for(size_t i=0;i<130;++i){equations.push_back({{i},bool(i%2)});coords.push_back({i,bool(i%3)});}
  Projection p;assert(checked_project(130,equations,coords,10000000,10000000,[]{return false;},p)==ProjectionStatus::Ready);
  Bits support(3,0);for(size_t i=0;i<130;++i)if(bool(i%2)^bool(i%3))flip(support,i);
  std::vector<uint8_t> assignment(130);uint64_t work;
  assert(checked_reconstruct(p,support,assignment,10000000,[]{return false;},work));
  flip(support,129);assert(!checked_reconstruct(p,support,assignment,10000000,[]{return false;},work));
  assert(checked_project(1,{},{{0,false},{0,true}},100000,100000,[]{return false;},p)==ProjectionStatus::Invalid);
  assert(checked_project(1,{},{{1,false}},100000,100000,[]{return false;},p)==ProjectionStatus::Invalid);
  assert(checked_project(1,{}, {},0,100000,[]{return false;},p)==ProjectionStatus::Budget);
  assert(checked_project(1,{}, {},100000,0,[]{return false;},p)==ProjectionStatus::Budget);
  assert(checked_project(1,{}, {},100000,100000,[]{return true;},p)==ProjectionStatus::Interrupted);
  std::cout<<"720 independent exhaustive affine images; "<<comparisons<<" reconstructed extensions; root/polarity/130-coordinate/budget/stop controls PASS\n";
}

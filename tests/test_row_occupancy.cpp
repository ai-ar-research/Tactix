#undef NDEBUG
#include "rules/semantic_extension/row_occupancy.hpp"
#include <cassert>
#include <cstdio>

using namespace tactix::row_occupancy;
static std::vector<Premise> permutation(uint32_t n, bool signed_inputs=false) {
  std::vector<Premise> source;
  auto lit=[&](uint32_t row,uint32_t col){const int32_t v=row*n+col+1;return signed_inputs&&(v%2)?-v:v;};
  for(uint32_t col=0;col<n;++col) {
    std::vector<int32_t> cover;for(uint32_t row=0;row<n;++row)cover.push_back(lit(row,col));
    source.push_back({source.size()+1,cover});
  }
  for(uint32_t row=0;row<n;++row)for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    source.push_back({source.size()+1,{-lit(row,a),-lit(row,b)}});
  return source;
}
static bool recognize_image(const std::vector<Premise> &source,Image &image,uint64_t limit=1000000) {
  uint64_t used=0;
  return recognize(source,32,[&](uint64_t cost){if(cost>limit-used)return false;used+=cost;return true;},image);
}
int main() {
  for(bool signs:{false,true}) {
    const auto source=permutation(3,signs);Image image;
    assert(recognize_image(source,image)&&image.width==3&&image.row_exclusions.size()==27);
    unsigned models=0;
    // Coverage check, separate from proof checking: on every original Boolean
    // assignment, the certified source constraints equal permutation semantics.
    for(unsigned mask=0;mask<512;++mask) {
      auto value=[&](int32_t l){return bool(mask&(1u<<(variable(l)-1)))==(l>0);};
      bool cnf=true;
      for(const auto &p:source) {bool sat=false;for(int32_t l:p.literals)sat|=value(l);cnf&=sat;}
      bool theory=true;
      for(unsigned row=0;row<3;++row) {
        unsigned count=0;for(unsigned col=0;col<3;++col)count+=value(image.primary[row*3+col]);theory&=count==1;
      }
      for(unsigned col=0;col<3;++col) {
        unsigned count=0;for(unsigned row=0;row<3;++row)count+=value(image.primary[row*3+col]);theory&=count==1;
      }
      assert(cnf==theory);models+=cnf;
    }
    assert(models==6);
    auto missing=source;missing.pop_back();assert(!recognize_image(missing,image));
    missing=source;missing.erase(missing.begin());assert(!recognize_image(missing,image));
    assert(!recognize_image(source,image,1)&&image.width==0);
  }
  auto source=permutation(4);
  auto add=[&](std::vector<int32_t> c){source.push_back({source.size()+1,std::move(c)});};
  auto nor=[&](int32_t z,int32_t a,int32_t b){add({z,a,b});add({-z,-a});add({-z,-b});};
  nor(17,1,2);nor(18,3,4);nor(19,-17,-18);
  Image image;assert(recognize_image(source,image));
  assert(image.gates.size()==3&&image.rows[0]==std::vector<int32_t>{-19});
  // Remove a necessary reverse implication: neither this gate nor a parent
  // may be used to claim a certified interval meaning.
  source.erase(source.end()-2);assert(recognize_image(source,image));
  assert(image.gates.size()==2&&image.rows[0]==std::vector<int32_t>({-17,-18}));
  std::puts("test_row_occupancy: passed");
}

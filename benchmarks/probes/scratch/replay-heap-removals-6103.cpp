// Replay fixed extracted heap states through the actual linked implementation.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace tactix;
struct Row { unsigned external,position;double score;unsigned assigned,active; };
static std::vector<Row> read(const char *path) {
  std::ifstream in(path);assert(in);std::string line;std::vector<Row> rows;
  while(std::getline(in,line)) {
    if(line.empty()||line[0]=='#')continue;
    Row row{};std::istringstream s(line);
    assert(s>>row.external>>row.position>>row.score>>row.assigned>>row.active);
    assert(row.external && row.position==rows.size());rows.push_back(row);
  }
  assert(!rows.empty());return rows;
}
static std::vector<unsigned> replay(const std::vector<Row> &rows,const char *order) {
  unsigned maxvar=0;for(const auto &r:rows)maxvar=std::max(maxvar,r.external);
  Kernel k;kernel_reserve_vars(k,maxvar);
  for(Var v=0;v<k.nvars;++v)kernel_record_import(k,v);
  ScopedHeuristics h(k);
  k.h->scores.stack.clear();
  std::fill(k.h->scores.pos.begin(),k.h->scores.pos.end(),UINT32_MAX);
  for(const auto &r:rows) {
    const Var v=r.external-1;
    assert(kernel_externalize(k,make_lit(v,false))==int(r.external));
    k.h->scores.stack.push_back(v);k.h->scores.pos[v]=r.position;k.h->scores.score[v]=r.score;
    k.values[make_lit(v,false)]=r.assigned?1:0;k.values[make_lit(v,true)]=r.assigned?-1:0;
  }
  std::ifstream in(order);assert(in);unsigned external,count=0;
  while(in>>external) {
    assert(external && external<=k.nvars);const Var v=external-1;
    assert(heuristics_heap_contains(k,v));heuristics_deactivate_var(k,v);
    assert(!heuristics_heap_contains(k,v));++count;
  }
  assert(in.eof() && k.h->scores.stack.size()+count==rows.size());
  std::vector<unsigned> result;
  for(Var v:k.h->scores.stack)result.push_back(v+1);
  return result;
}
int main(int argc,char **argv) {
  assert(argc==6);const auto incoming=read(argv[1]);
  for(unsigned arm=0;arm<2;++arm) {
    const auto got=replay(incoming,argv[2+arm]);const auto expected=read(argv[4+arm]);
    assert(got.size()==expected.size());size_t differences=0;
    for(size_t i=0;i<got.size();++i)differences+=got[i]!=expected[i].external;
    std::cout<<(arm?"native":"candidate")<<" removal order: "<<differences<<" differing final positions\n";
    if(differences)return 1;
  }
}

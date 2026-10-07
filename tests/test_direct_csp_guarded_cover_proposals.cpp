#undef NDEBUG
#include "rules/semantic_extension/direct_csp_guarded_cover_proposals.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
using namespace tactix::direct_csp_guarded_cover;
namespace proposal = tactix::direct_csp_guarded_cover::proposals;
namespace {
Bounds bounds{100,1000,10,10,10,10,10,10};
proposal::Limits limits{1000000,1000,1000,1000,100,1000,1};
std::vector<Premise> source(const Selection &owner) {
  auto image=expected_clauses(owner,bounds,[](uint64_t){return true;});
  assert(image.status==Status::Planned);
  std::vector<Premise> result;
  for(const auto &clause:image.clauses)
    result.push_back({result.size()+1,clause,result.size()+100});
  return result;
}
bool rup(const std::vector<Premise> &clauses,const std::vector<int32_t> &lemma) {
  std::map<int32_t,int> assigned;
  auto variable=[](int32_t literal){return literal<0?-literal:literal;};
  for(int32_t literal:lemma) assigned[variable(literal)]=literal>0?-1:1;
  bool changed=true;
  while(changed) {
    changed=false;
    for(const auto &clause:clauses) {
      bool satisfied=false;size_t unset=0;int32_t unit=0;
      for(int32_t literal:clause.literals) {
        int value=assigned[variable(literal)]*(literal>0?1:-1);
        if(value>0)satisfied=true;
        if(!value){++unset;unit=literal;}
      }
      if(satisfied)continue;
      if(!unset)return true;
      if(unset==1){assigned[variable(unit)]=unit>0?1:-1;changed=true;}
    }
  }
  return false;
}
Selection single() {
  return {{{1,2,3},{4,5},{11,12}}, {0,0,0}, {8,9,10},
          {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
}
Selection pair() {
  return {{{1,2},{4,5}}, {0,0}, {8,9,10},
          {{6,{5}},{7,{2}}}, {{8,0,0,0},{9,0,1,0},{10,1,1,1}}};
}
void checked(const proposal::Result &result,const std::vector<Premise> &clauses) {
  assert(result.status==proposal::Status::Nominated && !result.nominations.empty());
  std::set<std::vector<uint32_t>> unique;
  for(const auto &nomination:result.nominations) {
    assert(unique.insert(nomination.selection.selected_values).second);
    assert(rup(clauses,nomination.addition.clause));
    assert(nomination.addition.clause.size()==nomination.selection.domains.size());
    for(size_t d=0;d<nomination.selection.domains.size();++d) {
      int32_t selected=nomination.selection.domains[d][nomination.selection.selected_values[d]];
      assert(std::find(nomination.addition.clause.begin(),nomination.addition.clause.end(),-selected)!=nomination.addition.clause.end());
    }
  }
}
}
int main() {
  auto owner=single();auto clauses=source(owner);
  auto result=proposal::propose(clauses,owner,{0,1,0},bounds,limits);
  checked(result,clauses);
  assert(result.effort.single_moves>0 && result.effort.pair_moves==0);
  assert((result.nominations[0].selection.selected_values==std::vector<uint32_t>{0,0,0}));
  auto owner_pair=pair();auto pair_source=source(owner_pair);
  result=proposal::propose(pair_source,owner_pair,{0,0},bounds,limits);
  checked(result,pair_source);
  assert(result.effort.pair_moves>0);
  assert((result.nominations[0].selection.selected_values==std::vector<uint32_t>{1,1}));
  // Renumber and negate an entire domain and its authenticated OR input.
  auto renamed=owner_pair;
  for(auto &domain:renamed.domains)for(auto &literal:domain)literal=3*literal+40;
  for(auto &literal:renamed.cover)literal=3*literal+40;
  for(auto &definition:renamed.exact_or) {
    definition.output=3*definition.output+40;
    for(auto &literal:definition.inputs)literal=3*literal+40;
  }
  for(auto &blocker:renamed.blockers)blocker.literal=3*blocker.literal+40;
  renamed.domains[1][0]=-renamed.domains[1][0];
  renamed.domains[1][1]=-renamed.domains[1][1];
  renamed.exact_or[0].inputs[0]=-renamed.exact_or[0].inputs[0];
  auto renamed_source=source(renamed);
  checked(proposal::propose(renamed_source,renamed,{0,0},bounds,limits),renamed_source);
  auto multiple=limits;multiple.maximum_nominations=3;
  result=proposal::propose(clauses,owner,{0,0,0},bounds,multiple);
  checked(result,clauses);
  assert(result.nominations.size()==3 && result.nomination_limit_reached);
  // An input outside all selected domains cannot be invented as a selection.
  Selection dead{{{1,2}}, {0}, {8,9}, {{6,{7}}}, {{8,0,0,0},{9,0,1,0}}};
  auto dead_source=source(dead);
  result=proposal::propose(dead_source,dead,{0},bounds,limits);
  assert(result.status==proposal::Status::NoProgress && result.nominations.empty());
  assert(result.neighborhood_complete);
  auto missing=clauses;missing.pop_back();
  result=proposal::propose(missing,owner,{0,0,0},bounds,limits);
  assert(result.status==proposal::Status::IncompleteTheory && result.nominations.empty());
  result=proposal::propose(clauses,owner,{99,0,0},bounds,limits);
  assert(result.status==proposal::Status::Invalid && result.nominations.empty());
  result=proposal::propose(clauses,owner,{0,0,0},bounds,limits,[]{return true;});
  assert(result.status==proposal::Status::Interrupted && result.nominations.empty());
  // Cancellation at the final result boundary discards staged nominations.
  unsigned polls=0;
  checked(proposal::propose(clauses,owner,{0,0,0},bounds,limits,[&]{++polls;return false;}),clauses);
  unsigned late=0;
  result=proposal::propose(clauses,owner,{0,0,0},bounds,limits,[&]{return ++late==polls;});
  assert(result.status==proposal::Status::Interrupted && result.nominations.empty());
  auto deterministic=proposal::propose(pair_source,owner_pair,{0,0},bounds,limits);
  auto repeated=proposal::propose(pair_source,owner_pair,{0,0},bounds,limits);
  assert(deterministic.effort.work==repeated.effort.work);
  assert(deterministic.effort.visited_keys==repeated.effort.visited_keys);
  assert(deterministic.nominations[0].selection==repeated.nominations[0].selection);
  assert(deterministic.nominations[0].addition.clause==repeated.nominations[0].addition.clause);
  for(unsigned bound=0;bound<6;++bound) {
    auto tiny=limits;
    switch(bound) {
    case 0:tiny.maximum_work=1;break;
    case 1:tiny.maximum_proposals=1;break;
    case 2:tiny.maximum_single_moves=1;break;
    case 3:tiny.maximum_pair_moves=1;tiny.maximum_nominations=10;break;
    case 4:tiny.maximum_visited_keys=1;break;
    case 5:tiny.maximum_repair_steps=1;tiny.maximum_nominations=10;break;
    }
    if(bound==3) {
      auto extended=owner_pair;
      extended.domains.push_back({11,12}); extended.selected_values.push_back(0);
      extended.cover.push_back(14); extended.blockers.push_back({14,2,1,1});
      extended.exact_or[0].inputs.push_back(12);
      auto extended_source=source(extended);
      result=proposal::propose(extended_source,extended,{0,0,0},bounds,tiny);
    } else result=proposal::propose(pair_source,owner_pair,{0,0},bounds,tiny);
    if(result.status!=proposal::Status::ResourceExhausted) std::fprintf(stderr,"bound=%u status=%u pairs=%llu steps=%llu\n",bound,unsigned(result.status),(unsigned long long)result.effort.pair_moves,(unsigned long long)result.effort.repair_steps);
    assert(result.status==proposal::Status::ResourceExhausted && result.nominations.empty());
    assert(result.effort.work<=tiny.maximum_work);
    assert(result.effort.proposals<=tiny.maximum_proposals);
    assert(result.effort.single_moves<=tiny.maximum_single_moves);
    assert(result.effort.pair_moves<=tiny.maximum_pair_moves);
    assert(result.effort.visited_keys<=tiny.maximum_visited_keys);
    assert(result.effort.repair_steps<=tiny.maximum_repair_steps);
  }
}

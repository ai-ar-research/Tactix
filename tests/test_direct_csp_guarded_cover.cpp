#include "unit_test.hpp"
#include "clause_oracle.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover.hpp"
using namespace tactix::direct_csp_guarded_cover;
namespace {
Selection fixture() {
  return {{{1,2,3},{4,5},{11,12}}, {0,0,0}, {8,9,10},
          {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
}
}
int main() {
  Bounds bounds{100,1000,10,10,10,10,10,10};
  auto charge=[](uint64_t){return true;};
  Selection selection=fixture();
  auto expected=expected_clauses(selection,bounds,charge);
  assert(expected.status==Status::Planned && expected.clauses.size()==18);
  std::vector<Premise> source;
  for (const auto& c : expected.clauses) {
    Premise p; p.occurrence=source.size()+1; p.native_proof_id=p.occurrence+100;
    p.literals=c; source.push_back(p);
  }
  auto result=plan(source,selection,bounds,charge);
  assert(result.status==Status::Planned && result.additions.size()==1);
  assert((result.additions[0].clause==std::vector<int32_t>{-11,-4,-1}));
  assert(rup_implied(expected.clauses, result.additions[0].clause));
  assert(!rup_implied(expected.clauses, std::vector<int32_t>{-11,-5,-1}));
  unsigned models=0;
  for(unsigned assignment=0;assignment<4096;++assignment) {
    bool model=true;
    for(const auto& p:source) model &= clause_value(p.literals,assignment);
    if(model) { ++models; assert(clause_value(result.additions[0].clause,assignment)); }
  }
  assert(models>0);
  // Every premise class is required, including converse OR and domain ALO.
  for(size_t i=0;i<source.size();++i) {
    auto missing=source; missing.erase(missing.begin()+i);
    auto rejected=plan(missing,selection,bounds,charge);
    assert(rejected.status==Status::IncompleteTheory && rejected.additions.empty());
  }
  selection.selected_values[0]=1;
  assert(plan(source,selection,bounds,charge).status==Status::Planned);
  selection.selected_values[1]=1;
  assert(plan(source,selection,bounds,charge).status==Status::NoConsequence);
  // Cover literals are not an invented AMO: two blockers may hold together.
  Selection ordinary_cover=fixture(); ordinary_cover.blockers[1].value=0;
  auto ordinary_image=expected_clauses(ordinary_cover,bounds,charge);
  unsigned two_blockers=(1u<<0)|(1u<<4)|(1u<<7)|(1u<<8)|(1u<<10);
  for(const auto& clause:ordinary_image.clauses) assert(clause_value(clause,two_blockers));
  // Signed originals preserve polarity throughout OR authentication and output.
  Selection signed_selection=fixture();
  signed_selection.domains[1][0]=-4; signed_selection.exact_or[0].inputs[0]=-4;
  auto signed_image=expected_clauses(signed_selection,bounds,charge);
  std::vector<Premise> signed_source;
  for(const auto& c:signed_image.clauses) {
    Premise p; p.occurrence=signed_source.size()+1; p.literals=c;
    signed_source.push_back(p);
  }
  auto signed_result=plan(signed_source,signed_selection,bounds,charge);
  assert(signed_result.status==Status::Planned);
  assert((signed_result.additions[0].clause==std::vector<int32_t>{-11,-1,4}));
  assert(rup_implied(signed_image.clauses,signed_result.additions[0].clause));
  selection=fixture(); selection.blockers[0].domain=99;
  assert(plan(source,selection,bounds,charge).status==Status::Invalid);
  selection=fixture();
  uint64_t remaining=3;
  auto limited=[&](uint64_t n){if(n>remaining)return false;remaining-=n;return true;};
  auto rejected=plan(source,selection,bounds,limited);
  assert(rejected.status==Status::ResourceExhausted && rejected.additions.empty());
}

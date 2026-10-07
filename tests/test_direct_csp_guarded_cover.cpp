#undef NDEBUG
#include "rules/semantic_extension/direct_csp_guarded_cover.hpp"
#include <cassert>
#include <algorithm>
using namespace tactix::direct_csp_guarded_cover;
namespace {
Selection fixture() {
  return {{{1,2,3},{4,5},{11,12}}, {0,0,0}, {8,9,10},
          {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}};
}
bool satisfied(const std::vector<int32_t>& clause, unsigned assignment) {
  for (int32_t x : clause) {
    bool value = (assignment >> (unsigned(x < 0 ? -x : x)-1)) & 1;
    if (value == (x > 0)) return true;
  }
  return false;
}
bool rup(const std::vector<Premise> &source, const std::vector<int32_t> &clause) {
  int values[13] = {};
  for (int32_t literal : clause) values[unsigned(literal < 0 ? -literal : literal)] = literal > 0 ? -1 : 1;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &premise : source) {
      bool true_literal = false; unsigned unset = 0; int32_t unit = 0;
      for (int32_t literal : premise.literals) {
        unsigned variable = unsigned(literal < 0 ? -literal : literal);
        int value = values[variable] * (literal > 0 ? 1 : -1);
        if (value > 0) true_literal = true;
        if (!value) { ++unset; unit = literal; }
      }
      if (true_literal) continue;
      if (!unset) return true;
      if (unset == 1) {
        values[unsigned(unit < 0 ? -unit : unit)] = unit > 0 ? 1 : -1;
        changed = true;
      }
    }
  }
  return false;
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
  assert(rup(source, result.additions[0].clause));
  assert(!rup(source, {-11,-5,-1}));
  unsigned models=0;
  for(unsigned assignment=0;assignment<4096;++assignment) {
    bool model=true;
    for(const auto& p:source) model &= satisfied(p.literals,assignment);
    if(model) { ++models; assert(satisfied(result.additions[0].clause,assignment)); }
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
  for(const auto& clause:ordinary_image.clauses) assert(satisfied(clause,two_blockers));
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
  assert(rup(signed_source,signed_result.additions[0].clause));
  selection=fixture(); selection.blockers[0].domain=99;
  assert(plan(source,selection,bounds,charge).status==Status::Invalid);
  selection=fixture();
  uint64_t remaining=3;
  auto limited=[&](uint64_t n){if(n>remaining)return false;remaining-=n;return true;};
  auto rejected=plan(source,selection,bounds,limited);
  assert(rejected.status==Status::ResourceExhausted && rejected.additions.empty());
}

#undef NDEBUG
#include "recognize.hpp"
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>
using namespace ramsey_recovery;

static std::vector<Clause> formula(uint32_t n, std::mt19937 &rng) {
  const uint32_t m = n * (n - 1) / 2;
  std::vector<int32_t> names(m);
  // Sparse, permuted external names and independent variable complementation.
  for (uint32_t i = 0; i < m; ++i) names[i] = int32_t(3*i+7) * (rng()%2 ? 1 : -1);
  std::shuffle(names.begin(), names.end(), rng);
  std::vector<std::vector<int32_t>> edges(n, std::vector<int32_t>(n));
  uint32_t next = 0;
  for (uint32_t a = 0; a < n; ++a) for (uint32_t b = a+1; b < n; ++b)
    edges[a][b] = edges[b][a] = names[next++];
  std::vector<Clause> out;
  for (uint32_t a = 0; a < n; ++a) for (uint32_t b = a+1; b < n; ++b)
    for (uint32_t c = b+1; c < n; ++c) for (uint32_t d = c+1; d < n; ++d)
      for (int32_t sign : {1, -1}) {
        Clause row{edges[a][b],edges[a][c],edges[a][d],edges[b][c],edges[b][d],edges[c][d]};
        for (auto &lit : row) lit *= sign;
        std::shuffle(row.begin(), row.end(), rng);
        out.push_back(std::move(row));
      }
  std::shuffle(out.begin(), out.end(), rng);
  return out;
}
static void verify(const std::vector<Clause> &input, const Result &r, uint32_t n) {
  assert(r.recognized && !r.exhausted && r.edges.size() == n);
  assert(r.sources.size()*2 == input.size());
  std::set<uint64_t> used;
  for (const Source &s : r.sources) {
    for (int32_t sign : {1,-1}) {
      Clause expected;
      for (uint32_t i = 0; i < 4; ++i) for (uint32_t j = i+1; j < 4; ++j)
        expected.push_back(sign*r.edges[s.vertices[i]][s.vertices[j]]);
      std::sort(expected.begin(), expected.end());
      auto id = sign == 1 ? s.positive_clause : s.negative_clause;
      assert(id < input.size() && used.insert(id).second);
      auto actual = input[id]; std::sort(actual.begin(), actual.end());
      assert(actual == expected);
    }
  }
}
int main() {
  const Limits limits{10000, 200, 40000, 10000000};
  std::mt19937 rng(941630);
  uint32_t positive=0, negative=0, budgets=0;
  uint64_t maximum_work = 0;
  for (uint32_t n : {6,9,18,19}) for (uint32_t trial=0; trial<16; ++trial) {
    auto input = formula(n,rng);
    const auto r = recognize(input,limits); verify(input,r,n); ++positive;
    maximum_work = std::max(maximum_work,r.work);
    auto repeated = recognize(input,limits);
    assert(repeated.edges==r.edges && repeated.work==r.work);
    auto bound = limits; bound.work = r.work;
    assert(recognize(input,bound).recognized); ++budgets;
    --bound.work; auto short_result = recognize(input,bound);
    assert(short_result.exhausted && !short_result.recognized && short_result.edges.empty()); ++budgets;
    for (uint32_t corruption=0; corruption<6; ++corruption) {
      auto bad=input;
      if (corruption==0) bad.pop_back();
      if (corruption==1) bad[0][0]=-bad[0][0];
      if (corruption==2) bad[0]=bad[1];
      if (corruption==3) bad[0][0]=bad[0][1];
      if (corruption==4) bad[0][0]=0;
      if (corruption==5) bad[0][0]=INT32_MIN;
      const auto declined=recognize(bad,limits);
      assert(!declined.recognized && declined.edges.empty() && declined.sources.empty()); ++negative;
    }
    if (!trial) {
      for (uint32_t which=0; which<4; ++which) {
        auto small=limits;
        if (which==0) small.clauses=input.size()-1;
        if (which==1) small.variables=n*(n-1)/2-1;
        if (which==2) small.pair_cells=uint64_t(n*(n-1)/2)*(n*(n-1)/2)-1;
        if (which==3) small.work=0;
        assert(!recognize(input,small).recognized); ++budgets;
      }
    }
  }
  assert(!recognize({},limits).recognized);
  std::cout << "{\"scope\":\"fixed structural recognition controls, no solver or proof run\",\"positive\":" << positive
            << ",\"negative\":" << negative << ",\"budget_controls\":" << budgets
            << ",\"maximum_charged_work\":" << maximum_work << "}\n";
}

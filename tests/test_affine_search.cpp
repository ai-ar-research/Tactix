#undef NDEBUG
#include "engine/affine/syndrome_search.hpp"
#include <cassert>
#include <set>
#include <cstdio>
using namespace tactix::strategy::affine;
struct TestLimits : SearchLimits {
  TestLimits(size_t split,size_t left,size_t right,size_t total,uint64_t entries,
             uint64_t bytes,uint64_t work):SearchLimits{split,left,right,total,entries,bytes},maximum_work(work){}
  uint64_t maximum_work;
};
template<class Stop,class Accept>
SearchStatus checked_search(const std::vector<Bits>&c,const Bits&t,const TestLimits&l,
    Stop stop,Accept accept,SearchStats&s) {
  Work meter(l.maximum_work,stop);return search(c,t,l,meter,accept,s);
}
int main() {
  uint64_t calls = 0;
  for (unsigned seed = 0; seed < 32; ++seed) {
    std::vector<Bits> columns;
    for (unsigned p = 0; p < 8; ++p)
      columns.push_back({((seed + 3) * (p + 1) * 17u) & 31u,
                         (p * p + seed) & 3u});
    for (unsigned target = 0; target < 16; ++target) {
      Bits wanted{target, seed & 3u};
      std::set<uint64_t> expected, observed;
      for (uint64_t assignment = 0; assignment < 256; ++assignment) {
        if (std::popcount(assignment & 15u) > 2 ||
            std::popcount(assignment >> 4) > 3 || std::popcount(assignment) > 4)
          continue;
        Bits value{0, 0};
        for (unsigned p = 0; p < 8; ++p)
          if (assignment & (1u << p))
            for (unsigned w = 0; w < 2; ++w) value[w] ^= columns[p][w];
        if (value == wanted) expected.insert(assignment);
      }
      TestLimits limits{4,2,3,4,1000,1000000,1000000}; SearchStats stats;
      const auto status = checked_search(columns,wanted,limits,[]{return false;},
          [&](const Bits &bits){assert(observed.insert(bits[0]).second);return false;},stats);
      assert(status == SearchStatus::Exhausted && observed == expected);
      ++calls;
    }
  }
  // Equal syndromes must not collapse different supports; only the second
  // matching assignment may pass constraints outside the projected system.
  std::vector<Bits> zero(70, Bits(2,0)); SearchStats stats;
  TestLimits limits{35,1,1,1,1000,1000000,1000000};
  assert(checked_search(zero,Bits{0,0},limits,[]{return false;},
      [](const Bits &bits){return bits[1] == 1;},stats)==SearchStatus::Found);
  limits.maximum_bytes=0;
  assert(checked_search(zero,Bits{0,0},limits,[]{return false;},
      [](const Bits &){assert(false);return true;},stats)==SearchStatus::Budget);
  limits.maximum_bytes=1000000;limits.maximum_work=0;
  assert(checked_search(zero,Bits{0,0},limits,[]{return false;},
      [](const Bits &){assert(false);return true;},stats)==SearchStatus::Budget);
  limits.maximum_work=1000000;
  assert(checked_search(zero,Bits{0,0},limits,[]{return true;},
      [](const Bits &){assert(false);return true;},stats)==SearchStatus::Interrupted);
  // A callback spends the SAME ledger. Its exhausted candidate cannot leak a
  // second callback or consume a fresh allowance on the next support.
  Work shared(1000000,[]{return false;});
  uint64_t callbacks=0;
  const auto status=search(zero,Bits{0,0},limits,shared,[&](const Bits&){
    ++callbacks;assert(shared.charge(1000000-shared.used()));
    assert(!shared.charge(1));return false;
  },stats);
  assert(status==SearchStatus::Budget&&callbacks==1&&shared.used()==1000000);
  std::printf("%llu exact two-word syndrome/brute-force comparisons; duplicate supports,70coordinates,budget/stop PASS\n",(unsigned long long)calls);
}

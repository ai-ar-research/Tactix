// Fixed structural-work checks; no solver execution, default choice, or fuzz.
#include "rules/lucky/graph_labeling_search.hpp"
#include <cassert>
#include <iostream>
using namespace tactix::graph_labeling;
using Wide = unsigned __int128;

// Bounds count the current helper's charge calls, including unsuccessful
// comparisons and optional reflection, not CPU instructions or wall time.
static Wide construction_bound(const Shape &s) {
  const Wide n = s.vertices, m = s.edges.size();
  Wide degree_pairs = 0;
  for (const auto &row : s.adjacency)
    degree_pairs += Wide(row.size()) * (row.size() ? row.size() - 1 : 0);
  return 60*n + n*(n-1) + 28*m + degree_pairs;
}
static Wide window_bound(const Shape &s, const WindowLimits &limits) {
  const Wide n=s.vertices, m=s.edges.size(), words=(n+63)/64;
  const Wide sort=n*(n-1)/2;
  const Wide prefix=std::min(s.vertices-1,s.separation-1);
  const Wide internal=(prefix+1)*words+2*n+2*m+sort;
  const Wide leaf=1+2*n+m;
  return n*words+3*n+2*m+sort+Wide(std::min(s.vertices,limits.starts))*n+
         Wide(limits.nodes)*std::max(internal,leaf);
}
static Shape fixture(uint32_t n, bool clique) {
  Shape s; s.vertices=n; s.separation=2; s.adjacency.resize(n);
  for(uint32_t a=0;a<n;++a) for(uint32_t b=a+1;b<n;++b)
    if(clique || b==a+1) {
      s.edges.emplace_back(a,b);s.adjacency[a].push_back(b);s.adjacency[b].push_back(a);
    }
  return s;
}
int main() {
  // Only eight predetermined fixtures; mathematical derivation owns generality.
  for(uint32_t n : {3u,4u,8u,16u}) for(bool clique : {false,true}) {
    Shape s=fixture(n,clique); Candidate best;uint32_t seeds=0;
    uint64_t counted=0;
    auto charge=[&](uint64_t amount){counted+=amount;return true;};
    assert(order_seeds(s,charge,best,seeds));assert(seeds==24);
    const auto construction_work=counted;
    assert(Wide(counted)<=construction_bound(s));
    // A profile exactly at the derived bound must reproduce the unbounded seeds.
    uint64_t used=0;const uint64_t allowance=uint64_t(construction_bound(s));
    auto bounded=[&](uint64_t amount){if(amount>allowance-used)return false;used+=amount;return true;};
    Candidate replay;uint32_t replay_seeds=0;
    assert(order_seeds(s,bounded,replay,replay_seeds));
    assert(replay_seeds==seeds && replay.labels==best.labels && replay.score==best.score);
    counted=0;uint64_t nodes=0;WindowLimits limits{1000,12,28,123};
    const Candidate window_initial=best;
    const auto status=window_search(s,limits,charge,best,nodes);
    assert(Wide(counted)<=window_bound(s,limits));
    const uint64_t window_allowance=uint64_t(window_bound(s,limits));
    uint64_t window_used=0,replay_nodes=0;
    auto window_charge=[&](uint64_t amount){if(amount>window_allowance-window_used)return false;window_used+=amount;return true;};
    Candidate window_replay=window_initial;
    assert(window_search(s,limits,window_charge,window_replay,replay_nodes)==status);
    assert(replay_nodes==nodes && window_used==counted &&
           window_replay.labels==best.labels && window_replay.score==best.score);
    std::cout << n << ',' << (clique?"clique":"path") << ',' << construction_work
              << ',' << uint64_t(construction_bound(s)) << ',' << counted << ','
              << uint64_t(window_bound(s,limits)) << ',' << nodes << ',' << int(status) << '\n';
  }
}

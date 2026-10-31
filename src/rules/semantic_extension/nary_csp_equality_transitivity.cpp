#include "rules/semantic_extension/nary_csp_equality_transitivity.hpp"
#include "rules/semantic_extension/csp_domain_identity.hpp"
#include "kernel/types.hpp"
#include <algorithm>
#include <climits>
#include <utility>
namespace tactix::equality_transitivity {
bool domains_valid(const Domains &domains) {
  return csp_domain_identity::ordered_three_valid(domains,2,UINT32_MAX);
}
bool plan(const Domains &domains,const std::array<uint32_t,3> &e,
          uint64_t maximum_work,Plan &out) {
  out={}; const uint64_t n=domains[0].size();
  // Charge a conservative quadratic validation/sorting envelope BEFORE work.
  if (!n || n>UINT32_MAX || n>(UINT64_MAX-100)/(18*n+100)) return false;
  const uint64_t work=18*n*n+100*n+100;
  if (work>maximum_work || !domains_valid(domains)) return false;
  for (size_t i=0;i<3;++i) {
    if (!e[i] || e[i]>INT32_MAX) return false;
    for (size_t j=0;j<i;++j) if(e[i]==e[j]) return false;
    for(const auto &d:domains) for(auto l:d)
      if(external_variable_of(l)==e[i]) return false;
  }
  Plan p; p.planning_work=work;
  p.triangles={Clause{-int32_t(e[0]),-int32_t(e[1]),int32_t(e[2])},
               Clause{-int32_t(e[0]),-int32_t(e[2]),int32_t(e[1])},
               Clause{-int32_t(e[1]),-int32_t(e[2]),int32_t(e[0])}};
  const std::array<size_t,3> anchors{1,0,0};
  for(size_t direction=0;direction<3;++direction)
    for(auto value:domains[anchors[direction]]) {
      Clause bridge=p.triangles[direction];bridge.insert(bridge.begin()+2,-value);
      p.bridges[direction].push_back(std::move(bridge));
    }
  p.additions=3*n+3;p.deletions=3*n;p.literal_occurrences=24*n+9;
  p.publication_work=p.additions+p.deletions+p.literal_occurrences;
  out=std::move(p);return true;
}
} // namespace tactix::equality_transitivity

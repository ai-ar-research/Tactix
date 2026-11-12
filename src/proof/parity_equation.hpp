#pragma once

#include <algorithm>
#include <bit>
#include <compare>
#include <cstdint>
#include <vector>

namespace tactix {
// Semantic source, independent of a certificate's variable names or bounds.
struct PbpParityEquation {
  std::vector<int32_t> variables;
  bool rhs = false;
  friend bool operator==(const PbpParityEquation &,const PbpParityEquation &) = default;
  friend std::strong_ordering operator<=>(const PbpParityEquation &a,const PbpParityEquation &b) {
    if (const auto order=a.variables<=>b.variables; order!=0) return order;
    return a.rhs<=>b.rhs;
  }
};

inline bool valid_parity_source(const PbpParityEquation &row,uint32_t variables) {
  return row.variables.size()>=2 && row.variables.size()<=8 &&
      std::is_sorted(row.variables.begin(),row.variables.end()) &&
      std::adjacent_find(row.variables.begin(),row.variables.end())==row.variables.end() &&
      row.variables.front()>0 && uint32_t(row.variables.back())<=variables;
}

// Complete CNF, in a shared deterministic order. The caller authenticates the
// source and bounds storage/work; this helper is not a premise authority.
template<class Emit> bool visit_parity_source(const PbpParityEquation &row,Emit emit) {
  if (!valid_parity_source(row,INT32_MAX)) return false;
  for (uint32_t mask=0; mask<(uint32_t(1)<<row.variables.size()); ++mask) {
    if (bool(std::popcount(mask)&1)==row.rhs) continue;
    std::vector<int32_t> clause;
    for (size_t at=0; at<row.variables.size(); ++at)
      clause.push_back(mask&(uint32_t(1)<<at) ? -row.variables[at] : row.variables[at]);
    if (!emit(clause)) return false;
  }
  return true;
}
} // namespace tactix

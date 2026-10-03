// Bounded GF(2) row selection, with exact source-equation cancellation traces.
// This is discovery only: no clause, proof, or kernel mutation takes place.
#pragma once
#include "rules/lucky/parity_premises.hpp"
#include <iterator>
namespace tactix::parity_premises {
struct SelectionLimits {
  uint64_t work;
  uint32_t intermediate_width;
  uint32_t source_count;
  uint32_t output_width;
};
struct Consequence {
  std::vector<External> variables;
  bool rhs = false;
  // Sorted zero-based indices into the supplied CertifiedEquation vector.
  std::vector<size_t> sources;
};
struct Selection {
  std::vector<Consequence> consequences;
  uint64_t work = 0;
  uint64_t width_declines = 0;
  uint64_t source_declines = 0;
  bool exhausted = false;
};
// Pivot direction is a caller choice, not a hidden solver default. Every
// returned row is the XOR of its listed source equations even on exhaustion.
inline Selection select(const std::vector<CertifiedEquation> &equations,
                        bool descending_pivots, SelectionLimits limits) {
  Selection result;
  std::map<External, Consequence> basis;
  auto charge = [&](uint64_t amount) {
    if (amount > limits.work - result.work) {
      result.exhausted = true;
      return false;
    }
    result.work += amount;
    return true;
  };
  for (size_t index = 0; index < equations.size(); ++index) {
    const Equation &input = equations[index].equation;
    if (!charge(1 + input.size)) break;
    // Collector output is sorted and distinct. This helper is internal and
    // consumes that certified representation, not arbitrary raw equations.
    assert(input.size <= input.variables.size());
    Consequence row{{input.variables.begin(), input.variables.begin()+input.size},
                    input.rhs, {index}};
    bool declined = false;
    while (!row.variables.empty()) {
      if (row.variables.size() > limits.intermediate_width) {
        ++result.width_declines;declined = true;break;
      }
      if (row.sources.size() > limits.source_count) {
        ++result.source_declines;declined = true;break;
      }
      const External pivot = descending_pivots ? row.variables.back() : row.variables.front();
      const auto found = basis.find(pivot);
      if (found == basis.end()) break;
      const Consequence &other = found->second;
      if (!charge(row.variables.size()+other.variables.size()+
                  row.sources.size()+other.sources.size())) return result;
      std::vector<External> variables;
      std::vector<size_t> sources;
      std::set_symmetric_difference(row.variables.begin(),row.variables.end(),
          other.variables.begin(),other.variables.end(),std::back_inserter(variables));
      std::set_symmetric_difference(row.sources.begin(),row.sources.end(),
          other.sources.begin(),other.sources.end(),std::back_inserter(sources));
      row.variables = std::move(variables);
      row.sources = std::move(sources);
      row.rhs ^= other.rhs;
    }
    if (declined) continue;
    // Recheck provenance after a final cancellation to an empty relation.
    if (row.sources.size() > limits.source_count) {
      ++result.source_declines;continue;
    }
    if (row.sources.size()>1 && row.variables.size()<=limits.output_width &&
        (!row.variables.empty() || row.rhs)) result.consequences.push_back(row);
    if (!row.variables.empty()) {
      const External pivot=descending_pivots ? row.variables.back() : row.variables.front();
      basis.emplace(pivot,std::move(row));
    }
  }
  return result;
}
} // namespace tactix::parity_premises

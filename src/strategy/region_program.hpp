#pragma once
#include "rules/support/region.hpp"
namespace tactix::strategy {
using RegionQuery = tactix::RegionQuery;

// Strategy-owned ordering and continuation; rule-owned inference is separate.
class RegionProgram {
  Kernel &owner_;
  RegionSession session_;
  std::vector<RegionQuery> queries_;
  size_t cursor_ = 0;
public:
  explicit RegionProgram(Kernel &k) : owner_(k), session_(k) {}
  void begin_definition() { session_.begin_definition(); }
  void end_definition(int output) { session_.end_definition(output); }
  int add_region(std::span<const int32_t> variables) { return session_.add_region(variables); }
  bool valid_query(const RegionQuery &q) const { return session_.valid_query(q); }
  ApplyResult apply(const RegionQuery &q) { return session_.apply(q); }
  bool enqueue(const RegionQuery &);
  ApplyResult run();
  size_t pending() const { return queries_.size() - cursor_; }
};
} // namespace tactix::strategy

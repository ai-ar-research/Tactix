#pragma once

#include "kernel/query_projection.hpp"
#include "session/query_history.hpp"
#include <unordered_set>
#include <vector>

namespace tactix::session {

struct ProjectionSnapshot {
  uint64_t reconstruction_literals = 0;
  uint64_t translated_assumptions = 0;
  uint64_t preserved_restrictions = 0;
  uint64_t rejected_models = 0;
  bool restore = false;
  bool invalid_model = false;
};

// Preserves the projection theorem: a model of the current formula and this
// effective cube reconstructs a model of permanent input plus the original A.
// Equivalence translations publish query-only RUP premises before source
// deletion. Those premises never become native root assignments.
class QueryProjection final : public QueryProjectionService {
public:
  QueryProjection(const QueryHistory &, uint64_t query);
  std::span<const int32_t> constraints() const override { return assumptions_; }
  bool constrains(Var external) const override;
  bool preserves_constant(Lit external) const override;
  bool preserves_restriction(const Kernel &, std::span<const int8_t>,
                             std::span<const Lit>) const override;
  QueryModelAdmission admit_model(Kernel &) override;
  ModelCheck check_model(Kernel &);
  bool is_assumption(int32_t literal) const { return contains(literal); }
  void reconstruction_literal(Lit external) override;
  void substitute(Kernel &, std::span<const std::pair<Lit, Lit>>) override;
  void restrict(Kernel &, std::span<const int8_t> constants,
                 std::span<const Lit> images) override;
  void restrict_constant(Lit external) override;
  bool contradictory() const { return contradictory_; }
  const ProjectionSnapshot &snapshot() const { return snapshot_; }

private:
  bool contains(int32_t) const;
  void rebuild_cube();
  std::vector<int32_t> assumptions_;
  const QueryHistory &history_;
  uint64_t query_ = 0;
  std::unordered_set<int32_t> membership_;
  bool contradictory_ = false;
  ProjectionSnapshot snapshot_;
};
} // namespace tactix::session

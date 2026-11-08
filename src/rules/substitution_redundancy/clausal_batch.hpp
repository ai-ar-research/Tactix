#pragma once
#include "engine/clausal_replay.hpp"
#include "rules/registry.hpp"

namespace tactix {
enum class ClausalBatchKind { Rup=1, Rat=2, Delete=3 };
struct ClausalBatchStep {
  ClausalBatchKind kind;
  std::vector<int32_t> clause;
  uint32_t delete_step = 0; // one-based earlier addition, never an input clause
};
RuleEligibility clausal_batch_guard(const Kernel &);
class ClausalBatchPlan {
  friend bool clausal_batch_prepare(const Kernel &,int32_t,uint32_t,
      std::vector<ClausalBatchStep>,uint64_t,ClausalBatchPlan &,
      ClausalReplay::Effort &);
  friend ApplyResult clausal_batch_commit(Kernel &,const ClausalBatchPlan &);
  uint64_t epoch_=0, next_id_=0;
  uint32_t nvars_=0, extent_=0, fresh_=0;
  std::vector<ClausalBatchStep> steps_;
};
bool clausal_batch_prepare(const Kernel &,int32_t first_fresh,uint32_t fresh,
    std::vector<ClausalBatchStep>,uint64_t maximum_work,
    ClausalBatchPlan &,ClausalReplay::Effort &);
ApplyResult clausal_batch_commit(Kernel &,const ClausalBatchPlan &);
} // namespace tactix

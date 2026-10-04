#pragma once
#include "rules/registry.hpp"

namespace tactix {
struct RowCoverRequest {
  uint64_t maximum_clauses=0, maximum_literals=0, recognition_work=0;
  uint32_t maximum_width=0, maximum_external_extent=0;
  uint64_t maximum_additions=0, action_id=0;
  FILE *records=nullptr;
  bool original_input=false; // explicit startup-only, revalidated source view
};
struct RowCoverResult {
  ApplyResult result=ApplyResult::NoOp;
  uint64_t recognition_work=0, preflight_additions=0, additions=0;
  uint64_t reverse_implications=0, helpers=0;
  uint32_t width=0;
  uint64_t published=0;
};
// Internal end-to-end theory composition. Public typed execution and policy
// admission are separate wiring; this entry point makes no scheduling choice.
RowCoverResult derive_row_cover(Kernel &, const RowCoverRequest &);
} // namespace tactix

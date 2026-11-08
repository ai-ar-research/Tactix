#include "session/knowledge_proof.hpp"
#include <algorithm>

namespace tactix::session {
bool emit_query_knowledge(Proof &proof,const RetainedClause &record,uint64_t &next_id,
    uint32_t extent,PbpParityBoundsCache &cache,const std::function<bool(uint64_t)> &charge) {
  if (record.parity_equations.empty()) {
    return charge(record.literals.size()+1) &&
        proof.add_external_rup(++next_id,record.literals.data(),record.literals.size());
  }
  for (const auto &row:record.parity_equations)
    if (!visit_parity_source(row,[&](const std::vector<int32_t> &clause) {
      return charge(clause.size()+1) && proof.add_external_rup(++next_id,clause.data(),clause.size());
    })) return false;
  PbpParityReasonPlan plan;
  if (!proof.plan_pbp_parity_reason(record.parity_equations,record.literals.data(),record.literals.size(),
        std::max(extent,proof.certificate_logical_extent()),cache,plan) || !charge(plan.proof_constraints))
    return false;
  return proof.add_prevalidated_pbp_parity_reason(++next_id,record.parity_equations,
      record.literals.data(),record.literals.size(),cache,plan);
}
}

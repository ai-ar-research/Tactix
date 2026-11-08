#pragma once
#include "session/retained_clauses.hpp"
#include "proof/proof.hpp"
#include <functional>

namespace tactix::session {
// Shared serialization of already licensed F-only knowledge. It introduces no
// trusted premises: complete XOR encodings enter by RUP before their reason.
bool emit_query_knowledge(Proof &,const RetainedClause &,uint64_t &next_id,
    uint32_t extent,PbpParityBoundsCache &,const std::function<bool(uint64_t)> &charge);
}

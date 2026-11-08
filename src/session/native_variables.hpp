#pragma once

#include "kernel/types.hpp"
#include <cstdint>

namespace tactix {
struct Kernel;
namespace session {

enum class NativeVariableStatus : uint8_t {
  Existing, Reactivated, Fixed, UnknownIdentity, InvalidLiteral, Busy, Exhausted,
  Created, UnsupportedNamespace
};

struct NativeVariable {
  NativeVariableStatus status = NativeVariableStatus::UnknownIdentity;
  Lit literal = INVALID_LIT;
  int8_t value = 0; // signed requested literal; Fixed can have no native index
};

// Cold representation transition for an already admitted external identity.
// Reconnect a compacted coordinate or reactivate a retired one without changing
// permanent external IDs. Archived fixed values stay archived. Proof-only holes
// and never-admitted IDs are not client-variable allocation requests.
//
// This does NOT restore weakened clauses, discharge an SR restriction, or clear
// projection/restoration obligations. A persistent owner must perform those
// services before publishing a query result. No proof event or input is added.
NativeVariable reactivate_external_variable(Kernel &, int32_t external);

// Admit a client coordinate at a settled root, growing only the referenced
// internal coordinate while preserving sparse external gaps. Existing private
// helper namespaces require a separate renaming/composition owner and decline.
// Input/proof ownership and weakening repair remain the session's obligation.
NativeVariable admit_external_variable(Kernel &, int32_t external);

// Query-repair transaction only: retract the entire trail and archived root
// image through the common unassign contract, then reconnect former native
// roots to guidance. The caller must already have licensed the surviving
// population and a complete original-input restoration plan. This does not
// discharge reconstruction or remove dependent clauses; no proof is emitted.
void retract_query_roots(Kernel &);

} // namespace session
} // namespace tactix

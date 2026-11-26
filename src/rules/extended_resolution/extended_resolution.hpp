#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Signed, permanent external literals. These values also cross the C API.
enum class ExtensionGate : uint8_t { And = 1, Or = 2, Xor = 3, Ite = 4 };
struct ExtensionDefinition {
  ExtensionGate gate = ExtensionGate::And;
  int32_t left = 0, right = 0;
};

RuleEligibility extended_resolution_guard(const Kernel &);
bool extended_resolution_valid(const Kernel &, const ExtensionDefinition &);
// Invoke through registry structured dispatch. A successful call returns the
// positive external identity; zero means a mutation-free decline. Each call
// defines a fresh variable, even when the same expression was defined before.
int32_t extended_resolution_define(Kernel &, const ExtensionDefinition &);
// Shared publisher for automatic gate factoring; existing binary-definition
// payloads and the C API keep their layout.
int32_t extended_resolution_define_ite(Kernel &, int32_t selector,
                                       int32_t then_literal, int32_t else_literal);
// Post-assertion OR publication within ConflictAnalysis. The two operands
// are distinct live variables and the result is the positive internal literal.
Lit extended_resolution_define_search_or(Kernel &, Lit left, Lit right,
                                         bool search_support = false);

} // namespace tactix

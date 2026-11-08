#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Signed, permanent external literals. These values also cross the C API.
enum class ExtensionGate : uint8_t { And = 1, Or = 2, Xor = 3 };
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

} // namespace tactix

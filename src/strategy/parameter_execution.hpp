#pragma once

#include "kernel/kernel.hpp"

#include <type_traits>
#include <variant>

namespace tactix::execution {

// Most parameter families already use the same native save/install/restore
// implementation. Specialized families retain their existing scopes.
template <class Parameters> struct ParameterScopeFor {
  using type = strategy::ScopedRuleParameters<Parameters>;
};

template <> struct ParameterScopeFor<strategy::AnalyzeParameters> {
  using type = strategy::ScopedAnalyzeParameters;
};
template <> struct ParameterScopeFor<strategy::SubsumeForwardParameters> {
  using type = strategy::ScopedSubsumeForwardParameters;
};
template <> struct ParameterScopeFor<strategy::StrengthenParameters> {
  using type = strategy::ScopedStrengthenParameters;
};
template <> struct ParameterScopeFor<strategy::CongruenceParameters> {
  using type = strategy::ScopedCongruenceParameters;
};
template <> struct ParameterScopeFor<strategy::InstantiateParameters> {
  using type = strategy::ScopedInstantiateParameters;
};
template <> struct ParameterScopeFor<strategy::BackboneParameters> {
  using type = strategy::ScopedBackboneParameters;
};
template <> struct ParameterScopeFor<strategy::VivifyParameters> {
  using type = strategy::ScopedVivifyParameters;
};
template <> struct ParameterScopeFor<strategy::WalkParameters> {
  using type = strategy::ScopedWalkParameters;
};

// The calling policy names the override set and chooses its lifetime. Hold
// only the selected native scope, with no allocation or changed publication.
// Alternatives outside that set stay inherited, as at the original call site.
template <class... Parameters> class ParameterExecutionScope {
public:
  template <class Variant>
  ParameterExecutionScope(Kernel &k, const Variant &parameters) {
    std::visit([&](const auto &p) {
      using P = std::decay_t<decltype(p)>;
      if constexpr ((std::is_same_v<P, Parameters> || ...))
        scope_.template emplace<typename ParameterScopeFor<P>::type>(k, p);
    }, parameters);
  }
  bool active() const { return scope_.index() != 0; }
  ParameterExecutionScope(const ParameterExecutionScope &) = delete;
  ParameterExecutionScope &operator=(const ParameterExecutionScope &) = delete;

private:
  std::variant<std::monostate, typename ParameterScopeFor<Parameters>::type...>
      scope_;
};

} // namespace tactix::execution

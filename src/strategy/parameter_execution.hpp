#pragma once

#include "kernel/kernel.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/policy_completion.hpp"
#include "strategy/work_execution.hpp"

#include <type_traits>
#include <variant>

namespace tactix::execution {

// Native definition discovery retains one generator session across all BVE
// rounds of the enclosing phase. The policy chooses that boundary, not doses.
struct BeginBveDefinitionSessionRequest {};
inline void execute_action(Kernel &k, BeginBveDefinitionSessionRequest) {
  k.bve_definition_session = k.bve_definition_session + 1;
}
inline void execute_action(Kernel &k, BeginBveDefinitionSessionRequest request,
                           PolicyCompletion *completion) {
  (void)complete_action(completion, FeedbackKind::Support,
      "BeginBveDefinitionSession", INSTANCE_CANONICAL, [&] {
        NativeFeedbackSpec spec;
        spec.description.description = "session=" + std::to_string(k.bve_definition_session) +
            ";next=" + std::to_string(k.bve_definition_session + 1);
        return spec;
      }, [&] {
        execute_action(k, request);
        return ApplyResult::Ok;
      });
}

inline void execute_action(Kernel &k, const MaterialSettingsRequest &request,
                           PolicyCompletion *completion) {
  (void)complete_action(completion, FeedbackKind::Support,
      "InstallMaterialSettings", INSTANCE_CANONICAL, [&] {
        NativeFeedbackSpec spec;
        const auto selected = [](std::optional<bool> value) {
          return value ? (*value ? "1" : "0") : "inherit";
        };
        spec.description.description = std::string("touch-filter=") + selected(request.touch_filter) +
            ";likely-kept-redundant=" + selected(request.likely_kept_redundant);
        return spec;
      }, [&] {
        execute_action(k, request);
        return ApplyResult::Ok;
      });
}

// Policies choose complete or inherited profiles; permanent and scoped
// installation use the same native family setters without changing validation.
template <class Parameters> struct ParameterInstallationRequest {
  const Parameters &parameters;
};
template <class Parameters>
void execute_action(Kernel &k,
                    const ParameterInstallationRequest<Parameters> &request) {
  strategy::install_rule_parameters(k, request.parameters);
}

// Parameter installation is support, not an application of the named rule.
// Record the selected typed profile; disabled completion avoids its copy.
template <class Parameters>
void execute_action(Kernel &k,
                    const ParameterInstallationRequest<Parameters> &request,
                    PolicyCompletion *completion, const char *rule,
                    uint64_t instance = INSTANCE_CANONICAL) {
  (void)complete_action(completion, FeedbackKind::Support,
      "InstallRuleParameters", instance, [&] {
        NativeFeedbackSpec spec;
        spec.description.description = "install-permanent-profile";
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        auto &inv = spec.description.invocation.emplace();
        inv.rule = rule;
        inv.parameters = request.parameters;
        inv.granularity = strategy::InvocationGranularity::CanonicalApplication;
        if (instance != INSTANCE_CANONICAL) {
          inv.granularity = strategy::InvocationGranularity::OneInstance;
          inv.instances.push_back(instance);
        }
        return spec;
      }, [&] {
        execute_action(k, request);
        return ApplyResult::Ok;
      });
}

// The persistent phase profile owns only guidance, leaving the caller's Walk
// seed and native work ceiling intact. Borrow the curve without an extra copy.
// The record carries the permanent Walk profile a client installs: the
// guidance, with the per-application budget, seed and candidate at zero.
struct WalkGuidanceInstallationRequest {
  uint32_t even_break_base_milli;
  const std::vector<strategy::WalkParameters::ScorePoint> &score_curve;
};
inline void execute_action(Kernel &k,
                           const WalkGuidanceInstallationRequest &request) {
  strategy::install_walk_guidance(k, request.even_break_base_milli,
                                  request.score_curve);
}

inline void execute_action(Kernel &k,
                           const WalkGuidanceInstallationRequest &request,
                           PolicyCompletion *completion) {
  (void)complete_action(completion, FeedbackKind::Support,
      "InstallWalkGuidance", INSTANCE_CANONICAL, [&] {
        NativeFeedbackSpec spec;
        spec.description.description = "install-guidance-only;inherit-seed-and-budget";
        spec.description.effort_currency = "ticks";
        spec.meter = [](const Kernel &kernel) { return kernel.stats.ticks; };
        auto &inv = spec.description.invocation.emplace();
        inv.rule = "Walk";
        strategy::WalkParameters selected;
        selected.step_budget = 0;
        selected.even_break_base_milli = request.even_break_base_milli;
        selected.score_curve = request.score_curve;
        inv.parameters = std::move(selected);
        return spec;
      }, [&] {
        execute_action(k, request);
        return ApplyResult::Ok;
      });
}

// Most parameter families already use the same native save/install/restore
// implementation. Specialized families retain their existing scopes.
template <class Parameters> struct ParameterScopeFor {
  using type = strategy::ScopedRuleParameters<Parameters>;
};

// Affine profiles share native basis storage. Restore both profiles and the
// explicit dispatch bit, so nested legacy calls cannot inherit persistence.
template<class P> class AffineProfileScope {
public:
  AffineProfileScope(Kernel &k,const P &p)
      : kernel_(k), basis_(k.affine_consequence_parameters),
        persistent_(k.affine_matrix_initialize_parameters),
        active_(k.affine_matrix_initialize_parameters_active) {
    strategy::install_rule_parameters(k,p);
  }
  ~AffineProfileScope() {
    kernel_.affine_consequence_parameters=basis_;
    kernel_.affine_matrix_initialize_parameters=persistent_;
    kernel_.affine_matrix_initialize_parameters_active=active_;
  }
  AffineProfileScope(const AffineProfileScope &)=delete;
  AffineProfileScope &operator=(const AffineProfileScope &)=delete;
private:
  Kernel &kernel_;
  strategy::AffineConsequenceParameters basis_;
  strategy::AffineMatrixInitializeParameters persistent_;
  bool active_;
};
template<> struct ParameterScopeFor<strategy::AffineConsequenceParameters> {
  using type=AffineProfileScope<strategy::AffineConsequenceParameters>;
};
template<> struct ParameterScopeFor<strategy::AffineMatrixInitializeParameters> {
  using type=AffineProfileScope<strategy::AffineMatrixInitializeParameters>;
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

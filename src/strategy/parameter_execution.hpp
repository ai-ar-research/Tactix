#pragma once

#include "kernel/kernel.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/policy_completion.hpp"
#include "strategy/work_execution.hpp"

#include <type_traits>
#include <variant>

namespace tactix::execution {

// One definition generator session across a phase's BVE rounds.
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

// Permanent and scoped installs use the same native family setters.
template <class Parameters> struct ParameterInstallationRequest {
  const Parameters &parameters;
};
template <class Parameters>
ParameterInstallationRequest(const Parameters &)
    -> ParameterInstallationRequest<Parameters>;
template <class Parameters>
void execute_action(Kernel &k,
                    const ParameterInstallationRequest<Parameters> &request) {
  strategy::install_rule_parameters(k, request.parameters);
}

// Installation is support, not a rule application.
template <class Parameters>
void execute_action(Kernel &k,
                    const ParameterInstallationRequest<Parameters> &request,
                    PolicyCompletion *completion, const char *rule,
                    uint64_t instance = INSTANCE_CANONICAL) {
  (void)complete_action(completion, FeedbackKind::Support,
      "InstallRuleParameters", instance, [&] {
        NativeFeedbackSpec spec = ticks_feedback("install-permanent-profile");
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
        NativeFeedbackSpec spec =
            ticks_feedback("install-guidance-only;inherit-seed-and-budget");
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

// Families share native save/install/restore; some keep their scopes.
template <class Parameters> struct ParameterScopeFor {
  using type = strategy::ScopedRuleParameters<Parameters>;
};

// Affine profiles share basis storage; restore both and the dispatch bit.
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

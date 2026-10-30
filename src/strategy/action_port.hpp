#pragma once

// DefaultPolicy's action port (docs/dev/controller-interface.md, native search
// configuration and action port; plan §5.6, §8.4). default_policy.cpp leaves
// its translation unit only through `port::` funnels: execute_action,
// complete_action, apply_rule*, propagate_apply, InprocessPhase,
// affine_matrix_search_*, subsume_forward_try_fastel, bve_apply_one,
// execute_substitute_rounds, and the scopes it holds around requests
// (TickSinkScope, WorkSettingsScope, ScopedRuleWorkBudget, VivifyDoseScope,
// ScopedReduceParameters, BveInvocationSession). Production binds them to the registered functions
// themselves (StaticPort, no forwarding layer). A twin translation unit defines
// TACTIX_DEFAULT_POLICY_PUBLIC_PORT (PublicPort, tests/default_public_port.hpp)
// or TACTIX_DEFAULT_POLICY_HARDCODED_PORT (HardcodedPort) before including
// default_policy.cpp. Include this header after the executor headers it names.

#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/substitute_execution.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/phase_execution.hpp"
#include "strategy/policy_completion.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/vivify_execution.hpp"
#include "strategy/work_execution.hpp"

#include <concepts>
#include <functional>
#include <utility>

namespace tactix::port {

template <class P>
concept ActionPort = requires(Kernel &k, const RuleVTable &rule, uint64_t instance,
                              const std::function<ApplyResult()> &apply) {
  { P::apply_rule(rule, k) } -> std::same_as<ApplyResult>;
  { P::apply_rule_instance(rule, k, instance) } -> std::same_as<ApplyResult>;
  { P::apply_rule_semantic_instance(rule, k, instance) } -> std::same_as<ApplyResult>;
  { P::apply_rule_structured(rule, k, "", apply) } -> std::same_as<ApplyResult>;
  { P::propagate_apply(k) } -> std::same_as<ApplyResult>;
  { P::execute(k, execution::CloseStartupCertificateRequest{}) };
  typename P::Phase;
};

struct StaticPort {
  using Phase = execution::InprocessPhase;
  static ApplyResult propagate_apply(Kernel &k) { return ::tactix::propagate_apply(k); }
  static AffineMatrixInitResult affine_matrix_search_initialize(
      Kernel &k, AffineMatrixSearchSession &session,
      const strategy::AffineConsequenceParameters &parameters,
      AffineMatrixAdmission admission = AffineMatrixAdmission::AuthenticatedSubimage,
      const AffineMatrixPersistenceLimits &limits = {}) {
    return ::tactix::affine_matrix_search_initialize(k, session, parameters, admission, limits);
  }
  static AffineMatrixInitResult affine_matrix_search_refresh(
      Kernel &k, AffineMatrixSearchSession &session,
      const strategy::AffineConsequenceParameters &parameters,
      const AffineMatrixPersistenceLimits &limits, bool recognize) {
    return ::tactix::affine_matrix_search_refresh(k, session, parameters, limits, recognize);
  }
  static AffineMatrixServiceResult affine_matrix_search_step(Kernel &k,
                                                             AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_search_step(k, session);
  }
  static AffineMatrixServiceResult affine_matrix_search_step_deferred(
      Kernel &k, AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_search_step_deferred(k, session);
  }
  static AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
      Kernel &k, AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_search_step_detached_minimal(k, session);
  }
  static AffineMatrixServiceResult affine_matrix_search_step_detached_source(
      Kernel &k, AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_search_step_detached_source(k, session);
  }
  static AffineMatrixServiceResult affine_matrix_search_step_lazy_source(
      Kernel &k, AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_search_step_lazy_source(k, session);
  }
  static AffineMatrixServiceResult affine_matrix_materialize_reason(
      Kernel &k, AffineMatrixSearchSession &session) {
    return ::tactix::affine_matrix_materialize_reason(k, session);
  }
  static void affine_matrix_search_release(AffineMatrixSearchSession &session) {
    session.disable();
  }
  static void affine_matrix_search_release(AffineMatrixSearchSession &session, Kernel &k) {
    session.disable(k);
  }
  static ApplyResult subsume_forward_try_fastel(Kernel &k, Ref candidate, uint32_t limit,
                                                std::vector<uint8_t> &marks) {
    return ::tactix::subsume_forward_try_fastel(k, candidate, limit, marks);
  }
  static strategy::InvocationResult bve_apply_one(strategy::BveInvocationSession &session,
                                                  uint64_t instance) {
    return session.apply_one(instance);
  }
  template <class Publisher>
  static execution::SubstituteRoundsResult execute_substitute_rounds(
      Kernel &k, const strategy::SubstituteParameters &parameters, Publisher &&publish) {
    return execution::execute_substitute_rounds(k, parameters,
                                                std::forward<Publisher>(publish));
  }
  static ApplyResult apply_rule(const RuleVTable &rule, Kernel &k) {
    return ::tactix::apply_rule(rule, k);
  }
  static ApplyResult apply_rule_instance(const RuleVTable &rule, Kernel &k,
                                         uint64_t instance) {
    return ::tactix::apply_rule_instance(rule, k, instance);
  }
  static ApplyResult apply_rule_semantic_instance(const RuleVTable &rule, Kernel &k,
                                                  uint64_t instance) {
    return ::tactix::apply_rule_semantic_instance(rule, k, instance);
  }
  static ApplyResult apply_rule_structured(const RuleVTable &rule, Kernel &k,
                                           const char *description,
                                           const std::function<ApplyResult()> &apply) {
    return ::tactix::apply_rule_structured(rule, k, description, apply);
  }
  // Argument-dependent lookup at the policy's call site finds the executor
  // overloads declared after this header, as the qualified call does there.
  template <class... Args> static decltype(auto) execute(Kernel &k, Args &&...args) {
    using execution::execute_action;
    return execute_action(k, std::forward<Args>(args)...);
  }
  template <class... Args> static decltype(auto) complete(Args &&...args) {
    return execution::complete_action(std::forward<Args>(args)...);
  }
};
static_assert(ActionPort<StaticPort>);

// Registry dispatch less its accounting: the same startup-boundary closure,
// semantic-variable candidate bracket, rule call and strategy epoch as
// dispatch_rule (rules/registry.cpp), without the trace scope, rule timing
// or rule_calls. Trajectory equals StaticPort; only those counters differ.
struct HardcodedPort : StaticPort {
  template <class Apply> static ApplyResult dispatch(Kernel &k, Apply &&apply) {
    strategy::begin_semantic_variable_candidate_action(k);
    const ApplyResult result = apply();
    ++k.strategy_epoch;
    strategy::finish_semantic_variable_candidate_action(k, result);
    return result;
  }
  static void close_startup(const RuleVTable &rule, Kernel &k, uint64_t instance) {
    if (!rule.startup_instance || !rule.startup_instance(instance))
      execution::execute_action(k, execution::CloseStartupCertificateRequest{});
  }
  static ApplyResult apply_rule(const RuleVTable &rule, Kernel &k) {
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
    return dispatch(k, [&] { return rule.apply(k); });
  }
  static ApplyResult apply_rule_instance(const RuleVTable &rule, Kernel &k,
                                         uint64_t instance) {
    close_startup(rule, k, instance);
    return dispatch(k, [&] { return rule.apply_instance(k, instance); });
  }
  static ApplyResult apply_rule_semantic_instance(const RuleVTable &rule, Kernel &k,
                                                  uint64_t instance) {
    close_startup(rule, k, instance);
    return dispatch(k, [&] { return rule.apply_semantic_instance(k, instance); });
  }
  static ApplyResult apply_rule_structured(const RuleVTable &, Kernel &k, const char *,
                                           const std::function<ApplyResult()> &apply) {
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
    return dispatch(k, apply);
  }
};
static_assert(ActionPort<HardcodedPort>);

#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT) || defined(TACTIX_DEFAULT_POLICY_HARDCODED_PORT)
#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
struct PublicPort; // complete before default_policy.cpp's body
using SelectedPort = PublicPort;
#else
using SelectedPort = HardcodedPort;
#endif
// Templates, so the selected port need only be complete where the policy calls.
template <class P = SelectedPort>
ApplyResult apply_rule(const RuleVTable &rule, Kernel &k) {
  return P::apply_rule(rule, k);
}
template <class P = SelectedPort>
ApplyResult apply_rule_instance(const RuleVTable &rule, Kernel &k, uint64_t instance) {
  return P::apply_rule_instance(rule, k, instance);
}
template <class P = SelectedPort>
ApplyResult apply_rule_semantic_instance(const RuleVTable &rule, Kernel &k,
                                         uint64_t instance) {
  return P::apply_rule_semantic_instance(rule, k, instance);
}
template <class P = SelectedPort>
ApplyResult apply_rule_structured(const RuleVTable &rule, Kernel &k, const char *description,
                                  const std::function<ApplyResult()> &apply) {
  return P::apply_rule_structured(rule, k, description, apply);
}
template <class P = SelectedPort> ApplyResult propagate_apply(Kernel &k) {
  return P::propagate_apply(k);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_initialize(Kernel &k, Args &&...args) {
  return P::affine_matrix_search_initialize(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_refresh(Kernel &k, Args &&...args) {
  return P::affine_matrix_search_refresh(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_step(Kernel &k, Args &&...args) {
  return P::affine_matrix_search_step(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_step_deferred(Kernel &k, Args &&...args) {
  return P::affine_matrix_search_step_deferred(k,
                                                std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_step_detached_minimal(Kernel &k,
                                                          Args &&...args) {
  return P::affine_matrix_search_step_detached_minimal(
      k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_step_detached_source(Kernel &k,
                                                         Args &&...args) {
  return P::affine_matrix_search_step_detached_source(
      k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_search_step_lazy_source(Kernel &k,
                                                      Args &&...args) {
  return P::affine_matrix_search_step_lazy_source(
      k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) affine_matrix_materialize_reason(Kernel &k, Args &&...args) {
  return P::affine_matrix_materialize_reason(k,
                                              std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
void affine_matrix_search_release(AffineMatrixSearchSession &session, Args &&...args) {
  P::affine_matrix_search_release(session, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
execution::SubstituteRoundsResult execute_substitute_rounds(Kernel &k, Args &&...args) {
  return P::execute_substitute_rounds(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort>
strategy::InvocationResult bve_apply_one(strategy::BveInvocationSession &session,
                                         uint64_t instance) {
  return P::bve_apply_one(session, instance);
}
template <class P = SelectedPort, class... Args>
ApplyResult subsume_forward_try_fastel(Kernel &k, Args &&...args) {
  return P::subsume_forward_try_fastel(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) execute_action(Kernel &k, Args &&...args) {
  return P::execute(k, std::forward<Args>(args)...);
}
template <class P = SelectedPort, class... Args>
decltype(auto) complete_action(Args &&...args) {
  return P::complete(std::forward<Args>(args)...);
}
#if defined(TACTIX_DEFAULT_POLICY_HARDCODED_PORT)
using InprocessPhase = HardcodedPort::Phase;
using execution::TickSinkScope;
using execution::WorkSettingsScope;
using execution::VivifyDoseScope;
using strategy::ScopedRuleWorkBudget;
using strategy::ScopedReduceParameters;
using strategy::BveInvocationSession;
#endif // the public port declares port::InprocessPhase and the scopes
#else
using ::tactix::apply_rule;
using ::tactix::apply_rule_instance;
using ::tactix::apply_rule_semantic_instance;
using ::tactix::apply_rule_structured;
using ::tactix::propagate_apply;
using ::tactix::affine_matrix_search_initialize;
using ::tactix::affine_matrix_search_refresh;
using ::tactix::affine_matrix_search_step;
using ::tactix::affine_matrix_search_step_deferred;
using ::tactix::affine_matrix_search_step_detached_minimal;
using ::tactix::affine_matrix_search_step_detached_source;
using ::tactix::affine_matrix_search_step_lazy_source;
using ::tactix::affine_matrix_materialize_reason;
using ::tactix::subsume_forward_try_fastel;
using execution::execute_substitute_rounds;
inline strategy::InvocationResult bve_apply_one(strategy::BveInvocationSession &session,
                                                uint64_t instance) {
  return session.apply_one(instance);
}
inline void affine_matrix_search_release(AffineMatrixSearchSession &session) {
  session.disable();
}
inline void affine_matrix_search_release(AffineMatrixSearchSession &session, Kernel &k) {
  session.disable(k);
}
using execution::complete_action;
using execution::execute_action;
using execution::InprocessPhase;
using execution::TickSinkScope;
using execution::WorkSettingsScope;
using execution::VivifyDoseScope;
using strategy::ScopedRuleWorkBudget;
using strategy::ScopedReduceParameters;
using strategy::BveInvocationSession;
#endif

} // namespace tactix::port

#pragma once

// PublicPort: DefaultPolicy's action port over tactix.h only
// (the controller-interface design; strategy/action_port.hpp). The policy runs on
// a bound C session's kernel; each action with a public request is issued as
// that request (tactix_apply*, tactix_decide, tactix_install_*,
// tactix_support). Others run through StaticPort and are counted as fallbacks
// by request type; a refused public request (-1, no state change) is counted
// as declined and then runs statically. With a feedback receiver attached the
// recorded overloads run statically.

#ifndef TACTIX_DEFAULT_POLICY_PUBLIC_PORT
#error "define TACTIX_DEFAULT_POLICY_PUBLIC_PORT before including default_public_port.hpp"
#endif

// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "api/api_internal.hpp"
#include "policies/heuristics.hpp"
#include "strategy/action_port.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/heuristic_execution.hpp"
#include "strategy/conflict_analysis_execution.hpp"
#include "strategy/lucky_execution.hpp"
#include "rules/lucky/lucky.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/root_execution.hpp"
#include "strategy/rule_execution.hpp"
#include "strategy/slot_execution.hpp"
#include "strategy/sweep_execution.hpp"
#include "strategy/vivify_execution.hpp"
#include "strategy/work_execution.hpp"
#include "public_parameter_encoding.hpp"

#include <cstdlib>
#include "public_online_encoding.hpp"

#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace tactix::port {

struct PublicCensus {
  uint64_t total_calls = 0;                      // every counted tactix.h request
  // Per-entry detail is a string-keyed map: costly per call, so a benchmark
  // client (tactix-public) disables it and keeps only the total.
  bool detailed = true;
  std::map<std::string, uint64_t> public_calls; // tactix.h entry -> calls
  std::map<std::string, uint64_t> fallbacks;    // request kind without a public form
  std::map<std::string, uint64_t> declined;     // public request refused (-1)
  std::map<std::string, uint64_t> vacuous;      // refused no-op (nothing to apply)
  std::map<std::string, uint64_t> declared;     // declared non-action or composition
  uint64_t timed_dispatches = 0; // registry dispatches through tactix.h with a timing slot
  void clear() { const bool keep = detailed; *this = PublicCensus{}; detailed = keep; }
};

template <class T> std::string port_type_name() {
  std::string_view f = __PRETTY_FUNCTION__;
  const size_t at = f.find("T = ");
  if (at == std::string_view::npos) return std::string(f);
  f.remove_prefix(at + 4);
  const size_t end = f.find_first_of(";]");
  std::string out(f.substr(0, end));
  for (std::string_view prefix : {"tactix::execution::", "tactix::"})
    if (out.compare(0, prefix.size(), prefix) == 0) out.erase(0, prefix.size());
  return out;
}

template <class T> struct is_parameter_installation : std::false_type {};
template <class P>
struct is_parameter_installation<execution::ParameterInstallationRequest<P>>
    : std::true_type {};

class PublicInprocessPhase;
class PublicTickSinkScope;

struct PublicPort {
  using Phase = PublicInprocessPhase;
  static inline tactix_c_handle *session = nullptr;
  static inline PublicCensus census;
  // Route everything statically (identity of the census-free arms).
  static inline bool bypass = false;
  // A retirement-publication hold this port opened around a Sweep step.
  static inline bool retirements_held = false;
  static void release_retirements(tactix_c_handle *s, int mode) {
    if (!retirements_held) return;
    const int code = support(s, TACTIX_SUPPORT_HOLD_RETIREMENTS, static_cast<uint64_t>(mode));
    assert(code == 0);
    (void)code;
    retirements_held = false;
  }

  static tactix_c_handle *bound([[maybe_unused]] Kernel &k) {
    if (bypass || !session) return nullptr;
    assert(&api_detail::kernel_for_fixed_testing(session) == &k);
    return session;
  }
  static void counted(const char *entry) {
    ++census.total_calls;
    if (census.detailed) ++census.public_calls[entry];
  }
  // A dispatch through tactix.h that charges the rule's timing slot.
  static void timed(int id) {
    if (id >= 0 && static_cast<uint32_t>(id) < Stats::MAX_TIMED_RULES) ++census.timed_dispatches;
  }
  static ApplyResult result(int code) {
    switch (code) {
    case 0: return ApplyResult::Ok;
    case 1: return ApplyResult::NoOp;
    case 2: return ApplyResult::Conflict;
    case 10: return ApplyResult::Sat;
    case 20: return ApplyResult::Unsat;
    }
    std::abort();
  }

  // Registry dispatch: the public apply checks the guard first and rejects a
  // payload the rule has no instance surface for, without changing state.
  template <class Static>
  static ApplyResult dispatch(const RuleVTable &rule, [[maybe_unused]] Kernel &k,
                              const char *entry,
                              int code, Static &&fallback) {
    if (code == -1) {
      ++census.declined[std::string(entry) + ":" + rule.name];
      return fallback();
    }
    counted(entry);
    if (rule.id < Stats::MAX_TIMED_RULES) ++census.timed_dispatches;
    return result(code);
  }
  static ApplyResult apply_rule(const RuleVTable &rule, Kernel &k) {
    tactix_c_handle *s = bound(k);
    if (!s) return StaticPort::apply_rule(rule, k);
    return dispatch(rule, k, "tactix_apply", tactix_apply(s, static_cast<int>(rule.id)),
                    [&] { return StaticPort::apply_rule(rule, k); });
  }
  static ApplyResult apply_rule_instance(const RuleVTable &rule, Kernel &k,
                                         uint64_t instance) {
    tactix_c_handle *s = bound(k);
    if (!s) return StaticPort::apply_rule_instance(rule, k, instance);
    return dispatch(rule, k, "tactix_apply_instance",
                    tactix_apply_instance(s, static_cast<int>(rule.id), instance),
                    [&] { return StaticPort::apply_rule_instance(rule, k, instance); });
  }
  static ApplyResult apply_rule_semantic_instance(const RuleVTable &rule, Kernel &k,
                                                  uint64_t instance) {
    tactix_c_handle *s = bound(k);
    if (!s) return StaticPort::apply_rule_semantic_instance(rule, k, instance);
    return dispatch(rule, k, "tactix_apply_semantic_instance",
                    tactix_apply_semantic_instance(s, static_cast<int>(rule.id), instance),
                    [&] { return StaticPort::apply_rule_semantic_instance(rule, k, instance); });
  }
  // M1 Propagate; the public guard refuses a fixpoint, where the native call
  // is a no-op too.
  static ApplyResult propagate_apply(Kernel &k) {
    tactix_c_handle *s = bound(k);
    if (!s) return StaticPort::propagate_apply(k);
    static const int id = rule_id("Propagate");
    const int code = tactix_apply(s, id);
    if (code == -1) {
      ++(kernel_propagation_fixpoint(k) && !k.conflict_valid ? census.vacuous
                                                              : census.declined)["Propagate"];
      return StaticPort::propagate_apply(k);
    }
    counted("tactix_apply");
    timed(id);
    return result(code);
  }
  // The affine search session: each operation installs its profile
  // (tactix_install_affine_matrix_profile), applies AffineMatrixInitialize,
  // reads the typed result back from the session's public observation and
  // reinstalls the profile the policy holds.
  using AffineProfile = strategy::AffineMatrixInitializeParameters;
  struct InstalledAffine {
    bool active;
    AffineProfile profile;
    strategy::AffineConsequenceParameters basis;
  };
  static bool install_affine(tactix_c_handle *s, const AffineProfile &profile) {
    public_encoding::Encoded basis;
    if (!public_encoding::encode(strategy::InprocessParameters(profile.basis), basis))
      return false;
    tactix_online_affine_matrix_initialize_parameters c{};
    c.basis = basis.parameters.value.affine_consequence;
    c.admission = static_cast<uint32_t>(profile.admission);
    c.operation = static_cast<uint32_t>(profile.operation);
    c.maximum_retained_bytes = profile.maximum_retained_bytes;
    c.maximum_deferred_deletions = profile.maximum_deferred_deletions;
    c.maximum_deferred_literal_work = profile.maximum_deferred_literal_work;
    return tactix_install_affine_matrix_profile(s, &c) == 0;
  }
  static void restore_affine(tactix_c_handle *s, const InstalledAffine &held) {
    if (held.active) {
      const bool ok = install_affine(s, held.profile);
      assert(ok);
      (void)ok;
      return;
    }
    public_encoding::Encoded basis;
    const bool ok = public_encoding::encode(strategy::InprocessParameters(held.basis), basis) &&
                    tactix_install_parameters(s, rule_id("AffineMatrixInitialize"),
                                              &basis.parameters) == 0;
    assert(ok);
    (void)ok;
  }
  template <class Read, class Static>
  static auto affine_operation(Kernel &k, const AffineProfile &profile, const char *what,
                               Read &&read, Static &&fallback) -> decltype(fallback()) {
    tactix_c_handle *s = bound(k);
    if (!s) return fallback();
    const InstalledAffine held{k.affine_matrix_initialize_parameters_active,
                               k.affine_matrix_initialize_parameters,
                               k.affine_consequence_parameters};
    if (!install_affine(s, profile)) {
      ++census.fallbacks[std::string("affine-profile:") + what];
      return fallback();
    }
    const int code = tactix_apply(s, rule_id("AffineMatrixInitialize"));
    restore_affine(s, held);
    if (code == -1) {
      ++census.declined[std::string("tactix_apply:AffineMatrixInitialize:") + what];
      return fallback();
    }
    counted("tactix_apply:AffineMatrixInitialize");
    timed(rule_id("AffineMatrixInitialize"));
    return read();
  }
  static AffineMatrixInitResult affine_matrix_search_initialize(
      Kernel &k, AffineMatrixSearchSession &session,
      const strategy::AffineConsequenceParameters &parameters,
      AffineMatrixAdmission admission = AffineMatrixAdmission::AuthenticatedSubimage,
      const AffineMatrixPersistenceLimits &limits = {}) {
    AffineProfile profile;
    profile.basis = parameters;
    profile.admission = admission == AffineMatrixAdmission::CompleteSnapshot
                            ? AffineProfile::Admission::CompleteSnapshot
                            : AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::Initialize;
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "initialize",
        [&] { return *session.last_initialization(); },
        [&] { return StaticPort::affine_matrix_search_initialize(k, session, parameters,
                                                                 admission, limits); });
  }
  static AffineMatrixInitResult affine_matrix_search_refresh(
      Kernel &k, AffineMatrixSearchSession &session,
      const strategy::AffineConsequenceParameters &parameters,
      const AffineMatrixPersistenceLimits &limits, bool recognize) {
    AffineProfile profile;
    profile.basis = parameters;
    profile.operation = recognize ? AffineProfile::Operation::Refresh
                                  : AffineProfile::Operation::RootMaintenance;
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "refresh",
        [&] { return *session.last_initialization(); },
        [&] { return StaticPort::affine_matrix_search_refresh(k, session, parameters, limits,
                                                              recognize); });
  }
  static AffineMatrixServiceResult affine_matrix_search_step(Kernel &k,
                                                             AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = session.persistent() ? AffineProfile::Admission::AuthenticatedSubimage
                                             : AffineProfile::Admission::CompleteSnapshot;
    profile.operation = AffineProfile::Operation::SearchStep;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "step", [&] { return *session.last_service(); },
        [&] { return StaticPort::affine_matrix_search_step(k, session); });
  }
  static AffineMatrixServiceResult affine_matrix_search_step_deferred(
      Kernel &k, AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::DeferredSearchStep;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "deferred-step",
        [&] { return *session.last_service(); },
        [&] { return StaticPort::affine_matrix_search_step_deferred(k, session); });
  }
  static AffineMatrixServiceResult affine_matrix_search_step_detached_minimal(
      Kernel &k, AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::DetachedMinimalSearchStep;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    const auto result = affine_operation(k, profile, "detached-minimal-step",
        [&] { return *session.last_service(); },
        [&] {
          return StaticPort::affine_matrix_search_step_detached_minimal(k,
                                                                        session);
        });
    if (tactix_c_handle *s = bound(k)) {
      tactix_affine_matrix_result observed{};
      if (tactix_read_affine_matrix_result(s, &observed) != 0 ||
          observed.reason_detached != result.live.detached_reason)
        std::abort();
    }
    return result;
  }
  static AffineMatrixServiceResult affine_matrix_search_step_detached_source(
      Kernel &k, AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::DetachedSourceSearchStep;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    const auto result = affine_operation(k, profile, "detached-source-step",
        [&] { return *session.last_service(); },
        [&] {
          return StaticPort::affine_matrix_search_step_detached_source(k,
                                                                       session);
        });
    if (tactix_c_handle *s = bound(k)) {
      tactix_affine_matrix_result observed{};
      if (tactix_read_affine_matrix_result(s, &observed) != 0 ||
          observed.reason_detached != result.live.detached_reason)
        std::abort();
    }
    return result;
  }
  static AffineMatrixServiceResult affine_matrix_search_step_lazy_source(
      Kernel &k, AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::LazySourceSearchStep;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "lazy-source-step",
        [&] { return *session.last_service(); },
        [&] {
          return StaticPort::affine_matrix_search_step_lazy_source(k,
                                                                   session);
        });
  }
  static AffineMatrixServiceResult affine_matrix_materialize_reason(
      Kernel &k, AffineMatrixSearchSession &session) {
    AffineProfile profile;
    profile.basis = session.parameters();
    profile.admission = AffineProfile::Admission::AuthenticatedSubimage;
    profile.operation = AffineProfile::Operation::MaterializeReasons;
    const auto &limits = session.persistence_limits();
    profile.maximum_retained_bytes = limits.maximum_retained_bytes;
    profile.maximum_deferred_deletions = limits.maximum_deferred_deletions;
    profile.maximum_deferred_literal_work = limits.maximum_deferred_literal_work;
    return affine_operation(k, profile, "materialize",
        [&] { return *session.last_service(); },
        [&] { return StaticPort::affine_matrix_materialize_reason(k, session); });
  }
  static void affine_matrix_search_release(AffineMatrixSearchSession &affine) {
    release_affine(affine, nullptr);
  }
  static void affine_matrix_search_release(AffineMatrixSearchSession &affine, Kernel &k) {
    release_affine(affine, &k);
  }
  static void release_affine(AffineMatrixSearchSession &affine, Kernel *k) {
    tactix_c_handle *s = bypass ? nullptr : session;
    if (s && (!k || &api_detail::kernel_for_fixed_testing(s) == k) &&
        api_detail::kernel_for_fixed_testing(s).affine_matrix_search_session == &affine &&
        support(s, TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX,
                k ? 0 : TACTIX_AFFINE_MATRIX_RELEASE_DETACHED) == 0)
      return;
    if (s) ++census.fallbacks["affine-release"];
    if (k) affine.disable(*k);
    else affine.disable();
  }
  static ApplyResult subsume_forward_try_fastel(Kernel &k, Ref candidate, uint32_t limit,
                                                std::vector<uint8_t> &marks) {
    tactix_c_handle *s = bound(k);
    if (!s || limit > 0x7fffffffu)
      return StaticPort::subsume_forward_try_fastel(k, candidate, limit, marks);
    static const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id("SubsumeForward"))];
    return dispatch(rule, k, "tactix_apply_instance:fastel",
        tactix_apply_instance(s, static_cast<int>(rule.id),
                              subsume_forward_fastel_instance(candidate, limit)),
        [&] { return StaticPort::subsume_forward_try_fastel(k, candidate, limit, marks); });
  }
  // A Substitute round batch: one online Substitute step under the batch's
  // parameters, work budget and material settings. The executor publishes each
  // round's roots and retirements (the policy's per-round publication) and
  // reports a round-effort break as an incomplete selection.
  template <class Publisher>
  static execution::SubstituteRoundsResult execute_substitute_rounds(
      Kernel &k, const strategy::SubstituteParameters &parameters, Publisher &&publish) {
    tactix_c_handle *s = bound(k);
    const auto fallback = [&] {
      if (s) ++census.fallbacks["SubstituteRounds"];
      return StaticPort::execute_substitute_rounds(k, parameters,
                                                   std::forward<Publisher>(publish));
    };
    if (!s || !parameters.max_rounds) return fallback();
    // The follow-up Substitute's round publication carries a held union.
    release_retirements(s, TACTIX_RETIREMENTS_RELEASE_ONLY);
    strategy::OnlineInvocation step;
    step.id = 1;
    step.rule = "Substitute";
    step.parameters = parameters;
    step.granularity = strategy::InvocationGranularity::CanonicalApplication;
    step.rule_work_budget = {kernel_has_work_budget(k) ? strategy::RuleWorkBudgetMode::Bounded
                                                       : strategy::RuleWorkBudgetMode::Unlimited,
                             k.work_budget};
    step.material.touch_filter = k.touch_filter ? strategy::SettingOverride::Enable
                                                : strategy::SettingOverride::Disable;
    step.material.likely_kept_redundant = k.touch_redundant ? strategy::SettingOverride::Enable
                                                            : strategy::SettingOverride::Disable;
    public_encoding::EncodedStep encoded;
    if (!public_encoding::encode_step(k, step, rule_id("Substitute"), encoded)) return fallback();
    const tactix_online_program_v2 program{&encoded.step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    tactix_online_result out{};
    if (tactix_online_configure_v2(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) ||
        tactix_online_advance(s, &out)) {
      ++census.declined["tactix_online:Substitute"];
      return StaticPort::execute_substitute_rounds(k, parameters,
                                                   std::forward<Publisher>(publish));
    }
    counted("tactix_online_advance:Substitute");
    timed(rule_id("Substitute"));
    execution::SubstituteRoundsResult result;
    result.result = static_cast<ApplyResult>(out.rule_result);
    result.round_budget_break = !out.selection_complete;
    return result;
  }
  // A BVE candidate of the session's score-heap pass: the session keeps its
  // scope, ordering and accounting; the application is the public instance.
  static strategy::InvocationResult bve_apply_one(strategy::BveInvocationSession &session_ref,
                                                  uint64_t instance) {
    tactix_c_handle *s = bypass ? nullptr : session;
    if (!s) return session_ref.apply_one(instance);
    static const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id("BVE"))];
    Kernel &k = api_detail::kernel_for_fixed_testing(s);
    return session_ref.apply_one_with(instance, [&] {
      return dispatch(rule, k, "tactix_apply_instance:BVE",
                      tactix_apply_instance(s, static_cast<int>(rule.id), instance),
                      [&] { return ::tactix::apply_rule_instance(rule, k, instance); });
    });
  }
  // A caller-composed body has no public request: Lucky 26/27 graph-labeling
  // continuations and the fastel occurrence flush.
  static ApplyResult apply_rule_structured(const RuleVTable &rule, Kernel &k,
                                           const char *description,
                                           const std::function<ApplyResult()> &apply) {
    // The fastel body is one public SubsumeForward instance (below), which
    // dispatches itself.
    if (bound(k) && std::string_view(description) == "fastel-occurrence") return apply();
    // A Substitute round batch is one online Substitute step (below).
    if (bound(k) && std::string_view(rule.name) == "Substitute" &&
        (std::string_view(description) == "rounds" ||
         std::string_view(description).starts_with("policy-complete-rounds=")))
      return apply();
    // The Sweep batch is one online Sweep step (below).
    if (bound(k) && std::string_view(rule.name) == "Sweep" &&
        std::string_view(description) == "budgeted-batch")
      return apply();
    // Lucky 26/27: the graph-labeling request below is the public instance.
    if (bound(k) && std::string_view(rule.name) == "Lucky" &&
        (std::string_view(description) == "26" || std::string_view(description) == "27"))
      return apply();
    if (bound(k)) ++census.fallbacks[std::string("apply_rule_structured:") + rule.name];
    return StaticPort::apply_rule_structured(rule, k, description, apply);
  }

  // The enclosing records: a support record's instance is its public
  // argument (RootPreparation flags, the Warmup phase).
  struct Frame {
    const char *identity; // compared only by record_argument, not per push
    uint64_t instance;
  };
  // Records nest only a few deep; a fixed stack keeps this off the per-call
  // allocation and bounds-checking path.
  static constexpr size_t MAX_FRAMES = 64;
  static inline Frame frames[MAX_FRAMES];
  static inline size_t frame_depth = 0;
  template <class Completion, class... Rest>
  static decltype(auto) complete(Completion &&completion, execution::FeedbackKind kind,
                                 const char *identity, uint64_t instance, Rest &&...rest) {
    if (frame_depth == MAX_FRAMES) std::abort();
    frames[frame_depth++] = Frame{identity, instance};
    struct Pop {
      ~Pop() { --frame_depth; }
    } pop;
    return execution::complete_action(std::forward<Completion>(completion), kind, identity,
                                      instance, std::forward<Rest>(rest)...);
  }
  static std::optional<uint64_t> record_argument(std::string_view identity) {
    if (!frame_depth || std::string_view(frames[frame_depth - 1].identity) != identity ||
        frames[frame_depth - 1].instance == INSTANCE_CANONICAL)
      return std::nullopt;
    return frames[frame_depth - 1].instance;
  }

  // Request arguments beyond the request itself that a public request can
  // omit: a null completion (no record) is the only one.
  template <class... Args> static constexpr bool bare =
      (std::is_convertible_v<const std::remove_cvref_t<Args> &,
                             const execution::PolicyCompletion *> && ...);
  static bool recorded() { return false; }
  template <class A, class... Rest> static bool recorded(const A &a, const Rest &...rest) {
    if constexpr (std::is_convertible_v<const A &, const execution::PolicyCompletion *>)
      if (static_cast<const execution::PolicyCompletion *>(a)) return true;
    return recorded(rest...);
  }

  static tactix_retier_schedule c_schedule(const RetierSchedule &in) {
    tactix_retier_schedule out{};
    out.enabled = in.enabled;
    out.next_conflicts = in.next_conflicts;
    out.interval = in.interval;
    out.max_interval = in.max_interval;
    out.growth = in.growth;
    out.tier1_default = in.tier1_default;
    out.tier2_default = in.tier2_default;
    out.tier1_relative_permille = in.tier1_relative_permille;
    out.tier2_relative_permille = in.tier2_relative_permille;
    for (int mode : {0, 1}) {
      out.tier1_by_mode[mode] = in.tier1_by_mode[mode];
      out.tier2_by_mode[mode] = in.tier2_by_mode[mode];
    }
    return out;
  }
  static RetierSchedule internal_schedule(const tactix_retier_schedule &in) {
    RetierSchedule out;
    out.enabled = in.enabled != 0;
    out.next_conflicts = in.next_conflicts;
    out.interval = in.interval;
    out.max_interval = in.max_interval;
    out.growth = in.growth;
    out.tier1_default = in.tier1_default;
    out.tier2_default = in.tier2_default;
    out.tier1_relative_permille = in.tier1_relative_permille;
    out.tier2_relative_permille = in.tier2_relative_permille;
    for (int mode : {0, 1}) {
      out.tier1_by_mode[mode] = in.tier1_by_mode[mode];
      out.tier2_by_mode[mode] = in.tier2_by_mode[mode];
    }
    return out;
  }
  static int setting(const std::optional<bool> &value) {
    return !value ? TACTIX_ONLINE_SETTING_INHERIT
           : *value ? TACTIX_ONLINE_SETTING_ENABLE
                    : TACTIX_ONLINE_SETTING_DISABLE;
  }
  static int rule_id(const char *name) {
    for (int id = 0; id < tactix_rule_count(); ++id)
      if (const char *rule = tactix_rule_name(id); rule && std::string_view(rule) == name)
        return id;
    return -1;
  }
  static int support(tactix_c_handle *s, int code, uint64_t argument) {
    const int out = tactix_support(s, code, argument);
    if (out != -1) counted("tactix_support");
    return out;
  }
  // A typed profile installed permanently (tactix_install_parameters).
  template <class Parameters>
  static bool install(tactix_c_handle *s, const char *rule, const Parameters &parameters) {
    public_encoding::Encoded encoded;
    const int id = rule_id(rule);
    if (id < 0 || !public_encoding::encode(strategy::InprocessParameters(parameters), encoded) ||
        tactix_install_parameters(s, id, &encoded.parameters) != 0)
      return false;
    counted("tactix_install_parameters");
    return true;
  }
  static ApplyResult vivify_band(Kernel &k, tactix_c_handle *s, const RuleVTable &rule,
                                 const execution::VivifyBandRequest &band);
  static tactix_work_settings work_settings(tactix_c_handle *s) {
    tactix_work_settings out{};
    const int code = tactix_read_work_settings(s, &out);
    assert(code == 0);
    (void)code;
    counted("tactix_read_work_settings");
    return out;
  }
  static void install_work(tactix_c_handle *s, const execution::WorkSettingsRequest &request) {
    tactix_work_settings in{};
    if (request.budget) in.fields |= TACTIX_WORK_BUDGET, in.budget = *request.budget;
    if (request.offset) in.fields |= TACTIX_WORK_OFFSET, in.offset = *request.offset;
    if (request.bounded) in.fields |= TACTIX_WORK_BOUNDED, in.bounded = *request.bounded;
    if (request.midround) in.fields |= TACTIX_WORK_MIDROUND, in.midround = *request.midround;
    if (!in.fields) return;
    const int code = tactix_install_work_settings(s, &in);
    assert(code == 0);
    (void)code;
    counted("tactix_install_work_settings");
  }

  // The public form of one request, or nullopt when none applies (the caller
  // then runs the static request). A refused request returns -1 there too.
  template <class Request, class... Args>
  static decltype(auto) execute(Kernel &k, Request &&request, Args &&...args) {
    using R = std::remove_cvref_t<Request>;
    using namespace execution;
    tactix_c_handle *s = bound(k);
    const auto fallback = [&]() -> decltype(auto) {
      if (s) ++census.fallbacks[port_type_name<R>()];
      return StaticPort::execute(k, std::forward<Request>(request), std::forward<Args>(args)...);
    };
    const auto declined = [&](const char *what) -> decltype(auto) {
      ++census.declined[what];
      return StaticPort::execute(k, std::forward<Request>(request), std::forward<Args>(args)...);
    };
    if (!s || recorded(args...)) {
      if (s && recorded(args...)) ++census.fallbacks["recorded:" + port_type_name<R>()];
      return StaticPort::execute(k, std::forward<Request>(request), std::forward<Args>(args)...);
    }
    if constexpr (std::is_same_v<R, DecisionRequest> && bare<Args...>) {
      if (request.origin == DecisionOrigin::Policy) {
        const int elit = kernel_externalize(k, request.literal);
        if (elit && tactix_decide(s, elit) == 0) {
          counted("tactix_decide");
          return ApplyResult::Ok;
        }
        return declined("tactix_decide");
      }
      return fallback();
    } else if constexpr (std::is_same_v<R, RegisteredRuleRequest> && bare<Args...>) {
      const RuleVTable &rule = request.rule;
      if (request.instance == INSTANCE_CANONICAL ||
          (!rule.apply_semantic_instance && !rule.apply_instance))
        return apply_rule(rule, k);
      return rule.apply_semantic_instance
                 ? apply_rule_semantic_instance(rule, k, request.instance)
                 : apply_rule_instance(rule, k, request.instance);
    } else if constexpr (std::is_same_v<R, CloseStartupCertificateRequest>) {
      if (support(s, TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE, 0) == -1)
        return declined("support:close-startup-certificate");
    } else if constexpr (std::is_same_v<R, RestartRequest> && bare<Args...>) {
      if (request.purpose != RestartPurpose::Preparation) return fallback();
      const int code = support(s, TACTIX_SUPPORT_PREPARATION_RESTART, request.keep_level);
      if (code == -1) return declined("support:preparation-restart");
      return code == 20 ? ApplyResult::Unsat : code == 1 ? ApplyResult::NoOp : ApplyResult::Ok;
    } else if constexpr (std::is_same_v<R, ForwardClosureRequest> && bare<Args...>) {
      const int code = support(s, TACTIX_SUPPORT_FORWARD_CLOSURE, request.trail_from);
      if (code == -1) return declined("support:forward-closure");
      return code == 20 ? ApplyResult::Unsat : code == 1 ? ApplyResult::NoOp : ApplyResult::Ok;
    } else if constexpr (std::is_same_v<R, SparseCollectRequest> && bare<Args...>) {
      if (request.start) return fallback();
      const int code = support(s, TACTIX_SUPPORT_SPARSE_COLLECT, 0);
      if (code == -1) return declined("support:sparse-collect");
      return code == 20 ? RootCollectOutcome::Refuted
             : code == 2 ? RootCollectOutcome::Unit
             : code == 1 ? RootCollectOutcome::NoChange
                         : RootCollectOutcome::Changed;
    } else if constexpr (std::is_same_v<R, CompactRandomOrderRequest> && bare<Args...>) {
      if (support(s, TACTIX_SUPPORT_COMPACT_RANDOM_ORDER, 0) == -1)
        return declined("support:compact-random-order");
    } else if constexpr (std::is_same_v<R, CompactVariablesRequest> && bare<Args...>) {
      const int code = support(s, TACTIX_SUPPORT_COMPACT_VARIABLES,
                               request.fold_fixed ? TACTIX_COMPACT_FOLD_FIXED : 0);
      if (code == -1) return declined("support:compact-variables");
      return code == 0;
    } else if constexpr (std::is_same_v<R, ReductionPreparationRequest> &&
                         sizeof...(Args) == 1) {
      // The policy's continuation consumes derived roots into H, which the
      // session's NATIVE publication profile does for a support.
      if (support(s, TACTIX_SUPPORT_REDUCTION_PREPARATION, 0) == -1)
        return declined("support:reduction-preparation");
    } else if constexpr (std::is_same_v<R, CollectGarbageRequest> && bare<Args...>) {
      if (request.watch_start) return fallback();
      const uint64_t flags = (request.sparse_collection ? TACTIX_COLLECT_SPARSE : 0) |
                             (request.preserve_eager_recent ? TACTIX_COLLECT_PRESERVE_EAGER : 0);
      if (support(s, TACTIX_SUPPORT_COLLECT_GARBAGE, flags) == -1)
        return declined("support:collect-garbage");
    } else if constexpr (std::is_same_v<R, FlushRetiredRedundantRequest> && bare<Args...>) {
      if (support(s, TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT, 0) == -1)
        return declined("support:flush-retired-redundant");
    } else if constexpr (std::is_same_v<R, BeginBveDefinitionSessionRequest>) {
      if (support(s, TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION, 0) == -1)
        return declined("support:begin-bve-definition-session");
    } else if constexpr (std::is_same_v<R, ResetGraphLabelingSessionRequest>) {
      const int code = support(s, TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION, 0);
      if (code == -1) return declined("support:reset-graph-labeling-session");
      return code == 0;
    } else if constexpr (std::is_same_v<R, ReleaseInputBinaryOrderRequest>) {
      const int code = support(s, TACTIX_SUPPORT_RELEASE_INPUT_ORDER, 0);
      if (code == -1) return declined("support:release-input-order");
      return code == 0;
    } else if constexpr (std::is_same_v<R, GraphLabelingActionRequest> && bare<Args...>) {
      if (!(request.parameters == strategy::GraphLabelingParameters::from_kernel(k)))
        return fallback();
      static const RuleVTable &lucky = tactix_rules()[static_cast<size_t>(rule_id("Lucky"))];
      const uint64_t instance = request.operation == GraphLabelingOperation::Authenticate
                                    ? LUCKY_GRAPH_AUTHENTICATE
                                    : LUCKY_GRAPH_CONTINUE;
      const int code = tactix_apply_instance(s, static_cast<int>(lucky.id), instance);
      if (code == -1) return declined("tactix_apply_instance:Lucky-graph");
      counted("tactix_apply_instance:Lucky-graph");
      if (lucky.id < Stats::MAX_TIMED_RULES) ++census.timed_dispatches;
      GraphLabelingActionResult out;
      out.result = result(code);
      if (const auto *a = k.graph_labeling_session.last_authentication()) out.authentication = *a;
      if (const auto *slice = k.graph_labeling_session.last_slice()) out.slice = *slice;
      return out;
    } else if constexpr (std::is_same_v<R, SweepRequest> && bare<Args...>) {
      // One online Sweep step under the batch's exact limits, its work budget
      // and material settings, as the slot record names them.
      if (request.seed != INSTANCE_CANONICAL) return fallback();
      strategy::OnlineInvocation step;
      step.id = 1;
      step.rule = "Sweep";
      step.parameters = request.parameters;
      step.granularity = strategy::InvocationGranularity::CanonicalApplication;
      step.rule_work_budget = {kernel_has_work_budget(k) ? strategy::RuleWorkBudgetMode::Bounded
                                                         : strategy::RuleWorkBudgetMode::Unlimited,
                               k.work_budget};
      step.material.touch_filter = k.touch_filter ? strategy::SettingOverride::Enable
                                                  : strategy::SettingOverride::Disable;
      step.material.likely_kept_redundant = k.touch_redundant
                                                ? strategy::SettingOverride::Enable
                                                : strategy::SettingOverride::Disable;
      public_encoding::EncodedStep encoded;
      if (!public_encoding::encode_step(k, step, rule_id("Sweep"), encoded)) return fallback();
      // Hold retirement publication: a productive Sweep's union waits for the
      // follow-up Substitute; the policy's boundary publication releases it.
      if (!retirements_held &&
          support(s, TACTIX_SUPPORT_HOLD_RETIREMENTS, TACTIX_RETIREMENTS_HOLD) == 0)
        retirements_held = true;
      const tactix_online_program_v2 program{&encoded.step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
      tactix_online_result out{};
      if (tactix_online_configure_v2(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) ||
          tactix_online_advance(s, &out))
        return declined("tactix_online:Sweep");
      // The online Sweep step executes the shared request without dispatch
      // accounting, as the native batch's structured dispatch is skipped here.
      counted("tactix_online_advance:Sweep");
      return static_cast<ApplyResult>(out.rule_result);
    } else if constexpr (std::is_same_v<R, WalkRequest> && sizeof...(Args) == 1) {
      // One online Walk step under the request's scoped profile.
      strategy::OnlineInvocation step;
      step.id = 1;
      step.rule = "Walk";
      auto parameters = strategy::WalkParameters::from_kernel(k);
      parameters.step_budget = request.allowance;
      parameters.random_seed = request.seed;
      parameters.seed_candidate = request.seed_candidate;
      parameters.objective = request.objective;
      parameters.admission = request.admission;
      parameters.preparation_budget = request.preparation_allowance;
      step.parameters = std::move(parameters);
      step.granularity = strategy::InvocationGranularity::CanonicalApplication;
      public_encoding::EncodedStep encoded;
      if (!public_encoding::encode_step(k, step, rule_id("Walk"), encoded)) return fallback();
      const tactix_online_program_v2 program{&encoded.step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
      tactix_online_result out{};
      if (tactix_online_configure_v2(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) ||
          tactix_online_advance(s, &out))
        return declined("tactix_online:Walk");
      counted("tactix_online_advance:Walk");
      timed(rule_id("Walk"));
      return static_cast<ApplyResult>(out.rule_result);
    } else if constexpr (std::is_same_v<R, RuleVTable> && sizeof...(Args) == 1 &&
                         (std::is_same_v<std::remove_cvref_t<Args>, VivifyBandRequest> && ...)) {
      return vivify_band(k, s, request, std::get<0>(std::forward_as_tuple(args...)));
    } else if constexpr (std::is_same_v<R, PublishRetirementsRequest>) {
      if (retirements_held) {
        release_retirements(s, TACTIX_RETIREMENTS_RELEASE);
      } else if (support(s, TACTIX_SUPPORT_HOLD_RETIREMENTS, TACTIX_RETIREMENTS_RELEASE) != 0) {
        return declined("support:publish-retirements");
      }
    } else if constexpr (std::is_same_v<R, CompactFastelPrefixRequest>) {
      const uint64_t argument =
          (static_cast<uint64_t>(request.prefix) << 32) |
          static_cast<uint32_t>(kernel_externalize(k, request.lit));
      const int code = support(s, TACTIX_SUPPORT_COMPACT_FASTEL_PREFIX, argument);
      if (code == -1) return declined("support:compact-fastel-prefix");
      return code == 0;
    } else if constexpr (std::is_same_v<R, BeginForwardRoundRequest>) {
      const int code = support(s, TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 0);
      if (code == -1) return declined("support:begin-forward-round");
      return code == 0;
    } else if constexpr (std::is_same_v<R, RescheduleBveCandidatesRequest>) {
      const int code = support(s, TACTIX_SUPPORT_RESCHEDULE_BVE, 0);
      if (code == -1) return declined("support:reschedule-bve");
      return code == 0;
    } else if constexpr (std::is_same_v<R, RetireBveCandidateRequest>) {
      const int code = support(
          s, TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE,
          static_cast<uint64_t>(
              kernel_externalize(k, make_lit(request.var, false))));
      if (code == -1) return declined("support:retire-bve-candidate");
      return code == 0;
    } else if constexpr (std::is_same_v<R, ConsumeRootPrefixRequest>) {
      const int code = support(s, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX, 0);
      if (code == -1) return declined("support:consume-root-prefix");
      return code == 0;
    } else if constexpr (is_parameter_installation<R>::value && sizeof...(Args) >= 2) {
      const char *rule = std::get<1>(std::forward_as_tuple(args...));
      public_encoding::Encoded encoded;
      const int id = rule_id(rule);
      if (id < 0 || !public_encoding::encode(strategy::InprocessParameters(request.parameters),
                                             encoded))
        return fallback();
      if (tactix_install_parameters(s, id, &encoded.parameters) != 0)
        return declined("tactix_install_parameters");
      counted("tactix_install_parameters");
    } else if constexpr (std::is_same_v<R, MaterialSettingsRequest>) {
      if (tactix_install_material_settings(s, setting(request.touch_filter),
                                           setting(request.likely_kept_redundant)) != 0)
        return declined("tactix_install_material_settings");
      counted("tactix_install_material_settings");
    } else if constexpr (std::is_same_v<R, WalkGuidanceInstallationRequest>) {
      strategy::WalkParameters selected;
      selected.step_budget = 0;
      selected.even_break_base_milli = request.even_break_base_milli;
      selected.score_curve = request.score_curve;
      public_encoding::Encoded encoded;
      const int id = rule_id("Walk");
      if (id < 0 || !public_encoding::encode(strategy::InprocessParameters(selected), encoded))
        return fallback();
      if (tactix_install_parameters(s, id, &encoded.parameters) != 0)
        return declined("tactix_install_parameters:walk-guidance");
      counted("tactix_install_parameters");
    } else if constexpr (std::is_same_v<R, RootPreparationRequest> && sizeof...(Args) == 3) {
      const auto argument = record_argument("RootPreparation");
      if (!argument) return fallback();
      const int code = support(s, TACTIX_SUPPORT_ROOT_PREPARATION, *argument);
      if (code == -1) return declined("support:root-preparation");
      return code == 20 ? ApplyResult::Unsat : ApplyResult::Ok;
    } else if constexpr (std::is_same_v<R, WarmupRequest> && sizeof...(Args) == 3) {
      // Recorded with the WARMUP phase only when the default branching picks.
      const auto argument = record_argument("Warmup");
      if (!argument) return fallback();
      const int code = support(s, TACTIX_SUPPORT_WARMUP, *argument);
      if (code == -1) return declined("support:warmup");
      WarmupResult out;
      out.interrupted = code == 1;
      return out;
    } else if constexpr (std::is_same_v<R, NativeConflictAnalysisRequest> &&
                         sizeof...(Args) == 2) {
      // The public chrono instance under the request's payload: the bump
      // profile and the retier schedule installed around the application and
      // the schedule read back. The policy's advice (trace only in Chrono
      // mode) runs after the transaction; it reads only public statistics.
      if (request.target_mode != AnalysisTarget::Chrono) return fallback();
      auto &&[instrument, advise] = std::forward_as_tuple(args...);
      static const int id = rule_id("ConflictAnalysis");
      auto profile = strategy::AnalyzeParameters::from_kernel(k);
      const auto neutral = profile;
      profile.bump_reason_limit = request.bump_reason_limit;
      profile.bump_reason_depth = request.bump_reason_depth;
      public_encoding::Encoded payload, restore;
      tactix_retier_schedule idle{}, returned{};
      const tactix_retier_schedule installed = c_schedule(request.retier_schedule);
      if (!public_encoding::encode(strategy::InprocessParameters(profile), payload) ||
          !public_encoding::encode(strategy::InprocessParameters(neutral), restore) ||
          tactix_read_retier_schedule(s, &idle) != 0)
        return fallback();
      const auto reset = [&] {
        const bool ok = tactix_install_parameters(s, id, &restore.parameters) == 0 &&
                        tactix_install_retier_schedule(s, &idle) == 0;
        assert(ok);
        (void)ok;
      };
      if (tactix_install_parameters(s, id, &payload.parameters) != 0 ||
          tactix_install_retier_schedule(s, &installed) != 0) {
        reset();
        return declined("conflict-analysis:payload");
      }
      const uint64_t backjumps_before = k.stats.backjumps;
      int code = -1;
      const ApplyResult analysis = instrument([&] {
        code = tactix_apply_semantic_instance(s, id, TACTIX_ANALYZE_TARGET_CHRONO);
        return code == -1 ? ApplyResult::NoOp : result(code);
      });
      const bool read = tactix_read_retier_schedule(s, &returned) == 0;
      reset();
      if (code == -1) return declined("tactix_apply_semantic_instance:ConflictAnalysis");
      assert(read);
      (void)read;
      counted("tactix_apply_semantic_instance:ConflictAnalysis");
      timed(id);
      ConflictAnalysisExecutionResult out{};
      out.analysis = out.result = analysis;
      out.backjumped = k.stats.backjumps != backjumps_before;
      out.target = k.level;
      out.retier_schedule = internal_schedule(returned);
      (void)advise(analysis);
      return out;
    } else if constexpr (std::is_same_v<R, MaterializeColdStateRequest>) {
      // Declared non-action: a C session never defers its initial state.
      ++census.declared["MaterializeColdState"];
      return StaticPort::execute(k, std::forward<Request>(request), std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<R, RuleApplicationRequest>) {
      // Publication around a composed application; the application's own
      // requests go through the port and the session's NATIVE profile.
      ++census.declared["RuleApplicationRequest"];
      return StaticPort::execute(k, std::forward<Request>(request), std::forward<Args>(args)...);
    } else {
      return fallback();
    }
  }
};

// The bracket a public client opens with ENTER_INPROCESS_PHASE: the session
// owns its context, occurrence index and default Factor selection. A bracket
// the session refuses, or one with an injected selection (no public bound),
// runs natively.
class PublicInprocessPhase {
public:
  PublicInprocessPhase() = default;
  PublicInprocessPhase(const PublicInprocessPhase &) = delete;
  PublicInprocessPhase &operator=(const PublicInprocessPhase &) = delete;
  ~PublicInprocessPhase() {
    if (session_ && open_) (void)tactix_support(session_, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0);
  }
  bool open() const { return session_ ? open_ : native_.open(); }
  bool eliminate() const { return session_ ? eliminate_ : native_.eliminate(); }
  bool occurrences_open() const { return session_ ? occurrences_ : native_.occurrences_open(); }
  uint32_t sparse_resume_trail() const {
    return session_ ? sparse_resume_trail_ : native_.sparse_resume_trail();
  }
  void enter(Kernel &k, const execution::InprocessPhaseEntryRequest &request) {
    tactix_c_handle *s = PublicPort::bound(k);
    if (s && request.bound) {
      const uint32_t trail = static_cast<uint32_t>(k.trail.size());
      if (PublicPort::support(s, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,
                              execution::inprocess_phase_argument(request.eliminate,
                                                                  *request.bound)) == 0) {
        session_ = s;
        open_ = true;
        eliminate_ = request.eliminate;
        sparse_resume_trail_ = trail;
        return;
      }
      ++PublicPort::census.declined["support:enter-inprocess-phase"];
    } else if (s) {
      ++PublicPort::census.fallbacks["InprocessPhase:injected-selection"];
    }
    native_.enter(k, request);
  }
  void open_occurrences(Kernel &k, OccIndex &index) {
    if (!session_) return native_.open_occurrences(k, index);
    const int code = PublicPort::support(session_, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0);
    assert(code == 0);
    (void)code;
    occurrences_ = true;
  }
  void exit_terminal(Kernel &k) {
    if (!session_) return native_.exit_terminal(k);
    leave();
  }
  template <class PublishRetirements, class Propagate>
  ApplyResult exit(Kernel &k, bool refuted, PublishRetirements &&publish_retirements,
                   Propagate &&propagate) {
    if (!session_)
      return native_.exit(k, refuted, std::forward<PublishRetirements>(publish_retirements),
                          std::forward<Propagate>(propagate));
    // The session publishes retirements (NATIVE) and runs the sparse-resume
    // Propagate with its root publication.
    return leave() == 20 || k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
  }

private:
  int leave() {
    PublicPort::release_retirements(session_, TACTIX_RETIREMENTS_RELEASE_ONLY);
    const int code = PublicPort::support(session_, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0);
    assert(code != -1);
    open_ = occurrences_ = false;
    session_ = nullptr;
    return code;
  }
  execution::InprocessPhase native_;
  tactix_c_handle *session_ = nullptr;
  bool open_ = false, eliminate_ = false, occurrences_ = false;
  uint32_t sparse_resume_trail_ = 0;
};

// Policy-side scopes around public requests, established through tactix.h:
// typed profiles by tactix_install_parameters, work wires by
// tactix_install_work_settings, tick attribution by
// tactix_install_tick_attribution. A profile's restore value is read from the
// session's kernel (no typed readback exists); every write is a public call.
// Unbound (or a refused install), the static scope runs.
class PublicTickSinkScope {
public:
  PublicTickSinkScope(Kernel &k, uint64_t Stats::*counter, bool restore = false) {
    s_ = restore ? nullptr : PublicPort::bound(k);
    if (s_ && tactix_install_tick_attribution(s_, stat(counter)) == 0) {
      PublicPort::counted("tactix_install_tick_attribution");
      return;
    }
    if (PublicPort::bound(k)) ++PublicPort::census.fallbacks["TickSinkScope"];
    s_ = nullptr;
    native_.emplace(k, counter, restore);
  }
  ~PublicTickSinkScope() {
    if (!s_) return;
    const int code = tactix_install_tick_attribution(s_, TACTIX_TICK_ATTRIBUTION_NONE);
    assert(code == 0);
    (void)code;
    PublicPort::counted("tactix_install_tick_attribution");
  }
  PublicTickSinkScope(const PublicTickSinkScope &) = delete;
  PublicTickSinkScope &operator=(const PublicTickSinkScope &) = delete;

private:
  static int stat(uint64_t Stats::*counter) {
    if (!counter) return TACTIX_TICK_ATTRIBUTION_NONE;
    if (counter == &Stats::probe_ticks) return TACTIX_STAT_PROBE_TICKS;
    if (counter == &Stats::hbr_ticks) return TACTIX_STAT_HBR_TICKS;
    if (counter == &Stats::backbone_ticks) return TACTIX_STAT_BACKBONE_TICKS;
    if (counter == &Stats::vivify_ticks) return TACTIX_STAT_VIVIFY_TICKS;
    if (counter == &Stats::instantiate_ticks) return TACTIX_STAT_INSTANTIATE_TICKS;
    return -2; // refused: no public statistic
  }
  tactix_c_handle *s_ = nullptr;
  std::optional<execution::TickSinkScope> native_;
};

class PublicWorkSettingsScope {
public:
  PublicWorkSettingsScope(Kernel &k, execution::WorkSettingsRequest request,
                          execution::WorkSettingsExit exit = execution::WorkSettingsExit::Restore)
      : s_(PublicPort::bound(k)), request_(request), exit_(exit) {
    if (!s_) {
      native_.emplace(k, request, exit);
      return;
    }
    saved_ = PublicPort::work_settings(s_);
    PublicPort::install_work(s_, request_);
  }
  ~PublicWorkSettingsScope() {
    if (!s_) return;
    const bool clear = exit_ == execution::WorkSettingsExit::ClearBudget;
    execution::WorkSettingsRequest restore;
    if (request_.budget) restore.budget = clear ? 0 : saved_.budget;
    if (request_.offset) restore.offset = clear ? 0 : saved_.offset;
    if (request_.bounded) restore.bounded = clear ? false : saved_.bounded != 0;
    if (request_.midround) restore.midround = saved_.midround != 0;
    PublicPort::install_work(s_, restore);
  }
  PublicWorkSettingsScope(const PublicWorkSettingsScope &) = delete;
  PublicWorkSettingsScope &operator=(const PublicWorkSettingsScope &) = delete;

private:
  tactix_c_handle *s_;
  execution::WorkSettingsRequest request_;
  execution::WorkSettingsExit exit_;
  tactix_work_settings saved_{};
  std::optional<execution::WorkSettingsScope> native_;
};

class PublicScopedRuleWorkBudget {
public:
  PublicScopedRuleWorkBudget(Kernel &k, const strategy::RuleWorkBudget &budget)
      : scope_(k, request(budget)) {}
  PublicScopedRuleWorkBudget(const PublicScopedRuleWorkBudget &) = delete;
  PublicScopedRuleWorkBudget &operator=(const PublicScopedRuleWorkBudget &) = delete;

private:
  // strategy::ScopedRuleWorkBudget's wires, as an online step's rule budget.
  static execution::WorkSettingsRequest request(const strategy::RuleWorkBudget &budget) {
    if (budget.mode == strategy::RuleWorkBudgetMode::Inherit) return {};
    const bool bounded = budget.mode == strategy::RuleWorkBudgetMode::Bounded;
    return {bounded ? budget.amount : 0, 0, bounded, bounded};
  }
  PublicWorkSettingsScope scope_;
};

class PublicVivifyDoseScope {
public:
  explicit PublicVivifyDoseScope(Kernel &k)
      : k_(k), work_(k, {k.work_budget, k.work_budget_offset, true, std::nullopt},
                     execution::WorkSettingsExit::ClearBudget) {}
  ~PublicVivifyDoseScope() {
    if (tactix_c_handle *s = PublicPort::bound(k_)) {
      tactix_set_vivify_scope(s, 0);
      PublicPort::counted("tactix_set_vivify_scope");
    } else {
      k_.vivify_scope = 0; // the static scope's exit
    }
  }
  PublicVivifyDoseScope(const PublicVivifyDoseScope &) = delete;
  PublicVivifyDoseScope &operator=(const PublicVivifyDoseScope &) = delete;

private:
  Kernel &k_;
  PublicWorkSettingsScope work_;
};

class PublicScopedReduceParameters {
public:
  PublicScopedReduceParameters(Kernel &k, const strategy::ReduceParameters &parameters)
      : s_(PublicPort::bound(k)) {
    if (s_) {
      saved_ = strategy::ReduceParameters::from_kernel(k);
      if (PublicPort::install(s_, "Reduce", parameters)) return;
      ++PublicPort::census.declined["tactix_install_parameters:Reduce-scope"];
      s_ = nullptr;
    }
    native_.emplace(k, parameters);
  }
  ~PublicScopedReduceParameters() {
    if (!s_) return;
    const bool ok = PublicPort::install(s_, "Reduce", saved_);
    assert(ok);
    (void)ok;
  }
  PublicScopedReduceParameters(const PublicScopedReduceParameters &) = delete;
  PublicScopedReduceParameters &operator=(const PublicScopedReduceParameters &) = delete;

private:
  tactix_c_handle *s_;
  strategy::ReduceParameters saved_;
  std::optional<strategy::ScopedReduceParameters> native_;
};

// The BVE pass session with its parameters posed by tactix_install_parameters
// for the session's lifetime.
class PublicBveInvocationSession : public strategy::BveInvocationSession {
public:
  PublicBveInvocationSession(const RuleVTable &rule, Kernel &k,
                             const strategy::BveParameters &parameters,
                             strategy::InvocationBudget &budget)
      : BveInvocationSession(CallerPosed{}, rule, k, parameters, budget) {
    if (!valid()) return;
    s_ = PublicPort::bound(k);
    if (s_) {
      saved_ = strategy::BveParameters::from_kernel(k);
      if (PublicPort::install(s_, "BVE", parameters)) return;
      ++PublicPort::census.declined["tactix_install_parameters:BVE-session"];
      s_ = nullptr;
    }
    native_.emplace(k, parameters);
  }
  ~PublicBveInvocationSession() {
    if (!s_) return;
    const bool ok = PublicPort::install(s_, "BVE", saved_);
    assert(ok);
    (void)ok;
  }

private:
  tactix_c_handle *s_ = nullptr;
  strategy::BveParameters saved_;
  std::optional<strategy::detail::ScopedBveParameters> native_;
};

// A Vivify band: its scope and cumulative ledger installed publicly, ticks
// attributed to the band's currency, then the registered rule.
inline ApplyResult PublicPort::vivify_band(Kernel &k, tactix_c_handle *s, const RuleVTable &rule,
                                           const execution::VivifyBandRequest &band) {
  assert(band.band.band);
  tactix_set_vivify_scope(s, band.band.scope);
  counted("tactix_set_vivify_scope");
  install_work(s, execution::WorkSettingsRequest{band.band.band_budget, band.band.band_offset,
                                                 true, std::nullopt});
  PublicTickSinkScope sink(k, band.tick_counter);
  return execution::metered_rule_apply(k, band.meter, [&] { return apply_rule(rule, k); });
}

static_assert(ActionPort<PublicPort>);
using InprocessPhase = PublicInprocessPhase;
using TickSinkScope = PublicTickSinkScope;
using WorkSettingsScope = PublicWorkSettingsScope;
using ScopedRuleWorkBudget = PublicScopedRuleWorkBudget;
using VivifyDoseScope = PublicVivifyDoseScope;
using ScopedReduceParameters = PublicScopedReduceParameters;
using BveInvocationSession = PublicBveInvocationSession;

} // namespace tactix::port

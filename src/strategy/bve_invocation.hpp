#pragma once

#include "rules/bve/bve.hpp"
#include "strategy/rule_invocation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace tactix::strategy {

enum class BveCandidateScope : uint8_t {
  All = 0,
  PureOnly = 1,
};

constexpr bool valid_bve_candidate_scope(BveCandidateScope scope) {
  return scope == BveCandidateScope::All ||
         scope == BveCandidateScope::PureOnly;
}

// Every heuristic control consumed by BVE is supplied per invocation.  The
// legacy Kernel fields remain the rule ABI during migration; ScopedParameters
// poses these values only for the atomic request and restores them exactly.
struct BveParameters {
  static constexpr uint32_t GROWTH_BOUND_DEFAULT = 16;
  static constexpr uint32_t XOR_ARITY_DEFAULT = 5;
  static constexpr uint32_t RESOLVENT_SIZE_DEFAULT = 100;
  static constexpr uint32_t BACKWARD_OCCURRENCE_DEFAULT = 100;
  static constexpr uint64_t DEFINITION_BUDGET_DEFAULT = 1000000;
  static constexpr uint32_t DEFINITION_CORES_DEFAULT = 2;
  static constexpr uint32_t DEFINITION_CORE_BUDGET_MULTIPLIER_DEFAULT = 10;

  uint32_t gate_mask = BVE_GATE_KISSAT_DEFAULT;
  uint32_t growth_bound = GROWTH_BOUND_DEFAULT;
  uint32_t occurrence_limit = UINT32_MAX;
  uint32_t clause_size_limit = UINT32_MAX;
  uint32_t xor_arity_limit = XOR_ARITY_DEFAULT;
  uint32_t resolvent_size_limit = RESOLVENT_SIZE_DEFAULT;
  uint32_t backward_occurrence_limit = BACKWARD_OCCURRENCE_DEFAULT;
  uint64_t definition_budget = DEFINITION_BUDGET_DEFAULT;
  uint32_t definition_cores = DEFINITION_CORES_DEFAULT;
  uint32_t definition_core_budget_multiplier =
      DEFINITION_CORE_BUDGET_MULTIPLIER_DEFAULT;
  bool backward = false;
  // This filters only policy-generated complete passes. Explicit one-instance
  // and selected-batch requests retain the complete legal BVE surface.
  BveCandidateScope candidate_scope = BveCandidateScope::All;
  uint32_t resolvent_count_limit = UINT32_MAX;

  static BveParameters from_kernel(const Kernel &k) {
    return {k.bve_gate_mask,
            k.bve_bound,
            k.bve_occurrence_limit,
            k.bve_clause_size_limit,
            k.bve_xor_arity_limit,
            k.bve_resolvent_size_limit,
            k.bve_backward_occurrence_limit,
            k.bve_definition_budget,
            k.bve_definition_cores,
            k.bve_definition_core_budget_multiplier,
            k.bve_backward,
            BveCandidateScope::All,
            k.bve_resolvent_count_limit};
  }

  bool valid() const {
    // Preserve the historical `~0u` spelling for "all gates" and leave
    // unknown bits inert for forward-compatible gate-family additions.
    return definition_cores > 0 && xor_arity_limit >= BVE_XOR_ARITY_MIN &&
           xor_arity_limit <= BVE_XOR_ARITY_CAPACITY &&
           valid_bve_candidate_scope(candidate_scope);
  }
};

inline void bve_complete_instances(Kernel &k, const RuleVTable &rule,
                                   const BveParameters &parameters,
                                   std::vector<uint64_t> &out) {
  if (parameters.candidate_scope == BveCandidateScope::All) {
    out.resize(k.nvars);
    out.resize(rule.instances(k, out.data(), k.nvars));
    return;
  }

  BveOccurrences occurrences;
  bve_build_occurrences(k, occurrences);
  std::vector<Var> candidates;
  bve_schedule_from_occurrences(k, occurrences, candidates);
  out.clear();
  out.reserve(candidates.size());
  for (const Var variable : candidates) {
    const uint32_t positive =
        occurrences.count(make_lit(variable, false));
    const uint32_t negative = occurrences.count(make_lit(variable, true));
    if ((positive == 0) == (negative == 0))
      continue;
    out.push_back(lit_instance(k, make_lit(variable, false)));
  }
}

struct BveInvocationRequest {
  BveParameters parameters;
  InvocationGranularity granularity = InvocationGranularity::OneInstance;
  // Stable external-literal payloads for OneInstance/SelectedBatch.  A
  // CompleteCurrentPass enumerates the complete current BVE scope itself.
  std::span<const uint64_t> instances;
  uint64_t configuration_revision = 0;
};

namespace detail {

class ScopedBveParameters {
public:
  ScopedBveParameters(Kernel &k, const BveParameters &p)
      : k_(k), saved_(BveParameters::from_kernel(k)) {
    install(p);
  }
  ScopedBveParameters(const ScopedBveParameters &) = delete;
  ScopedBveParameters &operator=(const ScopedBveParameters &) = delete;
  ~ScopedBveParameters() { install(saved_); }

private:
  void install(const BveParameters &p) {
    k_.bve_gate_mask = p.gate_mask;
    k_.bve_bound = p.growth_bound;
    k_.bve_occurrence_limit = p.occurrence_limit;
    k_.bve_clause_size_limit = p.clause_size_limit;
    k_.bve_resolvent_count_limit = p.resolvent_count_limit;
    k_.bve_xor_arity_limit = p.xor_arity_limit;
    k_.bve_resolvent_size_limit = p.resolvent_size_limit;
    k_.bve_backward_occurrence_limit = p.backward_occurrence_limit;
    k_.bve_definition_budget = p.definition_budget;
    k_.bve_definition_cores = p.definition_cores;
    k_.bve_definition_core_budget_multiplier =
        p.definition_core_budget_multiplier;
    k_.bve_backward = p.backward;
  }
  Kernel &k_;
  BveParameters saved_;
};

inline InvocationStatus accounting_status(BudgetStatus status) {
  return status == BudgetStatus::Exhausted
             ? InvocationStatus::BudgetExhausted
             : InvocationStatus::AccountingError;
}

inline InvocationStatus terminal_status(ApplyResult result) {
  switch (result) {
  case ApplyResult::Conflict:
    return InvocationStatus::Conflict;
  case ApplyResult::Sat:
    return InvocationStatus::Sat;
  case ApplyResult::Unsat:
    return InvocationStatus::Unsat;
  default:
    return InvocationStatus::Completed;
  }
}

} // namespace detail

// A policy may keep one session alive while it chooses BVE candidates.  Rule
// identity, parameters and budget currency are validated once, and the legacy
// Kernel ABI is posed once for the whole safe pass.  Candidate ordering and
// post-application propagation remain policy decisions.
class BveInvocationSession {
public:
  BveInvocationSession(const RuleVTable &rule, Kernel &k,
                       const BveParameters &parameters,
                       InvocationBudget &budget,
                       uint64_t configuration_revision = 0)
      : rule_(rule), k_(k), budget_(budget),
        configuration_revision_(configuration_revision) {
    valid_ = !std::strcmp(rule_.name, "BVE") && rule_.apply_instance &&
             rule_.kind == ActionKind::ProofRule && parameters.valid() &&
             budget_.unit() == WorkUnit::BveTicks;
    if (valid_)
      posed_.emplace(k_, parameters);
  }
  BveInvocationSession(const BveInvocationSession &) = delete;
  BveInvocationSession &operator=(const BveInvocationSession &) = delete;

  bool valid() const { return valid_; }

  InvocationResult apply_canonical() {
    InvocationResult out;
    out.before = state_version(k_);
    out.after = out.before;
    out.work_unit = WorkUnit::BveTicks;
    out.rule_id = rule_.id;
    out.instance = INSTANCE_CANONICAL;
    out.configuration_revision = configuration_revision_;
    out.budget_status = budget_.status();
    if (!valid_) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    if (out.budget_status != BudgetStatus::Ready) {
      out.status = detail::accounting_status(out.budget_status);
      return out;
    }
    if (k_.stop && *k_.stop) {
      out.status = InvocationStatus::Interrupted;
      return out;
    }
    if (!rule_.guard(k_)) {
      out.status = InvocationStatus::Inapplicable;
      return out;
    }
    const uint64_t work_before = k_.stats.bve_resolutions;
    const ApplyResult result = apply_rule(rule_, k_);
    const uint64_t work_after = k_.stats.bve_resolutions;
    if (work_after < work_before) {
      out.status = InvocationStatus::AccountingError;
      out.budget_status = BudgetStatus::AccountingOverflow;
      out.after = state_version(k_);
      return out;
    }
    out.work = work_after - work_before;
    out.applications = 1;
    out.rule_result = result;
    out.made_rule_progress = result == ApplyResult::Ok;
    out.selection_complete = true;
    out.budget_status = budget_.charge(WorkUnit::BveTicks, out.work);
    out.status = result == ApplyResult::Conflict || result == ApplyResult::Sat ||
                         result == ApplyResult::Unsat
                     ? detail::terminal_status(result)
                     : out.made_rule_progress ? InvocationStatus::Completed
                                              : InvocationStatus::NoChange;
    if (out.budget_status != BudgetStatus::Ready &&
        out.budget_status != BudgetStatus::Exhausted)
      out.status = InvocationStatus::AccountingError;
    out.after = state_version(k_);
    return out;
  }

  InvocationResult apply_one(uint64_t instance) {
    InvocationResult out;
    out.before = state_version(k_);
    out.after = out.before;
    out.work_unit = WorkUnit::BveTicks;
    out.rule_id = rule_.id;
    out.instance = instance;
    out.configuration_revision = configuration_revision_;
    out.budget_status = budget_.status();
    if (!valid_ || instance == INSTANCE_CANONICAL) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    if (out.budget_status != BudgetStatus::Ready) {
      out.status = detail::accounting_status(out.budget_status);
      return out;
    }
    if (k_.stop && *k_.stop) {
      out.status = InvocationStatus::Interrupted;
      return out;
    }
    if (!rule_.guard(k_)) {
      out.status = InvocationStatus::Inapplicable;
      return out;
    }
    Lit lit;
    if (!lit_of_instance(k_, instance, &lit)) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    if (k_.value(lit) || kernel_var_retired(k_, var_of(lit))) {
      out.status = InvocationStatus::StaleRequest;
      return out;
    }

    const uint64_t work_before = k_.stats.bve_resolutions;
    const ApplyResult result = apply_rule_instance(rule_, k_, instance);
    const uint64_t work_after = k_.stats.bve_resolutions;
    if (work_after < work_before) {
      out.status = InvocationStatus::AccountingError;
      out.budget_status = BudgetStatus::AccountingOverflow;
      out.after = state_version(k_);
      return out;
    }
    out.work = work_after - work_before;
    out.applications = 1;
    out.rule_result = result;
    out.made_rule_progress = result == ApplyResult::Ok;
    out.selection_complete = true;
    out.budget_status = budget_.charge(WorkUnit::BveTicks, out.work);
    out.status = result == ApplyResult::Conflict || result == ApplyResult::Sat ||
                         result == ApplyResult::Unsat
                     ? detail::terminal_status(result)
                     : out.made_rule_progress ? InvocationStatus::Completed
                                              : InvocationStatus::NoChange;
    if (out.budget_status != BudgetStatus::Ready &&
        out.budget_status != BudgetStatus::Exhausted)
      out.status = InvocationStatus::AccountingError;
    out.after = state_version(k_);
    return out;
  }

private:
  const RuleVTable &rule_;
  Kernel &k_;
  InvocationBudget &budget_;
  uint64_t configuration_revision_;
  bool valid_ = false;
  std::optional<detail::ScopedBveParameters> posed_;
};

// Execute through the existing registered BVE primitive.  This intentionally
// does not perform DefaultPolicy's heap, propagation, H-publication or garbage
// collection duties; those are composition choices, not BVE rule semantics.
inline InvocationResult invoke_bve(const RuleVTable &rule, Kernel &k,
                                   const BveInvocationRequest &request,
                                   InvocationBudget &budget) {
  InvocationResult out;
  out.before = state_version(k);
  out.after = out.before;
    out.work_unit = WorkUnit::BveTicks;
    out.rule_id = rule.id;
  out.configuration_revision = request.configuration_revision;
  out.budget_status = budget.status();

  if (std::strcmp(rule.name, "BVE") || !rule.apply_instance ||
      rule.kind != ActionKind::ProofRule || !request.parameters.valid() ||
      budget.unit() != WorkUnit::BveTicks) {
    out.status = InvocationStatus::InvalidRequest;
    return out;
  }
  if (out.budget_status != BudgetStatus::Ready) {
    out.status = detail::accounting_status(out.budget_status);
    return out;
  }

  std::vector<uint64_t> complete;
  std::span<const uint64_t> selected = request.instances;
  switch (request.granularity) {
  case InvocationGranularity::OneInstance:
    if (selected.size() != 1 || selected.front() == INSTANCE_CANONICAL) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    break;
  case InvocationGranularity::SelectedBatch:
    if (selected.empty() ||
        std::find(selected.begin(), selected.end(), INSTANCE_CANONICAL) !=
            selected.end()) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    break;
  case InvocationGranularity::CompleteCurrentPass:
    if (!selected.empty() || !rule.instances) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    bve_complete_instances(k, rule, request.parameters, complete);
    selected = complete;
    break;
  case InvocationGranularity::CanonicalApplication:
    if (!selected.empty()) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
    {
      BveInvocationSession session(rule, k, request.parameters, budget,
                                   request.configuration_revision);
      return session.apply_canonical();
    }
  }

  // Validate external identities transactionally before posing parameters or
  // executing the first member.  A later member may legitimately go stale
  // because an earlier member changed the formula; that is reported below.
  for (const uint64_t instance : selected) {
    Lit lit;
    if (!lit_of_instance(k, instance, &lit)) {
      out.status = InvocationStatus::InvalidRequest;
      return out;
    }
  }

  BveInvocationSession session(rule, k, request.parameters, budget,
                               request.configuration_revision);
  bool any_progress = false;
  for (const uint64_t instance : selected) {
    const InvocationResult one = session.apply_one(instance);
    out.work += one.work;
    out.applications += one.applications;
    out.rule_result = one.rule_result;
    out.budget_status = one.budget_status;
    any_progress |= one.made_rule_progress;
    out.made_rule_progress = any_progress;
    out.status = one.status;

    if (!one.applications || one.status == InvocationStatus::Conflict ||
        one.status == InvocationStatus::Sat ||
        one.status == InvocationStatus::Unsat) {
      break;
    }
    if (out.budget_status != BudgetStatus::Ready &&
        out.applications != selected.size()) {
      out.status = detail::accounting_status(out.budget_status);
      break;
    }
  }

  if (out.applications == selected.size()) {
    out.selection_complete = true;
    const bool terminal = out.status == InvocationStatus::Conflict ||
                          out.status == InvocationStatus::Sat ||
                          out.status == InvocationStatus::Unsat;
    if (!terminal) {
      if (out.budget_status != BudgetStatus::Ready &&
          out.budget_status != BudgetStatus::Exhausted)
        out.status = InvocationStatus::AccountingError;
      else
        out.status = any_progress ? InvocationStatus::Completed
                                  : InvocationStatus::NoChange;
    }
  }
  out.after = state_version(k);
  return out;
}

} // namespace tactix::strategy

// C API implementation — v1, frozen at the phase-2 review (plan §5.1);
// post-v1 changes are additive only. Rename the public alias while parsing the
// header so its `tactix` typedef does not collide with the internal namespace;
// the underlying `struct tactix_solver` and C symbols remain identical.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/dsr_follow_on.hpp"
#include "frontend/parse.hpp"
#include "api/api_internal.hpp"
#include "session/query_kernel.hpp"
#include "policies/heuristics.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "strategy/bve_schedule.hpp"
#include "policies/strategy_settings.hpp"
#include "diagnostics/rule_journal.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/decide/decide.hpp"
#include "rules/condition/condition.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/structural_action_guard.hpp"
#include "rules/registry.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/heuristic_execution.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/region_program.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/phase_execution.hpp"
#include "strategy/publication_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/root_execution.hpp"
#include "strategy/slot_execution.hpp"
#include "strategy/structural_action_sessions.hpp"
#include "strategy/work_execution.hpp"
#include "policies/rule_publication.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace tactix;

struct tactix_solver {
  Kernel owned_kernel;
  // Named settings of the default strategy (tactix_set_default_option).
  StrategySettings owned_options;
  Heuristics owned_heuristics; // the session owns H, Kernel holds a non-owning
                         // pointer. Outlives every solve(), so tactix_decide
                         // before any solve still has an H.
  Proof owned_proof;
  StructuralActionSessions owned_services{owned_kernel};
  Kernel &kernel=owned_kernel;
  StrategySettings &default_options=owned_options;
  Heuristics &heuristics=owned_heuristics;
  Proof &proof=owned_proof;
  StructuralActionSessions &structural_action_sessions=owned_services;
  session::QueryKernel *query_owner=nullptr;
  session::KernelTicket query_ticket;
  DefaultPolicy *query_policy=nullptr;
  volatile sig_atomic_t *query_stop=nullptr;
  volatile sig_atomic_t *saved_query_stop=nullptr;
  session::QuerySolveStatus query_status=session::QuerySolveStatus::Complete;
  std::vector<Lit> scratch;
  bool input_completed = false;
  bool pending_input_proof = false;
  uint64_t model_strategy_epoch = UINT64_MAX;
  int guidance_publication = TACTIX_GUIDANCE_DISCARD;
  // Additive and lazy: ordinary tactix_solve sessions retain the pre-online
  // object layout's allocation behavior and never construct program vectors.
  std::unique_ptr<strategy::OnlineInprocessExecutor> online;
  std::unique_ptr<strategy::RegionProgram> regions;
  // The encoding tactix_read_parameters last returned (api_observe.cpp).
  std::shared_ptr<void> observe_scratch;
  // The default strategy's configuration as DefaultPolicy runs it, which
  // tactix_strategy_* and STRATEGY profile readback report.
  DefaultStrategyConfig strategy_configuration =
      default_policy_configuration(DefaultStrategyConfig{});
  // The open BVE schedule (tactix_bve_schedule_*), which the kernel's BVE
  // score notifications update.
  std::unique_ptr<strategy::BveCandidateHeap> bve_schedule;
  // The configured structured-root stage (tactix_stage_*) and the
  // SR-discovery allowance its advances charge.
  std::unique_ptr<strategy::OnlineStrategySequence> structured_stage;
  std::unique_ptr<strategy::Allowance> sr_discovery;
  // The recorded affine source census, if one is.
  std::optional<std::array<uint64_t, 13>> affine_census;
  // The startup retention scope (TACTIX_SUPPORT_STARTUP_RETENTION): whether
  // open, and the retention it replaced.
  bool startup_retention = false;
  const std::unordered_set<uint32_t> *retention_before = nullptr;
  // Execution contexts entered through tactix_support, innermost last.
  std::vector<std::pair<execution::ExecutionContext,
                        execution::ExecutionContextState>> contexts;
  // The inprocessing phase bracket entered through tactix_support: its
  // default Factor selection and occurrence index outlive the bracket, which
  // is declared last so that it closes before them. `phase_contexts` is the
  // context depth at entry; contexts nest strictly inside the bracket.
  DefaultFactorMatch phase_factor_selection;
  OccIndex phase_occurrences;
  size_t phase_contexts = 0;
  execution::InprocessPhase phase;
  tactix_solver() {
    kernel.h = &heuristics;
    kernel.saved_phases = &heuristics.saved;
    // Match the CLI/reference default; rule-level fixtures using a bare Kernel
    // can still exercise the non-reuse path explicitly.
    kernel.conflict_reuse = true;
  }
  tactix_solver(session::QueryKernel &q,DefaultPolicy &policy,StrategySettings &options,
      volatile sig_atomic_t *stop)
      :kernel(*q.kernel(q.ticket())),default_options(options),heuristics(*kernel.h),
       proof(*kernel.proof),structural_action_sessions(*q.execution_services(q.ticket())),
       query_owner(&q),query_ticket(q.ticket()),query_policy(&policy),query_stop(stop),
       saved_query_stop(kernel.stop),input_completed(true) {kernel.stop=stop;}
  ~tactix_solver() {if(query_owner && kernel.stop==query_stop)kernel.stop=saved_query_stop;}
};

extern "C" {
static void input_complete(tactix_solver *solver);
}

// Internal introspection of the opaque C handle, for fixed tests and for
// api_work.cpp's additive entry points; neither depends on the handle layout.
namespace tactix::api_detail {
int set_strategy_option(StrategySettings &options,const char *name,const char *value) {
  if(!name || std::strncmp(name,"TACTIX_",7)!=0)return -1;
  for(auto it=options.begin();it!=options.end();++it)
    if(it->first==name) {if(value)it->second=value;else options.erase(it);return 0;}
  if(value)options.emplace_back(name,value);
  return 0;
}
void *create_query_execution(session::QueryKernel &q,DefaultPolicy &policy,
    StrategySettings &options,volatile sig_atomic_t *stop) {
  if(!q.kernel(q.ticket()) || !q.execution_services(q.ticket()))return nullptr;
  return new ::tactix_solver(q,policy,options,stop);
}
void destroy_query_execution(void *opaque) {delete static_cast<::tactix_solver *>(opaque);}
bool query_execution_settled(void *opaque) {
  const auto &s=*static_cast<::tactix_solver *>(opaque);
  return s.contexts.empty() && !s.phase.open() && !s.kernel.inprocessing &&
      !s.kernel.phase_saving_suppressed && !s.kernel.dense && !s.kernel.pending.valid;
}
session::QuerySolveStatus query_execution_status(void *opaque) {
  return static_cast<::tactix_solver *>(opaque)->query_status;
}
Kernel &kernel_for_fixed_testing(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->kernel;
}
void complete_input(void *opaque) {
  assert(opaque);
  input_complete(static_cast<::tactix_solver *>(opaque));
}
std::shared_ptr<void> &observe_scratch(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->observe_scratch;
}
DefaultStrategyConfig &strategy_configuration(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->strategy_configuration;
}
std::unique_ptr<strategy::BveCandidateHeap> &bve_schedule(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->bve_schedule;
}
std::unique_ptr<strategy::OnlineStrategySequence> &structured_stage(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->structured_stage;
}
std::unique_ptr<strategy::Allowance> &sr_discovery(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->sr_discovery;
}
namespace {
constexpr std::pair<int, uint64_t Stats::*> TICK_ATTRIBUTIONS[] = {
    {TACTIX_STAT_PROBE_TICKS, &Stats::probe_ticks},
    {TACTIX_STAT_HBR_TICKS, &Stats::hbr_ticks},
    {TACTIX_STAT_BACKBONE_TICKS, &Stats::backbone_ticks},
    {TACTIX_STAT_VIVIFY_TICKS, &Stats::vivify_ticks},
    {TACTIX_STAT_INSTANTIATE_TICKS, &Stats::instantiate_ticks}};
} // namespace
uint64_t Stats::*tick_attribution_counter(int stat) {
  for (const auto &[id, counter] : TICK_ATTRIBUTIONS)
    if (id == stat) return counter;
  return nullptr;
}
int tick_attribution_stat(uint64_t Stats::*counter) {
  for (const auto &[id, member] : TICK_ATTRIBUTIONS)
    if (counter && member == counter) return id;
  return TACTIX_TICK_ATTRIBUTION_NONE;
}
strategy::RegionProgram *regions(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->regions.get();
}
size_t regions_pending(void *opaque) {
  const auto &regions = static_cast<::tactix_solver *>(opaque)->regions;
  return regions ? regions->pending() : 0;
}
std::optional<std::array<uint64_t, 13>> &affine_census(void *opaque) {
  assert(opaque);
  return static_cast<::tactix_solver *>(opaque)->affine_census;
}
// The source-population identity an empty affine census answers for: learned
// additions and in-place simplifications change it.
std::array<uint64_t, 13> affine_census_stamp(void *opaque) {
  const Kernel &kernel = kernel_for_fixed_testing(opaque);
  return {kernel.arena_generation, kernel.touch_epoch, kernel.touch_fixed_head,
          kernel.next_clause_id, kernel.affine_semantic_namespace_epoch, kernel.arena.size(),
          kernel.nvars, kernel.external_to_internal.size(),
          kernel.proof ? kernel.proof->premise_epoch() : 0,
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(kernel.proof)),
          kernel.stats.root_assignments, kernel.affine_trail_retraction_epoch,
          kernel.trail.size()};
}
const strategy::OnlineStrategyProgram *online_program_for_fixed_testing(void *opaque) {
  assert(opaque);
  const auto &online = static_cast<::tactix_solver *>(opaque)->online;
  return online ? &online->program() : nullptr;
}
}

namespace {

bool has_sat_assignment(const Kernel &kernel) {
  return !kernel.inconsistent && kernel.unassigned_active == 0 &&
         !kernel.conflict_valid && !kernel.pending.valid &&
         kernel_propagation_fixpoint(kernel);
}

void prepare_external_model(tactix_solver *solver) {
  if(solver->query_owner) {
    const auto model=solver->query_owner->check_model(solver->query_ticket);
    if(model.status!=session::ModelStatus::Valid && model.status!=session::ModelStatus::FalsifiedAssumption)
      solver->query_status=model.status==session::ModelStatus::InvalidQuery?
          session::QuerySolveStatus::RestorationRequired:session::QuerySolveStatus::InvalidModel;
    solver->model_strategy_epoch=solver->kernel.strategy_epoch;
    return;
  }
  Kernel &kernel = solver->kernel;
  if (kernel.model_extended &&
      solver->model_strategy_epoch == kernel.strategy_epoch)
    return;
  // Model access is a cache, not a change to the resumable search state.
  // Any public rule/configuration transition changes strategy_epoch; an old
  // reconstructed assignment must not survive a restart and new decisions.
  kernel.model_extended = false;
  kernel_extend_model(kernel, /*materialize_internal=*/false);
  solver->model_strategy_epoch = kernel.strategy_epoch;
}

void external_state_change(tactix_solver *solver) {
  ++solver->kernel.strategy_epoch;
  if (solver->query_owner)
    solver->query_owner->drain_learned(solver->query_ticket, solver->query_stop);
}

// An UNSAT answer rests on a complete proof. A DSR proof that met a startup
// counting refutation is written first, by a follow-on solve of the original
// formula under the session's strategy settings and the default configuration.
void certify_unsat(tactix_solver *solver) {
  if(solver->query_owner) {
    if(!solver->query_owner->flush_proof(solver->query_ticket))solver->query_status=session::QuerySolveStatus::ProofFailure;
    return;
  }
  if (solver->proof.dsr_follow_on_pending()) {
    ScopedStrategySettings settings(&solver->default_options);
    const DsrFollowOn follow_on = run_dsr_follow_on(
        solver->kernel, solver->proof, DefaultStrategyConfig{},
        [](Kernel &kernel) { kernel.conflict_reuse = true; }, nullptr);
    if (follow_on.result != 20)
      proof_error("the DSR follow-on solve did not refute the formula");
  }
  solver->proof.flush();
}

// Every public registry dispatch publishes its rule events through the
// session's selected profile, before invariants are checked or the result is
// observed. NATIVE is DefaultPolicy's executor profile with the same publisher.
template <class Apply>
ApplyResult apply_published(tactix_solver *solver, Apply &&apply) {
  if (solver->guidance_publication == TACTIX_GUIDANCE_NATIVE)
    return execution::execute_action(solver->kernel,
                                     execution::NATIVE_RULE_PUBLICATION,
                                     std::forward<Apply>(apply),
                                     HeuristicRulePublisher{})
        .result;
  return execution::execute_action(
             solver->kernel,
             execution::RuleApplicationRequest{
                 execution::RootPublication::Deferred, false, true},
             std::forward<Apply>(apply), DiscardedRulePublisher{})
      .result;
}

template <class Target, size_t Size>
bool online_enum(int value, const std::array<Target, Size> &mapping,
                 Target &out) {
  if (value < 0 || static_cast<size_t>(value) >= mapping.size())
    return false;
  out = mapping[static_cast<size_t>(value)];
  return true;
}

bool online_repeat(int mode, uint64_t limit, strategy::RepeatSpec &out) {
  strategy::RepeatMode mapped;
  static constexpr std::array mapping{
      strategy::RepeatMode::Once, strategy::RepeatMode::Count,
      strategy::RepeatMode::UntilScopedFixpoint,
      strategy::RepeatMode::UntilSizeScaledFixpoint};
  if (!online_enum(mode, mapping, mapped))
    return false;
  out = {mapped, limit};
  return true;
}

bool online_post_application(int value, strategy::PostApplication &out) {
  static constexpr std::array mapping{
      strategy::PostApplication::None, strategy::PostApplication::Propagate,
      strategy::PostApplication::ForwardClosure};
  return online_enum(value, mapping, out);
}

bool online_granularity(int value, strategy::InvocationGranularity &out) {
  static constexpr std::array mapping{
      strategy::InvocationGranularity::OneInstance,
      strategy::InvocationGranularity::SelectedBatch,
      strategy::InvocationGranularity::CompleteCurrentPass,
      strategy::InvocationGranularity::CanonicalApplication};
  return online_enum(value, mapping, out);
}

bool online_work_unit(int value, strategy::WorkUnit &out) {
  static constexpr std::array mapping{
      strategy::WorkUnit::Applications, strategy::WorkUnit::BveTicks,
      strategy::WorkUnit::Propagations};
  return online_enum(value, mapping, out);
}

bool online_rule_work_budget(int mode, uint64_t amount,
                             strategy::RuleWorkBudget &out) {
  switch (mode) {
  case TACTIX_ONLINE_RULE_BUDGET_INHERIT:
    if (amount)
      return false;
    out = {strategy::RuleWorkBudgetMode::Inherit, 0};
    return true;
  case TACTIX_ONLINE_RULE_BUDGET_UNLIMITED:
    if (amount)
      return false;
    out = {strategy::RuleWorkBudgetMode::Unlimited, 0};
    return true;
  case TACTIX_ONLINE_RULE_BUDGET_BOUNDED:
    out = {strategy::RuleWorkBudgetMode::Bounded, amount};
    return true;
  default:
    return false;
  }
}

bool online_setting_override(int mode, strategy::SettingOverride &out) {
  static constexpr std::array mapping{
      strategy::SettingOverride::Inherit, strategy::SettingOverride::Disable,
      strategy::SettingOverride::Enable};
  return online_enum(mode, mapping, out);
}

bool setting_mode(int mode) {
  return mode >= TACTIX_ONLINE_SETTING_INHERIT &&
         mode <= TACTIX_ONLINE_SETTING_ENABLE;
}

// An explicit DISABLE/ENABLE replaces the converted default; INHERIT keeps it.
void apply_setting(int mode, bool &target) {
  if (mode != TACTIX_ONLINE_SETTING_INHERIT)
    target = mode == TACTIX_ONLINE_SETTING_ENABLE;
}

// A choice tag other than INHERIT (0) replaces the converted default with the
// enumerator one below it.
template <class Choice> void apply_choice(int tag, Choice &target) {
  if (tag)
    target = static_cast<Choice>(tag - 1);
}

// A nonzero C value replaces the converted default; zero keeps it.
template <class Target, class Source>
void assign_if_nonzero(Target &target, Source source) {
  if (source)
    target = source;
}

// Whether a C array argument is well formed: a nonnegative count, with a
// pointer whenever the count is nonzero.
template <class Element, class Count>
bool c_array(const Element *elements, Count count) {
  static_assert(std::is_same_v<Count, int>);
  return count >= 0 && (!count || elements);
}

// The internal literal of an external literal: INVALID_LIT for 0, INT32_MIN
// or one the kernel does not know.
Lit internal_literal(const Kernel &kernel, int external) {
  if (!external || external == INT32_MIN)
    return INVALID_LIT;
  return kernel_internalize(kernel, external);
}

// Appends the internal literals of `count` external literals; false at the
// first one internal_literal() rejects.
bool append_internal_literals(const Kernel &kernel, const int *externals,
                              int count, std::vector<Lit> &target) {
  for (int i = 0; i < count; ++i) {
    const Lit literal = internal_literal(kernel, externals[i]);
    if (literal == INVALID_LIT)
      return false;
    target.push_back(literal);
  }
  return true;
}

// The refutation limits the ParityModel and OnehotModel C mirrors share.
template <class Converted, class Source>
void assign_refutation_limits(Converted &converted, const Source &p) {
  assign_if_nonzero(converted.refutation_maximum_variables,
                    p.refutation_maximum_variables);
  assign_if_nonzero(converted.refutation_maximum_groups,
                    p.refutation_maximum_groups);
  assign_if_nonzero(converted.refutation_maximum_constraints,
                    p.refutation_maximum_constraints);
  assign_if_nonzero(converted.refutation_maximum_arity,
                    p.refutation_maximum_arity);
  assign_if_nonzero(converted.refutation_maximum_proof_additions,
                    p.refutation_maximum_proof_additions);
}

// A converted family is installed only when its typed limits are valid.
template <class Parameters>
bool accept_valid(strategy::InprocessParameters &target,
                  Parameters parameters) {
  if (!parameters.valid())
    return false;
  target = std::move(parameters);
  return true;
}

// A family whose C mirror shares its field list converts field by field.
template <class Parameters, class Source>
bool accept_copied(strategy::InprocessParameters &target,
                   const Source &source) {
  Parameters parameters;
  strategy::copy_parameter_fields<Parameters>(parameters, source);
  return accept_valid(target, std::move(parameters));
}

bool online_graph_symmetry_parameters(const tactix_online_sr_parameters &p,
                                      GraphSymmetryParameters &graph) {
  if (p.symmetry_fixing_mode < TACTIX_ONLINE_SYMMETRY_STABILIZER_ONLY ||
      p.symmetry_fixing_mode >
          TACTIX_ONLINE_SYMMETRY_STABILIZER_THEN_GENERATORS ||
      p.symmetry_generator_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_generator_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_clause_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_clause_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_literal_order < TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL ||
      p.symmetry_literal_order > TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED ||
      p.symmetry_clause_scope <
          TACTIX_ONLINE_SYMMETRY_CLAUSES_UNIQUE_LITERALS ||
      p.symmetry_clause_scope > TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL ||
      p.symmetry_pair_orientation < TACTIX_ONLINE_SYMMETRY_PAIR_CANONICAL ||
      p.symmetry_pair_orientation > TACTIX_ONLINE_SYMMETRY_PAIR_SEEDED ||
      p.symmetry_binary_graph_mode <
          TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_INHERIT ||
      p.symmetry_binary_graph_mode >
          TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_AUTOMATIC ||
      p.symmetry_clause_selection <
          TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_INHERIT ||
      p.symmetry_clause_selection >
          TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_AMO_THEN_SHORTEST ||
      p.symmetry_chain_mode < TACTIX_ONLINE_SYMMETRY_CHAIN_INHERIT ||
      p.symmetry_chain_mode > TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT ||
      p.symmetry_nonforcing_cut_admission <
          TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT ||
      p.symmetry_nonforcing_cut_admission >
          TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS ||
      p.symmetry_cut_point < TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT ||
      p.symmetry_cut_point >
          TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST ||
      !setting_mode(p.symmetry_negation_fixing_first_mode) ||
      !setting_mode(p.symmetry_first_literal_fix_anchor_mode) ||
      p.symmetry_base_selection < TACTIX_ONLINE_SYMMETRY_BASE_INHERIT ||
      p.symmetry_base_selection >
          TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER ||
      !setting_mode(p.symmetry_direct_generator_involutions_mode) ||
      !setting_mode(p.symmetry_decompose_mode) ||
      !setting_mode(p.symmetry_prefer_depth_first_mode) ||
      !setting_mode(p.symmetry_strong_certification_mode) ||
      !setting_mode(p.symmetry_pointwise_root_stabilizer_mode) ||
      !setting_mode(p.symmetry_factor_support_components_mode) ||
      !setting_mode(p.symmetry_preserve_detector_order_mode) ||
      !setting_mode(p.symmetry_use_structured_generators_mode) ||
      !setting_mode(p.symmetry_stabilize_structured_variables_mode) ||
      p.symmetry_structured_scope < TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT ||
      p.symmetry_structured_scope > TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_AXIS_ANCHORS)
    return false;
  apply_choice(p.symmetry_structured_scope, graph.structured_scope);
  graph.maximum_graph_vertices = p.maximum_graph_vertices;
  graph.maximum_graph_edges = p.maximum_graph_edges;
  graph.maximum_generators = p.maximum_symmetry_generators;
  graph.backend_search_budget = p.symmetry_backend_search_budget;
  graph.backend_support_budget = p.symmetry_backend_support_budget;
  graph.backend_component_size_limit =
      p.symmetry_backend_component_size_limit;
  graph.maximum_fixing_instances = p.maximum_symmetry_fixing_instances;
  graph.maximum_generator_instances_per_pass =
      p.maximum_symmetry_generator_instances_per_pass;
  graph.maximum_group_ticks = p.maximum_symmetry_group_ticks;
  graph.chain_planning_tick_limit = p.symmetry_chain_planning_tick_limit;
  assign_if_nonzero(graph.schreier_domain_limit,
                    p.symmetry_schreier_domain_limit);
  graph.seed = p.symmetry_seed;
  assign_if_nonzero(graph.error_bound, p.symmetry_error_bound);
  graph.fixing_mode = static_cast<SymmetryFixingMode>(p.symmetry_fixing_mode);
  graph.generator_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_generator_order);
  graph.clause_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_clause_order);
  graph.literal_order =
      static_cast<SymmetryTraversalOrder>(p.symmetry_literal_order);
  graph.clause_scope =
      p.symmetry_clause_scope == TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL
          ? SymmetryClauseScope::All
          : SymmetryClauseScope::UniqueLiteralsOnly;
  graph.pair_orientation =
      static_cast<SymmetryPairOrientation>(p.symmetry_pair_orientation);
  apply_choice(p.symmetry_binary_graph_mode, graph.binary_graph_mode);
  assign_if_nonzero(graph.binary_graph_density_permille,
                    p.symmetry_binary_graph_density_permille);
  apply_choice(p.symmetry_clause_selection, graph.clause_selection);
  apply_choice(p.symmetry_chain_mode, graph.chain_mode);
  apply_choice(p.symmetry_nonforcing_cut_admission,
               graph.nonforcing_cut_admission);
  apply_choice(p.symmetry_base_selection, graph.base_selection);
  apply_setting(p.symmetry_direct_generator_involutions_mode,
                graph.direct_generator_involutions_only);
  apply_setting(p.symmetry_decompose_mode, graph.decompose);
  apply_setting(p.symmetry_prefer_depth_first_mode, graph.prefer_depth_first);
  apply_setting(p.symmetry_strong_certification_mode,
                graph.strong_certification);
  apply_setting(p.symmetry_factor_support_components_mode,
                graph.factor_support_components);
  apply_setting(p.symmetry_use_structured_generators_mode,
                graph.use_structured_generators);
  apply_setting(p.symmetry_stabilize_structured_variables_mode,
                graph.stabilize_structured_variables);
  apply_setting(p.symmetry_preserve_detector_order_mode,
                graph.preserve_detector_order);
  apply_setting(p.symmetry_pointwise_root_stabilizer_mode,
                graph.pointwise_root_stabilizer);
  apply_choice(p.symmetry_cut_point, graph.cut_point);
  apply_setting(p.symmetry_negation_fixing_first_mode,
                graph.negation_fixing_first);
  apply_setting(p.symmetry_first_literal_fix_anchor_mode,
                graph.first_literal_fix_anchor);
  return true;
}

// The orbitopal producer's fields of an SR profile, validated and converted.
bool online_orbitopal_parameters(const tactix_online_sr_parameters &p,
                                 SubstitutionRedundancyParameters &sr) {
  if (p.orbitopal_orientation < TACTIX_ONLINE_ORBITOPAL_ROWS ||
      p.orbitopal_orientation > TACTIX_ONLINE_ORBITOPAL_BOTH ||
      p.orbitopal_column_order <
          TACTIX_ONLINE_ORBITOPAL_ORDER_CANONICAL ||
      p.orbitopal_column_order >
          TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE ||
      p.orbitopal_completion <
          TACTIX_ONLINE_ORBITOPAL_COMPLETION_MATRIX_ONLY ||
      p.orbitopal_completion >
          TACTIX_ONLINE_ORBITOPAL_COMPLETION_INDIVIDUALIZED_GRAPH ||
      p.orbitopal_discovery_order <
          TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_THEN_UNIQUE_LITERAL ||
      p.orbitopal_discovery_order >
          TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_ONLY ||
      p.orbitopal_row_orbit_source <
          TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS ||
      p.orbitopal_row_orbit_source >
          TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_REFINEMENT_CELLS ||
      !setting_mode(p.orbitopal_unique_literal_amo_mode) ||
      !setting_mode(p.orbitopal_generator_fallback_mode) ||
      !setting_mode(p.orbitopal_refinement_mode) ||
      !setting_mode(p.orbitopal_progress_feedback_mode) ||
      !setting_mode(p.orbitopal_base_probe_filter_mode))
    return false;
  sr.orbitopal_orientation =
      static_cast<OrbitopalOrientation>(p.orbitopal_orientation);
  sr.orbitopal_column_order =
      static_cast<OrbitopalColumnOrder>(p.orbitopal_column_order);
  sr.orbitopal_completion =
      static_cast<OrbitopalCompletion>(p.orbitopal_completion);
  sr.orbitopal_discovery_order =
      static_cast<OrbitopalDiscoveryOrder>(p.orbitopal_discovery_order);
  sr.orbitopal_row_orbit_source =
      static_cast<OrbitopalRowOrbitSource>(p.orbitopal_row_orbit_source);
  sr.minimum_orbitopal_rows = p.minimum_orbitopal_rows;
  sr.minimum_orbitopal_columns = p.minimum_orbitopal_columns;
  sr.maximum_orbitopal_matrices = p.maximum_orbitopal_matrices;
  sr.maximum_orbitopal_instances = p.maximum_orbitopal_instances;
  assign_if_nonzero(sr.minimum_orbitopal_clique_columns,
                    p.minimum_orbitopal_clique_columns);
  sr.maximum_orbitopal_clique_vertices =
      p.maximum_orbitopal_clique_vertices;
  sr.maximum_orbitopal_clique_ticks = p.maximum_orbitopal_clique_ticks;
  sr.maximum_orbitopal_product_states =
      p.maximum_orbitopal_product_states;
  apply_setting(p.orbitopal_unique_literal_amo_mode,
                sr.complete_unique_literal_amos);
  apply_setting(p.orbitopal_generator_fallback_mode,
                sr.orbitopal_generator_fallback);
  if (p.orbitopal_refinement_mode == TACTIX_ONLINE_SETTING_DISABLE)
    sr.maximum_orbitopal_refinement_orbits = 0;
  else if (p.orbitopal_refinement_mode == TACTIX_ONLINE_SETTING_ENABLE)
    sr.maximum_orbitopal_refinement_orbits =
        p.maximum_orbitopal_refinement_orbits;
  apply_setting(p.orbitopal_progress_feedback_mode,
                sr.orbitopal_progress_feedback);
  apply_setting(p.orbitopal_base_probe_filter_mode,
                sr.orbitopal_base_probe_filter);
  assign_if_nonzero(sr.orbitopal_base_probe_factor,
                    p.orbitopal_base_probe_factor);
  return true;
}

bool online_parameters(const tactix_online_parameters &source,
                       strategy::InprocessParameters &target) {
  switch (source.kind) {
  case TACTIX_ONLINE_PARAMETERS_INHERIT:
    target = std::monostate{};
    return true;
  case TACTIX_ONLINE_PARAMETERS_BVE: {
    const tactix_online_bve_parameters &p = source.value.bve;
    strategy::BveParameters converted;
    converted.gate_mask = p.gate_mask;
    converted.growth_bound = p.growth_bound;
    converted.occurrence_limit = p.occurrence_limit;
    converted.clause_size_limit = p.clause_size_limit;
    converted.xor_arity_limit = p.xor_arity_limit;
    converted.resolvent_size_limit = p.resolvent_size_limit;
    converted.backward_occurrence_limit = p.backward_occurrence_limit;
    converted.definition_budget = p.definition_budget;
    converted.definition_cores = p.definition_cores;
    converted.definition_core_budget_multiplier =
        p.definition_core_budget_multiplier;
    converted.backward = p.backward != 0;
    converted.on_the_fly_strengthening =
        p.on_the_fly_strengthening != 0;
    if (p.candidate_scope < TACTIX_ONLINE_BVE_CANDIDATES_ALL ||
        p.candidate_scope > TACTIX_ONLINE_BVE_CANDIDATES_QUERY_PURE_CLOSURE)
      return false;
    converted.candidate_scope =
        static_cast<strategy::BveCandidateScope>(p.candidate_scope);
    converted.resolvent_count_limit =
        p.resolvent_count_limit ? p.resolvent_count_limit : UINT32_MAX;
    converted.traversal =
        static_cast<strategy::BveTraversal>(p.traversal);
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSUME: {
    const tactix_online_subsume_parameters &p = source.value.subsume;
    target = strategy::SubsumeForwardParameters{
        p.clause_size_limit, p.large_occurrence_limit,
        p.binary_occurrence_limit};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_STRENGTHEN: {
    const tactix_online_strengthen_parameters &p = source.value.strengthen;
    target = strategy::StrengthenParameters{p.clause_size_limit,
                                            p.occurrence_limit};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_PARITY_CONSEQUENCE: {
    const auto &p = source.value.parity_consequence;
    if (p.descending != 0 && p.descending != 1) return false;
    strategy::ParityConsequenceParameters converted{
        p.maximum_clauses, p.maximum_groups, p.selection_work,
        p.intermediate_width, p.source_count, p.output_width,
        p.descending != 0, p.maximum_proof_additions,
        p.maximum_tree_variables, p.consequence};
    return accept_valid(target, converted);
  }
  case TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE: {
    const auto &p = source.value.affine_consequence;
    strategy::AffineConsequenceParameters converted;
    strategy::copy_parameter_fields<strategy::AffineImageFields>(converted, p);
    strategy::copy_parameter_fields<strategy::AffineBasisFields>(converted, p);
    converted.consequence = p.consequence;
    if (!p.semantics ||
        p.semantics == TACTIX_ONLINE_AFFINE_CONSEQUENCE_SELECTED_V1)
      converted.semantics =
          strategy::AffineConsequenceSemantics::SelectedConsequenceV1;
    else if (p.semantics ==
             TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2)
      converted.semantics =
          strategy::AffineConsequenceSemantics::CertifiedBasisV2;
    else
      return false;
    return accept_valid(target, converted);
  }
  case TACTIX_ONLINE_PARAMETERS_EXACT_COVER_EXTENSION:
    return accept_copied<strategy::ExactCoverExtensionParameters>(
        target, source.value.exact_cover_extension);
  case TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_COVER:
    return accept_copied<strategy::IndependentSetCoverParameters>(
        target, source.value.independent_set_cover);
  case TACTIX_ONLINE_PARAMETERS_SEMANTIC_VARIABLE_ADVICE:
    return accept_copied<strategy::SemanticVariableAdviceParameters>(
        target, source.value.semantic_variable_advice);
  case TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE:
    return accept_copied<strategy::Gf3AffineConsequenceParameters>(
        target, source.value.gf3_affine_consequence);
  case TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT: {
    const auto &p = source.value.direct_csp_support;
    if (p.allow_extension != 0 && p.allow_extension != 1) return false;
    return accept_copied<strategy::DirectCspSupportParameters>(target, p);
  }
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX:
    return false; // Retired tag 42; never reinterpret old payloads.
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY:
    return accept_copied<strategy::NaryCspEqualityParameters>(
        target, source.value.nary_csp_equality);
  case TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY_AMO:
    return accept_copied<strategy::NaryCspEqualityAmoParameters>(
        target, source.value.nary_csp_equality_amo);
  case TACTIX_ONLINE_PARAMETERS_DIRECT_PHP_REFUTATION:
    return accept_copied<strategy::DirectPhpRefutationParameters>(
        target, source.value.direct_php_refutation);
  case TACTIX_ONLINE_PARAMETERS_GF3_GAUSSIAN_CONSEQUENCE:
    return accept_copied<strategy::Gf3GaussianConsequenceParameters>(
        target, source.value.gf3_gaussian_consequence);
  case TACTIX_ONLINE_PARAMETERS_GF3_GENERAL_ROW_CONSEQUENCE:
    return accept_copied<strategy::Gf3GeneralRowConsequenceParameters>(
        target, source.value.gf3_general_row_consequence);
  case TACTIX_ONLINE_PARAMETERS_TSEITIN_CYCLE_BREAKING:
    return accept_copied<strategy::TseitinCycleBreakingParameters>(
        target, source.value.tseitin_cycle_breaking);
  case TACTIX_ONLINE_PARAMETERS_CONGRUENCE: {
    const tactix_online_congruence_parameters &p = source.value.congruence;
    if (p.gate_count_limit > std::numeric_limits<size_t>::max() ||
        p.binary_watch_order < TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER ||
        p.binary_watch_order > TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER)
      return false;
    target = strategy::CongruenceParameters{
        p.and_arity_limit, static_cast<size_t>(p.gate_count_limit),
        p.xor_arity_limit,
        static_cast<strategy::CongruenceBinaryWatchOrder>(p.binary_watch_order)};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_INSTANTIATE: {
    const tactix_online_instantiate_parameters &p = source.value.instantiate;
    target = strategy::InstantiateParameters{
        p.minimum_clause_size, p.occurrence_limit, p.minimum_unassigned,
        p.include_redundant != 0, p.new_material_threshold};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BACKBONE: {
    const tactix_online_backbone_parameters &p = source.value.backbone;
    target = strategy::BackboneParameters{p.initial_rounds,
                                          p.maximum_rounds};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SWEEP: {
    const tactix_online_sweep_parameters &p = source.value.sweep;
    target = strategy::SweepParameters{
        p.max_depth, p.max_depth_ceiling, p.max_env_vars,
        p.max_env_vars_ceiling, p.max_env_clauses,
        p.max_env_clauses_ceiling, p.max_candidates, p.env_ticks,
        p.max_proof_steps, p.max_seeds, p.round_ticks,
        p.strip_false != 0, p.measure_closure != 0,
        p.full_partition != 0, p.skip_embedded_fixed != 0,
        p.flip_rounds, p.resume != 0, p.force_general_proof != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_VIVIFY: {
    const tactix_online_vivify_parameters &p = source.value.vivify;
    if (p.scope > UINT8_MAX)
      return false;
    target = strategy::VivifyParameters{
        static_cast<uint8_t>(p.scope), p.reuse != 0, p.flush != 0,
        p.irredundant_sort_ratio, p.band != 0, p.band_budget, p.band_offset};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_ANALYZE: {
    const tactix_online_analyze_parameters &p = source.value.analyze;
    target = strategy::AnalyzeParameters{
        p.minimize_depth_limit, p.conflict_reuse != 0,
        p.chronological_levels, p.bump_reason_limit, p.bump_reason_depth,
        p.eager_subsume, p.disable_jump_reasons == 0,
        p.enable_jump_reasons_bigbig != 0,
        p.disable_shrink_glue_promotion == 0,
        p.exclude_extension_reason_bumps == 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BCE:
    target = strategy::BceParameters{source.value.bce.occurrence_limit};
    return true;
  case TACTIX_ONLINE_PARAMETERS_TERNARY:
    target = strategy::TernaryParameters{
        source.value.ternary.occurrence_limit,
        source.value.ternary.maximum_add_percent};
    return true;
  case TACTIX_ONLINE_PARAMETERS_FACTOR: {
    const tactix_online_factor_parameters &p = source.value.factor;
    target = strategy::FactorParameters{
        p.maximum_clause_size, p.growth_bound, p.candidate_rounds,
        p.composite != 0, p.demote_fresh != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_TRANSITIVE: {
    const tactix_online_transitive_parameters &p = source.value.transitive;
    target = strategy::TransitiveReduceParameters{
        p.early_exit != 0, p.live_meter != 0,
        p.checked_budget_meter != 0};
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSTITUTE:
    target = strategy::SubstituteParameters{
        source.value.substitute.demote_substituted != 0,
        source.value.substitute.max_rounds,
        source.value.substitute.round_effort_ceiling != 0};
    return true;
  case TACTIX_ONLINE_PARAMETERS_REDUCE: {
    const tactix_online_reduce_parameters &source_reduce =
        source.value.reduce;
    if (!setting_mode(source_reduce.tier_protection_mode) ||
        !setting_mode(source_reduce.compact_collection_mode) ||
        !setting_mode(source_reduce.reduction_bookkeeping_mode))
      return false;
    strategy::ReduceParameters reduce;
    reduce.tier1 = source_reduce.tier1;
    reduce.tier2 = source_reduce.tier2;
    if (source_reduce.victim_denominator) {
      reduce.victim_numerator = source_reduce.victim_numerator;
      reduce.victim_denominator = source_reduce.victim_denominator;
    }
    apply_setting(source_reduce.tier_protection_mode, reduce.protect_tiers);
    apply_setting(source_reduce.compact_collection_mode,
                  reduce.compact_collection);
    apply_setting(source_reduce.reduction_bookkeeping_mode,
                  reduce.reduction_bookkeeping);
    target = reduce;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_WALK: {
    const tactix_online_walk_parameters &source_walk = source.value.walk;
    if (!c_array(source_walk.score_curve, source_walk.score_curve_count))
      return false;
    strategy::WalkParameters walk;
    walk.step_budget = source_walk.step_budget;
    walk.random_seed = source_walk.random_seed;
    walk.seed_candidate = source_walk.seed_candidate;
    walk.algorithm = static_cast<strategy::WalkParameters::Algorithm>(
        source_walk.algorithm);
    walk.initialization =
        static_cast<strategy::WalkParameters::Initialization>(
            source_walk.initialization);
    assign_if_nonzero(walk.maximum_tries, source_walk.maximum_tries);
    assign_if_nonzero(walk.flips_per_try, source_walk.flips_per_try);
    assign_if_nonzero(walk.weight_smoothing_threshold,
                      source_walk.weight_smoothing_threshold);
    if (source_walk.smoothing_denominator) {
      walk.smoothing_old_numerator = source_walk.smoothing_old_numerator;
      walk.smoothing_denominator = source_walk.smoothing_denominator;
    }
    if (!setting_mode(source_walk.aspiration_mode))
      return false;
    apply_setting(source_walk.aspiration_mode, walk.aspiration);
    assign_if_nonzero(walk.even_break_base_milli,
                      source_walk.even_break_base_milli);
    if (source_walk.score_curve_count) {
      walk.score_curve.clear();
      walk.score_curve.reserve(
          static_cast<size_t>(source_walk.score_curve_count));
      for (int i = 0; i < source_walk.score_curve_count; ++i)
        walk.score_curve.push_back(
            {source_walk.score_curve[i].average_clause_size_milli,
             source_walk.score_curve[i].break_base_milli});
    }
    target = std::move(walk);
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_REORDER: {
    const tactix_online_reorder_parameters &source_reorder =
        source.value.reorder;
    strategy::ReorderParameters reorder;
    reorder.maximum_clause_size = source_reorder.maximum_clause_size;
    assign_if_nonzero(reorder.clause_decay_milli,
                      source_reorder.clause_decay_milli);
    assign_if_nonzero(reorder.binary_weight_milli,
                      source_reorder.binary_weight_milli);
    assign_if_nonzero(reorder.balanced_polarity_milli,
                      source_reorder.balanced_polarity_milli);
    target = reorder;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_PARITY_MODEL: {
    const tactix_online_parity_model_parameters &p =
        source.value.parity_model;
    strategy::ParityModelParameters converted;
    converted.maximum_variables = p.maximum_variables;
    converted.maximum_groups = p.maximum_groups;
    converted.maximum_constraints = p.maximum_constraints;
    converted.minimum_group_clauses = p.minimum_group_clauses;
    converted.minimum_equations = p.minimum_equations;
    converted.maximum_free_variables = p.maximum_free_variables;
    converted.candidate_budget = p.candidate_budget;
    assign_refutation_limits(converted, p);
    assign_if_nonzero(converted.refutation_probe_constraints,
                      p.refutation_probe_constraints);
    assign_if_nonzero(converted.refutation_minimum_probe_repeats,
                      p.refutation_minimum_probe_repeats);
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL: {
    const tactix_online_onehot_model_parameters &p =
        source.value.onehot_model;
    strategy::OnehotModelParameters converted{
        p.maximum_variables, p.maximum_domains, p.maximum_constraints,
        p.maximum_equation_arity, p.minimum_domains, p.minimum_equations,
        p.maximum_free_domains, p.candidate_budget};
    assign_refutation_limits(converted, p);
    target = converted;
    return true;
  }
  case TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL:
    return accept_copied<strategy::BalancedSignatureModelParameters>(
        target, source.value.balanced_signature_model);
  case TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL:
    return accept_copied<strategy::FermatModelParameters>(
        target, source.value.fermat_model);
  case TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL:
    return accept_copied<strategy::LookupCircuitModelParameters>(
        target, source.value.lookup_circuit_model);
  case TACTIX_ONLINE_PARAMETERS_SIMON_MODEL:
    return accept_copied<strategy::SimonModelParameters>(
        target, source.value.simon_model);
  case TACTIX_ONLINE_PARAMETERS_STCONN_PROOF:
    return accept_copied<strategy::StconnProofParameters>(
        target, source.value.stconn_proof);
  case TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF:
    return accept_copied<strategy::SignedCardinalityProofParameters>(
        target, source.value.signed_cardinality_proof);
  case TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL:
    return accept_copied<strategy::ExactCoverModelParameters>(
        target, source.value.exact_cover_model);
  case TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL:
    return accept_copied<strategy::IndependentSetModelParameters>(
        target, source.value.independent_set_model);
  case TACTIX_ONLINE_PARAMETERS_RAMSEY_STARTUP: {
    const auto &p = source.value.ramsey_startup;
    strategy::RamseyStartupParameters ramsey{p.recognition_clauses,
        p.recognition_variables, p.recognition_pair_cells, p.recognition_work,
        p.certificate_work, p.maximum_nodes, p.maximum_additions, p.maximum_external_extent};
    return accept_valid(target, ramsey);
  }
  case TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING: {
    const auto &p = source.value.graph_labeling;
    strategy::GraphLabelingParameters graph;
    graph.maximum_vertices = p.maximum_vertices;
    graph.workspace_bytes = p.workspace_bytes;
    static_assert(TACTIX_GRAPH_STAGES == strategy::GRAPH_LABELING_STAGES);
    std::copy(std::begin(p.work), std::end(p.work), graph.work.begin());
    graph.maximum_dfs_vertices = p.maximum_dfs_vertices;
    graph.maximum_dfs_nodes = p.maximum_dfs_nodes;
    graph.maximum_dfs_starts = p.maximum_dfs_starts;
    graph.early_dfs_choices = p.early_dfs_choices;
    graph.repair_rounds = p.repair_rounds;
    graph.anneal_iterations = p.anneal_iterations;
    graph.stall_anneal_iterations = p.stall_anneal_iterations;
    graph.seed = p.seed;
    return accept_valid(target, graph);
  }
  case TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL: {
    const auto &p = source.value.affine_model;
    if (!c_array(p.projection, p.projection_count) ||
        !c_array(p.scopes, p.scope_count) ||
        p.projection_mode < TACTIX_ONLINE_AFFINE_BOUNDARY ||
        p.projection_mode > TACTIX_ONLINE_AFFINE_EXPLICIT ||
        p.automatic_negated < 0 || p.automatic_negated > 1 ||
        p.shuffle_first < 0 || p.shuffle_first > 1 ||
        p.free_phase < 0 || p.free_phase > 1 ||
        p.residual_phase < 0 || p.residual_phase > 1)
      return false;
    strategy::AffineModelParameters a;
    a.maximum_variables=p.maximum_variables; a.maximum_constraints=p.maximum_constraints;
    a.maximum_arity=p.maximum_arity; a.maximum_projection=p.maximum_projection;
    a.minimum_equations=p.minimum_equations; a.maximum_entries=p.maximum_entries;
    a.import_bytes=p.import_bytes; a.linear_bytes=p.linear_bytes;
    a.table_bytes=p.table_bytes; a.maximum_work=p.maximum_work;
    a.candidate_budget=p.candidate_budget; a.permutations=p.permutations; a.seed=p.seed;
    a.projection_mode=static_cast<strategy::AffineProjectionMode>(p.projection_mode);
    a.automatic_negated=p.automatic_negated; a.shuffle_first=p.shuffle_first;
    a.free_phase=p.free_phase; a.residual_phase=p.residual_phase;
    if(p.projection_count) a.projection.assign(p.projection,p.projection+p.projection_count);
    a.scopes.clear();
    for(int i=0;i<p.scope_count;++i) {
      const auto &scope=p.scopes[i];
      a.scopes.push_back({scope.left_size,scope.left_weight,scope.right_weight,scope.total_weight});
    }
    return accept_valid(target, std::move(a));
  }
  case TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY: {
    const tactix_online_sr_parameters &p =
        source.value.substitution_redundancy;
    if ((p.orbitopal_producer && p.general_producer) ||
        p.producer < TACTIX_ONLINE_SR_EXPLICIT_ONLY ||
        p.producer > TACTIX_ONLINE_SR_GRAPH_SYMMETRY)
      return false;
    SubstitutionRedundancyParameters sr;
    sr.maximum_validation_ticks = p.maximum_validation_ticks;
    sr.producer = static_cast<SubstitutionRedundancyProducer>(p.producer);
    if (p.orbitopal_producer || p.general_producer) {
      const auto legacy = p.orbitopal_producer
                              ? SubstitutionRedundancyProducer::Orbitopal
                              : SubstitutionRedundancyProducer::GeneralComplete;
      if (p.producer != TACTIX_ONLINE_SR_EXPLICIT_ONLY &&
          sr.producer != legacy)
        return false;
      sr.producer = legacy;
    }
    if (!setting_mode(p.complete_profile_mode))
      return false;
    const bool complete = p.complete_profile_mode == TACTIX_ONLINE_SETTING_ENABLE;
    if (complete) {
      if (!online_orbitopal_parameters(p, sr) ||
          !online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
      sr.maximum_general_candidates_per_advance =
          p.maximum_general_candidates_per_advance;
    } else if (sr.producer == SubstitutionRedundancyProducer::Orbitopal) {
      if (!online_orbitopal_parameters(p, sr))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
      if (sr.orbitopal_completion ==
              OrbitopalCompletion::IndividualizedGraphCompletion &&
          !online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
    } else if (sr.producer ==
               SubstitutionRedundancyProducer::GeneralComplete) {
      sr.maximum_general_candidates_per_advance =
          p.maximum_general_candidates_per_advance;
    } else if (sr.producer ==
               SubstitutionRedundancyProducer::GraphSymmetry) {
      if (!online_graph_symmetry_parameters(p, sr.graph_symmetry))
        return false;
      sr.maximum_detection_ticks = p.maximum_detection_ticks;
    }
    target = sr;
    return true;
  }
  default:
    return false;
  }
}

// A persistent affine profile: in-range tags, a converted basis, valid limits.
bool online_affine_matrix(
    const tactix_online_affine_matrix_initialize_parameters &p,
    strategy::AffineMatrixInitializeParameters &profile) {
  if (p.admission < 1 || p.admission > 2 || p.operation < 1 ||
      p.operation > 10)
    return false;
  tactix_online_parameters encoded{};
  encoded.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
  encoded.value.affine_consequence = p.basis;
  strategy::InprocessParameters basis;
  if (!online_parameters(encoded, basis) ||
      !std::holds_alternative<strategy::AffineConsequenceParameters>(basis))
    return false;
  profile.basis = std::get<strategy::AffineConsequenceParameters>(basis);
  profile.admission =
      static_cast<strategy::AffineMatrixInitializeParameters::Admission>(p.admission);
  profile.operation =
      static_cast<strategy::AffineMatrixInitializeParameters::Operation>(p.operation);
  profile.maximum_retained_bytes = p.maximum_retained_bytes;
  profile.maximum_deferred_deletions = p.maximum_deferred_deletions;
  profile.maximum_deferred_literal_work = p.maximum_deferred_literal_work;
  return profile.valid();
}

// The v1 conversion never validates the converted program; it ignores the
// trailing validate flag every converter takes.
bool online_program(const tactix_online_program &source, const Kernel &kernel,
                    strategy::OnlineInprocessProgram &target, bool) {
  if (!c_array(source.steps, source.step_count) ||
      !online_repeat(source.repeat_mode, source.repeat_limit, target.repeat))
    return false;
  target.sequence.clear();
  target.sequence.reserve(static_cast<size_t>(source.step_count));
  const auto &rules = tactix_rules();
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step &in = source.steps[i];
    if (in.rule_id < 0 || in.rule_id >= static_cast<int>(rules.size()) ||
        rules[in.rule_id].retired ||
        !c_array(in.instances, in.instance_count) ||
        !c_array(in.factor_grids, in.factor_grid_count))
      return false;
    strategy::OnlineInvocation out;
    out.id = in.step_id;
    out.rule = rules[static_cast<size_t>(in.rule_id)].name;
    if (!online_parameters(in.parameters, out.parameters) ||
        !online_granularity(in.granularity, out.granularity) ||
        !online_repeat(in.repeat_mode, in.repeat_limit, out.repeat) ||
        !online_post_application(in.post_application,
                                 out.post_application) ||
        !online_rule_work_budget(in.rule_budget_mode, in.rule_budget,
                                 out.rule_work_budget))
      return false;
    if (!online_setting_override(in.touch_filter_mode,
                                 out.material.touch_filter) ||
        !online_setting_override(in.likely_kept_redundant_mode,
                                 out.material.likely_kept_redundant))
      return false;
    if (!online_work_unit(in.work_unit, out.work_unit))
      return false;
    if (in.instance_count)
      out.instances.assign(in.instances, in.instances + in.instance_count);
    for (int grid_index = 0; grid_index < in.factor_grid_count; ++grid_index) {
      const tactix_online_factor_grid &source_grid =
          in.factor_grids[grid_index];
      if (!c_array(source_grid.factors, source_grid.factor_count) ||
          !c_array(source_grid.remainders, source_grid.remainder_count))
        return false;
      FactorGrid grid;
      grid.factors.reserve(static_cast<size_t>(source_grid.factor_count));
      if (!append_internal_literals(kernel, source_grid.factors,
                                    source_grid.factor_count, grid.factors))
        return false;
      grid.remainders.reserve(
          static_cast<size_t>(source_grid.remainder_count));
      for (int remainder_index = 0;
           remainder_index < source_grid.remainder_count; ++remainder_index) {
        const tactix_online_factor_remainder &source_remainder =
            source_grid.remainders[remainder_index];
        if (!c_array(source_remainder.literals, source_remainder.literal_count))
          return false;
        std::vector<Lit> remainder;
        remainder.reserve(
            static_cast<size_t>(source_remainder.literal_count));
        if (!append_internal_literals(kernel, source_remainder.literals,
                                      source_remainder.literal_count,
                                      remainder))
          return false;
        grid.remainders.push_back(std::move(remainder));
      }
      out.factor_grids.push_back(std::move(grid));
    }
    out.budget = in.budget_unlimited
                     ? strategy::WorkLimit::unlimited()
                     : strategy::WorkLimit::bounded(in.budget);
    target.sequence.push_back(std::move(out));
  }
  return true;
}

template <class BaseProgram, class Source, class Convert>
bool online_base_program(const Source &source, const Kernel &kernel,
                         strategy::OnlineInprocessProgram &target,
                         bool reserve_steps, Convert &&convert) {
  if (!c_array(source.steps, source.step_count))
    return false;
  std::vector<decltype(source.steps[0].base)> base_steps;
  if (reserve_steps)
    base_steps.reserve(static_cast<size_t>(source.step_count));
  for (int i = 0; i < source.step_count; ++i)
    base_steps.push_back(source.steps[i].base);
  const BaseProgram base{base_steps.data(), source.step_count,
                         source.repeat_mode, source.repeat_limit};
  return std::forward<Convert>(convert)(base, kernel, target, false);
}

bool online_program_v2(const tactix_online_program_v2 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program>(
          source, kernel, target, true, online_program))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v2 &input_step = source.steps[i];
    if (!c_array(input_step.sr_instances, input_step.sr_instance_count))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.substitution_redundancy_instances.reserve(
        static_cast<size_t>(input_step.sr_instance_count));
    for (int j = 0; j < input_step.sr_instance_count; ++j) {
      const tactix_online_sr_instance &input = input_step.sr_instances[j];
      if (!c_array(input.clause, input.clause_count) ||
          !c_array(input.witness, input.witness_count) ||
          !c_array(input.substitution, input.substitution_count))
        return false;
      SubstitutionRedundancyInstance output;
      output.clause.reserve(static_cast<size_t>(input.clause_count));
      if (!append_internal_literals(kernel, input.clause, input.clause_count,
                                    output.clause))
        return false;
      output.witness.reserve(static_cast<size_t>(input.witness_count));
      if (!append_internal_literals(kernel, input.witness, input.witness_count,
                                    output.witness))
        return false;
      output.substitution.reserve(
          static_cast<size_t>(input.substitution_count));
      for (int k = 0; k < input.substitution_count; ++k) {
        const Lit from =
            internal_literal(kernel, input.substitution[k].source_literal);
        if (from == INVALID_LIT)
          return false;
        const Lit to =
            internal_literal(kernel, input.substitution[k].target_literal);
        if (to == INVALID_LIT)
          return false;
        output.substitution.emplace_back(from, to);
      }
      output_step.substitution_redundancy_instances.push_back(
          std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v3(const tactix_online_program_v3 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v2>(
          source, kernel, target, true, online_program_v2))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v3 &input_step = source.steps[i];
    if (!c_array(input_step.condition_instances,
                 input_step.condition_instance_count))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.condition_instances.reserve(
        static_cast<size_t>(input_step.condition_instance_count));
    for (int j = 0; j < input_step.condition_instance_count; ++j) {
      const tactix_online_condition_instance &input =
          input_step.condition_instances[j];
      if (!c_array(input.clause, input.clause_count) ||
          !c_array(input.witness, input.witness_count))
        return false;
      ConditionInstance output;
      output.clause.reserve(static_cast<size_t>(input.clause_count));
      output.witness.reserve(static_cast<size_t>(input.witness_count));
      for (int k = 0; k < input.clause_count; ++k) {
        const int external = input.clause[k];
        if (!external || external == INT32_MIN)
          return false;
        if (kernel_internalize(kernel, external) == INVALID_LIT)
          return false;
        // Keep the request in external identities: an earlier online step
        // may compact variables before this instance is selected.
        output.clause.push_back(internalize(external));
      }
      for (int k = 0; k < input.witness_count; ++k) {
        const int external = input.witness[k];
        if (!external || external == INT32_MIN)
          return false;
        const Lit literal = internalize(external);
        if (var_of(literal) >= kernel.external_to_internal.size())
          return false;
        output.witness.push_back(literal);
      }
      output_step.condition_instances.push_back(std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v4(const tactix_online_program_v4 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v3>(
          source, kernel, target, true, online_program_v3))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v4 &input_step = source.steps[i];
    if (!c_array(input_step.independent_set_cover_instances,
                 input_step.independent_set_cover_instance_count))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.independent_set_cover_instances.reserve(
        static_cast<size_t>(
            input_step.independent_set_cover_instance_count));
    for (int j = 0; j < input_step.independent_set_cover_instance_count; ++j) {
      const tactix_online_independent_set_cover_instance &input =
          input_step.independent_set_cover_instances[j];
      if (input.vertex_count < 2 || !input.vertices)
        return false;
      std::vector<uint32_t> cover;
      cover.reserve(static_cast<size_t>(input.vertex_count));
      uint32_t previous = 0;
      for (int k = 0; k < input.vertex_count; ++k) {
        const uint32_t external = input.vertices[k];
        if (!external || external > static_cast<uint32_t>(INT32_MAX) ||
            external <= previous ||
            kernel_internalize(kernel, static_cast<int>(external)) ==
                INVALID_LIT)
          return false;
        cover.push_back(external);
        previous = external;
      }
      output_step.independent_set_cover_instances.push_back(std::move(cover));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

template <class Element, class Valid>
bool copy_online_int32_values(const Element *source, int count,
                              std::vector<int32_t> &target, bool reserve,
                              Valid valid) {
  if (!c_array(source, count)) return false;
  if (reserve) target.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const int64_t value = source[i];
    if (value < INT32_MIN || value > INT32_MAX ||
        !valid(static_cast<int32_t>(value)))
      return false;
    target.push_back(static_cast<int32_t>(value));
  }
  return true;
}

bool copy_online_int32_values(const int *source, int count,
                              std::vector<int32_t> &target, bool reserve) {
  return copy_online_int32_values(source, count, target, reserve,
                                  [](int32_t) { return true; });
}

// An external literal: nonzero and negatable.
bool nonzero_literal(int32_t literal) {
  return literal && literal != INT32_MIN;
}

bool copy_online_nary_domains(
    const tactix_online_nary_csp_domain *source, int count,
    std::vector<std::vector<int32_t>> &target, bool reserve) {
  if (!c_array(source, count)) return false;
  if (reserve) target.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    std::vector<int32_t> values;
    if (!copy_online_int32_values(source[i].values, source[i].value_count,
                                  values, reserve))
      return false;
    target.push_back(std::move(values));
  }
  return true;
}

bool copy_online_identity(const char *source, uint64_t maximum_bytes,
                          std::string &target) {
  const size_t maximum = static_cast<size_t>(std::min<uint64_t>(
      maximum_bytes, std::numeric_limits<size_t>::max() - 1));
  const size_t length = strnlen(source, maximum + 1);
  if (length > maximum) return false;
  target.assign(source, length);
  return true;
}

// Copies the opaque identity of each of `count` instances, bounded by the
// identity limit of the step's `Parameters`; a step with other parameters
// rejects any instance.
template <class Parameters, class Instance>
bool copy_identity_instances(const Instance *instances, int count,
                             const strategy::InprocessParameters &parameters,
                             std::vector<std::string> &output) {
  output.reserve(static_cast<size_t>(count));
  for (int j = 0; j < count; ++j) {
    const char *identity = instances[j].identity;
    if (!identity) return false;
    const auto *p = std::get_if<Parameters>(&parameters);
    if (!p) return false;
    std::string copied;
    if (!copy_online_identity(identity, p->maximum_identity_bytes, copied))
      return false;
    output.push_back(std::move(copied));
  }
  return true;
}

bool online_program_v5(const tactix_online_program_v5 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v4>(
          source, kernel, target, true, online_program_v4))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const tactix_online_step_v5 &input_step = source.steps[i];
    if (!c_array(input_step.gf3_affine_instances,
                 input_step.gf3_affine_instance_count) ||
        !c_array(input_step.direct_csp_support_instances,
                 input_step.direct_csp_support_instance_count))
      return false;
    strategy::OnlineInvocation &output_step = target.sequence[i];
    output_step.gf3_affine_instances.reserve(
        static_cast<size_t>(input_step.gf3_affine_instance_count));
    for (int j = 0; j < input_step.gf3_affine_instance_count; ++j) {
      const auto &input = input_step.gf3_affine_instances[j];
      output_step.gf3_affine_instances.push_back(
          {input.first_equation, input.second_equation});
    }
    output_step.direct_csp_support_instances.reserve(
        static_cast<size_t>(input_step.direct_csp_support_instance_count));
    for (int j = 0; j < input_step.direct_csp_support_instance_count; ++j) {
      const auto &input = input_step.direct_csp_support_instances[j];
      if (!c_array(input.left_values, input.left_value_count) ||
          !c_array(input.right_values, input.right_value_count))
        return false;
      DirectCspSupportActionSelection output;
      if (!copy_online_int32_values(input.left_values, input.left_value_count,
                                    output.left_values, true) ||
          !copy_online_int32_values(input.right_values,
                                    input.right_value_count,
                                    output.right_values, true))
        return false;
      output.selected_left_value = input.selected_left_value;
      output_step.direct_csp_support_instances.push_back(std::move(output));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v6(const tactix_online_program_v6 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v5>(
          source, kernel, target, true, online_program_v5))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    if (!c_array(input.gf3_gaussian_instances,
                 input.gf3_gaussian_instance_count))
      return false;
    if (!copy_identity_instances<strategy::Gf3GaussianConsequenceParameters>(
            input.gf3_gaussian_instances, input.gf3_gaussian_instance_count,
            target.sequence[i].parameters,
            target.sequence[i].gf3_gaussian_instances))
      return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v7(const tactix_online_program_v7 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v6>(
          source, kernel, target, true, online_program_v6))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    if (!c_array(input.tseitin_cycle_instances,
                 input.tseitin_cycle_instance_count) ||
        input.tseitin_cycle_instance_count > 1)
      return false;
    if (!copy_identity_instances<strategy::TseitinCycleBreakingParameters>(
            input.tseitin_cycle_instances, input.tseitin_cycle_instance_count,
            target.sequence[i].parameters,
            target.sequence[i].tseitin_cycle_instances))
      return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v8(const tactix_online_program_v8 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v7>(
          source, kernel, target, true, online_program_v7))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    // The inherited v8 ABI slots stay physical, but X17 payloads are retired.
    if (input_step.nary_csp_prefix_instance_count != 0 ||
        input_step.nary_csp_prefix_instances != nullptr)
      return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v9(const tactix_online_program_v9 &source,
                       const Kernel &kernel,
                       strategy::OnlineInprocessProgram &target,
                       bool validate) {
  if (!online_base_program<tactix_online_program_v8>(
          source, kernel, target, true, online_program_v8))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    auto *parameters = std::get_if<strategy::AffineConsequenceParameters>(
        &target.sequence[i].parameters);
    if (!c_array(input_step.affine_basis_instances,
                 input_step.affine_basis_instance_count) ||
        (input_step.affine_basis_instance_count &&
         (!parameters ||
          parameters->semantics !=
              strategy::AffineConsequenceSemantics::CertifiedBasisV2)) ||
        (parameters &&
         static_cast<uint64_t>(input_step.affine_basis_instance_count) >
             parameters->maximum_session_keys))
      return false;
    auto &output = target.sequence[i].affine_basis_instances;
    output.reserve(static_cast<size_t>(input_step.affine_basis_instance_count));
    for (int row_index = 0;
         row_index < input_step.affine_basis_instance_count; ++row_index) {
      const auto &input = input_step.affine_basis_instances[row_index];
      if (input.kind < TACTIX_ONLINE_AFFINE_ROW_CONTRADICTION ||
          input.kind > TACTIX_ONLINE_AFFINE_ROW_EXTENDED)
        return false;
      const bool extended = input.kind == TACTIX_ONLINE_AFFINE_ROW_EXTENDED;
      if (!c_array(input.variables, input.variable_count) ||
          !c_array(input.source_equation_ordinals,
                   input.source_equation_ordinal_count) ||
          !c_array(input.source_equations, input.source_equation_count) ||
          static_cast<uint64_t>(input.variable_count) >
              (extended ? parameters->basis_maximum_intermediate_width
                        : parameters->basis_maximum_direct_width) ||
          static_cast<uint64_t>(input.source_equation_ordinal_count) >
              parameters->basis_maximum_sources ||
          input.source_equation_count !=
              input.source_equation_ordinal_count)
        return false;
      AffineBasisRowSelection selection;
      selection.owner_version = input.owner_version;
      selection.elimination_profile = input.elimination_profile;
      selection.kind = static_cast<affine_semantic_extension::BasisRowKind>(
          input.kind);
      if (input.rhs != 0 && input.rhs != 1) return false;
      selection.rhs = input.rhs;
      if (!copy_online_int32_values(
              input.variables, input.variable_count, selection.variables,
              true, [](int32_t variable) { return variable > 0; }))
        return false;
      selection.source_equation_ordinals.reserve(
          static_cast<size_t>(input.source_equation_ordinal_count));
      for (int j = 0; j < input.source_equation_ordinal_count; ++j) {
        const uint64_t ordinal = input.source_equation_ordinals[j];
        if (ordinal > SIZE_MAX) return false;
        selection.source_equation_ordinals.push_back(
            static_cast<size_t>(ordinal));
      }
      uint64_t source_clauses = 0, source_literals = 0;
      selection.source_equations.reserve(
          static_cast<size_t>(input.source_equation_count));
      for (int equation_index = 0;
           equation_index < input.source_equation_count; ++equation_index) {
        const auto &source = input.source_equations[equation_index];
        if (source.variable_count <= 0 ||
            !c_array(source.premises, source.premise_count) ||
            !source.variables ||
            static_cast<uint64_t>(source.variable_count) >
                parameters->maximum_arity ||
            static_cast<uint64_t>(source.premise_count) >
                parameters->maximum_patterns ||
            static_cast<uint64_t>(source.premise_count) >
                parameters->maximum_clauses -
                    std::min(source_clauses, parameters->maximum_clauses) ||
            (source.rhs != 0 && source.rhs != 1))
          return false;
        AffineBasisEquationIdentity equation;
        equation.rhs = source.rhs;
        if (!copy_online_int32_values(
                source.variables, source.variable_count, equation.variables,
                true, [](int32_t variable) { return variable > 0; }))
          return false;
        source_clauses += static_cast<uint64_t>(source.premise_count);
        equation.premises.reserve(static_cast<size_t>(source.premise_count));
        for (int premise_index = 0; premise_index < source.premise_count;
             ++premise_index) {
          const auto &source_premise = source.premises[premise_index];
          if (!c_array(source_premise.clause, source_premise.clause_count) ||
              source_premise.clause_count != source.variable_count ||
              static_cast<uint64_t>(source_premise.clause_count) >
                  parameters->maximum_snapshot_literals -
                      std::min(source_literals,
                               parameters->maximum_snapshot_literals))
            return false;
          AffineBasisSourceIdentity premise;
          premise.native_clause_id = source_premise.native_clause_id;
          if (!copy_online_int32_values(
                  source_premise.clause, source_premise.clause_count,
                  premise.clause, true,
                  [](int32_t literal) { return literal != 0; }))
            return false;
          source_literals +=
              static_cast<uint64_t>(source_premise.clause_count);
          equation.premises.push_back(std::move(premise));
        }
        selection.source_equations.push_back(std::move(equation));
      }
      if (!affine_basis_row_selection_within_limits(
              selection, strategy::action_limits(*parameters)))
        return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v10(const tactix_online_program_v10 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v9>(
          source, kernel, target, true, online_program_v9))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (!c_array(input_step.nary_csp_equality_instances,
                 input_step.nary_csp_equality_instance_count))
      return false;
    auto &output = target.sequence[i].nary_csp_equality_instances;
    output.reserve(
        static_cast<size_t>(input_step.nary_csp_equality_instance_count));
    for (int j = 0; j < input_step.nary_csp_equality_instance_count; ++j) {
      const auto &input = input_step.nary_csp_equality_instances[j];
      NaryCspEqualityActionSelection selection;
      if (!copy_online_nary_domains(input.domains, input.domain_count,
                                    selection.domains, true))
        return false;
      selection.coordinates = {input.left_domain, input.right_domain,
                               input.residual_domain};
      if (!nary_csp_equality_action_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v11(const tactix_online_program_v11 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v10>(
          source, kernel, target, true, online_program_v10))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (!c_array(input_step.gf3_general_row_instances,
                 input_step.gf3_general_row_instance_count))
      return false;
    auto &output = target.sequence[i].gf3_general_row_instances;
    output.reserve(
        static_cast<size_t>(input_step.gf3_general_row_instance_count));
    for (int j = 0; j < input_step.gf3_general_row_instance_count; ++j) {
      const auto &input = input_step.gf3_general_row_instances[j];
      if (!input.identity) return false;
      const auto *parameters =
          std::get_if<strategy::Gf3GeneralRowConsequenceParameters>(
              &target.sequence[i].parameters);
      if (!parameters) return false;
      Gf3GeneralRowActionSelection selection{input.outcome, input.tuple, {}};
      if (!copy_online_identity(
              input.identity, parameters->gaussian.maximum_identity_bytes,
              selection.identity))
        return false;
      if (!gf3_general_row_action_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v12(const tactix_online_program_v12 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v11>(
          source, kernel, target, true, online_program_v11))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input_step = source.steps[i];
    if (!c_array(input_step.nary_csp_equality_amo_instances,
                 input_step.nary_csp_equality_amo_instance_count))
      return false;
    auto &output = target.sequence[i].nary_csp_equality_amo_instances;
    for (int j = 0; j < input_step.nary_csp_equality_amo_instance_count; ++j) {
      const auto &input = input_step.nary_csp_equality_amo_instances[j];
      NaryCspEqualityAmoSelection selection;
      if (!copy_online_nary_domains(input.domains, input.domain_count,
                                    selection.domains, false))
        return false;
      if (!nary_csp_equality_amo_selection_valid(selection)) return false;
      output.push_back(std::move(selection));
    }
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v13(const tactix_online_program_v13 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v12>(
          source, kernel, target, false, online_program_v12))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &profile = source.steps[i].walk_profile;
    if (!profile.objective && !profile.admission && !profile.preparation_budget)
      continue;
    auto *walk = std::get_if<strategy::WalkParameters>(&target.sequence[i].parameters);
    if (!walk || target.sequence[i].rule != "Walk") return false;
    walk->objective = static_cast<strategy::WalkParameters::Objective>(profile.objective);
    walk->admission = static_cast<strategy::WalkParameters::Admission>(profile.admission);
    walk->preparation_budget = profile.preparation_budget;
    if (!walk->valid()) return false;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v14(const tactix_online_program_v14 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v13>(
          source, kernel, target, false, online_program_v13))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    auto &step = target.sequence[i];
    if (input.direct_csp_profile == TACTIX_DIRECT_CSP_PROFILE_LEGACY) {
      if (input.guarded_cover_instance_count || input.guarded_cover_instances) return false;
      if (strategy::any_parameter_field<strategy::DirectCspGuardedCoverParameters>(
              input.guarded_cover_parameters)) return false;
      continue;
    }
    if (input.direct_csp_profile != TACTIX_DIRECT_CSP_PROFILE_GUARDED_COVER_V2 ||
        step.rule != "DirectCspSupport" ||
        !std::holds_alternative<std::monostate>(step.parameters) ||
        !step.direct_csp_support_instances.empty()) return false;
    strategy::DirectCspGuardedCoverParameters p;
    strategy::copy_parameter_fields<strategy::DirectCspGuardedCoverParameters>(
        p, input.guarded_cover_parameters);
    if (!p.valid() || input.guarded_cover_instance_count <= 0 ||
        !input.guarded_cover_instances ||
        static_cast<uint64_t>(input.guarded_cover_instance_count) > p.maximum_session_keys)
      return false;
    const auto copy_literals = [](const tactix_direct_csp_literal_array &src,
                                  uint32_t maximum, std::vector<int32_t> &dst) {
      return static_cast<uint64_t>(src.count) <= maximum &&
             copy_online_int32_values(src.literals, src.count, dst, false,
                                      nonzero_literal);
    };
    for (int j = 0; j < input.guarded_cover_instance_count; ++j) {
      const auto &src = input.guarded_cover_instances[j];
      if (src.domain_count <= 0 || static_cast<uint64_t>(src.domain_count) > p.maximum_domains ||
          !src.domains || !src.selected_values ||
          !c_array(src.exact_or, src.exact_or_count) ||
          static_cast<uint64_t>(src.exact_or_count) > p.maximum_or_definitions ||
          !c_array(src.blockers, src.blocker_count) ||
          static_cast<uint64_t>(src.blocker_count) > p.maximum_cover_literals)
        return false;
      DirectCspGuardedCoverActionSelection dst;
      for (int d = 0; d < src.domain_count; ++d) {
        dst.domains.emplace_back();
        if (!copy_literals(src.domains[d], p.maximum_domain_values, dst.domains.back()) ||
            dst.domains.back().size() < 2 || src.selected_values[d] >= dst.domains.back().size())
          return false;
        dst.selected_values.push_back(src.selected_values[d]);
      }
      if (!copy_literals(src.cover, p.maximum_cover_literals, dst.cover)) return false;
      for (int o = 0; o < src.exact_or_count; ++o) {
        dst.exact_or.push_back({src.exact_or[o].output, {}});
        if (!src.exact_or[o].output || src.exact_or[o].output == INT32_MIN ||
            !copy_literals(src.exact_or[o].inputs, p.maximum_or_inputs, dst.exact_or.back().inputs))
          return false;
      }
      for (int b = 0; b < src.blocker_count; ++b) {
        const auto &v = src.blockers[b];
        dst.blockers.push_back({v.literal, v.domain, v.value, v.or_definition});
      }
      step.direct_csp_guarded_cover_instances.push_back(std::move(dst));
    }
    step.parameters = p;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v15(const tactix_online_program_v15 &source,
                        const Kernel &kernel, strategy::OnlineInprocessProgram &target, bool validate) {
  if (!online_base_program<tactix_online_program_v14>(
          source, kernel, target, false, online_program_v14))
    return false;
  for(int i=0;i<source.step_count;++i) {
    const auto &input=source.steps[i]; auto &step=target.sequence[i];
    const auto &srcp=input.equality_transitivity_parameters;
    if(input.equality_transitivity_profile==TACTIX_EQUALITY_TRANSITIVITY_PROFILE_LEGACY) {
      if(input.equality_transitivity_instances || input.equality_transitivity_instance_count ||
         strategy::any_parameter_field<strategy::NaryCspEqualityTransitivityParameters>(srcp)) return false;
      continue;
    }
    if(input.equality_transitivity_profile!=TACTIX_EQUALITY_TRANSITIVITY_PROFILE_EXACT ||
       step.rule!="NaryCspEqualityTransitivity" || !std::holds_alternative<std::monostate>(step.parameters)) return false;
    strategy::NaryCspEqualityTransitivityParameters p;
    strategy::copy_parameter_fields<strategy::NaryCspEqualityTransitivityParameters>(p, srcp);

    if(!p.valid() || input.equality_transitivity_instance_count<=0 || !input.equality_transitivity_instances ||
       uint64_t(input.equality_transitivity_instance_count)>p.maximum_session_keys) return false;
    const auto copy_domain=[&](const tactix_online_nary_csp_domain &d, std::vector<int32_t> &out) {
      if(d.value_count<2 || uint64_t(d.value_count)>p.maximum_domain_values || !d.values) return false;
      return copy_online_int32_values(
          d.values, d.value_count, out, false, [&](int32_t value) {
            return value && value != INT32_MIN &&
                   uint64_t(value < 0 ? -value : value) <
                       p.maximum_external_extent;
          });
    };
    for(int j=0;j<input.equality_transitivity_instance_count;++j) {
      const auto &input_instance=input.equality_transitivity_instances[j];
      NaryCspEqualityTransitivitySelection choice;
      for(size_t d=0;d<3;++d) {
        if(!copy_domain(input_instance.domains[d],choice.domains[d])) return false;
        const auto &e=input_instance.equalities[d];
        if(e.domain_count!=3 || !e.domains) return false;
        auto &key=choice.equalities[d];
        key.first_equal_domain=e.left_domain; key.second_equal_domain=e.right_domain; key.residual_domain=e.residual_domain;
        for(size_t owner_domain=0;owner_domain<3;++owner_domain) {
          key.domains.emplace_back();
          if(!copy_domain(e.domains[owner_domain],key.domains.back())) return false;
        }
        choice.equality_variables[d]=input_instance.equality_variables[d];
        if(!choice.equality_variables[d] || choice.equality_variables[d]>=p.maximum_external_extent) return false;
      }
      if(!nary_csp_equality_transitivity_selection_valid(choice)) return false;
      step.nary_csp_equality_transitivity_instances.push_back(std::move(choice));
    }
    step.parameters=p;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v16(const tactix_online_program_v16 &source,
                        const Kernel &kernel, strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v15>(
          source, kernel, target, false, online_program_v15))
    return false;
  for (int i=0;i<source.step_count;++i) {
    const auto &input=source.steps[i]; const auto &p=input.affine_matrix;
    auto &step=target.sequence[i];
    if (input.affine_matrix_profile==TACTIX_AFFINE_MATRIX_PROFILE_LEGACY) {
      // Require a truly empty extension. Validate fields, not struct padding.
      if (p.admission || p.operation || p.maximum_retained_bytes ||
          p.maximum_deferred_deletions || p.maximum_deferred_literal_work) return false;
      const auto &b=p.basis;
      if (strategy::any_parameter_field<strategy::AffineImageFields>(b) ||
          b.consequence || b.semantics ||
          strategy::any_parameter_field<strategy::AffineBasisFields>(b))
        return false;
      continue;
    }
    if (input.affine_matrix_profile!=TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1 ||
        step.rule!="AffineMatrixInitialize" || !std::holds_alternative<std::monostate>(step.parameters)) return false;
    strategy::AffineMatrixInitializeParameters out;
    if (!online_affine_matrix(p, out)) return false;
    step.parameters=out;
  }
  return !validate || strategy::validate_online_strategy_program(target);
}

bool online_program_v17(const tactix_online_program_v17 &source,
                        const Kernel &kernel,
                        strategy::OnlineInprocessProgram &target,
                        bool validate) {
  if (!online_base_program<tactix_online_program_v16>(
          source, kernel, target, true, online_program_v16))
    return false;
  for (int i = 0; i < source.step_count; ++i) {
    const auto &input = source.steps[i];
    const auto &p = input.cardinality_search;
    auto &step = target.sequence[i];
    if (step.rule != "CardinalitySearch") {
      if (input.cardinality_search_instance_count ||
          input.cardinality_search_instances ||
          p.maximum_support_variables || p.maximum_constraints ||
          p.maximum_source_clauses || p.maximum_reason_width ||
          p.maximum_inspection_work || p.maximum_resolution_work ||
          p.maximum_proof_additions || p.maximum_proof_literals ||
          p.maximum_source_scan_work || p.maximum_arena_words ||
          p.maximum_journal_bytes)
        return false;
      continue;
    }
    if (!std::holds_alternative<std::monostate>(step.parameters) ||
        input.cardinality_search_instance_count != 1 ||
        !input.cardinality_search_instances)
      return false;
    CardinalitySearchActionLimits limits;
    limits.inspection = {p.maximum_support_variables, p.maximum_constraints,
                         p.maximum_source_clauses, p.maximum_reason_width,
                         p.maximum_inspection_work};
    limits.resolution = {p.maximum_resolution_work,
                         p.maximum_proof_additions, p.maximum_proof_literals};
    limits.maximum_source_scan_work = p.maximum_source_scan_work;
    limits.maximum_arena_words = p.maximum_arena_words;
    limits.maximum_journal_bytes = p.maximum_journal_bytes;
    if (!limits.valid()) return false;
    const auto &instance = input.cardinality_search_instances[0];
    if (instance.bound_count <= 0 ||
        static_cast<uint32_t>(instance.bound_count) >
            limits.inspection.maximum_constraints ||
        !instance.bounds)
      return false;
    std::vector<CardinalitySearchBound> bounds;
    bounds.reserve(static_cast<size_t>(instance.bound_count));
    for (int b = 0; b < instance.bound_count; ++b) {
      const auto &source_bound = instance.bounds[b];
      if (source_bound.literal_count <= 0 ||
          static_cast<uint32_t>(source_bound.literal_count) >
              limits.inspection.maximum_support_variables ||
          !source_bound.literals)
        return false;
      CardinalitySearchBound bound;
      bound.lower = source_bound.lower;
      bound.upper = source_bound.upper;
      if (!copy_online_int32_values(source_bound.literals,
                                    source_bound.literal_count, bound.literals,
                                    true, nonzero_literal))
        return false;
      bounds.push_back(std::move(bound));
    }
    step.parameters = limits;
    step.cardinality_search_instances.push_back(std::move(bounds));
  }
  return !validate || strategy::validate_online_strategy_program(target);
}


void online_result(const strategy::InvocationResult &source,
                   tactix_online_result &target) {
  target.status = static_cast<int>(source.status);
  target.rule_result = static_cast<int>(source.rule_result);
  target.budget_status = static_cast<int>(source.budget_status);
  target.work_unit = static_cast<int>(source.work_unit);
  target.work = source.work;
  target.applications = source.applications;
  target.step_id = source.step_id;
  target.rule_id = source.rule_id;
  target.instance = source.instance;
  target.made_rule_progress = source.made_rule_progress;
  target.selection_complete = source.selection_complete;
  target.before_arena_generation = source.before.arena_generation;
  target.before_touch_epoch = source.before.touch_epoch;
  target.after_arena_generation = source.after.arena_generation;
  target.after_touch_epoch = source.after.touch_epoch;
  target.before_strategy_epoch = source.before.strategy_epoch;
  target.after_strategy_epoch = source.after.strategy_epoch;
  target.configuration_revision = source.configuration_revision;
}

template <class Source, class Convert>
int configure_online_program(tactix_solver *solver, const Source *source,
                             uint64_t revision, int reconfigure_mode,
                             Convert &&convert) {
  if (!solver || !source ||
      (reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RESET &&
       reconfigure_mode != TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS))
    return -1;
  strategy::OnlineInprocessProgram program;
  if (!convert(*source, solver->kernel, program, true))
    return -1;
  std::unique_ptr<strategy::OnlineInprocessExecutor> replacement;
  strategy::OnlineInprocessExecutor *executor = solver->online.get();
  if (!executor) {
    replacement = std::make_unique<strategy::OnlineInprocessExecutor>();
    executor = replacement.get();
  }
  const strategy::ReconfigureMode mode =
      reconfigure_mode == TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS
          ? strategy::ReconfigureMode::RetainMatchingStepBudgets
          : strategy::ReconfigureMode::ResetRuntime;
  if (!executor->configure(program, revision, mode))
    return -1;
  if (replacement)
    solver->online = std::move(replacement);
  // A rejected request must not close the C input stream. Conversion and
  // executor validation only read the stable external-to-internal map;
  // finish_input retires unused variables but does not remap those literals.
  input_complete(solver);
  return 0;
}

// The rule a public entry point names: registered, unretired and admitted by
// its guard. `rejected` is the caller's own argument check, made before the
// guard runs.
const RuleVTable *guarded_rule(tactix_solver *s, int rule_id,
                               bool rejected = false) {
  input_complete(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired || rejected)
    return nullptr;
  const RuleVTable &r = rules[rule_id];
  return rule_guard(r, s->kernel) ? &r : nullptr;
}

int public_rule_instances(Kernel &k,
                          uint32_t (*enumerate)(Kernel &, uint64_t *, uint32_t),
                          uint64_t *out, int cap) {
  if (!enumerate) { // no uint64 choice; X5 has a separate structured tuple
    if (cap >= 1)
      out[0] = INSTANCE_CANONICAL;
    return cap >= 1 ? 1 : 0;
  }
  return static_cast<int>(enumerate(k, out, static_cast<uint32_t>(cap)));
}

// tactix_retier_schedule mirrors RetierSchedule field for field.
struct RetierScheduleFields {};
template <class Schedule, class Visitor>
auto visit_parameter_fields(strategy::FieldsOf<RetierScheduleFields>,
                            Schedule &s, Visitor visit) {
  return visit(s.enabled, s.next_conflicts, s.interval, s.max_interval,
               s.growth, s.tier1_default, s.tier2_default,
               s.tier1_relative_permille, s.tier2_relative_permille,
               s.tier1_by_mode[0], s.tier1_by_mode[1], s.tier2_by_mode[0],
               s.tier2_by_mode[1]);
}

} // namespace

extern "C" {

tactix_solver *tactix_init(void) { return new tactix_solver(); }

void tactix_release(tactix_solver *s) {
  if(s && s->query_owner) {s->query_status=session::QuerySolveStatus::InvalidState;return;}
  delete s;
}
int tactix_bind_structural_provenance(tactix_solver *s, FILE *stream) {
  if (!s || (stream && std::ferror(stream))) return -1;
  s->kernel.structural_provenance = stream;
  external_state_change(s);
  return 0;
}

int tactix_discard_structural_provenance(tactix_solver *s) {
  if (!s) return -1;
  s->kernel.structural_provenance = StructuralJournal::discard();
  external_state_change(s);
  return 0;
}

static void attach_proof(tactix_solver *s) {
  s->kernel.proof = &s->proof;
  s->proof.bind_variable_map(&s->kernel.internal_to_external);
  s->proof.bind_external_variable_map(&s->kernel.external_to_internal);
}

// The CLI's ingestion into a fresh session. finish_input runs exactly once, in
// parse_dimacs or below, so the session's own boundary is marked closed.
int tactix_read_dimacs(tactix_solver *s, FILE *in) {
  if (!s || !in || s->input_completed || !s->scratch.empty() ||
      s->kernel.nvars || s->kernel.next_clause_id)
    return -1;
  // N2: a session with no proof takes the search of a session with one.
  if (!s->pending_input_proof && !s->proof.open_nowhere())
    return -1;
  s->pending_input_proof = false;
  attach_proof(s);
  const char *err = parse_dimacs(in, s->kernel);
  // A malformed stream stops before the input closes, and its partial formula can
  // already be refuted: the proof needs that empty clause.
  if (err)
    finish_input(s->kernel);
  s->input_completed = true;
  external_state_change(s);
  return err ? -1 : 0;
}

int tactix_add_checked(tactix_solver *s, int lit) {
  // finish_input retires absent variables and fixes proof input ids. Until O7
  // supplies a full incremental lifecycle, accepting a later clause would
  // leave both the live population and the certificate input out of sync.
  if (s->input_completed)
    return -1;
  if (lit == 0) {
    ingest_clause(s->kernel, s->scratch);
    s->scratch.clear();
  } else {
    s->scratch.push_back(internalize(lit));
  }
  external_state_change(s);
  return 0;
}

void tactix_add(tactix_solver *s, int lit) {
  if (tactix_add_checked(s, lit) != 0)
    std::abort();
}

// The C API has no end of file, so "the input is complete" is the first call
// that reasons about it, so every entry point below that could consume a derived
// id goes through here first. Idempotent.
// Adding clauses after this point is outside the v1 contract: retired
// variables are no longer in the active search population, and with a proof
// attached their ids would also land above already-derived ones.
static void input_complete(tactix_solver *s) {
  if (s->input_completed)
    return;
  // N2: tactix_add imports with no writer in every session, and the proof attaches
  // here by replaying the input, so no session's ids or root view depend on it.
  if (!s->pending_input_proof && !s->proof.open_nowhere())
    std::abort();
  attach_replayed_input_proof(s->kernel, s->proof);
  s->pending_input_proof = false;
  finish_input(s->kernel);
  s->input_completed = true;
}

int tactix_proof(tactix_solver *s, const char *path, int format) {
  if (s->input_completed || s->pending_input_proof)
    return -1;
  // Public values are append-only; see TACTIX_PROOF_* in tactix.h. Every format
  // takes the same search, on the streaming-PBP writer (N2).
  bool opened;
  switch (format) {
  case 3:
    opened = s->proof.open(path, ProofFormat::DsrBinary);
    break;
  case 4:
    opened = s->proof.open(path, ProofFormat::DsrAscii);
    break;
  case 6:
    opened = s->proof.open(path, ProofFormat::PbpStreaming);
    break;
  case 7:
    opened = s->proof.open_automatic(path, true);
    break;
  default:
    return -1; // includes the retired values; public values are never reused
  }
  if (!opened)
    return -1;
  s->pending_input_proof = true;
  external_state_change(s);
  return 0;
}

const char *tactix_proof_route(tactix_solver *s) {
  if (!s || s->proof.nowhere() || (!s->kernel.proof && !s->pending_input_proof))
    return nullptr;
  return s->proof.verification_route();
}

int tactix_set_default_option(tactix_solver *s, const char *name, const char *value) {
  return s?api_detail::set_strategy_option(s->default_options,name,value):-1;
}

int tactix_defer_startup_state(tactix_solver *s) {
  if (!s || s->input_completed || s->kernel.nvars || s->kernel.h != &s->heuristics)
    return -1;
  s->kernel.h = nullptr;
  s->kernel.saved_phases = nullptr;
  kernel_defer_cold_state(s->kernel);
  return 0;
}

void tactix_set_bve_gates(tactix_solver *s, unsigned mask) {
  s->kernel.bve_gate_mask = mask;
  external_state_change(s);
}

// Kernel configuration setters — the §5.6 pose surface. Plain writes to config
// fields (kernel.hpp documents each field's soundness class); no scheduling
// state, no guard interaction.
void tactix_set_bve_bound(tactix_solver *s, unsigned bound) {
  s->kernel.bve_bound = bound;
  external_state_change(s);
}
void tactix_set_factor_bound(tactix_solver *s, unsigned bound) {
  s->kernel.factor_bound = bound;
  external_state_change(s);
}
void tactix_set_factor_composite(tactix_solver *s, int on) {
  s->kernel.factor_composite = on != 0;
  external_state_change(s);
}
void tactix_set_work_budget(tactix_solver *s, uint64_t budget) {
  s->kernel.work_budget = budget;
  external_state_change(s);
}
void tactix_set_subsume_limits(tactix_solver *s, uint32_t clause_size,
                               uint32_t large_occurrences,
                               uint32_t binary_occurrences) {
  s->kernel.subsume_forward_parameters = {
      clause_size, large_occurrences, binary_occurrences};
  external_state_change(s);
}
void tactix_set_strengthen_limits(tactix_solver *s, uint32_t clause_size,
                                  uint32_t occurrences) {
  s->kernel.strengthen_parameters = {clause_size, occurrences};
  external_state_change(s);
}
void tactix_set_analyze_minimize_depth(tactix_solver *s, uint32_t depth) {
  s->kernel.analyze_parameters.minimize_depth_limit = depth;
  external_state_change(s);
}
int tactix_set_congruence_limits(tactix_solver *s, uint32_t and_arity,
                                 uint64_t gate_count, uint32_t xor_arity) {
  strategy::CongruenceParameters parameters{
      and_arity, static_cast<size_t>(gate_count), xor_arity,
      s->kernel.congruence_parameters.binary_watch_order};
  if (uint64_t{parameters.gate_count_limit} != gate_count ||
      !parameters.valid())
    return -1;
  s->kernel.congruence_parameters = parameters;
  external_state_change(s);
  return 0;
}
int tactix_set_congruence_watch_order(tactix_solver *s, int order) {
  if (order < TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER ||
      order > TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER)
    return -1;
  s->kernel.congruence_parameters.binary_watch_order =
      static_cast<strategy::CongruenceBinaryWatchOrder>(order);
  external_state_change(s);
  return 0;
}
void tactix_set_instantiate_limits(tactix_solver *s,
                                   uint32_t minimum_clause_size,
                                   uint32_t occurrence_limit,
                                   uint32_t minimum_unassigned,
                                   int include_redundant) {
  s->kernel.instantiate_parameters.minimum_clause_size = minimum_clause_size;
  s->kernel.instantiate_parameters.occurrence_limit = occurrence_limit;
  s->kernel.instantiate_parameters.minimum_unassigned = minimum_unassigned;
  s->kernel.instantiate_parameters.include_redundant = include_redundant != 0;
  external_state_change(s);
}
void tactix_set_instantiate_material_threshold(tactix_solver *s,
                                               uint32_t threshold) {
  s->kernel.instantiate_parameters.new_material_threshold = threshold;
  external_state_change(s);
}
void tactix_set_backbone_round_limits(tactix_solver *s,
                                      uint32_t initial_rounds,
                                      uint32_t maximum_rounds) {
  s->kernel.backbone_parameters = {initial_rounds, maximum_rounds};
  external_state_change(s);
}
// Every band Vivify implements is settable, and a clamp here would be an N1
// breach rather than input validation: substituting a different band makes
// scopes the policy still uses unreachable through the API, so the guard says
// applicable while apply quietly declines. Out-of-range leaves the pose
// untouched (tests/test_vivify_n1.cpp).
void tactix_set_vivify_scope(tactix_solver *s, unsigned scope) {
  if (scope <= 5) {
    s->kernel.vivify_scope = static_cast<uint8_t>(scope);
    external_state_change(s);
  }
}

int tactix_set_vivify_parameters(tactix_solver *s, unsigned scope, int reuse,
                                  int flush,
                                  uint32_t irredundant_sort_ratio) {
  strategy::VivifyParameters parameters{
      static_cast<uint8_t>(scope), reuse != 0, flush != 0,
      irredundant_sort_ratio};
  if (scope > 5 || !parameters.valid())
    return -1;
  s->kernel.vivify_scope = parameters.scope;
  s->kernel.vivify_reuse = parameters.reuse;
  s->kernel.vivify_flush = parameters.flush;
  s->kernel.vivify_irredundant_sort_ratio =
      parameters.irredundant_sort_ratio;
  external_state_change(s);
  return 0;
}

namespace {
int leave_phase(tactix_solver *s);
}

int tactix_solve(tactix_solver *s) {
  input_complete(s);
  // Search runs outside the inprocessing phase bracket.
  if (s->phase.open()) {
    if (s->phase.occurrences_open()) (void)leave_phase(s);
    else s->phase.exit_terminal(s->kernel);
  }
  int r;
  {
    ScopedStrategySettings settings(&s->default_options);
    r = default_solve(s->kernel, nullptr, s->regions.get());
  }
  rule_journal_flush();
  external_state_change(s);
  if (r == 10)
    // A public solver can be solved again. Replay of ρ belongs in the
    // external model cache only: writing its repaired values into the live
    // assignment would desynchronize values from trail/reasons and make the
    // next solve start from a state that no longer models the live clauses.
    prepare_external_model(s);
  if (r == 20)
    certify_unsat(s);
  return r;
}

int tactix_value(tactix_solver *s, int elit) {
  if(s->query_owner) {
    if(!elit || elit==INT32_MIN)return 0;
    if(s->query_owner->check_model(s->query_ticket).status!=session::ModelStatus::Valid)return 0;
    const Lit literal=internalize(elit);
    if(var_of(literal)>=s->kernel.model_values.size()/2)return 0;
    const auto value=s->kernel.model_values[literal];
    return value>0?elit:value<0?-elit:0;
  }
  // Direct rule applications can report SAT without an intervening status
  // query. Preserve partial-value reads outside a terminal state.
  if (s->input_completed && has_sat_assignment(s->kernel))
    prepare_external_model(s);
  const Lit external = internalize(elit);
  if (var_of(external) >= kernel_num_output_vars(s->kernel))
    return -elit; // unmentioned variable: any value works
  const int8_t v = s->kernel.model_extended &&
                          s->model_strategy_epoch == s->kernel.strategy_epoch
                       ? s->kernel.model_values[external]
                       : kernel_external_value(s->kernel, external);
  return v > 0 ? elit : -elit;
}

int tactix_rule_count(void) {
  return static_cast<int>(tactix_rules().size());
}

const char *tactix_rule_name(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return nullptr;
  return rules[rule_id].name;
}

int tactix_action_kind(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[rule_id].retired)
    return -1;
  return static_cast<int>(rules[rule_id].kind);
}

int tactix_rules_applicable(tactix_solver *s, int *out, int cap) {
  input_complete(s);
  const auto &rules = tactix_rules();
  int n = 0;
  for (size_t i = 0; i < rules.size() && n < cap; i++)
    if (rule_guard(rules[i], s->kernel))
      out[n++] = static_cast<int>(i);
  return n;
}

static int result_code(tactix_solver *solver, ApplyResult res) {
  if(solver->query_owner)
    solver->query_owner->drain_learned(solver->query_ticket, solver->query_stop);
  if(solver->query_owner && res==ApplyResult::Sat) {
    prepare_external_model(solver);
    return solver->query_status==session::QuerySolveStatus::Complete &&
        solver->query_owner->check_model(solver->query_ticket).status==session::ModelStatus::Valid?10:0;
  }
  if(solver->query_owner && res==ApplyResult::Unsat)return tactix_status(solver);
  switch (res) {
  case ApplyResult::Ok:
    return 0;
  case ApplyResult::NoOp:
    return 1;
  case ApplyResult::Conflict:
    return 2;
  case ApplyResult::Sat:
    prepare_external_model(solver);
    return 10;
  case ApplyResult::Unsat:
    certify_unsat(solver);
    return 20;
  }
  return 1;
}

int tactix_apply(tactix_solver *s, int rule_id) {
  const RuleVTable *r = guarded_rule(s, rule_id);
  if (!r)
    return -1;
  const ApplyResult res =
      apply_published(s, [&] { return apply_rule(*r, s->kernel); });
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(s, res);
}

int tactix_rule_instances(tactix_solver *s, int rule_id, uint64_t *out,
                          int cap) {
  const RuleVTable *r = guarded_rule(s, rule_id, cap < 0);
  return r ? public_rule_instances(s->kernel, r->instances, out, cap) : -1;
}

int tactix_apply_instance(tactix_solver *s, int rule_id, uint64_t instance) {
  const RuleVTable *r = guarded_rule(s, rule_id);
  if (!r)
    return -1;
  // A rule with no uint64 instance surface rejects a non-canonical payload.
  // Running the canonical round instead would be payload aliasing: the caller
  // asked for one instance and silently got another action, with no way to notice
  // (tests/test_api_instances.c).
  if (!r->apply_instance && instance != INSTANCE_CANONICAL)
    return -1;
  const ApplyResult res = apply_published(s, [&] {
    if (instance == INSTANCE_CANONICAL || !r->apply_instance)
      return apply_rule(*r, s->kernel); // canonical (the only instance if no slot)
    return apply_rule_instance(*r, s->kernel, instance);
  });
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(s, res);
}

int tactix_apply_condition(tactix_solver *s, const int *clause,
                           int clause_count, const int *witness,
                           int witness_count) {
  input_complete(s);
  if (!c_array(clause, clause_count) || !c_array(witness, witness_count))
    return -1;
  ConditionInstance instance;
  instance.clause.reserve(static_cast<size_t>(clause_count));
  instance.witness.reserve(static_cast<size_t>(witness_count));
  if (!append_internal_literals(s->kernel, clause, clause_count,
                                instance.clause))
    return -1;
  for (int i = 0; i < witness_count; ++i) {
    const int external = witness[i];
    if (!external || external == INT32_MIN)
      return -1;
    instance.witness.push_back(internalize(external));
  }
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("Condition")];
  ConditionPrepared prepared;
  const uint64_t validation_started = now_nanos();
  const bool applicable = condition_prepare(s->kernel, instance, prepared);
  if (rule.id < Stats::MAX_TIMED_RULES)
    s->kernel.stats.rule_nanos[rule.id] += now_nanos() - validation_started;
  if (!applicable)
    return -1;
  std::string candidate = "clause=";
  for (int i = 0; i < clause_count; ++i) {
    if (i)
      candidate.push_back(',');
    candidate += std::to_string(clause[i]);
  }
  candidate += ";witness=";
  for (int i = 0; i < witness_count; ++i) {
    if (i)
      candidate.push_back(',');
    candidate += std::to_string(witness[i]);
  }
  const ApplyResult result = apply_published(s, [&] {
    return apply_rule_structured(
        rule, s->kernel, candidate.c_str(),
        [&] { return condition_commit(s->kernel, instance, prepared); });
  });
  kernel_check_invariants(s->kernel);
  return result_code(s, result);
}

int tactix_define_extension(tactix_solver *s, int gate, int left, int right) {
  if (!s || gate < TACTIX_EXTENSION_AND || gate > TACTIX_EXTENSION_XOR)
    return -1;
  input_complete(s);
  const ExtensionDefinition definition{static_cast<ExtensionGate>(gate), left, right};
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("ExtendedResolution")];
  if (!rule_guard(rule, s->kernel) ||
      !extended_resolution_valid(s->kernel, definition)) return -1;
  const std::string description = "gate=" + std::to_string(gate) +
      ";left=" + std::to_string(left) + ";right=" + std::to_string(right);
  int32_t output = 0;
  if (s->regions) s->regions->begin_definition();
  apply_published(s, [&] {
    return apply_rule_structured_preserve_startup(rule, s->kernel,
        description.c_str(), [&] {
          output = extended_resolution_define(s->kernel, definition);
          return output ? ApplyResult::Ok : ApplyResult::NoOp;
        });
  });
  if (s->regions) s->regions->end_definition(output);
  if (output) s->kernel.variable_retention.insert(static_cast<uint32_t>(output-1));
  kernel_check_invariants(s->kernel);
  return output ? output : -1;
}

int tactix_regions_begin(tactix_solver *s) {
  if (!s || s->regions) return -1;
  input_complete(s);
  if (s->kernel.level || s->kernel.inconsistent || s->kernel.probing.recording)
    return -1;
  s->regions = std::make_unique<strategy::RegionProgram>(s->kernel);
  external_state_change(s); return 0;
}
int tactix_regions_clear(tactix_solver *s) {
  if (!s) return -1;
  s->regions.reset(); external_state_change(s); return 0;
}
int tactix_region_add(tactix_solver *s, const int *variables, int count) {
  if (!s || !s->regions || !c_array(variables, count)) return -1;
  const int id = s->regions->add_region({variables, size_t(count)});
  if (id >= 0) external_state_change(s);
  return id;
}
static bool region_request(tactix_solver *s, int rule, int literal, int region,
                           uint64_t ticks, strategy::RegionQuery &q) {
  if (!s || !s->regions || region < 0) return false;
  q = {rule, literal, uint32_t(region), ticks};
  return s->regions->valid_query(q);
}
int tactix_region_queue(tactix_solver *s, int rule, int literal, int region,
                        uint64_t ticks) {
  strategy::RegionQuery q;
  if (!region_request(s, rule, literal, region, ticks, q) ||
      !s->regions->enqueue(q)) return -1;
  external_state_change(s); return 0;
}
int tactix_region_apply(tactix_solver *s, int rule, int literal, int region,
                        uint64_t ticks) {
  strategy::RegionQuery q;
  if (!region_request(s, rule, literal, region, ticks, q) ||
      !root_action_guard(s->kernel, false).clausal_certificate().finish()) return -1;
  const auto result = apply_published(s, [&] { return s->regions->apply(q); });
  kernel_check_invariants(s->kernel);
  return result_code(s, result);
}

int tactix_fresh_variable_start(tactix_solver *s) {
  if (!s) return -1;
  input_complete(s);
  const auto extent=s->kernel.external_to_internal.size();
  return extent<INT32_MAX ? int(extent+1) : -1;
}

int tactix_set_variable_retention(tactix_solver *s, int external, int retain) {
  if (!s || external<=0 || (retain!=0 && retain!=1)) return -1;
  input_complete(s);
  auto &k=s->kernel;
  const Lit l=kernel_internalize(k,external);
  if (k.level || l==INVALID_LIT || kernel_var_retired(k,var_of(l))) return -1;
  if (retain) k.variable_retention.insert(uint32_t(external-1));
  else k.variable_retention.erase(uint32_t(external-1));
  external_state_change(s);
  return 0;
}

int tactix_variable_retained(tactix_solver *s, int external) {
  if (!s || external<=0) return -1;
  input_complete(s);
  const Lit l=kernel_internalize(s->kernel,external);
  if (l==INVALID_LIT) return -1;
  return s->kernel.variable_retention.count(uint32_t(external-1)) ? 1:0;
}

int tactix_bump_variable(tactix_solver *s, int external, uint32_t dose) {
  if (!s || external<=0 || !dose) return -1;
  input_complete(s);
  auto &k=s->kernel;
  if (k.inconsistent || k.level || k.conflict_valid || k.pending.valid || !k.h) return -1;
  strategy::SemanticVariableAdvice advice;
  advice.action_id=1;
  advice.source=strategy::state_version(k);
  const Lit literal=kernel_internalize(k,external);
  if (literal==INVALID_LIT) return -1;
  const auto role=kernel_is_solver_extension(k,var_of(literal))
      ? strategy::SemanticVariableRole::IntroducedTheory
      : strategy::SemanticVariableRole::ExistingTheory;
  advice.variables.push_back({uint32_t(external),role,dose});
  strategy::SemanticVariableAdviceResult outcome;
  const auto &rule=tactix_rules()[tactix_rule_id("SemanticVariableAdvice")];
  apply_published(s,[&] {
    return apply_rule_structured_preserve_startup(rule,k,"explicit-variable-priority",[&] {
      outcome=strategy::commit_semantic_variable_advice(k,advice,{1,dose});
      return outcome.status==strategy::SemanticVariableAdviceStatus::Applied
          ? ApplyResult::Ok:ApplyResult::NoOp;
    });
  });
  return outcome.status==strategy::SemanticVariableAdviceStatus::Applied ? 0:-1;
}

int tactix_rule_semantic_instances(tactix_solver *s, int rule_id,
                                   uint64_t *out, int cap) {
  const RuleVTable *r = guarded_rule(s, rule_id, cap < 0);
  if (!r)
    return -1;
  return public_rule_instances(
      s->kernel, r->semantic_instances ? r->semantic_instances : r->instances,
      out, cap);
}

int tactix_apply_semantic_instance(tactix_solver *s, int rule_id,
                                   uint64_t instance) {
  const RuleVTable *r = guarded_rule(s, rule_id);
  if (!r)
    return -1;
  const auto apply = r->apply_semantic_instance ? r->apply_semantic_instance
                                                : r->apply_instance;
  if (!apply && instance != INSTANCE_CANONICAL)
    return -1;
  const ApplyResult result = apply_published(s, [&] {
    if (instance == INSTANCE_CANONICAL || !apply)
      return apply_rule(*r, s->kernel);
    if (r->apply_semantic_instance)
      return apply_rule_semantic_instance(*r, s->kernel, instance);
    return apply_rule_instance(*r, s->kernel, instance);
  });
  kernel_check_invariants(s->kernel);
  return result_code(s, result);
}

static_assert(TACTIX_ANALYZE_TARGET_CHRONO == ANALYZE_TARGET_CHRONO);

int tactix_install_retier_schedule(tactix_solver *s,
                                   const tactix_retier_schedule *schedule) {
  if (!s || !schedule || schedule->growth == 0 ||
      schedule->tier1_relative_permille > 1000 ||
      schedule->tier2_relative_permille > 1000)
    return -1;
  strategy::copy_parameter_fields<RetierScheduleFields>(
      s->kernel.retier_schedule, *schedule);
  return 0;
}

int tactix_read_retier_schedule(tactix_solver *s, tactix_retier_schedule *out) {
  if (!s || !out)
    return -1;
  *out = {};
  strategy::copy_parameter_fields<RetierScheduleFields>(
      *out, s->kernel.retier_schedule);
  return 0;
}

int tactix_online_configure(tactix_solver *s,
                            const tactix_online_program *source,
                            uint64_t revision, int reconfigure_mode) {
  return configure_online_program(s, source, revision, reconfigure_mode,
                                  online_program);
}

#define TACTIX_DEFINE_ONLINE_CONFIGURE(version)                               \
  int tactix_online_configure_v##version(                                    \
      tactix_solver *s, const tactix_online_program_v##version *source,       \
      uint64_t revision, int reconfigure_mode) {                             \
    return configure_online_program(s, source, revision, reconfigure_mode,   \
                                    online_program_v##version);              \
  }

TACTIX_DEFINE_ONLINE_CONFIGURE(2)
TACTIX_DEFINE_ONLINE_CONFIGURE(3)
TACTIX_DEFINE_ONLINE_CONFIGURE(4)
TACTIX_DEFINE_ONLINE_CONFIGURE(5)
TACTIX_DEFINE_ONLINE_CONFIGURE(6)
TACTIX_DEFINE_ONLINE_CONFIGURE(7)
TACTIX_DEFINE_ONLINE_CONFIGURE(8)
TACTIX_DEFINE_ONLINE_CONFIGURE(9)
TACTIX_DEFINE_ONLINE_CONFIGURE(10)
TACTIX_DEFINE_ONLINE_CONFIGURE(11)
TACTIX_DEFINE_ONLINE_CONFIGURE(12)
TACTIX_DEFINE_ONLINE_CONFIGURE(13)
TACTIX_DEFINE_ONLINE_CONFIGURE(14)
TACTIX_DEFINE_ONLINE_CONFIGURE(15)
TACTIX_DEFINE_ONLINE_CONFIGURE(16)
TACTIX_DEFINE_ONLINE_CONFIGURE(17)

#undef TACTIX_DEFINE_ONLINE_CONFIGURE

int tactix_online_advance(tactix_solver *s, tactix_online_result *result) {
  if (!s || !result || !s->online)
    return -1;
  input_complete(s);
  // The online executor publishes each application natively; the session
  // profile governs only events left after the composed step.
  strategy::InvocationResult advanced;
  (void)apply_published(s, [&] {
    advanced = s->online->advance(s->kernel);
    return advanced.rule_result;
  });
  kernel_check_invariants(s->kernel);
  if (advanced.status == strategy::InvocationStatus::Sat) {
    prepare_external_model(s);
  }
  if (advanced.status == strategy::InvocationStatus::Unsat)
    certify_unsat(s);
  if(s->query_owner)
    s->query_owner->drain_learned(s->query_ticket, s->query_stop);
  if(s->query_owner && (advanced.status==strategy::InvocationStatus::Sat ||
                         advanced.status==strategy::InvocationStatus::Unsat)) {
    const int terminal=result_code(s,advanced.rule_result);
    if(!terminal) {
      advanced.status=strategy::InvocationStatus::NoChange;
      advanced.rule_result=ApplyResult::NoOp;
    }
  }
  online_result(advanced, *result);
  return 0;
}

uint64_t tactix_online_step_consumed(tactix_solver *s, uint64_t step_id) {
  return s && s->online ? s->online->step_consumed(step_id) : UINT64_MAX;
}

int tactix_online_complete(tactix_solver *s) {
  return s && s->online ? (s->online->complete() ? 1 : 0) : -1;
}

uint64_t tactix_stat(tactix_solver *s, int stat_id) {
  const Kernel &k = s->kernel;
  switch (stat_id) {
  case TACTIX_STAT_CONFLICTS: return k.stats.conflicts;
  case TACTIX_STAT_DECISIONS: return k.stats.decisions;
  case TACTIX_STAT_PROPAGATIONS: return k.stats.propagations;
  case TACTIX_STAT_TICKS: return k.stats.ticks;
  case TACTIX_STAT_LEARNED: return k.stats.learned;
  case TACTIX_STAT_RESTARTS: return k.stats.restarts;
  case TACTIX_STAT_REDUCTIONS: return k.stats.reductions;
  case TACTIX_STAT_SWITCHES: return k.stats.switched;
  case TACTIX_STAT_LEVEL: return k.level;
  case TACTIX_STAT_UNASSIGNED: return k.unassigned;
  case TACTIX_STAT_LAST_LEARNED_GLUE:
    // pending.glue persists after Backjump consumes the pending clause.
    return k.pending.glue;
  case TACTIX_STAT_STABLE:
    // 0 for an unguided solver with no H.
    return k.h && k.h->stable ? 1 : 0;
  case TACTIX_STAT_REPHASED: return k.stats.rephased;
  case TACTIX_STAT_SUBSUMPTIONS: return k.stats.forward_subsumptions;
  case TACTIX_STAT_IRREDUNDANT_CLAUSES: return kernel_irredundant_clauses(k);
  case TACTIX_STAT_ROOT_FIXED: return kernel_logical_root_fixed(k);
  case TACTIX_STAT_BINARY_CLAUSES: return k.binary_ids.size();
  case TACTIX_STAT_STRENGTHENED: return k.stats.strengthened;
  case TACTIX_STAT_VIVIFIED: return k.stats.vivified;
  case TACTIX_STAT_PROBE_FAILED: return k.stats.probe_failed;
  case TACTIX_STAT_BACKBONE_FAILED: return k.stats.backbone_failed;
  case TACTIX_STAT_TRANSITIVE_DELETED: return k.stats.transitive_deleted;
  case TACTIX_STAT_FACTORINGS: return k.stats.factorings;
  case TACTIX_STAT_FACTOR_TICKS: return k.stats.factor_ticks;
  case TACTIX_STAT_BVE_ELIMINATED: return k.stats.bve_eliminated;
  case TACTIX_STAT_BVE_RESOLUTIONS: return k.stats.bve_resolutions;
  // BCE / HyperTernaryRes / Instantiate / Reorder, each a yield/effort pair
  case TACTIX_STAT_BCE_DELETED: return k.stats.bce_deleted;
  case TACTIX_STAT_BCE_TICKS: return k.stats.bce_ticks;
  case TACTIX_STAT_TERNARY_RESOLVENTS: return k.stats.ternary_resolvents;
  case TACTIX_STAT_TERNARY_TICKS: return k.stats.ternary_ticks;
  case TACTIX_STAT_INSTANTIATED: return k.stats.instantiated;
  case TACTIX_STAT_REORDERED: return k.stats.reordered;
  // Each effort id is the counter that family's EFFORT_SPECS meter reads, so
  // the cost-per-unit axis uses the currency the schedule doses in.
  case TACTIX_STAT_VIVIFY_TICKS: return k.stats.vivify_ticks;
  case TACTIX_STAT_PROBE_TICKS: return k.stats.probe_ticks;
  case TACTIX_STAT_BACKBONE_TICKS: return k.stats.backbone_ticks;
  case TACTIX_STAT_TRANSITIVE_TICKS: return k.stats.transitive_ticks;
  case TACTIX_STAT_FORWARD_SUBSUMED: return k.stats.forward_subsumed;
  case TACTIX_STAT_FORWARD_VISITS: return k.stats.forward_visits;
  case TACTIX_STAT_STRENGTHEN_VISITS: return k.stats.strengthen_visits;
  case TACTIX_STAT_INSTANTIATE_TICKS: return k.stats.instantiate_ticks;
  case TACTIX_STAT_SWEEP_EQUIVALENCES: return k.stats.sweep_equivalences;
  case TACTIX_STAT_SWEEP_TICKS:
    // The public Sweep effort statistic follows the currency DefaultPolicy
    // spends.  `sweep_ticks` remains the broader scan diagnostic.
    return k.stats.sweep_ticks_sub;
  case TACTIX_STAT_CONGRUENCE_MERGES: return k.stats.congruence_merges;
  case TACTIX_STAT_CONGRUENCE_TICKS: return k.stats.congruence_ticks;
  case TACTIX_STAT_CCE_DELETED: return k.stats.cce_deleted;
  case TACTIX_STAT_CCE_TICKS: return k.stats.cce_ticks;
  case TACTIX_STAT_SUBSTITUTE_VARS: return k.stats.substitute_vars;
  case TACTIX_STAT_SUBSTITUTE_SCC_LITS: return k.stats.substitute_scc_lits;
  case TACTIX_STAT_HBR_BINARIES: return k.stats.hbr_binaries;
  case TACTIX_STAT_HBR_TICKS: return k.stats.hbr_ticks;
  case TACTIX_STAT_SUBSTITUTE_TICKS: return k.stats.substitute_ticks;
  case TACTIX_STAT_HBR_UNITS: return k.stats.hbr_units;
  case TACTIX_STAT_PARITY_CONSEQUENCE_ATTEMPTS:
    return k.stats.parity_consequence_attempts;
  case TACTIX_STAT_PARITY_CONSEQUENCE_CLAUSES:
    return k.stats.parity_consequence_clauses;
  case TACTIX_STAT_PARITY_CONSEQUENCE_EQUATIONS:
    return k.stats.parity_consequence_equations;
  case TACTIX_STAT_PARITY_CONSEQUENCE_SELECTION_WORK:
    return k.stats.parity_consequence_selection_work;
  case TACTIX_STAT_PARITY_CONSEQUENCE_PREFLIGHT_ADDITIONS:
    return k.stats.parity_consequence_preflight_additions;
  case TACTIX_STAT_PARITY_CONSEQUENCE_ADDITIONS:
    return k.stats.parity_consequence_additions;
  case TACTIX_STAT_PARITY_CONSEQUENCE_DELETIONS:
    return k.stats.parity_consequence_deletions;
  case TACTIX_STAT_PARITY_CONSEQUENCE_HELPERS:
    return k.stats.parity_consequence_helpers;
  case TACTIX_STAT_PARITY_CONSEQUENCE_PUBLISHED:
    return k.stats.parity_consequence_published;
  case TACTIX_STAT_BACKJUMPS: return k.stats.backjumps;
  case TACTIX_STAT_BVE_OTF_STRENGTHENED: return k.stats.bve_otf_strengthened;
  case TACTIX_STAT_BVE_OTF_SUBSUMED: return k.stats.bve_otf_subsumed;
  case TACTIX_STAT_FASTEL_EXTENSIONS_PROTECTED:
    return k.stats.fastel_extensions_protected;
  case TACTIX_STAT_DECISIONS_EXTENSION: return k.stats.decisions_extension;
  case TACTIX_STAT_REGION_QUERIES: return k.stats.region_queries;
  case TACTIX_STAT_REGION_UNITS: return k.stats.region_units;
  case TACTIX_STAT_REGION_LEMMAS: return k.stats.region_lemmas;
  case TACTIX_STAT_REGION_TICKS: return k.stats.region_ticks;
  case TACTIX_STAT_REGION_BUDGET: return k.stats.region_budget;
  case TACTIX_STAT_REGION_INVALIDATED: return k.stats.region_invalidated;

  default:
    return UINT64_MAX;
  }
}

uint64_t tactix_config(tactix_solver *s, int cfg_id) {
  const Kernel &k = s->kernel;
  switch (cfg_id) {
  case TACTIX_CFG_BVE_GATES: return k.bve_gate_mask;
  case TACTIX_CFG_BVE_BOUND: return k.bve_bound;
  case TACTIX_CFG_FACTOR_BOUND: return k.factor_bound;
  case TACTIX_CFG_FACTOR_COMPOSITE: return k.factor_composite ? 1 : 0;
  case TACTIX_CFG_WORK_BUDGET: return k.work_budget;
  case TACTIX_CFG_VIVIFY_SCOPE: return k.vivify_scope;
  case TACTIX_CFG_SUBSUME_CLAUSE_SIZE:
    return k.subsume_forward_parameters.clause_size_limit;
  case TACTIX_CFG_SUBSUME_LARGE_OCCURRENCES:
    return k.subsume_forward_parameters.large_occurrence_limit;
  case TACTIX_CFG_SUBSUME_BINARY_OCCURRENCES:
    return k.subsume_forward_parameters.binary_occurrence_limit;
  case TACTIX_CFG_STRENGTHEN_CLAUSE_SIZE:
    return k.strengthen_parameters.clause_size_limit;
  case TACTIX_CFG_STRENGTHEN_OCCURRENCES:
    return k.strengthen_parameters.occurrence_limit;
  case TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH:
    return k.analyze_parameters.minimize_depth_limit;
  case TACTIX_CFG_CONGRUENCE_AND_ARITY:
    return k.congruence_parameters.and_arity_limit;
  case TACTIX_CFG_CONGRUENCE_GATE_COUNT:
    return k.congruence_parameters.gate_count_limit;
  case TACTIX_CFG_CONGRUENCE_XOR_ARITY:
    return k.congruence_parameters.xor_arity_limit;
  case TACTIX_CFG_CONGRUENCE_WATCH_ORDER:
    return static_cast<uint64_t>(k.congruence_parameters.binary_watch_order);
  case TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE:
    return k.instantiate_parameters.minimum_clause_size;
  case TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT:
    return k.instantiate_parameters.occurrence_limit;
  case TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED:
    return k.instantiate_parameters.minimum_unassigned;
  case TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT:
    return k.instantiate_parameters.include_redundant ? 1 : 0;
  case TACTIX_CFG_BACKBONE_INITIAL_ROUNDS:
    return k.backbone_parameters.initial_rounds;
  case TACTIX_CFG_BACKBONE_MAXIMUM_ROUNDS:
    return k.backbone_parameters.maximum_rounds;
  case TACTIX_CFG_VIVIFY_REUSE: return k.vivify_reuse ? 1 : 0;
  case TACTIX_CFG_VIVIFY_FLUSH: return k.vivify_flush ? 1 : 0;
  case TACTIX_CFG_VIVIFY_IRREDUNDANT_SORT_RATIO:
    return k.vivify_irredundant_sort_ratio;
  case TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD:
    return k.instantiate_parameters.new_material_threshold;
  case TACTIX_CFG_GUIDANCE_PUBLICATION:
    return static_cast<uint64_t>(s->guidance_publication);
  case TACTIX_CFG_TOUCH_FILTER: return k.touch_filter ? 1 : 0;
  case TACTIX_CFG_LIKELY_KEPT_REDUNDANT: return k.touch_redundant ? 1 : 0;
  default:
    return UINT64_MAX;
  }
}

int tactix_set_guidance_publication(tactix_solver *s, int mode) {
  if (!s || (mode != TACTIX_GUIDANCE_DISCARD && mode != TACTIX_GUIDANCE_NATIVE))
    return -1;
  s->guidance_publication = mode;
  return 0;
}

int tactix_install_parameters(tactix_solver *s, int rule_id,
                              const tactix_online_parameters *parameters) {
  const auto &rules = tactix_rules();
  if (!s || !parameters || rule_id < 0 ||
      rule_id >= static_cast<int>(rules.size()) ||
      rules[static_cast<size_t>(rule_id)].retired)
    return -1;
  strategy::InprocessParameters converted;
  if (!online_parameters(*parameters, converted) ||
      !strategy::validate_parameter_installation(
          rules[static_cast<size_t>(rule_id)].name, converted))
    return -1;
  return strategy::install_permanent_parameters(s->kernel, converted) ? 0 : -1;
}

int tactix_install_affine_matrix_profile(
    tactix_solver *s, const tactix_online_affine_matrix_initialize_parameters *p) {
  strategy::AffineMatrixInitializeParameters profile;
  if (!s || !p || !online_affine_matrix(*p, profile))
    return -1;
  const strategy::InprocessParameters converted = profile;
  if (!strategy::validate_parameter_installation("AffineMatrixInitialize", converted))
    return -1;
  return strategy::install_permanent_parameters(s->kernel, converted) ? 0 : -1;
}

int tactix_read_affine_matrix_result(tactix_solver *s, tactix_affine_matrix_result *out) {
  if (!s || !out) return -1;
  *out = {};
  const AffineMatrixSearchSession &session =
      s->structural_action_sessions.affine_matrix_search;
  if (const auto &init = session.last_initialization()) {
    out->initialization_status = static_cast<int>(init->status) + 1;
    out->initialization_result = static_cast<int>(init->result);
    out->unchanged = init->unchanged;
    out->equations = init->equations;
    out->rows = init->rows;
    out->admitted_sources = init->admitted_sources;
  }
  if (const auto &service = session.last_service()) {
    out->service_status = static_cast<int>(service->status) + 1;
    out->service_result = static_cast<int>(service->result);
    out->bcp_pending = service->bcp_pending;
    out->reason_deferred = service->reason_deferred;
    out->reason_materialized = service->reason_materialized;
    out->reason_detached = service->live.detached_reason;
  }
  out->enabled = session.enabled();
  out->persistent = session.persistent();
  out->basis_rank = session.enabled() ? session.basis_rank() : 0;
  return 0;
}

int tactix_install_material_settings(tactix_solver *s, int touch_filter_mode,
                                     int likely_kept_redundant_mode) {
  strategy::SettingOverride touch, kept;
  if (!s || !online_setting_override(touch_filter_mode, touch) ||
      !online_setting_override(likely_kept_redundant_mode, kept))
    return -1;
  const auto selected = [](strategy::SettingOverride value) -> std::optional<bool> {
    if (value == strategy::SettingOverride::Inherit) return std::nullopt;
    return value == strategy::SettingOverride::Enable;
  };
  execution::execute_action(s->kernel, execution::MaterialSettingsRequest{
                                           selected(touch), selected(kept)});
  return 0;
}

namespace {

// Native support records carry these public arguments as their instance.
static_assert(execution::execution_context_argument(execution::ExecutionContext::Startup) ==
              TACTIX_CONTEXT_STARTUP);
static_assert(execution::execution_context_argument(execution::ExecutionContext::Inprocessing) ==
              TACTIX_CONTEXT_INPROCESSING);
static_assert(execution::execution_context_argument(execution::ExecutionContext::Speculation) ==
              TACTIX_CONTEXT_SPECULATION);
static_assert(execution::root_preparation_argument({true, false}, false) ==
                  TACTIX_ROOT_PREPARATION_PROBING &&
              execution::root_preparation_argument({false, false}, true) ==
                  TACTIX_ROOT_PREPARATION_CAPTURE_TARGET &&
              execution::root_preparation_argument({false, true}, false) ==
                  TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX);
static_assert(execution::PHASE_ELIMINATE == TACTIX_PHASE_ELIMINATE &&
              execution::inprocess_phase_argument(false, 1) ==
                  uint64_t{1} << TACTIX_PHASE_FACTOR_BOUND_SHIFT);
static_assert(execution::COMPACT_FOLD_FIXED == TACTIX_COMPACT_FOLD_FIXED &&
              execution::COMPACT_PUBLISH_RANDOM_ORDER ==
                  TACTIX_COMPACT_PUBLISH_RANDOM_ORDER);
static_assert(static_cast<int>(execution::WarmupPhase::Heuristic) ==
                  TACTIX_WARMUP_PHASE_HEURISTIC &&
              static_cast<int>(execution::WarmupPhase::Positive) ==
                  TACTIX_WARMUP_PHASE_POSITIVE &&
              static_cast<int>(execution::WarmupPhase::Negative) ==
                  TACTIX_WARMUP_PHASE_NEGATIVE);

// A quiescent root: no pending conflict, analysis, occurrence session or
// probing transaction.
bool support_root(const Kernel &k) {
  return !k.inconsistent && !k.level && !k.conflict_valid && !k.pending.valid &&
         !k.dense && !k.probing.recording;
}

// Roots a support derives reach H as a registry dispatch's would under the
// session profile.
void publish_support_roots(tactix_solver *s, size_t begin) {
  if (s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
    HeuristicRulePublisher{}.roots(s->kernel, begin);
}

// Propagates at the root and publishes any roots it derived.
static ApplyResult propagate_publishing_roots(tactix_solver *s) {
  Kernel &k = s->kernel;
  const size_t begin = k.trail.size();
  const uint64_t roots = k.stats.root_assignments;
  const ApplyResult propagated = propagate_apply(k);
  if (k.stats.root_assignments != roots) publish_support_roots(s, begin);
  return propagated;
}

// Leave the open inprocessing phase bracket (DefaultPolicy's phase exit).
int leave_phase(tactix_solver *s) {
  Kernel &k = s->kernel;
  const ApplyResult left = s->phase.exit(
      k, k.inconsistent,
      [&] {
        if (s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
          HeuristicRulePublisher{}.retirements(k);
      },
      [&] { return propagate_publishing_roots(s); });
  if (left == ApplyResult::Unsat) {
    certify_unsat(s);
    return 20;
  }
  return 0;
}

int support_result(tactix_solver *s, ApplyResult result) {
  if (result == ApplyResult::Unsat) {
    certify_unsat(s);
    return 20;
  }
  return result == ApplyResult::NoOp ? 1 : 0;
}

} // namespace

int tactix_support(tactix_solver *s, int support, uint64_t argument) {
  if (!s)
    return -1;
  input_complete(s);
  Kernel &k = s->kernel;
  // Support operations are compound state transitions outside the rule
  // registry.  None may cross an outstanding affine reason debt; the sole
  // legal discharge is the public MaterializeReasons rule operation.
  if (k.affine_matrix_search_session &&
      k.affine_matrix_search_session->has_deferred_reason())
    return -1;
  const auto flags = [&](uint64_t allowed) { return !(argument & ~allowed); };
  int result = -1;
  switch (support) {
  case TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE:
    if (argument)
      return -1;
    execution::execute_action(k, execution::CloseStartupCertificateRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_PREPARATION_RESTART:
    if (argument > UINT32_MAX || !restart_guard(k))
      return -1;
    result = support_result(s, execution::execute_action(
        k, execution::RestartRequest{static_cast<uint32_t>(argument),
                                     execution::RestartPurpose::Preparation}));
    break;
  case TACTIX_SUPPORT_ROOT_PREPARATION: {
    if (!flags(TACTIX_ROOT_PREPARATION_PROBING |
               TACTIX_ROOT_PREPARATION_CAPTURE_TARGET |
               TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) ||
        k.inconsistent || k.pending.valid || k.dense || k.probing.recording)
      return -1;
    const ApplyResult prepared = execution::execute_action(
        k, execution::RootPreparationRequest{
               (argument & TACTIX_ROOT_PREPARATION_PROBING) != 0,
               (argument & TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX) != 0},
        [&] {
          if (argument & TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)
            heuristics_update_target_phases(k);
        },
        [&](const execution::RestartRequest &request) {
          return execution::execute_action(k, request);
        },
        [&] { return propagate_publishing_roots(s); });
    result = support_result(s, prepared);
    break;
  }
  case TACTIX_SUPPORT_SPARSE_COLLECT: {
    if (argument || !support_root(k))
      return -1;
    // Derived units stay on the propagation queue for the caller's Propagate,
    // which publishes the roots it assigns.
    const RootCollectOutcome collected =
        execution::execute_action(k, execution::SparseCollectRequest{});
    result = collected == RootCollectOutcome::Refuted ? support_result(s, ApplyResult::Unsat)
             : collected == RootCollectOutcome::Unit  ? 2
             : collected == RootCollectOutcome::NoChange ? 1 : 0;
    break;
  }
  case TACTIX_SUPPORT_COMPACT_RANDOM_ORDER:
    if (argument || !support_root(k) || !k.h)
      return -1;
    execution::execute_action(k, execution::CompactRandomOrderRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_COMPACT_VARIABLES: {
    // Any consistent state, as Reduce compacts above the root; the kernel
    // renumbers the trail with the variables.
    if (!flags(TACTIX_COMPACT_FOLD_FIXED | TACTIX_COMPACT_PUBLISH_RANDOM_ORDER) ||
        k.inconsistent || k.conflict_valid || k.pending.valid || k.dense ||
        k.probing.recording)
      return -1;
    const bool compacted = execution::execute_action(
        k, execution::CompactVariablesRequest{(argument & TACTIX_COMPACT_FOLD_FIXED) != 0});
    if (compacted && (argument & TACTIX_COMPACT_PUBLISH_RANDOM_ORDER))
      heuristics_compact_random_order_fixed(k);
    result = compacted ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_REDUCTION_PREPARATION:
    if (argument || !restart_guard(k) || k.pending.valid || k.dense ||
        k.probing.recording)
      return -1;
    execution::execute_action(k, execution::ReductionPreparationRequest{},
        [&](size_t begin) { publish_support_roots(s, begin); });
    result = support_result(s, k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok);
    break;
  case TACTIX_SUPPORT_COLLECT_GARBAGE:
    // kernel_collect_garbage remaps an open occurrence session, so unlike the
    // other root supports it does not require !k.dense.
    if (!flags(TACTIX_COLLECT_SPARSE | TACTIX_COLLECT_PRESERVE_EAGER) ||
        k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
        k.probing.recording || k.bve_occurrence_count_image)
      return -1;
    execution::execute_action(k, execution::CollectGarbageRequest{
        0, (argument & TACTIX_COLLECT_SPARSE) != 0,
        (argument & TACTIX_COLLECT_PRESERVE_EAGER) != 0});
    result = 0;
    break;
  case TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT:
    if (argument || !support_root(k))
      return -1;
    execution::execute_action(k, execution::FlushRetiredRedundantRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION:
    if (argument)
      return -1;
    execution::execute_action(k, execution::BeginBveDefinitionSessionRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_WARMUP: {
    if (argument > TACTIX_WARMUP_PHASE_NEGATIVE || !support_root(k) || !k.h ||
        !kernel_propagation_fixpoint(k))
      return -1;
    const auto phase = static_cast<execution::WarmupPhase>(argument);
    const auto warmed = execution::execute_action(k, execution::WarmupRequest{},
        [&] { return execution::warmup_decision(k, phase); }, [](Lit) {},
        [](size_t) {});
    result = warmed.interrupted ? 1 : 0;
    break;
  }
  case TACTIX_SUPPORT_RESEED_GUIDANCE:
    if (argument || !support_root(k) || k.h != &s->heuristics)
      return -1;
    // The session's H is replaced by the one a native policy attaches after
    // preparing the residual seed (DefaultPolicy's residual handoff).
    execution::execute_action(k, execution::HeuristicDetachmentRequest{s->heuristics});
    (void)execution::execute_action(k, execution::ResidualActivationRequest{});
    {
      const std::vector<Var> order = k.input_activation_order;
      s->heuristics = Heuristics{};
      execution::execute_action(k, execution::HeuristicAttachmentRequest{s->heuristics});
      execution::execute_action(k, execution::ResidualImportRequest{order});
    }
    result = 0;
    break;
  case TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ResetGraphLabelingSessionRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_RELEASE_INPUT_ORDER:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ReleaseInputBinaryOrderRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_CONSUME_ROOT_PREFIX:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::ConsumeRootPrefixRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_ENTER_CONTEXT: {
    if (argument < TACTIX_CONTEXT_STARTUP || argument > TACTIX_CONTEXT_SPECULATION)
      return -1;
    const auto context = argument == TACTIX_CONTEXT_STARTUP
                             ? execution::ExecutionContext::Startup
                         : argument == TACTIX_CONTEXT_INPROCESSING
                             ? execution::ExecutionContext::Inprocessing
                             : execution::ExecutionContext::Speculation;
    s->contexts.emplace_back(context, execution::enter_execution_context(k, context));
    return 0; // a context changes no solver state a client observes
  }
  case TACTIX_SUPPORT_BEGIN_FORWARD_ROUND:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::BeginForwardRoundRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_COMPACT_FASTEL_PREFIX: {
    const Lit lit = kernel_internalize(
        k, static_cast<int32_t>(static_cast<uint32_t>(argument)));
    if (lit == INVALID_LIT || !k.dense || !k.dense->track_order)
      return -1;
    result = execution::execute_action(
                 k, execution::CompactFastelPrefixRequest{lit, static_cast<size_t>(argument >> 32)})
                 ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_HOLD_RETIREMENTS:
    // HOLD defers retirement publication; RELEASE ends the hold and publishes
    // the queued events under the session profile; RELEASE_ONLY leaves them
    // for the next publication. Guidance only: no state a rule reads.
    if (argument > TACTIX_RETIREMENTS_RELEASE_ONLY)
      return -1;
    k.retirement_publication_held = argument == TACTIX_RETIREMENTS_HOLD;
    if (argument == TACTIX_RETIREMENTS_RELEASE &&
        s->guidance_publication == TACTIX_GUIDANCE_NATIVE)
      HeuristicRulePublisher{}.retirements(k);
    return 0;
  case TACTIX_SUPPORT_RESCHEDULE_BVE:
    if (argument)
      return -1;
    result = execution::execute_action(k, execution::RescheduleBveCandidatesRequest{}) ? 0 : 1;
    break;
  case TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE: {
    if (!argument || argument > static_cast<uint64_t>(INT32_MAX))
      return -1;
    const Lit lit = kernel_internalize(k, static_cast<int>(argument));
    if (lit == INVALID_LIT)
      return -1;
    result = execution::execute_action(
                 k, execution::RetireBveCandidateRequest{var_of(lit)})
                 ? 0 : 1;
    break;
  }
  case TACTIX_SUPPORT_FORWARD_CLOSURE:
    if (argument > k.trail.size() || k.level || k.conflict_valid ||
        k.pending.valid || k.probing.recording || !k.dense ||
        !k.dense->track_order || k.large_watches_connected)
      return -1;
    result = support_result(s, execution::execute_action(
        k, execution::ForwardClosureRequest{static_cast<uint32_t>(argument)}));
    break;
  case TACTIX_SUPPORT_BVE_FLUSH_UNITS:
    if (!k.dense || !k.dense->track_order || argument > k.propagate_head)
      return -1;
    bve_flush_units_while_connected(k, static_cast<uint32_t>(argument));
    return 0;
  case TACTIX_SUPPORT_AFFINE_CENSUS:
    if (argument > 1)
      return -1;
    if (!argument)
      s->affine_census.reset();
    else if (k.affine_capture_epochs_trusted)
      s->affine_census = api_detail::affine_census_stamp(s);
    return 0;
  case TACTIX_SUPPORT_RUN_REGIONS: {
    if (argument)
      return -1;
    if (!s->regions)
      return 1;
    result = support_result(s, s->regions->run());
    break;
  }
  case TACTIX_SUPPORT_STARTUP_RETENTION:
    // DefaultPolicy's initial selection of substitution/elimination choices;
    // explicit applications keep their complete action surface.
    if (argument > 2 || (argument == 1) == s->startup_retention)
      return -1;
    if (argument == 1) {
      s->retention_before = k.substitute_retention;
      k.substitute_retention = k.variable_retention.empty() ? nullptr : &k.variable_retention;
    } else {
      k.substitute_retention = s->retention_before;
      if (argument == 0)
        k.variable_retention.clear();
    }
    s->startup_retention = argument == 1;
    return 0;
  case TACTIX_SUPPORT_MATERIALIZE_COLD_STATE:
    if (argument)
      return -1;
    if (!k.cold_state_deferred)
      return 1;
    execution::execute_action(k, execution::MaterializeColdStateRequest{});
    result = 0;
    break;
  case TACTIX_SUPPORT_ATTACH_HEURISTICS:
    if (argument)
      return -1;
    if (k.h)
      return 1;
    execution::execute_action(k, execution::HeuristicAttachmentRequest{s->heuristics});
    result = 0;
    break;
  case TACTIX_SUPPORT_PUBLISH_ROOTS:
    if (!k.h || argument > k.trail.size())
      return -1;
    heuristics_consume_root_assignments(k, static_cast<size_t>(argument));
    return 0;
  case TACTIX_SUPPORT_CAPTURE_TARGET_PHASES:
    // Guidance only: the stable-only capture a scheduled restart below the
    // current level performs before backtracking (DefaultPolicy's restart).
    if (argument || !k.h)
      return -1;
    heuristics_update_target_phases(k);
    return 0;
  case TACTIX_SUPPORT_LEAVE_CONTEXT:
    if (argument || s->contexts.empty() ||
        (s->phase.open() && s->contexts.size() <= s->phase_contexts))
      return -1;
    execution::leave_execution_context(k, s->contexts.back().first,
                                       s->contexts.back().second);
    s->contexts.pop_back();
    return 0;
  case TACTIX_SUPPORT_ENTER_INPROCESS_PHASE: {
    const bool eliminate = (argument & TACTIX_PHASE_ELIMINATE) != 0;
    if ((argument & ~(TACTIX_PHASE_ELIMINATE | (uint64_t{UINT32_MAX} << 32))) ||
        s->phase.open() || !support_root(k) || k.factor_selection ||
        (eliminate && !k.large_watches_connected))
      return -1;
    s->phase_contexts = s->contexts.size();
    s->phase.enter(k, execution::InprocessPhaseEntryRequest{
                          eliminate, s->phase_factor_selection,
                          static_cast<uint32_t>(argument >> 32)});
    result = 0;
    break;
  }
  case TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION:
    if (argument || !s->phase.open() || s->phase.occurrences_open() || k.inconsistent ||
        k.level || k.conflict_valid || k.pending.valid || k.dense || k.probing.recording)
      return -1;
    s->phase.open_occurrences(k, s->phase_occurrences);
    result = 0;
    break;
  case TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX:
    // Drop the affine search session's algebraic and proof-planning state;
    // published clauses stay. DETACHED clears without releasing leases.
    if (argument > TACTIX_AFFINE_MATRIX_RELEASE_DETACHED ||
        !k.affine_matrix_search_session ||
        k.affine_matrix_search_session->has_deferred_reason())
      return -1;
    if (argument) k.affine_matrix_search_session->disable();
    else k.affine_matrix_search_session->disable(k);
    result = 0;
    break;
  case TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE:
    if (argument || !s->phase.open() || s->contexts.size() != s->phase_contexts ||
        k.level || k.conflict_valid || k.pending.valid || k.probing.recording)
      return -1;
    if (!s->phase.occurrences_open()) {
      // Without a session only a Probe bracket may close (a structured
      // startup terminal); an Eliminate bracket must reconnect its watches.
      if (s->phase.eliminate())
        return -1;
      s->phase.exit_terminal(k);
      result = 0;
      break;
    }
    if (k.dense != nullptr && k.dense != &s->phase_occurrences)
      return -1;
    result = leave_phase(s);
    break;
  default:
    return -1;
  }
  if (result == 0 || result == 2 || result == 20)
    external_state_change(s);
  kernel_check_invariants(k); // no-op in NDEBUG
  return result;
}

int tactix_decide_candidates(tactix_solver *s, int *lits, int cap) {
  if (cap <= 0)
    return 0;
  input_complete(s);
  std::vector<Lit> buf(static_cast<size_t>(cap));
  const uint32_t n =
      heuristics_decide_candidates(s->kernel, buf.data(),
                                   static_cast<uint32_t>(cap));
  for (uint32_t i = 0; i < n; i++)
    lits[i] = kernel_externalize(s->kernel, buf[i]);
  return static_cast<int>(n);
}

int tactix_decide(tactix_solver *s, int elit) {
  input_complete(s);
  if (!decide_guard(s->kernel))
    return -1;
  const Lit l = kernel_internalize(s->kernel, elit);
  if (l == INVALID_LIT)
    return -1;
  // This legacy convenience call bypasses registry dispatch, but consumes the
  // same original-input action boundary as the typed Decide rule.
  s->kernel.close_startup_certificate();
  if (s->kernel.proof && !s->kernel.proof->commit_clause_stream())
    return -1;
  const int ok = decide_apply(s->kernel, l, true) == ApplyResult::Ok ? 0 : -1;
  // NATIVE publication records the decision's target provenance (H), as a
  // DefaultPolicy decision does; whether a target was consumed follows the
  // mode, as there.
  if (!ok && s->guidance_publication == TACTIX_GUIDANCE_NATIVE && s->kernel.h)
    heuristics_note_target_decision(s->kernel, l, s->kernel.h->stable);
  if (!ok)
    external_state_change(s);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return ok;
}

int tactix_status(tactix_solver *s) {
  // Also a verdict-reporting path: a caller that adds a contradictory input and
  // then only asks the status must still get the refutation written.
  input_complete(s);
  if(s->query_owner) {
    if(s->query_owner->poll_terminate(s->query_stop)) {s->query_status=session::QuerySolveStatus::Interrupted;return 0;}
    const auto next=s->query_owner->next_assumption(s->query_ticket);
    if(next.status==session::AssumptionStatus::Restore) {s->query_status=session::QuerySolveStatus::RestorationRequired;return 0;}
    if(next.status==session::AssumptionStatus::BaseUnsat || next.status==session::AssumptionStatus::QueryUnsat) {
      if(next.status==session::AssumptionStatus::QueryUnsat &&
          !s->query_owner->publish_assumption_refutation(s->query_ticket)) {
        s->query_status=session::QuerySolveStatus::ProofFailure;return 0;
      }
      certify_unsat(s);
      return s->query_status==session::QuerySolveStatus::Complete?20:0;
    }
    if(next.status==session::AssumptionStatus::Ready && has_sat_assignment(s->kernel)) {
      const auto model=s->query_owner->check_model(s->query_ticket);
      if(model.status==session::ModelStatus::Valid)return 10;
      if(model.status!=session::ModelStatus::FalsifiedAssumption)
        s->query_status=model.status==session::ModelStatus::InvalidQuery?
            session::QuerySolveStatus::RestorationRequired:session::QuerySolveStatus::InvalidModel;
    }
    return 0;
  }
  if (s->kernel.inconsistent) {
    certify_unsat(s);
    return 20;
  }
  // SAT requires every active variable assigned at a propagation fixpoint,
  // with no conflict in flight. Retired, unassigned identities occur in no
  // live clause and kernel_extend_model supplies their external values. A
  // recorded conflict (conflict_valid) or an analyzed but not yet backjumped
  // one (pending.valid) still prevents a verdict.
  if (has_sat_assignment(s->kernel)) {
    prepare_external_model(s);
    return 10;
  }
  return 0;
}

} // extern "C"

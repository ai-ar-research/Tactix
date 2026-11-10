// Native-strategy observation and branching (tactix.h): counters, state
// observations, rule/instance admission, H's decision structures and profile
// readback. Reads only, except tactix_branch_next's lazy cursor (H). A
// translation unit of its own so api.cpp's code generation is unchanged.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "api/api_internal.hpp"
#include "api/parameter_encoding.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/default_strategy_config.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/bve/bve.hpp"
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/ramsey_startup.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/registry.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "strategy/bve_schedule.hpp"
#include "strategy/online_sequence.hpp"
#include "strategy/inprocess_program.hpp"
#include "strategy/online_inprocess.hpp"

#include <csignal>
#include <cstddef>
#include <type_traits>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

using namespace tactix;

// tactix.h names the instance payloads the rules define.
static_assert(TACTIX_REPHASE_BEST == REPHASE_BEST && TACTIX_REPHASE_INVERTED == REPHASE_INVERTED &&
              TACTIX_REPHASE_ORIGINAL == REPHASE_ORIGINAL);
static_assert(TACTIX_BVE_GATES_DEFAULT == BVE_GATE_KISSAT_DEFAULT);
static_assert(TACTIX_ONLINE_BUDGET_EXHAUSTED == static_cast<int>(strategy::InvocationStatus::BudgetExhausted) &&
              TACTIX_ONLINE_SAT == static_cast<int>(strategy::InvocationStatus::Sat) &&
              TACTIX_ONLINE_ACCOUNTING_ERROR == static_cast<int>(strategy::InvocationStatus::AccountingError));
static_assert(TACTIX_STAGE_EXHAUSTION_STOP == static_cast<int>(strategy::BudgetExhaustionPolicy::Stop) &&
              TACTIX_STAGE_EXHAUSTION_CONTINUE == static_cast<int>(strategy::BudgetExhaustionPolicy::Continue) &&
              TACTIX_STAGE_EXHAUSTION_FINISH_ROUND ==
                  static_cast<int>(strategy::BudgetExhaustionPolicy::FinishRoundThenStop));
static_assert(TACTIX_RULE_RESULT_OK == static_cast<int>(ApplyResult::Ok) &&
              TACTIX_RULE_RESULT_NOOP == static_cast<int>(ApplyResult::NoOp) &&
              TACTIX_RULE_RESULT_CONFLICT == static_cast<int>(ApplyResult::Conflict) &&
              TACTIX_RULE_RESULT_SAT == static_cast<int>(ApplyResult::Sat) &&
              TACTIX_RULE_RESULT_UNSAT == static_cast<int>(ApplyResult::Unsat));
static_assert(static_cast<int>(GraphLabelingSliceStatus::Sat) + 1 == 4 &&
              static_cast<int>(GraphLabelingSliceStatus::Interrupted) + 1 == 5);
static_assert(TACTIX_AFFINE_INITIALIZATION_APPLIED_PARTIAL ==
                  static_cast<int>(AffineMatrixInitStatus::AppliedPartial) + 1 &&
              TACTIX_AFFINE_INITIALIZATION_NO_STRUCTURE ==
                  static_cast<int>(AffineMatrixInitStatus::NoStructure) + 1 &&
              TACTIX_AFFINE_SERVICE_INAPPLICABLE ==
                  static_cast<int>(AffineMatrixServiceStatus::Inapplicable) + 1 &&
              TACTIX_AFFINE_SERVICE_INTERRUPTED ==
                  static_cast<int>(AffineMatrixServiceStatus::Interrupted) + 1 &&
              TACTIX_AFFINE_SERVICE_PENDING == static_cast<int>(AffineMatrixServiceStatus::Pending) + 1);
static_assert(TACTIX_AFFINE_INITIALIZATION_APPLIED ==
                  static_cast<int>(AffineMatrixInitStatus::Applied) + 1 &&
              TACTIX_AFFINE_SERVICE_APPLIED == static_cast<int>(AffineMatrixServiceStatus::Applied) + 1);
static_assert(TACTIX_FOLLOWUP_AFTER_CONGRUENCE == static_cast<int>(strategy::Followup::AfterCongruence) &&
              TACTIX_FOLLOWUP_AFTER_SWEEP_OR_FINAL ==
                  static_cast<int>(strategy::Followup::AfterSweepOrFinal));
static_assert(TACTIX_CANDIDATE_ORDER_RANDOM == static_cast<int>(strategy::CandidateOrder::Random) &&
              TACTIX_CANDIDATE_ORDER_NEGATIVE_OCCURRENCES_DESCENDING ==
                  static_cast<int>(strategy::CandidateOrder::NegOccDesc) &&
              TACTIX_CANDIDATE_ORDER_HEURISTIC == static_cast<int>(strategy::CandidateOrder::Heuristic));
static_assert(TACTIX_REORDER_QUEUE == REORDER_QUEUE && TACTIX_REORDER_HEAP == REORDER_HEAP);
static_assert(TACTIX_LUCKY_STARTUP_PARITY == LUCKY_STARTUP_PARITY &&
              TACTIX_LUCKY_STARTUP_COUNT == LUCKY_STARTUP_COUNT &&
              TACTIX_LUCKY_GRAPH_LABELING == LUCKY_GRAPH_LABELING &&
              TACTIX_LUCKY_STARTUP_RAMSEY == LUCKY_STARTUP_RAMSEY &&
              TACTIX_LUCKY_PARITY_PROOF == LUCKY_PARITY_PROOF &&
              TACTIX_LUCKY_EXACT_ONE_PROOF == LUCKY_EXACT_ONE_PROOF &&
              TACTIX_LUCKY_GRAPH_AUTHENTICATE == LUCKY_GRAPH_AUTHENTICATE &&
              TACTIX_LUCKY_GRAPH_CONTINUE == LUCKY_GRAPH_CONTINUE);

namespace {

// One row per Stats field (kernel/stats.inc): flattened 64- or 32-bit storage.
struct CounterRow {
  const char *name;
  size_t offset;
  uint32_t length;
  bool wide;
};

// Stats has default member initializers but no virtual or private members,
// so it is standard-layout and offsetof is defined.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
constexpr CounterRow COUNTERS[] = {
#define TACTIX_STATS_U64(name) {#name, offsetof(Stats, name), 1, true},
#define TACTIX_STATS_U32(name) {#name, offsetof(Stats, name), 1, false},
#define TACTIX_STATS_MUTABLE(name) {#name, offsetof(Stats, name), 1, true},
#define TACTIX_STATS_INIT(name, value) {#name, offsetof(Stats, name), 1, true},
#define TACTIX_STATS_ARRAY(name, dims)                                         \
  {#name, offsetof(Stats, name),                                               \
   static_cast<uint32_t>(sizeof(Stats::name) / sizeof(uint64_t)), true},
#define TACTIX_STATS_TIMER(name) {#name, offsetof(Stats, name), 1, true},
#define TACTIX_STATS_TIMER_ARRAY(name, dims)                                   \
  {#name, offsetof(Stats, name),                                               \
   static_cast<uint32_t>(sizeof(Stats::name) / sizeof(uint64_t)), true},
#include "kernel/stats.inc"
#undef TACTIX_STATS_U64
#undef TACTIX_STATS_U32
#undef TACTIX_STATS_MUTABLE
#undef TACTIX_STATS_INIT
#undef TACTIX_STATS_ARRAY
#undef TACTIX_STATS_TIMER
#undef TACTIX_STATS_TIMER_ARRAY
};
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
constexpr int COUNTER_COUNT = static_cast<int>(sizeof COUNTERS / sizeof COUNTERS[0]);

const CounterRow *counter_row(int id) {
  return id >= 0 && id < COUNTER_COUNT ? &COUNTERS[id] : nullptr;
}

Kernel &kernel_of(tactix_c_handle *s) { return api_detail::kernel_for_fixed_testing(s); }

// The internal variable of a positive external variable, or INVALID_VAR.
Var internal_variable(const Kernel &k, int variable) {
  if (variable <= 0) return INVALID_VAR;
  const Lit lit = kernel_internalize(k, variable);
  return lit == INVALID_LIT ? INVALID_VAR : var_of(lit);
}

int external_variable(const Kernel &k, Var v) {
  return kernel_externalize(k, make_lit(v, false));
}

// The admission predicate behind each Lucky instance (lucky.cpp
// lucky_apply_instance's guarded runs, and the guards DefaultPolicy tests
// before its startup Ramsey/count opportunities).
bool lucky_instance_admissible(const Kernel &k, uint64_t instance) {
  switch (instance) {
  case INSTANCE_CANONICAL: return true;
  case TACTIX_LUCKY_PARITY_MODEL: case LUCKY_PARITY_PROOF: return parity_model_guard(k);
  case TACTIX_LUCKY_ONEHOT_MODEL: case TACTIX_LUCKY_DIRECT_CSP_MODEL:
  case LUCKY_EXACT_ONE_PROOF: return onehot_model_guard(k);
  case TACTIX_LUCKY_AFFINE_MODEL: return affine_model_guard(k);
  case TACTIX_LUCKY_BALANCED_SIGNATURE_MODEL: return balanced_signature_model_guard(k);
  case TACTIX_LUCKY_FERMAT_MODEL: return fermat_model_guard(k);
  case TACTIX_LUCKY_CERNY_MODEL: return cerny_model_guard(k);
  case TACTIX_LUCKY_LOOKUP_CIRCUIT_MODEL: return lookup_circuit_model_guard(k);
  case TACTIX_LUCKY_SIMON_MODEL: return simon_model_guard(k);
  case TACTIX_LUCKY_STCONN_PROOF: return stconn_proof_guard(k);
  case TACTIX_LUCKY_SIGNED_CARDINALITY_PROOF: return signed_cardinality_proof_guard(k);
  case TACTIX_LUCKY_EXACT_COVER_MODEL: return exact_cover_model_guard(k);
  case TACTIX_LUCKY_INDEPENDENT_SET_MODEL: return independent_set_model_guard(k);
  case LUCKY_STARTUP_PARITY: return startup_parity_guard(k);
  case LUCKY_STARTUP_COUNT: return count_preservation_guard(k);
  case LUCKY_STARTUP_RAMSEY: return ramsey_startup_guard(k);
  case LUCKY_GRAPH_LABELING:
  case LUCKY_GRAPH_AUTHENTICATE:
  case LUCKY_GRAPH_CONTINUE: return graph_labeling_model_guard(k);
  default: return !lucky_retired_constructor_instance(instance);
  }
}

} // namespace

int tactix_counter_id(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < COUNTER_COUNT; ++i)
    if (!std::strcmp(COUNTERS[i].name, name)) return i;
  return -1;
}

uint32_t tactix_counter_length(int counter_id) {
  const CounterRow *row = counter_row(counter_id);
  return row ? row->length : 0;
}

uint64_t tactix_counter(tactix_c_handle *s, int counter_id, uint32_t index) {
  const CounterRow *row = counter_row(counter_id);
  if (!s || !row || index >= row->length) return 0;
  const auto *base =
      reinterpret_cast<const unsigned char *>(&kernel_of(s).stats) + row->offset;
  if (row->wide) return reinterpret_cast<const uint64_t *>(base)[index];
  return reinterpret_cast<const uint32_t *>(base)[index];
}

const uint64_t *tactix_counter_view(tactix_c_handle *s, int counter_id) {
  const CounterRow *row = counter_row(counter_id);
  if (!s || !row || !row->wide) return nullptr;
  const auto *base =
      reinterpret_cast<const unsigned char *>(&kernel_of(s).stats) + row->offset;
  return reinterpret_cast<const uint64_t *>(base);
}

static_assert(std::is_same_v<sig_atomic_t, int>, "tactix_set_interrupt takes a sig_atomic_t flag");

int tactix_set_interrupt(tactix_c_handle *s, volatile int *flag) {
  if (!s) return -1;
  kernel_of(s).stop = flag;
  return 0;
}

uint64_t tactix_observe(tactix_c_handle *s, int observation, uint64_t argument) {
  if (!s) return UINT64_MAX;
  api_detail::complete_input(s);
  const Kernel &k = kernel_of(s);
  const bool per_variable = observation == TACTIX_OBSERVE_VARIABLE_OPEN ||
                            observation == TACTIX_OBSERVE_VARIABLE_RETIRED ||
                            observation == TACTIX_OBSERVE_EXTENSION_VARIABLE ||
                            observation == TACTIX_OBSERVE_STARTUP_RETAINED ||
                            observation == TACTIX_OBSERVE_BVE_RETRY ||
                            observation == TACTIX_OBSERVE_COMPACT_POSITION ||
                            observation == TACTIX_OBSERVE_IMPORT_POSITION ||
                            observation == TACTIX_OBSERVE_AFFINE_SUPPORT;
  if (argument && !per_variable && observation != TACTIX_OBSERVE_COMPACTION_DUE &&
      observation != TACTIX_OBSERVE_REDUCE_CANDIDATES)
    return UINT64_MAX;
  Var variable = INVALID_VAR;
  if (per_variable) {
    if (!argument || argument > INT32_MAX) return UINT64_MAX;
    variable = internal_variable(k, static_cast<int>(argument));
    if (variable == INVALID_VAR || variable >= k.nvars) return UINT64_MAX;
  }
  switch (observation) {
  case TACTIX_OBSERVE_INCONSISTENT: return k.inconsistent;
  case TACTIX_OBSERVE_CONFLICT: return k.conflict_valid;
  case TACTIX_OBSERVE_UNASSIGNED_ACTIVE: return k.unassigned_active;
  case TACTIX_OBSERVE_LIVE_BINARY: return k.watches.binary_clauses();
  case TACTIX_OBSERVE_LIVE_LARGE_IRREDUNDANT: return k.live_irredundant_large;
  case TACTIX_OBSERVE_COMPLEMENTARY_TERNARY_PERMILLE:
    return walk_complementary_ternary_fraction_permille(k);
  case TACTIX_OBSERVE_EXTERNAL_VARIABLES: return k.external_to_internal.size();
  case TACTIX_OBSERVE_PROOF: return k.proof != nullptr;
  case TACTIX_OBSERVE_STRUCTURAL_PROVENANCE:
    return static_cast<bool>(k.structural_provenance);
  case TACTIX_OBSERVE_AFFINE_SESSION: {
    const AffineMatrixSearchSession *session = k.affine_matrix_search_session;
    if (!session) return 0;
    return 1u | (session->enabled() ? 2u : 0u) | (session->persistent() ? 4u : 0u) |
           (session->has_deferred_reason() ? 8u : 0u) |
           (k.affine_matrix_initialize_parameters_active ? 16u : 0u);
  }
  case TACTIX_OBSERVE_AFFINE_REASONS_UNAFFORDABLE: return k.affine_reasons_unaffordable;
  case TACTIX_OBSERVE_MODEL_EXTENDED: return k.model_extended;
  case TACTIX_OBSERVE_GRAPH_LABELING:
  {
    const auto *authentication = k.graph_labeling_session.last_authentication();
    return (k.graph_labeling_session.authenticated() ? 1u : 0u) |
           (k.graph_labeling_session.complete() ? 2u : 0u) |
           (authentication &&
                    authentication->status == GraphLabelingAuthenticationStatus::Interrupted
                ? 4u
                : 0u);
  }
  case TACTIX_OBSERVE_INPUT_BINARY_ORDER: return k.input_binary_order_captured;
  case TACTIX_OBSERVE_COMPACTION_DUE:
    if (argument > 100) return UINT64_MAX;
    return kernel_should_compact_variables(k, static_cast<uint32_t>(argument));
  case TACTIX_OBSERVE_PROPAGATION_FIXPOINT: return kernel_propagation_fixpoint(k);
  case TACTIX_OBSERVE_ACTIVE_VARIABLES: return kernel_active_variables(k);
  case TACTIX_OBSERVE_SUBSUME_MATERIAL: return k.subsume_material_count;
  case TACTIX_OBSERVE_FORWARD_COMPLETE:
    return (k.subsume_complete ? 1u : 0u) | (k.strengthen_complete ? 2u : 0u);
  case TACTIX_OBSERVE_FACTOR_BOUND: return k.factor_bound;
  case TACTIX_OBSERVE_VARIABLE_OPEN:
    return k.values[make_lit(variable, true)] == 0 && !kernel_var_retired(k, variable);
  case TACTIX_OBSERVE_TRAIL: return k.trail.size();
  case TACTIX_OBSERVE_BRANCH_POPULATION: return k.compact_import_order.size();
  case TACTIX_OBSERVE_REDUCE_CANDIDATES: {
    if (argument > 1) return UINT64_MAX;
    // The ranking Reduce applies; it locks and unlocks reasons, nothing else.
    std::vector<Ref> candidates;
    reduce_candidates(kernel_of(s), candidates, /*measure=*/false, argument != 0);
    return candidates.size();
  }
  case TACTIX_OBSERVE_VARIABLES: return k.nvars;
  case TACTIX_OBSERVE_INPUT_VARIABLES: return kernel_num_output_vars(k);
  case TACTIX_OBSERVE_PROPAGATION_HEAD: return k.propagate_head;
  case TACTIX_OBSERVE_FACTOR_STATE:
    return (k.factor_completed ? 1u : 0u) | (k.factor_composite ? 2u : 0u);
  case TACTIX_OBSERVE_FACTOR_MARKED: return k.factor_literals_marked;
  case TACTIX_OBSERVE_BVE_RETRY:
    return variable < k.bve_retry.size() && k.bve_retry[variable];
  case TACTIX_OBSERVE_COMPACT_POSITION:
    return variable < k.compact_import_position.size() &&
                   k.compact_import_position[variable] != INVALID_VAR
               ? k.compact_import_position[variable]
               : UINT32_MAX;
  case TACTIX_OBSERVE_AFFINE_SUPPORT: {
    const AffineMatrixSearchSession *session = k.affine_matrix_search_session;
    if (!session || !session->enabled() || !session->persistent()) return 0;
    const auto &support = session->support();
    return std::binary_search(support.begin(), support.end(),
                              kernel_externalize(k, make_lit(variable, false)));
  }
  case TACTIX_OBSERVE_VARIABLE_RETENTION: return k.variable_retention.size();
  case TACTIX_OBSERVE_VARIABLE_RETIRED: return kernel_var_retired(k, variable);
  case TACTIX_OBSERVE_GRAPH_LABELING_SLICE: {
    const auto *slice = k.graph_labeling_session.last_slice();
    return slice ? static_cast<uint64_t>(slice->status) + 1 : 0;
  }
  case TACTIX_OBSERVE_GRAPH_LABELING_CONTINUATION:
    return k.graph_labeling_session.recommended_continuation_work();
  case TACTIX_OBSERVE_EXTENSION_VARIABLE: return kernel_is_solver_extension(k, variable);
  case TACTIX_OBSERVE_SR_DISCOVERY_CONSUMED: {
    const auto &allowance = api_detail::sr_discovery(s);
    return allowance ? allowance->consumed() : 0;
  }
  case TACTIX_OBSERVE_SR_DISCOVERY_OVERFLOW: {
    const auto &allowance = api_detail::sr_discovery(s);
    return allowance && allowance->accounting_overflow();
  }
  case TACTIX_OBSERVE_AFFINE_CENSUS_UNCHANGED: {
    const auto &census = api_detail::affine_census(s);
    return k.affine_capture_epochs_trusted && census &&
           *census == api_detail::affine_census_stamp(s);
  }
  case TACTIX_OBSERVE_REGIONS_PENDING: return api_detail::regions_pending(s);
  case TACTIX_OBSERVE_STARTUP_RETAINED: return kernel_startup_retained(k, variable);
  case TACTIX_OBSERVE_IMPORT_POSITION:
    return variable < k.compact_import_position.size() &&
                   k.compact_import_position[variable] != INVALID_VAR
               ? k.compact_import_position[variable]
               : variable;
  case TACTIX_OBSERVE_AFFINE_SOURCE_BYTES:
    return sizeof(AffineLiveSource) + sizeof(affine_semantic_extension::Premise) +
           2 * sizeof(int32_t);
  }
  return UINT64_MAX;
}

int tactix_literal_value(tactix_c_handle *s, int literal) {
  if (!s) return 0;
  const Kernel &k = kernel_of(s);
  const Lit lit = kernel_internalize(k, literal);
  return lit == INVALID_LIT || var_of(lit) >= k.nvars ? 0 : k.value(lit);
}

int tactix_occurrences(tactix_c_handle *s, int literal, tactix_occurrence *out, int cap) {
  if (!s || cap < 0 || (cap && !out)) return -1;
  const Kernel &k = kernel_of(s);
  const Lit lit = kernel_internalize(k, literal);
  if (!k.dense || !k.dense->track_order || lit == INVALID_LIT || var_of(lit) >= k.nvars)
    return -1;
  const auto entries = k.dense->ordered[lit];
  int n = 0;
  for (const OccIndex::Entry entry : entries) {
    if (n < cap) {
      out[n].binary = entry.binary;
      out[n].partner = entry.binary ? kernel_externalize(k, entry.payload) : 0;
      out[n].clause = entry.binary ? 0 : entry.payload;
    }
    ++n;
  }
  return n;
}

int tactix_clause(tactix_c_handle *s, uint64_t clause, int *literals, int cap, unsigned *flags) {
  if (!s || cap < 0 || (cap && !literals)) return -1;
  const Kernel &k = kernel_of(s);
  if (clause >= k.arena.size()) return -1;
  const Clause *c = clause_at(k.arena, static_cast<Ref>(clause));
  unsigned out = (c->garbage() ? TACTIX_CLAUSE_GARBAGE : 0u) |
                 (c->redundant() ? TACTIX_CLAUSE_REDUNDANT : 0u);
  int n = 0;
  for (const Lit lit : *c) {
    if (k.value(lit) > 0) out |= TACTIX_CLAUSE_SATISFIED;
    if (n < cap) literals[n] = kernel_externalize(k, lit);
    ++n;
  }
  if (flags) *flags = out;
  return n;
}

uint64_t tactix_clause_after(tactix_c_handle *s, uint64_t clause) {
  if (!s) return UINT64_MAX;
  const Kernel &k = kernel_of(s);
  uint64_t next = 0;
  if (clause != UINT64_MAX) {
    if (clause >= k.arena.size()) return UINT64_MAX;
    next = clause + clause_actual_words(clause_at(k.arena, static_cast<Ref>(clause)));
  }
  return next < k.arena.size() ? next : UINT64_MAX;
}

int tactix_binary_partners(tactix_c_handle *s, int literal, int *partners, int cap) {
  if (!s || cap < 0 || (cap && !partners)) return -1;
  const Kernel &k = kernel_of(s);
  const Lit lit = kernel_internalize(k, literal);
  if (lit == INVALID_LIT || var_of(lit) >= k.nvars) return -1;
  int n = 0;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary()) {
      if (n < cap) partners[n] = kernel_externalize(k, watch.other());
      ++n;
    }
  return n;
}

int tactix_stage_configure(tactix_c_handle *s, int list, int index, unsigned feedback,
                           int *on_budget_exhaustion) {
  if (!s || index < 0) return -1;
  const auto &inprocess = api_detail::strategy_configuration(s).inprocess;
  const auto *stages = list == TACTIX_STAGE_LIST_ORDINARY ? &inprocess.structured_root_stages
                       : list == TACTIX_STAGE_LIST_BOUNDED_BACKTRACKING
                           ? &inprocess.structured_root_bounded_backtracking_stages
                       : list == TACTIX_STAGE_LIST_LARGE_DOMAIN
                           ? &inprocess.structured_root_large_domain_stages
                           : nullptr;
  if (!stages || static_cast<size_t>(index) >= stages->size()) return -1;
  const auto stage = DefaultStrategyConfig::structured_root_stage_after_feedback(
      (*stages)[static_cast<size_t>(index)], (feedback & TACTIX_STAGE_FEEDBACK_PROGRESS) != 0,
      (feedback & TACTIX_STAGE_FEEDBACK_UNIQUE_LITERAL_AMO) != 0,
      (feedback & TACTIX_STAGE_FEEDBACK_NONFORCING_CUT) != 0);
  auto sequence = std::make_unique<strategy::OnlineStrategySequence>();
  if (!sequence->configure(stage, 1, strategy::ReconfigureMode::ResetRuntime)) return -1;
  api_detail::structured_stage(s) = std::move(sequence);
  if (on_budget_exhaustion) *on_budget_exhaustion = static_cast<int>(stage.on_budget_exhaustion);
  return 0;
}

int tactix_stage_complete(tactix_c_handle *s) {
  if (!s) return 0;
  const auto &sequence = api_detail::structured_stage(s);
  return sequence && sequence->complete();
}

int tactix_stage_advance(tactix_c_handle *s, uint64_t ticks_per_generator, uint64_t hard_limit,
                         tactix_stage_result *out) {
  if (!s || !out) return -1;
  auto &sequence = api_detail::structured_stage(s);
  if (!sequence) return -1;
  Kernel &k = kernel_of(s);
  const strategy::InvocationResult advanced =
      sequence->advance(k, k.stop, api_detail::sr_discovery(s).get(),
                        strategy::DiscoveryFeedbackPolicy{ticks_per_generator, hard_limit});
  *out = {};
  out->status = static_cast<int>(advanced.status);
  out->made_rule_progress = advanced.made_rule_progress;
  out->discovery_entry_required = advanced.discovery_entry_required;
  out->applications = advanced.applications;
  out->discovery_progress_units = advanced.discovery_progress_units;
  return 0;
}

int tactix_sr_discovery_begin(tactix_c_handle *s, uint64_t limit) {
  if (!s) return -1;
  api_detail::sr_discovery(s) = std::make_unique<strategy::Allowance>(
      strategy::WorkUnit::SrDiscoveryTicks,
      limit == UINT64_MAX ? strategy::WorkLimit::unlimited() : strategy::WorkLimit::bounded(limit));
  return 0;
}

int tactix_sr_discovery_limit(tactix_c_handle *s, uint64_t limit) {
  if (!s) return -1;
  auto &allowance = api_detail::sr_discovery(s);
  if (!allowance) return -1;
  allowance->set_limit(limit == UINT64_MAX ? strategy::WorkLimit::unlimited()
                                           : strategy::WorkLimit::bounded(limit));
  return 0;
}

namespace {
uint64_t saturated(std::initializer_list<uint64_t> terms) {
  uint64_t sum = 0;
  for (const uint64_t term : terms) sum = term > UINT64_MAX - sum ? UINT64_MAX : sum + term;
  return sum;
}

void affine_service_state(const AffineMatrixServiceResult &in, tactix_affine_service_state &out) {
  out.status = static_cast<int>(in.status) + 1;
  out.result = static_cast<int>(in.result);
  out.bcp_pending = in.bcp_pending;
  out.synchronized = in.synchronized;
  out.cached_event = in.cached_event;
  out.terminal_failure = in.terminal_failure();
  out.synchronization_work = in.synchronization_work;
  out.work = saturated({in.trail_capture_work, in.synchronization_work, in.session_work,
                        in.live.planner_work, in.live.revalidation_work, in.lifecycle_work});
}
} // namespace

int tactix_read_affine_search(tactix_c_handle *s, tactix_affine_search_state *out) {
  if (!s || !out) return -1;
  *out = {};
  const Kernel &k = kernel_of(s);
  const AffineMatrixSearchSession *session = k.affine_matrix_search_session;
  if (!session) return 0;
  if (const auto &init = session->last_initialization()) {
    out->initialization_status = static_cast<int>(init->status) + 1;
    out->unchanged = init->unchanged;
    out->root_unit_additions = init->root_unit_additions;
    out->root_reason_additions = init->root_reason_additions;
    out->recognition_work = saturated({init->snapshot_work, init->recognition_work,
                                       init->matrix_work, init->premise_retention_work,
                                       init->root_unit_work});
    out->maintenance_work = init->maintenance_service_work;
    out->has_root_service = init->root_service.has_value();
    if (init->root_service) affine_service_state(*init->root_service, out->root_service);
  }
  if (const auto &service = session->last_service()) affine_service_state(*service, out->service);
  out->enabled = session->enabled();
  out->persistent = session->persistent();
  out->deferred_reason = session->has_deferred_reason();
  out->owner_version = session->owner_version();
  out->lazy_reasons = session->lazy_reason_count();
  public_encoding::Encoded basis;
  if (public_encoding::encode(strategy::InprocessParameters(session->parameters()), basis))
    out->basis = basis.parameters.value.affine_consequence;
  out->maximum_retained_bytes = session->persistence_limits().maximum_retained_bytes;
  out->maximum_deferred_deletions = session->persistence_limits().maximum_deferred_deletions;
  out->maximum_deferred_literal_work = session->persistence_limits().maximum_deferred_literal_work;
  return 0;
}

namespace {
AffineMatrixInputCursor input_cursor(const tactix_affine_inputs &in) {
  return {in.root_scanned, in.namespace_epoch, in.owner_version, in.premise_epoch, in.valid != 0};
}
void store_cursor(const AffineMatrixInputCursor &in, tactix_affine_inputs &out) {
  out = {in.root_scanned, in.namespace_epoch, in.owner_version, in.premise_epoch, in.valid};
}
} // namespace

int tactix_affine_inputs_changed(tactix_c_handle *s, tactix_affine_inputs *cursor) {
  if (!s || !cursor) return -1;
  const Kernel &k = kernel_of(s);
  const AffineMatrixSearchSession *session = k.affine_matrix_search_session;
  if (!session || !session->enabled()) return -1;
  AffineMatrixInputCursor current = input_cursor(*cursor);
  uint64_t work = 0;
  const bool changed = affine_matrix_search_inputs_changed(k, *session, current, work);
  store_cursor(current, *cursor);
  return changed;
}

int tactix_affine_inputs_consume(tactix_c_handle *s, tactix_affine_inputs *cursor) {
  if (!s || !cursor) return -1;
  const Kernel &k = kernel_of(s);
  const AffineMatrixSearchSession *session = k.affine_matrix_search_session;
  if (!session || !session->enabled()) return -1;
  AffineMatrixInputCursor current = input_cursor(*cursor);
  affine_matrix_search_consume_inputs(k, *session, current);
  store_cursor(current, *cursor);
  return 0;
}

int tactix_rule_applicable(tactix_c_handle *s, int rule_id) {
  if (!s) return -1;
  api_detail::complete_input(s);
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[static_cast<size_t>(rule_id)].retired)
    return -1;
  return rule_guard(rules[static_cast<size_t>(rule_id)], kernel_of(s)) ? 1 : 0;
}

int tactix_instance_applicable(tactix_c_handle *s, int rule_id, uint64_t instance) {
  const int guard = tactix_rule_applicable(s, rule_id);
  if (guard != 1) return guard;
  const RuleVTable &rule = tactix_rules()[static_cast<size_t>(rule_id)];
  if (!std::strcmp(rule.name, "Lucky"))
    return lucky_instance_admissible(kernel_of(s), instance) ? 1 : 0;
  return 1;
}

int tactix_branch_next(tactix_c_handle *s) {
  if (!s) return -1;
  Kernel &k = kernel_of(s);
  if (!k.h) return -1;
  if (!k.unassigned_active) return 0;
  return external_variable(k, heuristics_next_active_decision_var(k));
}

int tactix_branch_phase(tactix_c_handle *s, int variable) {
  if (!s) return -1;
  const Kernel &k = kernel_of(s);
  if (!k.h) return -1;
  const Var v = internal_variable(k, variable);
  if (v == INVALID_VAR || v >= k.nvars) return 0;
  return kernel_externalize(k, heuristics_decide_lit(k, v));
}

int tactix_branch_restart_level(tactix_c_handle *s, int variable) {
  if (!s) return -1;
  Kernel &k = kernel_of(s);
  if (!k.h) return -1;
  const Var v = internal_variable(k, variable);
  if (v == INVALID_VAR || v >= k.nvars) return -1;
  return static_cast<int>(heuristics_restart_reuse_level_for(k, v));
}

int tactix_branch_population(tactix_c_handle *s, uint32_t slot) {
  if (!s) return -1;
  const Kernel &k = kernel_of(s);
  if (!k.h) return -1;
  if (slot >= k.compact_import_order.size()) return 0;
  return external_variable(k, k.compact_import_order[slot]);
}

int tactix_strategy_integer(tactix_c_handle *s, const char *key, uint64_t *value) {
  StrategySettingValue setting;
  if (!s || !key || !value ||
      !api_detail::strategy_configuration(s).setting(key, setting) ||
      setting.kind != StrategySettingValue::Integer)
    return -1;
  *value = setting.integer;
  return 0;
}

int tactix_strategy_real(tactix_c_handle *s, const char *key, double *value) {
  StrategySettingValue setting;
  if (!s || !key || !value ||
      !api_detail::strategy_configuration(s).setting(key, setting) ||
      setting.kind != StrategySettingValue::Real)
    return -1;
  *value = setting.real;
  return 0;
}

int tactix_strategy_text(tactix_c_handle *s, const char *key, char *buffer, size_t size) {
  StrategySettingValue setting;
  if (!s || !key || (size && !buffer) ||
      !api_detail::strategy_configuration(s).setting(key, setting) ||
      setting.kind != StrategySettingValue::Text || setting.text.size() > INT32_MAX)
    return -1;
  if (size) {
    const size_t copied = std::min(size - 1, setting.text.size());
    std::memcpy(buffer, setting.text.data(), copied);
    buffer[copied] = '\0';
  }
  return static_cast<int>(setting.text.size());
}

namespace {

// Calls `visit(profile)` with every rule profile the default strategy's
// configuration poses; Walk's is its scoring guidance.
template <class Visit>
void visit_strategy_profiles(const DefaultStrategyConfig &c, Visit &&visit) {
  visit(c.parity_model.parameters);
  visit(c.onehot_model.parameters);
  visit(c.affine_model.parameters);
  visit(c.balanced_signature_model.parameters);
  visit(c.fermat_model.parameters);
  visit(c.lookup_circuit_model.parameters);
  visit(c.simon_model.parameters);
  visit(c.stconn_proof.parameters);
  visit(c.signed_cardinality_proof.parameters);
  visit(c.exact_cover_model.parameters);
  visit(c.independent_set_model.parameters);
  visit(c.graph_labeling_model.parameters);
  visit(c.ramsey_startup.parameters);
  visit(c.reorder.parameters);
  visit(c.inprocess.congruence_parameters);
  visit(c.inprocess.instantiate_parameters);
  visit(c.inprocess.backbone_parameters);
  visit(c.inprocess.vivify_parameters);
  visit(c.inprocess.bce_parameters);
  visit(c.inprocess.ternary_parameters);
  visit(c.inprocess.factor_parameters);
  visit(c.inprocess.transitive_parameters);
  visit(c.inprocess.substitute_parameters);
  visit(c.inprocess.sweep_parameters);
  visit(c.inprocess.initial_fast_elimination_parameters);
  strategy::WalkParameters guidance;
  guidance.step_budget = 0;
  guidance.even_break_base_milli = c.rephase.walk_even_break_base_milli;
  guidance.score_curve = c.rephase.walk_score_curve;
  visit(guidance);
}

// Every profile family's installed value (from_kernel). Plain templates
// rather than a pack-expanded lambda, which Clang 18 fails to instantiate.
template <size_t I, class Visit> void visit_installed_profile(const Kernel &k, Visit &visit) {
  using P = std::variant_alternative_t<I, strategy::InprocessParameters>;
  if constexpr (requires { P::from_kernel(k); }) visit(P::from_kernel(k));
}
template <class Visit, size_t... I>
void visit_installed_profiles(const Kernel &k, Visit &visit, std::index_sequence<I...>) {
  (visit_installed_profile<I>(k, visit), ...);
}
template <class Visit> void visit_installed_profiles(const Kernel &k, Visit &&visit) {
  visit_installed_profiles(
      k, visit, std::make_index_sequence<std::variant_size_v<strategy::InprocessParameters>>{});
}

} // namespace

double tactix_branch_rank(tactix_c_handle *s, int variable) {
  if (!s) return 0;
  const Kernel &k = kernel_of(s);
  const Var v = internal_variable(k, variable);
  if (!k.h || v == INVALID_VAR || v >= k.nvars) return 0;
  return k.h->stable ? k.h->scores.score[v] : static_cast<double>(k.h->links[v].stamp);
}

int tactix_bve_schedule_open(tactix_c_handle *s) {
  if (!s) return -1;
  Kernel &k = kernel_of(s);
  auto &schedule = api_detail::bve_schedule(s);
  if (schedule || !k.dense || !k.dense->track_order || k.bve_score_update_hook ||
      k.bve_occurrence_count_image)
    return -1;
  // The candidates BVE's own enumeration offers, as the default strategy's
  // score-heap pass orders them.
  const RuleVTable &bve = tactix_rules()[static_cast<size_t>(tactix_rule_id("BVE"))];
  std::vector<uint64_t> candidates(2 * static_cast<size_t>(k.nvars) + 1);
  const uint32_t n = bve.instances(k, candidates.data(), static_cast<uint32_t>(candidates.size()));
  schedule = std::make_unique<strategy::BveCandidateHeap>();
  schedule->initialize(k, candidates.data(), n);
  k.bve_score_update_hook = strategy::BveCandidateHeap::update_hook;
  k.bve_score_update_state = schedule.get();
  return 0;
}

uint64_t tactix_bve_schedule_next(tactix_c_handle *s) {
  if (!s) return TACTIX_INSTANCE_CANONICAL;
  auto &schedule = api_detail::bve_schedule(s);
  return schedule && !schedule->empty() ? schedule->pop_max() : TACTIX_INSTANCE_CANONICAL;
}

void tactix_bve_schedule_close(tactix_c_handle *s) {
  if (!s) return;
  auto &schedule = api_detail::bve_schedule(s);
  if (!schedule) return;
  Kernel &k = kernel_of(s);
  k.bve_score_update_hook = nullptr;
  k.bve_score_update_state = nullptr;
  k.bve_occurrence_count_image = nullptr;
  schedule.reset();
}

namespace {
// One program step as typed C fields; parameters encode into `scratch`.
int read_step(const strategy::InprocessStep &step, tactix_strategy_step *out,
              std::shared_ptr<void> *scratch) {
  *out = {};
  out->id = step.id;
  out->rule_id = tactix_rule_id(step.rule.c_str());
  out->phase = step.phase == strategy::InprocessPhase::Probe ? 0 : 1;
  out->enabled = step.enabled;
  out->preprocessing = step.preprocessing;
  out->followup = static_cast<int>(step.followup);
  out->effort_permille = step.effort_permille ? static_cast<int64_t>(*step.effort_permille) : -1;
  out->max_applications = step.max_applications;
  out->cursor = step.cursor_enabled ? *step.cursor_enabled : -1;
  out->candidate_order = step.candidate_order ? static_cast<int>(*step.candidate_order) : -1;
  if (std::holds_alternative<std::monostate>(step.parameters)) {
    out->parameters.kind = TACTIX_ONLINE_PARAMETERS_INHERIT;
    return 0;
  }
  auto encoded = std::make_shared<public_encoding::Encoded>();
  if (!scratch || !public_encoding::encode(step.parameters, *encoded)) return -1;
  *scratch = encoded;
  out->parameters = encoded->parameters;
  return 0;
}
} // namespace

int tactix_strategy_read_step(tactix_c_handle *s, int index, tactix_strategy_step *out) {
  if (!s || !out || index < 0) return -1;
  const auto &program = api_detail::strategy_configuration(s).inprocess.program;
  if (static_cast<size_t>(index) >= program.size()) return -1;
  return read_step(program[static_cast<size_t>(index)], out, &api_detail::observe_scratch(s));
}

int tactix_strategy_read_reference_step(int index, tactix_strategy_step *out) {
  // The shipped program, read once; its parameter encodings live for the
  // process, so every returned step stays valid.
  struct Reference {
    std::vector<strategy::InprocessStep> program = strategy::reference_inprocess_program();
    std::vector<tactix_strategy_step> steps;
    std::vector<std::shared_ptr<void>> encodings;
    Reference() : steps(program.size()), encodings(program.size()) {
      for (size_t i = 0; i < program.size(); ++i)
        if (read_step(program[i], &steps[i], &encodings[i])) steps[i].rule_id = -1;
    }
  };
  static const Reference reference;
  if (!out || index < 0 || static_cast<size_t>(index) >= reference.steps.size()) return -1;
  *out = reference.steps[static_cast<size_t>(index)];
  return out->rule_id < 0 ? -1 : 0;
}

namespace {
const RuleVTable *live_rule(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size())) return nullptr;
  const RuleVTable &rule = rules[static_cast<size_t>(rule_id)];
  return rule.retired ? nullptr : &rule;
}

// The configuration-independent part of each rule's schedule (its meter and
// productivity functions), indexed by rule id for the per-candidate reads.
const DefaultRuleSchedule &rule_functions(int rule_id) {
  static const std::vector<DefaultRuleSchedule> table = [] {
    std::vector<DefaultRuleSchedule> out;
    for (const RuleVTable &rule : tactix_rules())
      out.push_back(default_rule_schedule(DefaultStrategyConfig{}, rule.name));
    return out;
  }();
  return table[static_cast<size_t>(rule_id)];
}
} // namespace

int tactix_strategy_read_rule(tactix_c_handle *s, int rule_id, tactix_strategy_rule *out) {
  const RuleVTable *rule = live_rule(rule_id);
  if (!s || !out || !rule) return -1;
  const DefaultRuleSchedule r =
      default_rule_schedule(api_detail::strategy_configuration(s), rule->name);
  *out = {};
  out->instances = rule->instances && rule->apply_instance;
  out->metered = r.meter != nullptr;
  out->dosed = r.dosed;
  out->budget_aware = r.budget_aware;
  out->shared_meter = r.shared_meter;
  out->tick_stat = api_detail::tick_attribution_stat(r.tick_counter);
  out->effort_permille = r.permille;
  out->delayed = r.yield != nullptr;
  out->delay_delta = r.delay_delta;
  out->delay_permille = r.delay_permille;
  out->appended = r.appended;
  out->appended_enabled = r.appended_enabled;
  return 0;
}

uint64_t tactix_strategy_meter(tactix_c_handle *s, int rule_id) {
  if (!s || !live_rule(rule_id)) return 0;
  const auto meter = rule_functions(rule_id).meter;
  return meter ? meter(kernel_of(s)) : 0;
}

int tactix_strategy_productivity(tactix_c_handle *s, int rule_id, uint64_t *yield,
                                 uint64_t *denominator) {
  if (!s || !yield || !denominator || !live_rule(rule_id)) return -1;
  const DefaultRuleSchedule &r = rule_functions(rule_id);
  if (!r.yield) return -1;
  *yield = r.yield(kernel_of(s));
  *denominator = r.denominator(kernel_of(s));
  return 0;
}

int tactix_read_parameters(tactix_c_handle *s, int rule_id, int kind, int source,
                           tactix_online_parameters *out) {
  const auto &rules = tactix_rules();
  if (!s || !out || rule_id < 0 || rule_id >= static_cast<int>(rules.size()) ||
      rules[static_cast<size_t>(rule_id)].retired ||
      (source != TACTIX_PARAMETERS_INSTALLED && source != TACTIX_PARAMETERS_STRATEGY) ||
      kind <= TACTIX_ONLINE_PARAMETERS_INHERIT)
    return -1;
  const std::string rule = rules[static_cast<size_t>(rule_id)].name;
  auto encoded = std::make_shared<public_encoding::Encoded>();
  bool found = false;
  // The first profile of the family whose encoding names `kind`, provided
  // the rule takes that family (the installation's match, without its value
  // restrictions: a readback reports what is held).
  const auto consider = [&](const auto &profile) {
    if (found || !strategy::action_parameter_metadata(profile, rule).matches_rule) return;
    const strategy::InprocessParameters candidate(profile);
    public_encoding::Encoded attempt;
    if (!public_encoding::encode(candidate, attempt) || attempt.parameters.kind != kind) return;
    found = true;
    (void)public_encoding::encode(candidate, *encoded); // retained pointer storage
  };
  if (source == TACTIX_PARAMETERS_INSTALLED)
    visit_installed_profiles(kernel_of(s), consider);
  else
    visit_strategy_profiles(api_detail::strategy_configuration(s), consider);
  if (!found) return -1;
  api_detail::observe_scratch(s) = encoded;
  *out = encoded->parameters;
  return 0;
}

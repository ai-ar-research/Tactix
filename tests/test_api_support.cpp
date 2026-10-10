// Public support transactions (gap plan Step 4, A5/A11). Two C-API solvers
// load the same formula; one issues tactix_support under NATIVE guidance
// publication, the other runs DefaultPolicy's composition of the same shared
// requests on its kernel. Formula, assignment, lifecycle and H state must stay
// identical after every transaction. Also: rejections leave the solver
// unchanged, execution contexts nest and restore, the inprocessing phase
// bracket matches DefaultPolicy's shared bracket and nests with contexts, and
// an SR profile in complete-profile mode converts every field whatever the
// producer.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "policies/rule_publication.hpp"
#include "rules/registry.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/execution_scope.hpp"
#include "strategy/guidance_execution.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/phase_execution.hpp"
#include "strategy/reduction_execution.hpp"
#include "strategy/representation_execution.hpp"
#include "strategy/root_execution.hpp"

#include <cassert>
#include <cstring>
#include <vector>

using namespace tactix;

namespace {

constexpr int VARIABLES = 40;

// A fixed random 3-CNF with two input units and an implication chain, so the
// root has fixed variables and decisions propagate.
tactix_c_handle *load() {
  tactix_c_handle *s = tactix_init();
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) == 0);
  uint64_t state = 0x9e3779b97f4a7c15ull;
  const auto next = [&] {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<int>(state >> 33);
  };
  for (int c = 0; c < 120; ++c) {
    for (int j = 0; j < 3; ++j) {
      const int v = 1 + next() % VARIABLES;
      tactix_add(s, next() % 2 ? v : -v);
    }
    tactix_add(s, 0);
  }
  for (int lits : {1, 2}) {
    tactix_add(s, lits);
    tactix_add(s, 0);
  }
  for (int v = 3; v < 8; ++v) {
    tactix_add(s, -v);
    tactix_add(s, v + 1);
    tactix_add(s, 0);
  }
  assert(tactix_status(s) == 0);
  return s;
}

Kernel &kernel(tactix_c_handle *s) { return api_detail::kernel_for_fixed_testing(s); }

std::vector<Lit> candidates(const Kernel &k) {
  std::vector<Lit> out(k.nvars + 1);
  out.resize(heuristics_decide_candidates(k, out.data(), static_cast<uint32_t>(out.size())));
  return out;
}

void same(const Kernel &a, const Kernel &b) {
  assert(a.level == b.level && a.trail == b.trail && a.inconsistent == b.inconsistent);
  assert(a.live_irredundant_large == b.live_irredundant_large);
  assert(a.binary_ids.size() == b.binary_ids.size());
  assert(a.unflushed_root == b.unflushed_root);
  assert(a.inprocessing == b.inprocessing &&
         a.phase_saving_suppressed == b.phase_saving_suppressed);
  assert(a.compact_import_order == b.compact_import_order);
  assert(a.h->saved == b.h->saved && a.h->target == b.h->target);
  assert(a.h->stable == b.h->stable && candidates(a) == candidates(b));
  assert(a.stats.restarts == b.stats.restarts);
}

// DefaultPolicy's root propagation: Propagate, then its new roots into H.
ApplyResult native_propagate(Kernel &k) {
  const size_t begin = k.trail.size();
  const uint64_t roots = k.stats.root_assignments;
  const ApplyResult result = propagate_apply(k);
  if (k.stats.root_assignments != roots) heuristics_consume_root_assignments(k, begin);
  return result;
}

void decide_both(tactix_c_handle *a, tactix_c_handle *b, int count) {
  for (int i = 0; i < count; ++i) {
    int lits[VARIABLES + 1];
    const int n = tactix_decide_candidates(a, lits, VARIABLES + 1);
    if (!n) return;
    assert(tactix_decide(a, lits[0]) == 0 && tactix_decide(b, lits[0]) == 0);
    const int propagate = tactix_rule_id("Propagate");
    const int ra = tactix_apply(a, propagate), rb = tactix_apply(b, propagate);
    assert(ra == rb);
    if (ra == 2 || ra == 20) return; // a conflict: leave it to the next support
  }
}

void test_twins() {
  tactix_c_handle *pub = load(), *nat = load();
  Kernel &p = kernel(pub), &n = kernel(nat);
  same(p, n);

  // Rephase root preparation: target capture, preparation restart, root
  // Propagate, root-prefix consumption.
  decide_both(pub, nat, 4);
  assert(p.level > 0);
  const uint64_t restarts = p.stats.restarts;
  const execution::RootPreparationRequest rephase{false, true};
  assert(tactix_support(pub, TACTIX_SUPPORT_ROOT_PREPARATION,
                        execution::root_preparation_argument(rephase, true)) == 0);
  assert(execution::execute_action(n, rephase, [&] { heuristics_update_target_phases(n); },
             [&](const execution::RestartRequest &r) { return execution::execute_action(n, r); },
             [&] { return native_propagate(n); }) == ApplyResult::Ok);
  same(p, n);
  assert(!p.level && !p.unflushed_root && p.stats.restarts == restarts);

  // Startup representation and guidance: sparse collection, random order.
  int collected = tactix_support(pub, TACTIX_SUPPORT_SPARSE_COLLECT, 0);
  const RootCollectOutcome native = execution::execute_action(n, execution::SparseCollectRequest{});
  assert(collected == (native == RootCollectOutcome::Unit       ? 2
                       : native == RootCollectOutcome::NoChange ? 1 : 0));
  assert(collected != 1); // the input units leave satisfied clauses
  same(p, n);
  if (collected == 2) {
    assert(tactix_apply(pub, tactix_rule_id("Propagate")) == 0);
    assert(native_propagate(n) == ApplyResult::Ok);
    same(p, n);
  }
  assert(tactix_support(pub, TACTIX_SUPPORT_COMPACT_RANDOM_ORDER, 0) == 0);
  execution::execute_action(n, execution::CompactRandomOrderRequest{});
  same(p, n);

  // Warm-up in H's order: focused heuristic phase, a forced focused-pattern
  // phase, then stable mode. The native side picks with DefaultBranching.
  DefaultStrategyConfig config;
  DefaultBranching branching(&config.branching);
  for (int round = 0; round < 3; ++round) {
    if (round == 1) p.stats.switched = n.stats.switched = 2; // pattern epoch 1: positive
    if (round == 2) {
      const int mode = tactix_rule_id("SwitchMode");
      assert(tactix_apply(pub, mode) == 0 && tactix_apply(nat, mode) == 0);
    }
    const int8_t forced = branching.forced_phase(n);
    assert(round != 1 || forced > 0);
    const uint64_t phase = forced > 0 ? TACTIX_WARMUP_PHASE_POSITIVE
                           : forced < 0 ? TACTIX_WARMUP_PHASE_NEGATIVE
                                        : TACTIX_WARMUP_PHASE_HEURISTIC;
    const auto saved = p.h->saved;
    assert(tactix_support(pub, TACTIX_SUPPORT_WARMUP, phase) == 0);
    (void)execution::execute_action(n, execution::WarmupRequest{},
        [&] { return branching.pick(n); }, [](Lit) {}, [](size_t) {});
    same(p, n);
    assert(round || p.h->saved != saved); // the warm-up saved phases
  }

  // Reduce preparation and the composite compaction from a decided state.
  decide_both(pub, nat, 3);
  assert(tactix_support(pub, TACTIX_SUPPORT_REDUCTION_PREPARATION, 0) != -1);
  execution::execute_action(n, execution::ReductionPreparationRequest{},
      [&](size_t begin) { heuristics_consume_root_assignments(n, begin); });
  same(p, n);
  if (p.level) {
    // A preparation restart keeps no scheduled-restart occasion.
    assert(tactix_support(pub, TACTIX_SUPPORT_PREPARATION_RESTART, 0) == 0);
    assert(execution::execute_action(n, execution::RestartRequest{
               0, execution::RestartPurpose::Preparation}) == ApplyResult::Ok);
    same(p, n);
  }
  const uint64_t compact = TACTIX_COMPACT_FOLD_FIXED | TACTIX_COMPACT_PUBLISH_RANDOM_ORDER;
  const int compacted = tactix_support(pub, TACTIX_SUPPORT_COMPACT_VARIABLES, compact);
  const bool native_compacted =
      execution::execute_action(n, execution::CompactVariablesRequest{true});
  if (native_compacted) heuristics_compact_random_order_fixed(n);
  assert(compacted == (native_compacted ? 0 : 1));
  same(p, n);

  // Collections and lifecycle releases.
  assert(tactix_support(pub, TACTIX_SUPPORT_COLLECT_GARBAGE, 0) == 0);
  execution::execute_action(n, execution::CollectGarbageRequest{});
  assert(tactix_support(pub, TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT, 0) == 0);
  execution::execute_action(n, execution::FlushRetiredRedundantRequest{});
  same(p, n);
  const uint64_t session = p.bve_definition_session;
  assert(tactix_support(pub, TACTIX_SUPPORT_BEGIN_BVE_DEFINITION_SESSION, 0) == 0);
  assert(p.bve_definition_session == session + 1);
  assert(tactix_support(pub, TACTIX_SUPPORT_RESET_GRAPH_LABELING_SESSION, 0) == 0);
  // A C session never retains the parser's binary order.
  assert(tactix_support(pub, TACTIX_SUPPORT_RELEASE_INPUT_ORDER, 0) == 1);
  p.unflushed_root = 3;
  assert(tactix_support(pub, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX, 0) == 0 && !p.unflushed_root);
  assert(tactix_support(pub, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX, 0) == 1);
  assert(tactix_support(pub, TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE, 0) == 0);
  tactix_release(pub);
  tactix_release(nat);
}

void test_contexts() {
  tactix_c_handle *s = load();
  Kernel &k = kernel(s);
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  assert(tactix_support(s, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == -1);
  for (uint64_t invalid : {0ull, 4ull})
    assert(tactix_support(s, TACTIX_SUPPORT_ENTER_CONTEXT, invalid) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_STARTUP) == 0);
  assert(k.inprocessing && !k.phase_saving_suppressed);
  assert(tactix_support(s, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_INPROCESSING) == 0);
  assert(k.inprocessing && k.phase_saving_suppressed);
  assert(tactix_support(s, TACTIX_SUPPORT_LEAVE_CONTEXT, 1) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == 0);
  assert(k.inprocessing && !k.phase_saving_suppressed);
  assert(tactix_support(s, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_SPECULATION) == 0);
  assert(k.inprocessing && k.phase_saving_suppressed);
  assert(tactix_support(s, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == 0);
  assert(tactix_support(s, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == 0);
  assert(!k.inprocessing && !k.phase_saving_suppressed);
  tactix_release(s);
}

void test_rejections() {
  tactix_c_handle *s = load();
  Kernel &k = kernel(s);
  const auto trail = k.trail;
  const uint64_t epoch = k.strategy_epoch;
  assert(tactix_support(nullptr, TACTIX_SUPPORT_SPARSE_COLLECT, 0) == -1);
  for (int unknown : {-1, 0, 26})
    assert(tactix_support(s, unknown, 0) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_ROOT_PREPARATION, 8) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_COMPACT_VARIABLES, 4) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_COLLECT_GARBAGE, 4) == -1);
  assert(tactix_support(s, TACTIX_SUPPORT_WARMUP, 3) == -1);
  for (int support : {TACTIX_SUPPORT_CLOSE_STARTUP_CERTIFICATE, TACTIX_SUPPORT_SPARSE_COLLECT,
                      TACTIX_SUPPORT_COMPACT_RANDOM_ORDER, TACTIX_SUPPORT_REDUCTION_PREPARATION,
                      TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT, TACTIX_SUPPORT_RESEED_GUIDANCE,
                      TACTIX_SUPPORT_RELEASE_INPUT_ORDER, TACTIX_SUPPORT_CONSUME_ROOT_PREFIX})
    assert(tactix_support(s, support, 1) == -1);
  assert(k.trail == trail && k.strategy_epoch == epoch);
  // Root-only transactions decline above the root.
  int lits[VARIABLES + 1];
  assert(tactix_decide_candidates(s, lits, VARIABLES + 1) > 0);
  assert(tactix_decide(s, lits[0]) == 0);
  const auto decided = k.trail;
  for (int support : {TACTIX_SUPPORT_SPARSE_COLLECT, TACTIX_SUPPORT_COMPACT_RANDOM_ORDER,
                      TACTIX_SUPPORT_COLLECT_GARBAGE,
                      TACTIX_SUPPORT_FLUSH_RETIRED_REDUNDANT, TACTIX_SUPPORT_WARMUP,
                      TACTIX_SUPPORT_RESEED_GUIDANCE})
    assert(tactix_support(s, support, 0) == -1);
  assert(k.trail == decided && k.level == 1);
  // Variable compaction also runs above the root, as Reduce compacts.
  const int compacted = tactix_support(s, TACTIX_SUPPORT_COMPACT_VARIABLES, 0);
  assert((compacted == 0 || compacted == 1) && k.level == 1);
  if (compacted == 1) assert(k.trail == decided);
  tactix_release(s);
}

void test_reseed() {
  tactix_c_handle *s = load();
  Kernel &k = kernel(s);
  assert(tactix_support(s, TACTIX_SUPPORT_WARMUP, TACTIX_WARMUP_PHASE_HEURISTIC) == 0);
  // The residual seed replaces H: no saved phase survives, every active
  // unassigned variable is a candidate, and the import order is the seed's.
  assert(tactix_support(s, TACTIX_SUPPORT_RESEED_GUIDANCE, 0) == 0);
  assert(k.h && k.saved_phases == &k.h->saved);
  for (int8_t phase : k.h->saved) assert(!phase);
  size_t active = 0;
  for (Var v = 0; v < k.nvars; ++v)
    active += !k.values[make_lit(v, false)] && heuristics_var_active(k, v);
  assert(candidates(k).size() == active && active > 0);
  assert(!k.compact_import_order.empty());
  tactix_release(s);
}

// The phase bracket: the public session against DefaultPolicy's shared
// bracket with its default Factor selection, H publication and Propagate.
void test_phase_bracket() {
  tactix_c_handle *pub = load(), *nat = load();
  Kernel &p = kernel(pub), &n = kernel(nat);
  const int propagate = tactix_rule_id("Propagate"), bve = tactix_rule_id("BVE");
  assert(tactix_apply(pub, propagate) == tactix_apply(nat, propagate));
  same(p, n);
  for (const bool eliminate : {true, false}) {
    const uint32_t bound = eliminate ? 3 : 0;
    assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,
                          (eliminate ? TACTIX_PHASE_ELIMINATE : 0) |
                              (uint64_t{bound} << TACTIX_PHASE_FACTOR_BOUND_SHIFT)) == 0);
    DefaultFactorMatch selection;
    OccIndex occurrences;
    execution::InprocessPhase phase;
    phase.enter(n, {eliminate, selection, bound});
    same(p, n);
    assert(p.inprocessing && p.phase_saving_suppressed);
    assert(p.large_watches_connected == !eliminate && p.factor_selection &&
           static_cast<DefaultFactorMatch *>(p.factor_selection)->bound == bound);
    // One bracket at a time; supports that need a quiescent root decline
    // while the session is open; contexts nest strictly inside.
    assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, 0) == -1);
    if (eliminate) assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == -1);
    assert(tactix_support(pub, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 1) == -1);
    assert(tactix_support(pub, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0);
    phase.open_occurrences(n, occurrences);
    assert(p.dense && tactix_support(pub, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == -1);
    assert(tactix_support(pub, TACTIX_SUPPORT_SPARSE_COLLECT, 0) == -1);
    assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_SPECULATION) == 0);
    assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == -1);
    assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == 0);
    assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == -1);
    if (eliminate) {
      const int ra = tactix_apply(pub, bve), rb = tactix_apply(nat, bve);
      assert(ra == rb && ra != -1);
    }
    same(p, n);
    const int left = tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0);
    const ApplyResult exited = phase.exit(n, n.inconsistent,
        [&] { heuristics_consume_retirements(n); }, [&] { return native_propagate(n); });
    assert(left == (exited == ApplyResult::Unsat ? 20 : 0));
    same(p, n);
    assert(!p.inprocessing && !p.phase_saving_suppressed && !p.dense &&
           p.large_watches_connected && !p.factor_selection);
    assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == -1);
  }
  // A Probe bracket closes without a session (structured startup terminal).
  assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, 2) == -1);
  assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, 0) == 0);
  assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == 0);
  assert(!p.inprocessing && !p.factor_selection);
  // An outer context cannot be left inside the bracket.
  assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_CONTEXT, TACTIX_CONTEXT_STARTUP) == 0);
  assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, 0) == 0);
  assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == -1);
  assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0) == 0);
  assert(tactix_support(pub, TACTIX_SUPPORT_LEAVE_CONTEXT, 0) == 0);
  assert(!p.inprocessing);
  // tactix_solve leaves an open bracket before searching.
  assert(tactix_support(pub, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, TACTIX_PHASE_ELIMINATE) == 0);
  assert(tactix_support(pub, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0);
  const int verdict = tactix_solve(pub);
  assert(verdict == 10 || verdict == 20);
  assert(!p.dense && p.large_watches_connected && !p.factor_selection);
  tactix_release(pub);
  tactix_release(nat);
}

// complete_profile_mode: a GraphSymmetry profile carries the orbitopal
// fields its structured-generator handoff reads.
void test_complete_sr_profile() {
  tactix_c_handle *s = load();
  tactix_online_step step{};
  step.step_id = 1;
  step.rule_id = tactix_rule_id("SubstitutionRedundancy");
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.budget_unlimited = 1;
  auto &sr = step.parameters.value.substitution_redundancy;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  sr.producer = TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  sr.maximum_validation_ticks = UINT64_MAX;
  sr.maximum_detection_ticks = UINT64_MAX;
  sr.orbitopal_discovery_order = TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_ONLY;
  sr.minimum_orbitopal_rows = 3;
  sr.symmetry_use_structured_generators_mode = TACTIX_ONLINE_SETTING_ENABLE;
  const tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  const auto converted = [&] {
    const auto *configured = api_detail::online_program_for_fixed_testing(s);
    assert(configured && configured->sequence.size() == 1);
    return std::get<SubstitutionRedundancyParameters>(configured->sequence[0].parameters);
  };
  const SubstitutionRedundancyParameters defaults;
  for (int mode : {TACTIX_ONLINE_SETTING_INHERIT, TACTIX_ONLINE_SETTING_DISABLE}) {
    sr.complete_profile_mode = mode;
    assert(tactix_online_configure(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    const auto typed = converted();
    assert(typed.orbitopal_discovery_order == defaults.orbitopal_discovery_order);
    assert(typed.minimum_orbitopal_rows == defaults.minimum_orbitopal_rows);
    assert(typed.graph_symmetry.use_structured_generators);
  }
  sr.complete_profile_mode = TACTIX_ONLINE_SETTING_ENABLE;
  sr.minimum_orbitopal_clique_columns = 5;
  sr.orbitopal_base_probe_factor = 8;
  sr.minimum_orbitopal_columns = 2;
  sr.maximum_orbitopal_matrices = 7;
  sr.maximum_orbitopal_instances = sr.maximum_orbitopal_clique_vertices =
      sr.maximum_orbitopal_clique_ticks = sr.maximum_orbitopal_product_states =
          sr.maximum_general_candidates_per_advance = UINT64_MAX;
  assert(tactix_online_configure(s, &program, 2, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  const auto typed = converted();
  assert(typed.orbitopal_discovery_order == OrbitopalDiscoveryOrder::RefinementOnly);
  assert(typed.minimum_orbitopal_rows == 3 && typed.maximum_orbitopal_matrices == 7);
  assert(typed.graph_symmetry.use_structured_generators);
  for (int invalid : {-1, 3}) {
    sr.complete_profile_mode = invalid;
    assert(tactix_online_configure(s, &program, 3, TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  }
  tactix_release(s);
}

} // namespace

int main() {
  test_twins();
  test_contexts();
  test_rejections();
  test_reseed();
  test_phase_bracket();
  test_complete_sr_profile();
  return 0;
}

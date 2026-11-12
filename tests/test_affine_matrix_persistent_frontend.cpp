#include "unit_test.hpp"
#include <set>
#include "strategy/online_inprocess.hpp"
#include <unistd.h>
namespace capi {
extern "C" {
#include "tactix.h"
}
}
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/online_sequence.hpp"
#include "api/api_internal.hpp"
#include "test_io.hpp"
using namespace tactix::strategy;

static AffineMatrixInitializeParameters profile() {
  Kernel k; kernel_reserve_vars(k,8);
  DefaultStrategyConfig config; config.affine_search.enabled=true;
  auto basis=default_affine_search_parameters(k,config); assert(basis);
  AffineMatrixInitializeParameters p;
  p.basis=*basis;p.maximum_retained_bytes=100000000;
  p.maximum_deferred_deletions=10000;p.maximum_deferred_literal_work=1000000;
  assert(p.valid());return p;
}
static OnlineInvocation invocation(const AffineMatrixInitializeParameters &p) {
  OnlineInvocation step;step.id=941;step.rule="AffineMatrixInitialize";
  step.parameters=p;step.granularity=InvocationGranularity::CanonicalApplication;
  step.repeat={RepeatMode::Once,1};step.work_unit=WorkUnit::Applications;
  step.budget=WorkLimit::bounded(1);return step;
}
static void replay_and_scope() {
  auto p=profile();OnlineStrategyProgram program;program.sequence={invocation(p)};
  p.operation=AffineMatrixInitializeParameters::Operation::LazyMinimalSearchStep;
  program.sequence[0].parameters=p;
  assert(serialize_online_strategy_program(program).find(":affine-matrix-persistent1(")!=std::string::npos);
  auto invalid=p;invalid.maximum_deferred_deletions=0;
  program.sequence[0].parameters=invalid;assert(!validate_online_strategy_program(program));
  program.sequence[0].parameters=p;program.sequence[0].rule="AffineConsequence";
  assert(!validate_online_strategy_program(program));
  program.sequence[0].rule="AffineMatrixInitialize";program.sequence[0].parameters=p.basis;
  auto legacy=serialize_online_strategy_program(program);
  assert(legacy.find(":affine-consequence2(")!=std::string::npos);
  assert(legacy.find("persistent")==std::string::npos);
  Kernel k;install_rule_parameters(k,p);
  {
    execution::ParameterExecutionScope<AffineConsequenceParameters,AffineMatrixInitializeParameters> scope(k,program.sequence[0].parameters);
    assert(!k.affine_matrix_initialize_parameters_active);
  }
  assert(k.affine_matrix_initialize_parameters_active && k.affine_matrix_initialize_parameters==p);
}
static capi::tactix_online_affine_consequence_parameters c_basis(const AffineConsequenceParameters &p) {
  capi::tactix_online_affine_consequence_parameters out{};
#define COPY(field) out.field=p.field
  COPY(maximum_clauses);COPY(maximum_snapshot_literals);COPY(maximum_snapshot_work);
  COPY(maximum_groups);COPY(maximum_patterns);COPY(maximum_elimination_work);
  COPY(maximum_arity);COPY(maximum_intermediate_width);COPY(maximum_sources);
  COPY(maximum_output_width);COPY(maximum_proof_additions);COPY(maximum_proof_deletions);
  COPY(maximum_journal_bytes);COPY(consequence);COPY(basis_maximum_work);
  COPY(basis_maximum_intermediate_width);COPY(basis_maximum_sources);COPY(basis_maximum_premises);
  COPY(basis_maximum_nodes);COPY(basis_maximum_direct_width);COPY(maximum_external_extent);
  COPY(maximum_session_keys);COPY(maximum_session_work);
#undef COPY
  out.semantics=TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;return out;
}
// A C solver bound to a fresh journal; proof() writes to a fresh temporary
// file. Both are released with the solver.
struct CSolver {
  capi::tactix *s = capi::tactix_init();
  FILE *records = std::tmpfile();
  char path[64] = "/tmp/tactix-persistent-c16-XXXXXX";
  CSolver() {
    assert(s && records);
    assert(capi::tactix_bind_structural_provenance(s, records) == 0);
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
  }
  ~CSolver() {
    capi::tactix_release(s);
    std::fclose(records);
    std::remove(path);
  }
  void proof(int format = TACTIX_PROOF_DSR_ASCII) {
    assert(capi::tactix_proof(s, path, format) == 0);
  }
  void add(const std::vector<int32_t> &clause) {
    for (int32_t literal : clause) capi::tactix_add(s, literal);
    capi::tactix_add(s, 0);
  }
  // The XOR of first = 0 and the XOR of second = second_rhs, as clauses.
  void add_xors(const std::vector<int32_t> &first, const std::vector<int32_t> &second,
                bool second_rhs) {
    for (const auto &clause : affine_semantic_extension::clausal_form(first, false)) add(clause);
    for (const auto &clause : affine_semantic_extension::clausal_form(second, second_rhs))
      add(clause);
  }
  Kernel &kernel() { return api_detail::kernel_for_fixed_testing(s); }
};

static auto &step_base(capi::tactix_online_step_v16 &step) {
  return step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base;
}

// One canonical application of AffineMatrixInitialize (rule 41) under the
// persistent profile of p and operation.
static capi::tactix_online_step_v16 persistent_step(const AffineMatrixInitializeParameters &p,
                                                    uint32_t operation = 1) {
  capi::tactix_online_step_v16 step{};
  auto &base = step_base(step);
  base.step_id = 941;
  base.rule_id = 41;
  base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  step.affine_matrix_profile = TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
  step.affine_matrix = {c_basis(p.basis), 1, operation, p.maximum_retained_bytes,
                        p.maximum_deferred_deletions, p.maximum_deferred_literal_work};
  return step;
}

static void c_surface(bool with_proof) {
  CSolver solver;
  auto *s = solver.s;
  if (with_proof) solver.proof();
  solver.add_xors({1, 2, 3}, {2, 3, 4}, true);
  auto p = profile();
  auto step = persistent_step(p);
  auto &base = step_base(step);
  capi::tactix_online_program_v16 program{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  auto valid=step.affine_matrix;
  step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_DETACHED_SOURCE_SEARCH_STEP;
  assert(capi::tactix_online_configure_v16(s,&program,938,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_DETACHED_MINIMAL_SEARCH_STEP;
  assert(capi::tactix_online_configure_v16(s,&program,939,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_LAZY_SOURCE_SEARCH_STEP;
  assert(capi::tactix_online_configure_v16(s,&program,942,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_LAZY_MINIMAL_SEARCH_STEP;
  assert(capi::tactix_online_configure_v16(s,&program,943,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  step.affine_matrix=valid;step.affine_matrix.maximum_deferred_deletions=0;
  assert(capi::tactix_online_configure_v16(s,&program,940,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
  step.affine_matrix=valid;
  assert(capi::tactix_online_configure_v16(s,&program,941,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  const auto saved_base=base;
  step={}; // Configuration owns an exact deep copy of the profile.
  capi::tactix_online_result result{};assert(capi::tactix_online_advance(s,&result)==0);
  // N2: a step does what it does whether or not the session writes a proof.
  assert(result.status==TACTIX_ONLINE_COMPLETED);
  {
    int propagate=-1;
    for(int i=0;i<capi::tactix_rule_count();++i)
      if(!std::strcmp(capi::tactix_rule_name(i),"Propagate"))propagate=i;
    assert(propagate>=0 && capi::tactix_decide(s,1)==0);
    assert(capi::tactix_apply(s,propagate)>=0);
    step_base(step)=saved_base;
    step.affine_matrix_profile=TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
    step.affine_matrix=valid;step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_SEARCH_STEP;
    // A mismatched profile is rejected without changing the live search state.
    ++step.affine_matrix.maximum_retained_bytes;
    assert(capi::tactix_online_configure_v16(s,&program,942,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
    assert(capi::tactix_online_advance(s,&result)==0);
    assert(result.status==TACTIX_ONLINE_INAPPLICABLE && !result.made_rule_progress);
    --step.affine_matrix.maximum_retained_bytes;
    assert(capi::tactix_online_configure_v16(s,&program,943,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
    assert(capi::tactix_online_advance(s,&result)==0);
    assert(result.status==TACTIX_ONLINE_COMPLETED && result.made_rule_progress);
    assert(capi::tactix_apply(s,propagate)>=0);
    assert(kernel_propagation_fixpoint(api_detail::kernel_for_fixed_testing(s)));
    auto &kernel=api_detail::kernel_for_fixed_testing(s);
    assert(kernel_external_value(kernel,internalize(4))<0);
  }
}
// The request and retry use the actual C16 configure/advance surface. Private
// introspection only snapshots native state and injects one fixed stop case.
static void c_decline_retry(unsigned mode) {
  CSolver c;
  auto *solver = c.s;
  FILE *records = c.records;
  const char *path = c.path;
  c.proof();
  c.add_xors({1, 2, 3}, {2, 3, 4}, false);
  auto bounded = profile();
  const bool legacy = mode == 4;
  if (mode == 0 || legacy) bounded.basis.maximum_clauses = 2;
  if (mode == 1) bounded.basis.maximum_snapshot_work = 1;
  if (mode == 2) bounded.maximum_retained_bytes = 1;
  assert(bounded.valid());
  auto step = persistent_step(bounded);
  auto &base = step_base(step);
  if (legacy) {
    step.affine_matrix_profile = TACTIX_AFFINE_MATRIX_PROFILE_LEGACY;
    step.affine_matrix = {};
    base.parameters.kind = TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE;
    base.parameters.value.affine_consequence = c_basis(bounded.basis);
  }
  capi::tactix_online_program_v16 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(capi::tactix_online_configure_v16(solver, &program, 1901, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  auto &kernel = api_detail::kernel_for_fixed_testing(solver);
  auto &session = *kernel.affine_matrix_search_session;
  const auto arena = kernel.arena;
  const auto watches = kernel.watches.arena;
  const std::vector<Lit> trail(kernel.trail.begin(), kernel.trail.end());
  const auto values = kernel.values;
  const auto next_id = kernel.next_clause_id;
  const auto premises = kernel.proof->premise_bytes();
  const auto premise_epoch = kernel.proof->premise_epoch();
  const auto exhausted = kernel.stats.affine_matrix_init_exhausted;
  const auto interrupted = kernel.stats.affine_matrix_init_interrupted;
  kernel.proof->flush();
  const auto proof_before = read_file(path);
  const auto records_before = std::ftell(records);
  if (mode == 3) kernel.stop_fuzz = 1; // Every native safe-boundary poll stops.
  capi::tactix_online_result result{};
  assert(capi::tactix_online_advance(solver, &result) == 0);
  assert(result.status == (legacy ? TACTIX_ONLINE_NO_CHANGE :
      mode == 3 ? TACTIX_ONLINE_INTERRUPTED : TACTIX_ONLINE_RESOURCE_EXHAUSTED));
  assert(!result.made_rule_progress && bool(result.selection_complete) == legacy);
  assert(result.applications == 1 && result.work == 1);
  assert(capi::tactix_online_step_consumed(solver, 941) == 1);
  assert(kernel.stats.affine_matrix_init_exhausted == exhausted + (mode != 3));
  assert(kernel.stats.affine_matrix_init_interrupted == interrupted + (mode == 3));
  if (mode == 3) assert(kernel.stats.stop_fuzz_fires > 0);
  assert(kernel.arena == arena && kernel.watches.arena == watches);
  assert(kernel.trail == trail && kernel.values == values && kernel.next_clause_id == next_id);
  assert(kernel.proof->premise_bytes() == premises && kernel.proof->premise_epoch() == premise_epoch);
  assert(!session.enabled() && session.owner_version() == 0 && session.retained_bytes() == 0);
  kernel.proof->flush();
  assert(read_file(path) == proof_before && std::ftell(records) == records_before);

  kernel.stop_fuzz = 0;
  const auto fresh = profile();
  step.affine_matrix = {c_basis(fresh.basis), 1, 1, fresh.maximum_retained_bytes,
      fresh.maximum_deferred_deletions, fresh.maximum_deferred_literal_work};
  if (legacy) {
    step.affine_matrix = {};
    base.parameters.value.affine_consequence = c_basis(fresh.basis);
  }
  assert(capi::tactix_online_configure_v16(solver, &program, 1902, TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  assert(capi::tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED && result.made_rule_progress);
  assert(session.enabled() && session.persistent() == !legacy && session.active_sources() == 2);
}


// Exact admitted limits are reused for the service request; tightening a
// different profile would correctly decline at the public guard instead.
static void c_service_continuation(bool typed_replay) {
  CSolver c;
  auto *solver = c.s;
  FILE *records = c.records;
  const char *path = c.path;
  c.proof();
  c.add_xors({1, 2, 3}, {2, 3, 4}, true);
  c.add({5, 6, 7});
  auto p = profile();
  p.basis.basis_maximum_work = 100;
  auto step = persistent_step(p);
  auto &base = step_base(step);
  capi::tactix_online_program_v16 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(capi::tactix_online_configure_v16(solver, &program, 2001,
      TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  capi::tactix_online_result result{};
  assert(capi::tactix_online_advance(solver, &result) == 0);
  assert(result.status == TACTIX_ONLINE_COMPLETED && result.made_rule_progress);
  auto &kernel = api_detail::kernel_for_fixed_testing(solver);
  auto &session = *kernel.affine_matrix_search_session;
  const auto owner = session.owner_version();
  const auto id = kernel.next_clause_id;
  const auto arena = kernel.arena;
  const auto watches = kernel.watches.arena;
  const auto values = kernel.values;
  const auto premises = kernel.proof->premise_bytes();
  const auto epoch = kernel.proof->premise_epoch();
  kernel.proof->flush();
  const auto proof_before = read_file(path);
  const auto journal_before = std::ftell(records);
  step.affine_matrix.operation = TACTIX_AFFINE_MATRIX_SEARCH_STEP;
  base.budget = 64;
  OnlineStrategySequence replay;
  if (typed_replay) {
    p.operation = AffineMatrixInitializeParameters::Operation::SearchStep;
    OnlineStrategyProgram exact;
    exact.sequence = {invocation(p)};
    exact.sequence[0].budget = WorkLimit::bounded(64);
    assert(replay.configure(exact, 2002));
  } else {
    assert(capi::tactix_online_configure_v16(solver, &program, 2002,
        TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  }
  unsigned applications = 0, pending = 0;
  bool complete = false;
  for (unsigned dose = 0; dose < 64 && !complete; ++dose) {
    int status, budget_status;
    bool selection_complete;
    if (typed_replay) {
      const auto out = replay.advance(kernel);
      status = static_cast<int>(out.status);
      budget_status = static_cast<int>(out.budget_status);
      selection_complete = out.selection_complete;
      applications += out.applications;
    } else {
      assert(capi::tactix_online_advance(solver, &result) == 0);
      status = result.status; budget_status = result.budget_status;
      selection_complete = result.selection_complete;
      applications += result.applications;
    }
    assert(!(status == TACTIX_ONLINE_BUDGET_EXHAUSTED &&
             budget_status == static_cast<int>(BudgetStatus::Ready)));
    assert(status == TACTIX_ONLINE_COMPLETED || status == TACTIX_ONLINE_NO_CHANGE);
    assert(session.enabled() && session.owner_version() == owner);
    assert(kernel.next_clause_id == id && kernel.trail.empty());
    assert(kernel.arena == arena && kernel.watches.arena == watches && kernel.values == values);
    assert(kernel.proof->premise_bytes() == premises && kernel.proof->premise_epoch() == epoch);
    kernel.proof->flush();
    assert(read_file(path) == proof_before && std::ftell(records) == journal_before);
    complete = selection_complete;
    if (!dose) assert(!complete);
    if (!complete) ++pending;
  }
  assert(complete && pending > 0 && applications > 1);
  assert(kernel.stats.affine_matrix_service_pending_slices > 0);
  assert(kernel.stats.affine_matrix_service_resumed_slices > 0);
}

// N2: a root implied by the input is known whichever way the proof arrived, so the
// same limit gives the same outcome on every route. The first capture runs before
// any collection, so these roots count against the clause limit.
enum class ProofRoute { None, Early, Late };
static int routed_status(ProofRoute route, uint64_t maximum_clauses) {
  CSolver solver;
  if (route == ProofRoute::Early) solver.proof(TACTIX_PROOF_AUTO);
  solver.add({-1, 2});
  solver.add({1});
  for (const auto &clause : affine_semantic_extension::clausal_form({3, 4, 5}, false)) solver.add(clause);
  for (const auto &clause : affine_semantic_extension::clausal_form({4, 5, 6}, true)) solver.add(clause);
  if (route == ProofRoute::Late) solver.proof(TACTIX_PROOF_AUTO);
  auto p = profile();
  p.basis.maximum_clauses = maximum_clauses;
  auto step = persistent_step(p);
  capi::tactix_online_program_v16 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(capi::tactix_online_configure_v16(solver.s, &program, 941,
                                           TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  capi::tactix_online_result result{};
  assert(capi::tactix_online_advance(solver.s, &result) == 0);
  return result.status;
}
static void every_route_counts_the_same_roots() {
  std::set<int> outcomes;
  for(uint64_t limit=1;limit<=16;++limit) {
    const auto none=routed_status(ProofRoute::None,limit);
    assert(routed_status(ProofRoute::Early,limit)==none);
    assert(routed_status(ProofRoute::Late,limit)==none);
    outcomes.insert(none);
  }
  assert(outcomes.size()>1); // the limit must have mattered
}

int main() {
  replay_and_scope();
  every_route_counts_the_same_roots();
  c_surface(true);
  c_surface(false);
  c_service_continuation(false);
  c_service_continuation(true);
  for (unsigned mode = 0; mode < 5; ++mode) c_decline_retry(mode);
}

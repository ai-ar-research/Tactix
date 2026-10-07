#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstring>
#include <unistd.h>
namespace capi {
extern "C" {
#include "tactix.h"
}
}
#include "policies/default_policy.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "strategy/parameter_execution.hpp"
#include "strategy/random_online.hpp"
#include "api/api_internal.hpp"
#include <fstream>
#include <iterator>
using namespace tactix;
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
  for (auto op:{AffineMatrixInitializeParameters::Operation::Initialize,
       AffineMatrixInitializeParameters::Operation::Refresh,
       AffineMatrixInitializeParameters::Operation::RootMaintenance,
       AffineMatrixInitializeParameters::Operation::SearchStep}) {
    p.operation=op;program.sequence[0].parameters=p;
    const auto wire=serialize_online_strategy_program(program);
    assert(wire.find(":affine-matrix-persistent1(")!=std::string::npos);
    OnlineStrategyProgram decoded;std::string error;
    assert(deserialize_online_strategy_program(wire,decoded,&error));
    assert(serialize_online_strategy_program(decoded)==wire);
    assert(std::get<AffineMatrixInitializeParameters>(decoded.sequence[0].parameters)==p);
  }
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
static void c_surface(bool with_proof) {
  auto *s=capi::tactix_init();assert(s);FILE *records=std::tmpfile();assert(records);
  assert(capi::tactix_bind_structural_provenance(s,records)==0);
  char path[]="/tmp/tactix-persistent-public-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
  if(with_proof)assert(capi::tactix_proof(s,path,TACTIX_PROOF_DSR_ASCII)==0);
  for(auto variables:{std::vector<int32_t>{1,2,3},std::vector<int32_t>{2,3,4}})
    for(auto clause:affine_semantic_extension::clausal_form(variables,variables.front()==2)) {
      for(auto lit:clause)capi::tactix_add(s,lit);
      capi::tactix_add(s,0);
    }
  auto p=profile();capi::tactix_online_step_v16 step{};
  auto &base=step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base;
  base.step_id=941;base.rule_id=41;base.granularity=TACTIX_ONLINE_GRANULARITY_CANONICAL;
  base.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;base.repeat_limit=1;
  base.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;base.budget=1;
  step.affine_matrix_profile=TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
  step.affine_matrix={c_basis(p.basis),1,1,p.maximum_retained_bytes,p.maximum_deferred_deletions,p.maximum_deferred_literal_work};
  capi::tactix_online_program_v16 program{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  auto valid=step.affine_matrix;step.affine_matrix.maximum_deferred_deletions=0;
  assert(capi::tactix_online_configure_v16(s,&program,940,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
  step.affine_matrix=valid;
  assert(capi::tactix_online_configure_v16(s,&program,941,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  const auto saved_base=base;
  step={}; // Configuration owns an exact deep copy of the profile.
  capi::tactix_online_result result{};assert(capi::tactix_online_advance(s,&result)==0);
  assert(result.status==(with_proof?TACTIX_ONLINE_COMPLETED:TACTIX_ONLINE_INAPPLICABLE));
  if(with_proof) {
    int propagate=-1;
    for(int i=0;i<capi::tactix_rule_count();++i)
      if(!std::strcmp(capi::tactix_rule_name(i),"Propagate"))propagate=i;
    assert(propagate>=0 && capi::tactix_decide(s,1)==0);
    assert(capi::tactix_apply(s,propagate)>=0);
    step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base=saved_base;
    step.affine_matrix_profile=TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
    step.affine_matrix=valid;step.affine_matrix.operation=TACTIX_AFFINE_MATRIX_SEARCH_STEP;
    // The explicit service is legal above root and installs a conditional
    // ordinary watched reason. It must not be blocked by an initializer guard.
    ++step.affine_matrix.maximum_retained_bytes;
    assert(capi::tactix_online_configure_v16(s,&program,942,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
    assert(capi::tactix_online_advance(s,&result)==0);
    assert(result.status==TACTIX_ONLINE_INAPPLICABLE && !result.made_rule_progress);
    --step.affine_matrix.maximum_retained_bytes;
    assert(capi::tactix_online_configure_v16(s,&program,943,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
    assert(capi::tactix_online_advance(s,&result)==0);
    assert(result.status==TACTIX_ONLINE_COMPLETED && result.made_rule_progress);
    assert(capi::tactix_apply(s,propagate)>=0);
    assert(capi::tactix_value(s,4)==-4);
  }
  capi::tactix_release(s);std::fclose(records);std::remove(path);
}
static std::string file_bytes(const char *path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The request and retry use the actual C16 configure/advance surface. Private
// introspection only snapshots native state and injects one fixed stop case.
static void c_decline_retry(unsigned mode) {
  auto *solver = capi::tactix_init();
  assert(solver);
  FILE *records = std::tmpfile();
  assert(records);
  assert(capi::tactix_bind_structural_provenance(solver, records) == 0);
  char path[] = "/tmp/tactix-c16-status-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  assert(capi::tactix_proof(solver, path, TACTIX_PROOF_DSR_ASCII) == 0);
  for (const auto &variables : {std::vector<int32_t>{1,2,3},
                                std::vector<int32_t>{2,3,4}})
    for (const auto &clause : affine_semantic_extension::clausal_form(variables, false)) {
      for (int32_t literal : clause) capi::tactix_add(solver, literal);
      capi::tactix_add(solver, 0);
    }
  auto bounded = profile();
  const bool legacy = mode == 4;
  if (mode == 0 || legacy) bounded.basis.maximum_clauses = 2;
  if (mode == 1) bounded.basis.maximum_snapshot_work = 1;
  if (mode == 2) bounded.maximum_retained_bytes = 1;
  assert(bounded.valid());
  OnlineStrategyProgram typed;
  typed.sequence = {invocation(bounded)};
  const auto wire = serialize_online_strategy_program(typed);
  OnlineStrategyProgram decoded;
  assert(deserialize_online_strategy_program(wire, decoded));
  assert(serialize_online_strategy_program(decoded) == wire);
  assert(std::get<AffineMatrixInitializeParameters>(decoded.sequence[0].parameters) == bounded);

  capi::tactix_online_step_v16 step{};
  auto &base = step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base;
  base.step_id = 941;
  base.rule_id = 41;
  base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  step.affine_matrix_profile = TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
  step.affine_matrix = {c_basis(bounded.basis), 1, 1, bounded.maximum_retained_bytes,
      bounded.maximum_deferred_deletions, bounded.maximum_deferred_literal_work};
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
  const auto proof_before = file_bytes(path);
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
  assert(file_bytes(path) == proof_before && std::ftell(records) == records_before);

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
  capi::tactix_release(solver);
  std::fclose(records);
  std::remove(path);
}


// Exact admitted limits are reused for the service request; tightening a
// different profile would correctly decline at the public guard instead.
static void c_service_resource() {
  auto *solver = capi::tactix_init();
  assert(solver);
  FILE *records = std::tmpfile();
  assert(records);
  assert(capi::tactix_bind_structural_provenance(solver, records) == 0);
  char path[] = "/tmp/tactix-c16-service-resource-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  assert(capi::tactix_proof(solver, path, TACTIX_PROOF_DSR_ASCII) == 0);
  for (const auto &variables : {std::vector<int32_t>{1,2,3},
                                std::vector<int32_t>{2,3,4}})
    for (const auto &clause : affine_semantic_extension::clausal_form(
             variables, variables.front() == 2)) {
      for (int32_t literal : clause) capi::tactix_add(solver, literal);
      capi::tactix_add(solver, 0);
    }
  for (int32_t literal : {5,6,7}) capi::tactix_add(solver, literal);
  capi::tactix_add(solver, 0);
  auto p = profile();
  p.basis.basis_maximum_work = 100;
  capi::tactix_online_step_v16 step{};
  auto &base = step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base;
  base.step_id = 941;
  base.rule_id = 41;
  base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  step.affine_matrix_profile = TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1;
  step.affine_matrix = {c_basis(p.basis), 1, 1, p.maximum_retained_bytes,
      p.maximum_deferred_deletions, p.maximum_deferred_literal_work};
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
  const auto proof_before = file_bytes(path);
  const auto journal_before = std::ftell(records);
  step.affine_matrix.operation = TACTIX_AFFINE_MATRIX_SEARCH_STEP;
  for (uint64_t revision : {2002,2003}) {
    assert(capi::tactix_online_configure_v16(solver, &program, revision,
        TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
    assert(capi::tactix_online_advance(solver, &result) == 0);
    assert(result.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED);
    assert(!result.made_rule_progress && !result.selection_complete);
    assert(session.enabled() && session.owner_version() == owner);
    assert(kernel.next_clause_id == id && kernel.trail.empty());
    assert(kernel.arena == arena && kernel.watches.arena == watches && kernel.values == values);
    assert(kernel.proof->premise_bytes() == premises && kernel.proof->premise_epoch() == epoch);
    kernel.proof->flush();
    assert(file_bytes(path) == proof_before && std::ftell(records) == journal_before);
  }
  capi::tactix_release(solver);
  std::fclose(records);
  std::remove(path);
}

int main() {
  replay_and_scope();
  c_surface(true);
  c_surface(false);
  c_service_resource();
  for (unsigned mode = 0; mode < 5; ++mode) c_decline_retry(mode);
}

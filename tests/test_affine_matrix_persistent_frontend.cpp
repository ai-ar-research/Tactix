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
int main() {replay_and_scope();c_surface(true);c_surface(false);}

#include "frontend/dsr_follow_on.hpp"

#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"
#include "strategy/structural_action_sessions.hpp"

namespace tactix {

DsrFollowOn run_dsr_follow_on(const Kernel &refuted, Proof &proof,
                              DefaultStrategyConfig config,
                              const std::function<void(Kernel &)> &pose,
                              volatile sig_atomic_t *stop) {
  const uint64_t started = now_nanos();
  Kernel kernel;
  StructuralActionSessions sessions(kernel);
  pose(kernel);
  kernel.structural_provenance = StructuralJournal::discard();
  Proof follow_on;
  kernel.startup_certificate_open = true;
  if (!proof.open_dsr_follow_on(follow_on))
    proof_error("cannot open the follow-on DSR proof");
  kernel.proof = &follow_on;
  follow_on.bind_variable_map(&kernel.internal_to_external);
  follow_on.bind_external_variable_map(&kernel.external_to_internal);
  if (refuted.input_header.declared)
    declare_input(kernel, refuted.input_header.variables,
                  refuted.input_header.clauses);
  ingest_dimacs(kernel, proof.follow_on_input());
  config.startup_count.enabled = false;
  DefaultPolicy policy(config);
  DsrFollowOn out;
  out.result = policy.solve(kernel, stop);
  follow_on.flush();
  if (out.result == 20 && !proof.settle_dsr_follow_on(follow_on))
    proof_error("the follow-on search wrote no complete DSR proof");
  out.conflicts = kernel.stats.conflicts;
  out.decisions = kernel.stats.decisions;
  out.propagations = kernel.stats.propagations;
  out.sr = kernel.stats.sr_applications;
  out.nanos = now_nanos() - started;
  return out;
}

} // namespace tactix

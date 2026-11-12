#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "gf3_formula_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "proof/proof.hpp"
#include "strategy/online_inprocess.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;

namespace {

Kernel install() { return gf3_native_kernel(gf3_chained_units(), 9); }

Gf3GaussianActionLimits limits(const Gf3GaussianConsequenceParameters &p = {}) {
  return gf3_gaussian_action_limits(p);
}

struct Bound {
  Kernel kernel = install();
  Gf3GaussianActionSession session;
  FILE *records = std::tmpfile();
  Bound() {
    assert(records);
    kernel.structural_provenance = records;
    kernel.gf3_gaussian_action_session = &session;
  }
  ~Bound() { std::fclose(records); }
};

void singular_batch_and_atomic_publication() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.status == Gf3GaussianActionStatus::Applied);
  assert(offered.offers.size() == 1);
  assert(offered.offers[0].rfind("g3b1:", 0) == 0);
  assert(offered.live.effort.identity_bytes >= offered.offers[0].size());
  assert(offered.live.effort.identity_work ==
         offered.live.effort.identity_bytes);
  const auto applied = gf3_gaussian_action_apply(
      b.kernel, b.session, limits(), offered.offers[0]);
  assert(applied.status == Gf3GaussianActionStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.live.published_units.size() >= 3);
  assert(b.session.keys() == offered.offers);
  assert(b.kernel.stats.gf3_gaussian_attempts == 0);
  record_gf3_gaussian_action_feedback(b.kernel, applied);
  assert(b.kernel.stats.gf3_gaussian_applied == 1);
  // All consequences in the authenticated closure were consumed in the one
  // declared action before its root units closed the assignment-free boundary.
  assert(!b.kernel.trail.empty());
}

void corruption_rebinding_and_limits_decline_without_mutation() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.offers.size() == 1);
  const auto before_id = b.kernel.next_clause_id;
  std::string corrupt = offered.offers[0];
  corrupt.back() = corrupt.back() == '0' ? '1' : '0';
  assert(gf3_gaussian_action_apply(b.kernel, b.session, limits(), corrupt)
             .status == Gf3GaussianActionStatus::Stale);
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty() &&
         b.session.keys().empty());

  Gf3GaussianConsequenceParameters small;
  small.maximum_identity_bytes = 1;
  assert(gf3_gaussian_action_offer(b.kernel, b.session, limits(small)).status ==
         Gf3GaussianActionStatus::ResourceExhausted);
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty());

  Gf3GaussianConsequenceParameters small_session;
  small_session.maximum_session_identity_bytes = offered.offers[0].size() - 1;
  const auto session_limited = gf3_gaussian_action_offer(
      b.kernel, b.session, limits(small_session));
  assert(session_limited.status ==
             Gf3GaussianActionStatus::ResourceExhausted &&
         session_limited.candidates == 1 && !session_limited.complete &&
         session_limited.offers.empty());
  assert(b.kernel.next_clause_id == before_id && b.kernel.trail.empty());

  Kernel other = install();
  other.structural_provenance = b.records;
  Gf3GaussianActionSession foreign;
  // Bind foreign to the first kernel through a successful application.
  assert(gf3_gaussian_action_apply(b.kernel, foreign, limits(),
                                   offered.offers[0]).status ==
         Gf3GaussianActionStatus::Applied);
  assert(gf3_gaussian_action_offer(other, foreign, limits()).status ==
         Gf3GaussianActionStatus::Stale);
}

void public_wrapper_applies_with_dsr() {
  Bound b;
  const auto offered = gf3_gaussian_action_offer(b.kernel, b.session, limits());
  assert(offered.offers.size() == 1);
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&b.kernel.internal_to_external);
  proof.bind_external_variable_map(&b.kernel.external_to_internal);
  b.kernel.proof = &proof;
  const auto applied = gf3_gaussian_action_apply(
      b.kernel, b.session, limits(), offered.offers.front());
  assert(applied.status == Gf3GaussianActionStatus::Applied &&
         applied.result == ApplyResult::Ok);
  proof.flush();
  b.kernel.proof = nullptr;
  assert(!read_file(file.path).empty());
}

void c_v6_replays_the_exact_batch_identity() {
  Bound source;
  source.kernel = gf3_parsed_kernel(gf3_chained_units());
  source.kernel.structural_provenance = source.records;
  source.kernel.gf3_gaussian_action_session = &source.session;
  const auto source_offer =
      gf3_gaussian_action_offer(source.kernel, source.session, limits());
  assert(source_offer.status == Gf3GaussianActionStatus::Applied &&
         source_offer.offers.size() == 1);
  const std::string identity = source_offer.offers.front();
  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  for (const auto &clause : gf3_chained_units().clauses) {
    for (int32_t literal : clause) tactix_add(solver, literal);
    tactix_add(solver, 0);
  }
  const Gf3GaussianConsequenceParameters p;
  tactix_online_step_v6 step{};
  auto &base = step.base.base.base.base.base;
  base.step_id = 26;
  base.rule_id = 37;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_GF3_GAUSSIAN_CONSEQUENCE;
  auto &c = base.parameters.value.gf3_gaussian_consequence;
  c = gf3_c_gaussian(p);
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  const tactix_online_gf3_gaussian_instance selected{identity.c_str()};
  step.gf3_gaussian_instances = &selected;
  step.gf3_gaussian_instance_count = 1;
  const tactix_online_program_v6 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  const uint64_t maximum_identity_bytes = c.maximum_identity_bytes;
  c.maximum_identity_bytes = identity.size() - 1;
  assert(tactix_online_configure_v6(solver, &program, 87,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  c.maximum_identity_bytes = maximum_identity_bytes;
  assert(tactix_online_configure_v6(solver, &program, 88,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_result result{};
  assert(tactix_online_advance(solver, &result) == 0);
  assert(result.rule_id == 37 && result.status == TACTIX_ONLINE_COMPLETED);
  tactix_release(solver);
  std::fclose(records);
}

} // namespace

int main() {
  singular_batch_and_atomic_publication();
  corruption_rebinding_and_limits_decline_without_mutation();
  public_wrapper_applies_with_dsr();
  c_v6_replays_the_exact_batch_identity();
  std::puts("test_gf3_gaussian_public: passed");
}

#undef NDEBUG
#include "rules/direct_php/direct_php_action.hpp"

#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <limits>
#include <unistd.h>
#include <vector>

using namespace tactix;

namespace {

Kernel fixture(uint32_t holes = 3) {
  assert(holes >= 2);
  const uint32_t pigeons = holes + 1;
  Kernel kernel;
  kernel_reserve_vars(kernel, pigeons * holes);
  const auto variable = [holes](uint32_t pigeon, uint32_t hole) {
    return int32_t(pigeon * holes + hole + 1);
  };
  for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<Lit> alo;
    for (uint32_t hole = 0; hole < holes; ++hole)
      alo.push_back(internalize(variable(pigeon, hole)));
    if (alo.size() == 2)
      kernel_add_binary_clause(kernel, alo[0], alo[1],
                               ++kernel.next_clause_id);
    else
      kernel_add_large_clause(kernel, alo.data(), alo.size(), false, 0,
                              ++kernel.next_clause_id);
  }
  for (uint32_t hole = 0; hole < holes; ++hole)
    for (uint32_t first = 0; first < pigeons; ++first)
      for (uint32_t second = first + 1; second < pigeons; ++second)
        kernel_add_binary_clause(
            kernel, internalize(-variable(first, hole)),
            internalize(-variable(second, hole)), ++kernel.next_clause_id);
  return kernel;
}

DirectPhpActionRequest request(uint64_t id = 37) {
  DirectPhpActionRequest request;
  request.action_id = id;
  request.limits.snapshot = {100000, 1000000, 10000000};
  request.limits.recognition = {100000, 1000000, 100000000, 10000,
                                1000000, 1000000, 20};
  request.limits.certificate = {1000000, 1000000, 100000000, 1000000,
                                2000000};
  request.limits.maximum_replay_work = 100000000;
  assert(request.limits.valid());
  return request;
}

void preparation_is_pure_and_accounted() {
  Kernel kernel = fixture();
  const uint64_t next_id = kernel.next_clause_id;
  const uint32_t variables = kernel.nvars;
  const size_t extent = kernel.external_to_internal.size();
  DirectPhpActionSession session;
  DirectPhpPreparedAction prepared;
  const auto result =
      direct_php_action_prepare(kernel, session, request(), prepared);
  assert(result.status == DirectPhpActionStatus::Prepared);
  assert(result.result == ApplyResult::NoOp && result.action_id == 37);
  assert(!result.replay_checked && prepared.ready());
  assert(!session.committed() && !kernel.inconsistent);
  assert(kernel.next_clause_id == next_id && kernel.nvars == variables &&
         kernel.external_to_internal.size() == extent);
  assert(result.effort.snapshot_clauses == 22);
  assert(result.effort.snapshot_literals == 48);
  assert(result.effort.snapshot_work && result.effort.recognition_work &&
         result.effort.replay_work);

  const auto &record = prepared.record();
  assert(record.version == 1 && record.action_id == 37);
  assert(record.snapshot.sources.size() == 22);
  assert(record.events.size() == record.accounting.events);
  assert(record.accounting == result.proof);
  assert(record.first_fresh == 13);

  uint64_t additions = 0, rup = 0, rat = 0, deletions = 0;
  uint64_t addition_literals = 0, deletion_literals = 0;
  uint64_t maximum_width = 0;
  for (const auto &event : record.events) {
    maximum_width = std::max<uint64_t>(maximum_width, event.clause.size());
    if (event.kind == direct_php_certificate::EventKind::Delete) {
      ++deletions;
      deletion_literals += event.clause.size();
      assert(event.deleted_id && !event.id);
    } else {
      ++additions;
      addition_literals += event.clause.size();
      assert(event.id && !event.deleted_id && !event.source_occurrence);
      if (event.kind == direct_php_certificate::EventKind::AddRup)
        ++rup;
      else
        ++rat;
    }
  }
  assert(additions == result.proof.additions &&
         rup == result.proof.rup_additions &&
         rat == result.proof.rat_additions &&
         deletions == result.proof.deletions &&
         addition_literals == result.proof.addition_literals &&
         deletion_literals == result.proof.deletion_literals &&
         maximum_width == result.proof.maximum_event_width);
}

void apply_commits_exact_session_once() {
  Kernel kernel = fixture();
  DirectPhpActionSession session;
  const auto applied = direct_php_action_apply(kernel, session, request());
  assert(applied.status == DirectPhpActionStatus::Applied);
  assert(applied.result == ApplyResult::Unsat && kernel.inconsistent);
  assert(session.committed() && session.belongs_to(kernel));
  assert(session.action_id() == 37 && session.record());
  assert(session.record()->accounting == applied.proof);

  const uint64_t next_id = kernel.next_clause_id;
  const size_t extent = kernel.external_to_internal.size();
  const auto repeated = direct_php_action_apply(kernel, session, request());
  assert(repeated.status == DirectPhpActionStatus::AlreadyApplied);
  assert(repeated.result == ApplyResult::NoOp);
  assert(kernel.next_clause_id == next_id &&
         kernel.external_to_internal.size() == extent);

  Kernel other = fixture();
  DirectPhpPreparedAction prepared;
  const auto stale =
      direct_php_action_prepare(other, session, request(), prepared);
  assert(stale.status == DirectPhpActionStatus::Stale && !prepared.ready());
}

DirectPhpActionRecord record_for(const DirectPhpActionRequest &request) {
  Kernel kernel = fixture();
  DirectPhpActionSession session;
  DirectPhpPreparedAction prepared;
  const auto result =
      direct_php_action_prepare(kernel, session, request, prepared);
  assert(result.status == DirectPhpActionStatus::Prepared);
  return prepared.record();
}

void exact_replay_accepts_only_exact_record() {
  const auto requested = request(91);
  const DirectPhpActionRecord exact = record_for(requested);
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    const auto replayed =
        direct_php_action_replay(kernel, session, requested, exact);
    assert(replayed.status == DirectPhpActionStatus::Applied);
    assert(replayed.result == ApplyResult::Unsat && replayed.replay_checked);
    assert(session.record() &&
           session.record()->events.size() == exact.events.size());
  }
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    auto wrong_request = requested;
    ++wrong_request.action_id;
    const auto mismatch = direct_php_action_prepare_replay(
        kernel, session, wrong_request, exact, prepared);
    assert(mismatch.status == DirectPhpActionStatus::ReplayMismatch);
    assert(mismatch.replay_checked && !prepared.ready() &&
           !kernel.inconsistent);
  }
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    auto changed = exact;
    assert(!changed.events.empty());
    changed.events.front().layer ^= 1;
    const auto mismatch = direct_php_action_prepare_replay(
        kernel, session, requested, changed, prepared);
    assert(mismatch.status == DirectPhpActionStatus::ReplayMismatch);
    assert(mismatch.replay_checked && !prepared.ready() &&
           !kernel.inconsistent);
  }
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    auto changed = exact;
    changed.snapshot.sources.front().native_proof_id =
        std::numeric_limits<uint64_t>::max();
    const auto mismatch = direct_php_action_prepare_replay(
        kernel, session, requested, changed, prepared);
    assert(mismatch.status == DirectPhpActionStatus::ReplayMismatch);
    assert(!prepared.ready() && !kernel.inconsistent);
  }
}

void replay_budget_and_interruption_are_atomic() {
  auto requested = request(101);
  DirectPhpActionRecord exact = record_for(requested);
  Kernel probe = fixture();
  DirectPhpActionSession probe_session;
  DirectPhpPreparedAction probe_prepared;
  const auto measured = direct_php_action_prepare(
      probe, probe_session, requested, probe_prepared);
  assert(measured.status == DirectPhpActionStatus::Prepared &&
         measured.effort.replay_work);

  // Exact replay performs one additional metered comparison after rebuilding
  // the record.  Giving it exactly the construction cost must decline before
  // any terminal publication.
  requested.limits.maximum_replay_work = measured.effort.replay_work;
  exact.limits.maximum_replay_work = measured.effort.replay_work;
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    const auto exhausted = direct_php_action_prepare_replay(
        kernel, session, requested, exact, prepared);
    assert(exhausted.status == DirectPhpActionStatus::ResourceExhausted);
    assert(exhausted.replay_checked && !prepared.ready() &&
           !session.committed() && !kernel.inconsistent);
    assert(exhausted.effort.replay_work ==
           requested.limits.maximum_replay_work);
    assert(exhausted.proof == exact.accounting);
  }
  {
    auto ordinary = request(102);
    Kernel kernel = fixture();
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    DirectPhpActionSession session;
    const auto interrupted =
        direct_php_action_apply(kernel, session, ordinary);
    assert(interrupted.status == DirectPhpActionStatus::Interrupted);
    assert(!session.committed() && !kernel.inconsistent);
  }
}

void stale_and_invalid_paths_do_not_leak_preparation() {
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    assert(direct_php_action_prepare(kernel, session, request(), prepared)
               .status == DirectPhpActionStatus::Prepared);
    ++kernel.next_clause_id;
    const auto stale = direct_php_action_publish(kernel, session, prepared);
    assert(stale.status == DirectPhpActionStatus::Stale);
    assert(!session.committed() && !kernel.inconsistent);
  }
  {
    Kernel kernel = fixture();
    DirectPhpActionSession first, wrong;
    DirectPhpPreparedAction prepared;
    assert(direct_php_action_prepare(kernel, first, request(), prepared)
               .status == DirectPhpActionStatus::Prepared);
    const auto stale = direct_php_action_publish(kernel, wrong, prepared);
    assert(stale.status == DirectPhpActionStatus::Stale);
    assert(!first.committed() && !wrong.committed() && !kernel.inconsistent);
  }
  {
    Kernel kernel = fixture();
    DirectPhpActionSession session;
    DirectPhpPreparedAction prepared;
    assert(direct_php_action_prepare(kernel, session, request(), prepared)
               .status == DirectPhpActionStatus::Prepared);
    auto invalid = request();
    invalid.action_id = 0;
    const auto declined =
        direct_php_action_prepare(kernel, session, invalid, prepared);
    assert(declined.status == DirectPhpActionStatus::Invalid);
    assert(!prepared.ready() && !kernel.inconsistent);
  }
  {
    Kernel kernel = fixture();
    Proof proof;
    char path[] = "/tmp/direct-php-action-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    assert(proof.open(path, ProofFormat::Lrat));
    kernel.proof = &proof;
    DirectPhpActionSession session;
    const auto unsupported =
        direct_php_action_apply(kernel, session, request());
    assert(unsupported.status == DirectPhpActionStatus::UnsupportedProof);
    kernel.proof = nullptr;
    unlink(path);
  }
}

void guard_and_recognition_declines_are_typed() {
  Kernel kernel = fixture();
  assert(direct_php_action_guard(kernel));
  OccIndex dense;
  kernel.dense = &dense;
  assert(!direct_php_action_guard(kernel));
  DirectPhpActionSession session;
  assert(direct_php_action_apply(kernel, session, request()).status ==
         DirectPhpActionStatus::Inapplicable);

  Kernel unrelated;
  kernel_reserve_vars(unrelated, 3);
  const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(unrelated, clause, 3, false, 0,
                          ++unrelated.next_clause_id);
  DirectPhpActionSession unrelated_session;
  const auto no_structure =
      direct_php_action_apply(unrelated, unrelated_session, request());
  assert(no_structure.status == DirectPhpActionStatus::NotRecognized);
  assert(!unrelated_session.committed() && !unrelated.inconsistent);
}

} // namespace

int main() {
  preparation_is_pure_and_accounted();
  apply_commits_exact_session_once();
  exact_replay_accepts_only_exact_record();
  replay_budget_and_interruption_are_atomic();
  stale_and_invalid_paths_do_not_leak_preparation();
  guard_and_recognition_declines_are_typed();
  std::puts("test_direct_php_action: passed");
}

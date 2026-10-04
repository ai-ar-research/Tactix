#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "policies/restart_controller.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/parity_certificate.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

struct Equation {
  bool rhs;
  std::vector<int32_t> variables;
};

const std::vector<Equation> triangle = {
    {true, {1, 3}}, {false, {1, 2}}, {false, {2, 3}}};
const std::vector<Equation> wide_cycle = {
    {true, {1, 2, 3}}, {false, {3, 4, 5}},
    {false, {1, 5, 6}}, {false, {2, 4, 6}}};

void add_equations(Kernel &kernel, const std::vector<Equation> &equations,
                   int32_t offset = 0, bool reverse = false) {
  std::vector<std::vector<int32_t>> encoded_clauses;
  for (const auto &equation : equations) {
    std::vector<int32_t> variables = equation.variables;
    for (auto &variable : variables) variable += offset;
    const auto clauses = parity_certificate::xor_equation(
        variables.data(), variables.size(), equation.rhs);
    for (const auto &clause : clauses)
      encoded_clauses.emplace_back(clause.lits.begin(),
                                   clause.lits.begin() + clause.size);
  }
  if (reverse) std::reverse(encoded_clauses.begin(), encoded_clauses.end());
  for (auto clause : encoded_clauses) {
    if (reverse) std::reverse(clause.begin(), clause.end());
    if (clause.size() == 2) {
      kernel_add_binary_clause(kernel, internalize(clause[0]),
                               internalize(clause[1]),
                               ++kernel.next_clause_id);
    } else {
      std::vector<Lit> literals;
      for (int32_t literal : clause) literals.push_back(internalize(literal));
      kernel_add_large_clause(kernel, literals.data(), literals.size(), false,
                              0, ++kernel.next_clause_id);
    }
  }
}

struct Fixture {
  Kernel kernel;
  FILE *journal = nullptr;
  explicit Fixture(bool two = false, bool reverse = false) {
    kernel_reserve_vars(kernel, 12);
    add_equations(kernel, triangle, 0, reverse);
    if (two) add_equations(kernel, triangle, 3);
    journal = std::tmpfile();
    assert(journal);
    kernel.structural_provenance = journal;
  }
  ~Fixture() {
    kernel.structural_provenance = nullptr;
    assert(!std::fclose(journal));
  }
};

TseitinCycleBreakingLimits limits() {
  TseitinCycleBreakingLimits result;
  result.snapshot = {1000, 10000, 100000, 1000, nullptr, nullptr};
  result.discovery = {100000, 1000, 100, 4, nullptr, nullptr, 100,
                      1u << 20};
  result.admission = {100000, nullptr, nullptr};
  result.planning = {100000, 64, 1};
  result.transaction = {100000, 3, 2, 2, 1000, nullptr, nullptr};
  result.maximum_validation_ticks = 100000;
  result.maximum_components = 100;
  result.maximum_identity_bytes = 1u << 16;
  result.maximum_total_identity_bytes = 1u << 18;
  result.maximum_identity_work = 1u << 20;
  result.maximum_journal_bytes = 1u << 20;
  result.maximum_session_keys = 100;
  result.maximum_session_work = 1u << 20;
  result.maximum_session_identity_bytes = 1u << 20;
  assert(result.valid());
  return result;
}

std::string contents(FILE *file) {
  assert(!std::fflush(file));
  assert(!std::fseek(file, 0, SEEK_SET));
  std::string result;
  char buffer[4096];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, count);
  assert(!std::ferror(file));
  return result;
}

bool interrupted(void *) { return true; }

struct StopAfter {
  uint64_t calls = 0;
  uint64_t stop_at = UINT64_MAX;
};

bool stop_after(void *opaque) {
  auto &state = *static_cast<StopAfter *>(opaque);
  return ++state.calls >= state.stop_at;
}

Kernel parsed_triangle() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  for (const auto &equation : triangle) {
    const auto clauses = parity_certificate::xor_equation(
        equation.variables.data(), equation.variables.size(), equation.rhs);
    for (const auto &clause : clauses) {
      std::vector<Lit> literals;
      for (uint32_t i = 0; i < clause.size; ++i)
        literals.push_back(kernel_internalize(kernel, clause.lits[i]));
      ingest_clause(kernel, literals);
    }
  }
  finish_input(kernel);
  return kernel;
}

void add_triangle(tactix_c_handle *solver) {
  for (const auto &equation : triangle) {
    const auto clauses = parity_certificate::xor_equation(
        equation.variables.data(), equation.variables.size(), equation.rhs);
    for (const auto &clause : clauses) {
      for (uint32_t i = 0; i < clause.size; ++i)
        tactix_add(solver, clause.lits[i]);
      tactix_add(solver, 0);
    }
  }
}

void assign_c_parameters(tactix_online_tseitin_cycle_breaking_parameters &c,
                         const TseitinCycleBreakingParameters &p) {
  c = {p.maximum_snapshot_clauses,
       p.maximum_snapshot_literals,
       p.maximum_snapshot_work,
       p.maximum_snapshot_variables,
       p.maximum_discovery_work,
       p.maximum_discovery_clauses,
       p.maximum_discovery_groups,
       p.maximum_discovery_retained_bytes,
       p.maximum_admission_work,
       p.maximum_planning_work,
       p.maximum_witness_length,
       p.maximum_planning_additions,
       p.maximum_transaction_work,
       p.maximum_transaction_additions,
       p.maximum_transaction_deletions,
       p.maximum_transaction_helpers,
       p.maximum_transaction_witness_literals,
       p.maximum_validation_ticks,
       p.maximum_components,
       p.maximum_identity_bytes,
       p.maximum_total_identity_bytes,
       p.maximum_identity_work,
       p.maximum_journal_bytes,
       p.maximum_session_keys,
       p.maximum_session_work,
       p.maximum_session_identity_bytes};
}

void offers_all_components_and_applies_exact_selection() {
  Fixture fixture(true);
  TseitinCycleBreakingSession session;
  const auto first =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(first.status == TseitinCycleBreakingStatus::Applied);
  assert(first.offers.size() == 2 && first.offers[0] != first.offers[1]);
  assert(first.effort.candidates == 2 && first.effort.offers == 2);
  const auto repeated =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(repeated.offers == first.offers);

  const uint64_t before = fixture.kernel.next_clause_id;
  const auto applied = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), first.offers[1]);
  assert(applied.status == TseitinCycleBreakingStatus::Applied);
  assert(!fixture.kernel.startup_certificate_open);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.effort.identity_bytes == first.offers[1].size());
  assert(applied.effort.proof_additions == 0 &&
         applied.effort.proof_deletions == 0 &&
         applied.effort.proof_literal_occurrences == 0 &&
         applied.effort.proof_helpers == 0 &&
         applied.effort.proof_only_variables == 0 &&
         applied.effort.proof_witness_literals == 0 &&
         applied.effort.proof_routes == 0);
  assert(applied.committed_action_id == 1);
  assert(fixture.kernel.next_clause_id == before + 1);
  assert(session.retained() && session.belongs_to(fixture.kernel) &&
         session.keys().size() == 1);
  record_tseitin_cycle_breaking_feedback(fixture.kernel, applied);
  assert(fixture.kernel.stats.tseitin_cycle_attempts == 1 &&
         fixture.kernel.stats.tseitin_cycle_applied == 1 &&
         fixture.kernel.stats.tseitin_cycle_units == 1 &&
         fixture.kernel.stats.tseitin_cycle_discovery_work > 0 &&
         fixture.kernel.stats.tseitin_cycle_recapture_work > 0);
  const std::string journal = contents(fixture.journal);
  if (const char *path = std::getenv("TACTIX_TEST_TSEITIN_JOURNAL")) {
    FILE *copy = std::fopen(path, "wb");
    assert(copy && std::fwrite(journal.data(), 1, journal.size(), copy) ==
                       journal.size());
    assert(!std::fclose(copy));
  }
  assert(journal.rfind("{\"action\":\"TseitinCycleBreaking\"", 0) == 0);
  assert(journal.find("\"proof_events\":[") != std::string::npos);
  assert(journal.find("\"mapping\":{") != std::string::npos);
  assert(journal.back() == '\n');

  const auto invalidated = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), first.offers[0]);
  assert(invalidated.status == TseitinCycleBreakingStatus::Inapplicable);
}

void stale_corrupt_rebound_and_limits_are_nonmutating() {
  {
    Kernel empty;
    FILE *records = std::tmpfile();
    assert(records);
    empty.structural_provenance = records;
    TseitinCycleBreakingSession empty_session;
    const auto none =
        tseitin_cycle_breaking_offer(empty, empty_session, limits());
    assert(none.status == TseitinCycleBreakingStatus::NoStructure &&
           none.offers.empty());
    empty.structural_provenance = nullptr;
    std::fclose(records);
  }
  Fixture fixture;
  TseitinCycleBreakingSession session;
  const auto offered =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(offered.offers.size() == 1);
  const uint64_t id = fixture.kernel.next_clause_id;
  const uint64_t epoch = fixture.kernel.touch_epoch;
  const std::string overlong(limits().maximum_identity_bytes + 1, 'a');
  const auto too_large = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), overlong);
  assert(too_large.status == TseitinCycleBreakingStatus::ResourceExhausted &&
         too_large.effort.identity_work == 0 && too_large.identity.empty() &&
         fixture.kernel.next_clause_id == id && session.keys().empty() &&
         fixture.kernel.startup_certificate_open);
  std::string corrupt = offered.offers.front();
  corrupt.back() = corrupt.back() == '0' ? '1' : '0';
  const auto stale = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), corrupt);
  assert(stale.status == TseitinCycleBreakingStatus::Stale);
  assert(fixture.kernel.next_clause_id == id &&
         fixture.kernel.touch_epoch == epoch && fixture.kernel.trail.empty());
  assert(fixture.kernel.startup_certificate_open);

  auto small = limits();
  small.maximum_journal_bytes = 32;
  const auto journal_limited = tseitin_cycle_breaking_apply(
      fixture.kernel, session, small, offered.offers.front());
  assert(journal_limited.status ==
         TseitinCycleBreakingStatus::ResourceExhausted);
  assert(fixture.kernel.next_clause_id == id && fixture.kernel.trail.empty());
  assert(fixture.kernel.startup_certificate_open);

  small = limits();
  small.maximum_identity_bytes = 16;
  const auto identity_limited =
      tseitin_cycle_breaking_offer(fixture.kernel, session, small);
  assert(identity_limited.status ==
         TseitinCycleBreakingStatus::ResourceExhausted);
  assert(identity_limited.offers.empty());

  small = limits();
  small.discovery.interrupted = interrupted;
  const auto stopped =
      tseitin_cycle_breaking_offer(fixture.kernel, session, small);
  assert(stopped.status == TseitinCycleBreakingStatus::Interrupted);
  assert(stopped.offers.empty() && fixture.kernel.trail.empty());

  assert(tseitin_cycle_breaking_apply(fixture.kernel, session, limits(),
                                     offered.offers.front())
             .status == TseitinCycleBreakingStatus::Applied);
  Fixture other;
  const auto other_offer =
      tseitin_cycle_breaking_offer(other.kernel, TseitinCycleBreakingSession{},
                                   limits());
  assert(other_offer.offers.size() == 1);
  const auto rebound = tseitin_cycle_breaking_apply(
      other.kernel, session, limits(), other_offer.offers.front());
  assert(rebound.status == TseitinCycleBreakingStatus::Stale);
  assert(other.kernel.trail.empty());
}

void every_precommit_phase_is_interruptible() {
  for (const bool planning : {true, false}) {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    auto bounded = limits();
    if (planning)
      bounded.planning.interrupted = interrupted;
    else
      bounded.transaction.interrupted = interrupted;
    const uint64_t id = fixture.kernel.next_clause_id;
    const auto stopped =
        tseitin_cycle_breaking_offer(fixture.kernel, session, bounded);
    assert(stopped.status == TseitinCycleBreakingStatus::Interrupted &&
           stopped.offers.empty() && fixture.kernel.next_clause_id == id &&
           fixture.kernel.trail.empty() && session.keys().empty() &&
           fixture.kernel.startup_certificate_open);
  }

  uint64_t identity_polls = 0;
  {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    auto measured = limits();
    StopAfter counter;
    measured.discovery.interrupted = stop_after;
    measured.discovery.interrupt_state = &counter;
    assert(tseitin_cycle_breaking_offer(fixture.kernel, session, measured)
               .status == TseitinCycleBreakingStatus::Applied);
    identity_polls = counter.calls;
  }
  {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    auto bounded = limits();
    StopAfter stop{0, identity_polls};
    bounded.discovery.interrupted = stop_after;
    bounded.discovery.interrupt_state = &stop;
    const uint64_t id = fixture.kernel.next_clause_id;
    const auto result =
        tseitin_cycle_breaking_offer(fixture.kernel, session, bounded);
    assert(result.status == TseitinCycleBreakingStatus::Interrupted &&
           result.effort.identity_work > 0 &&
           fixture.kernel.next_clause_id == id &&
           fixture.kernel.trail.empty() && session.keys().empty());
  }

  uint64_t apply_polls = 0;
  {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    const auto offered =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    auto measured = limits();
    StopAfter counter;
    measured.discovery.interrupted = stop_after;
    measured.discovery.interrupt_state = &counter;
    assert(tseitin_cycle_breaking_apply(fixture.kernel, session, measured,
                                        offered.offers.front())
               .status == TseitinCycleBreakingStatus::Applied);
    apply_polls = counter.calls;
  }
  bool journal_stopped = false;
  for (uint64_t threshold = 1;
       threshold <= apply_polls && !journal_stopped;
       ++threshold) {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    const auto offered =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    auto bounded = limits();
    StopAfter stop{0, threshold};
    bounded.discovery.interrupted = stop_after;
    bounded.discovery.interrupt_state = &stop;
    const uint64_t id = fixture.kernel.next_clause_id;
    const auto result = tseitin_cycle_breaking_apply(
        fixture.kernel, session, bounded, offered.offers.front());
    if (result.status == TseitinCycleBreakingStatus::Interrupted &&
        result.effort.journal_work) {
      assert(fixture.kernel.next_clause_id == id &&
             fixture.kernel.trail.empty() && session.keys().empty() &&
             fixture.kernel.structural_action_sequence == 0 &&
             fixture.kernel.startup_certificate_open);
      journal_stopped = true;
    }
  }
  assert(journal_stopped);
}

void selected_apply_is_independent_of_offer_aggregate_limits() {
  Fixture fixture(true);
  TseitinCycleBreakingSession session;
  const auto offered =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(offered.offers.size() == 2);
  auto selected_limits = limits();
  selected_limits.maximum_components = 1;
  selected_limits.maximum_total_identity_bytes = 1;
  const auto incomplete_offer = tseitin_cycle_breaking_offer(
      fixture.kernel, session, selected_limits);
  assert(incomplete_offer.status ==
         TseitinCycleBreakingStatus::ResourceExhausted);
  assert(incomplete_offer.offers.empty());
  assert(incomplete_offer.effort.candidates == 2);
  const auto applied = tseitin_cycle_breaking_apply(
      fixture.kernel, session, selected_limits, offered.offers.back());
  assert(applied.status == TseitinCycleBreakingStatus::Applied);
}

void malformed_and_residual_staleness_are_nonmutating() {
  Fixture fixture;
  TseitinCycleBreakingSession session;
  const auto offered =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(offered.offers.size() == 1);
  const uint64_t clause_id = fixture.kernel.next_clause_id;
  const uint64_t epoch = fixture.kernel.touch_epoch;
  for (const std::string &malformed :
       {std::string(), std::string("tcx1:00"), std::string("tcx2::00"),
        std::string("tcx2:00:"), std::string("tcx2:0:00"),
        std::string("tcx2:00:gg")}) {
    const auto rejected = tseitin_cycle_breaking_apply(
        fixture.kernel, session, limits(), malformed);
    assert(rejected.status == TseitinCycleBreakingStatus::Invalid);
  }
  assert(fixture.kernel.next_clause_id == clause_id &&
         fixture.kernel.touch_epoch == epoch && fixture.kernel.trail.empty());

  kernel_add_binary_clause(fixture.kernel, internalize(1), internalize(8),
                           ++fixture.kernel.next_clause_id);
  const uint64_t changed_id = fixture.kernel.next_clause_id;
  const uint64_t changed_epoch = fixture.kernel.touch_epoch;
  const auto stale = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), offered.offers.front());
  assert(stale.status == TseitinCycleBreakingStatus::Stale);
  assert(fixture.kernel.next_clause_id == changed_id &&
         fixture.kernel.touch_epoch == changed_epoch &&
         fixture.kernel.trail.empty());
}

void semantic_duplicate_key_is_storage_order_independent() {
  Fixture ordered(false, false), reversed(false, true);
  TseitinCycleBreakingSession first, second;
  const auto first_offer =
      tseitin_cycle_breaking_offer(ordered.kernel, first, limits());
  const auto second_offer =
      tseitin_cycle_breaking_offer(reversed.kernel, second, limits());
  assert(first_offer.offers.size() == 1 && second_offer.offers.size() == 1);
  assert(tseitin_cycle_breaking_apply(ordered.kernel, first, limits(),
                                     first_offer.offers.front())
             .status == TseitinCycleBreakingStatus::Applied);
  assert(tseitin_cycle_breaking_apply(reversed.kernel, second, limits(),
                                     second_offer.offers.front())
             .status == TseitinCycleBreakingStatus::Applied);
  assert(first.keys().size() == 1 && first.keys() == second.keys());
}

void dsr_and_unsupported_proof_contract() {
  char dsr_path[] = "/tmp/tseitin-cycle-action-dsr-XXXXXX";
  const int descriptor = mkstemp(dsr_path);
  assert(descriptor >= 0);
  close(descriptor);
  {
    Fixture fixture;
    Proof proof;
    assert(proof.open(dsr_path, ProofFormat::DsrAscii));
    proof.bind_variable_map(&fixture.kernel.internal_to_external);
    proof.bind_external_variable_map(&fixture.kernel.external_to_internal);
    fixture.kernel.proof = &proof;
    TseitinCycleBreakingSession session;
    const auto offered =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    assert(offered.offers.size() == 1);
    const auto applied = tseitin_cycle_breaking_apply(
        fixture.kernel, session, limits(), offered.offers.front());
    assert(applied.status == TseitinCycleBreakingStatus::Applied);
    assert(applied.effort.identity_bytes == offered.offers.front().size());
    assert(applied.effort.proof_additions == 3);
    assert(applied.effort.proof_deletions == 2);
    assert(applied.effort.proof_literal_occurrences == 5);
    assert(applied.effort.proof_helpers == 2);
    assert(applied.effort.proof_only_variables == 1);
    assert(applied.effort.proof_witness_literals == 5);
    assert(applied.effort.proof_routes == 1);
    record_tseitin_cycle_breaking_feedback(fixture.kernel, applied);
    assert(fixture.kernel.stats.tseitin_cycle_proof_additions == 3);
    assert(fixture.kernel.stats.tseitin_cycle_proof_deletions == 2);
    assert(fixture.kernel.stats.tseitin_cycle_proof_literal_occurrences == 5);
    assert(fixture.kernel.stats.tseitin_cycle_proof_helpers == 2);
    assert(fixture.kernel.stats.tseitin_cycle_proof_only_variables == 1);
    assert(fixture.kernel.stats.tseitin_cycle_proof_witness_literals == 5);
    assert(fixture.kernel.stats.tseitin_cycle_proof_routes == 1);
    const std::string journal = contents(fixture.journal);
    assert(journal.find("\"proof_format\":\"dsr-ascii\"") !=
           std::string::npos);
    assert(journal.find("\"helper_additions\":[[") != std::string::npos);
    assert(journal.find("\"helper_deletions\":[[") != std::string::npos);
    assert(journal.find("\"deletion_state\":\"helpers-removed\"") !=
           std::string::npos);
    proof.flush();
    fixture.kernel.proof = nullptr;
  }
  FILE *proof_file = std::fopen(dsr_path, "rb");
  assert(proof_file);
  const std::string proof_text = contents(proof_file);
  assert(!std::fclose(proof_file));
  assert(!proof_text.empty());
  if (const char *path = std::getenv("TACTIX_TEST_TSEITIN_DSR_ASCII")) {
    FILE *copy = std::fopen(path, "wb");
    assert(copy && std::fwrite(proof_text.data(), 1, proof_text.size(), copy) ==
                       proof_text.size());
    assert(!std::fclose(copy));
  }
  unlink(dsr_path);

  char binary_path[] = "/tmp/tseitin-cycle-action-dsr-bin-XXXXXX";
  const int binary_descriptor = mkstemp(binary_path);
  assert(binary_descriptor >= 0);
  close(binary_descriptor);
  {
    Fixture fixture;
    Proof proof;
    assert(proof.open(binary_path, ProofFormat::DsrBinary));
    proof.bind_variable_map(&fixture.kernel.internal_to_external);
    proof.bind_external_variable_map(&fixture.kernel.external_to_internal);
    fixture.kernel.proof = &proof;
    TseitinCycleBreakingSession session;
    const auto offered =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    assert(offered.offers.size() == 1);
    const auto applied = tseitin_cycle_breaking_apply(
        fixture.kernel, session, limits(), offered.offers.front());
    assert(applied.status == TseitinCycleBreakingStatus::Applied);
    assert(applied.effort.proof_additions == 3 &&
           applied.effort.proof_deletions == 2 &&
           applied.effort.proof_literal_occurrences == 5);
    const std::string journal = contents(fixture.journal);
    assert(journal.find("\"proof_format\":\"dsr-binary\"") !=
           std::string::npos);
    proof.flush();
    fixture.kernel.proof = nullptr;
  }
  FILE *binary_file = std::fopen(binary_path, "rb");
  assert(binary_file);
  const std::string binary_proof = contents(binary_file);
  assert(!std::fclose(binary_file));
  assert(!binary_proof.empty());
  if (const char *path = std::getenv("TACTIX_TEST_TSEITIN_DSR_BINARY")) {
    FILE *copy = std::fopen(path, "wb");
    assert(copy &&
           std::fwrite(binary_proof.data(), 1, binary_proof.size(), copy) ==
               binary_proof.size());
    assert(!std::fclose(copy));
  }
  unlink(binary_path);

  char drat_path[] = "/tmp/tseitin-cycle-action-drat-XXXXXX";
  const int drat_descriptor = mkstemp(drat_path);
  assert(drat_descriptor >= 0);
  close(drat_descriptor);
  {
    Fixture fixture;
    Proof proof;
    assert(proof.open(drat_path, ProofFormat::DratAscii));
    fixture.kernel.proof = &proof;
    TseitinCycleBreakingSession session;
    const auto rejected =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    assert(rejected.status == TseitinCycleBreakingStatus::UnsupportedProof);
    assert(rejected.offers.empty() && fixture.kernel.trail.empty());
    fixture.kernel.proof = nullptr;
  }
  unlink(drat_path);

  for (const ProofFormat format : {ProofFormat::DratBinary,
                                   ProofFormat::Lrat}) {
    char unsupported_path[] = "/tmp/tseitin-cycle-action-proof-XXXXXX";
    const int unsupported_descriptor = mkstemp(unsupported_path);
    assert(unsupported_descriptor >= 0);
    close(unsupported_descriptor);
    Fixture fixture;
    Proof proof;
    assert(proof.open(unsupported_path, format));
    fixture.kernel.proof = &proof;
    TseitinCycleBreakingSession session;
    const auto rejected =
        tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
    assert(rejected.status == TseitinCycleBreakingStatus::UnsupportedProof);
    assert(rejected.offers.empty() && fixture.kernel.trail.empty());
    fixture.kernel.proof = nullptr;
    unlink(unsupported_path);
  }
}

RandomControllerConfig random_tseitin_config() {
  RandomControllerConfig config;
  config.parameters.rules = {"TseitinCycleBreaking"};
  config.tseitin_cycle_action_producer.enabled = true;
  return config;
}

void typed_replay_random_reach_and_separate_propagation() {
  Fixture source(true);
  TseitinCycleBreakingSession source_session;
  const auto offered = tseitin_cycle_breaking_offer(
      source.kernel, source_session, limits());
  assert(offered.offers.size() == 2);

  OnlineInvocation step;
  step.id = 27;
  step.rule = "TseitinCycleBreaking";
  step.parameters = TseitinCycleBreakingParameters{};
  step.granularity = InvocationGranularity::OneInstance;
  step.tseitin_cycle_instances = {offered.offers.back()};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram program{{step}, {}};
  const std::string text = serialize_online_strategy_program(program);
  OnlineStrategyProgram replay;
  std::string error;
  assert(deserialize_online_strategy_program(text, replay, &error));
  assert(replay.sequence[0].tseitin_cycle_instances ==
         step.tseitin_cycle_instances);

  Fixture executed(true);
  TseitinCycleBreakingSession executed_session;
  executed.kernel.tseitin_cycle_breaking_session = &executed_session;
  OnlineStrategyExecutor executor;
  assert(executor.configure(replay, 71, ReconfigureMode::ResetRuntime,
                            &error));
  const auto applied = executor.advance(executed.kernel);
  assert(applied.status == InvocationStatus::Completed &&
         applied.applications == 1 &&
         executed_session.keys().size() == 1);
  assert(!executed.kernel.startup_certificate_open);
  assert(!kernel_propagation_fixpoint(executed.kernel));
  const size_t trail_after_action = executed.kernel.trail.size();
  assert(trail_after_action == 1);

  OnlineInvocation propagation;
  propagation.id = 100;
  propagation.rule = "Propagate";
  propagation.granularity = InvocationGranularity::CanonicalApplication;
  propagation.work_unit = WorkUnit::Propagations;
  propagation.budget = WorkLimit::bounded(1000);
  OnlineStrategyExecutor propagation_executor;
  assert(propagation_executor.configure({{propagation}, {}}, 72,
                                        ReconfigureMode::ResetRuntime,
                                        &error));
  const auto propagated = propagation_executor.advance(executed.kernel);
  assert(propagated.status == InvocationStatus::Completed ||
         propagated.status == InvocationStatus::ScopedFixpoint ||
         propagated.status == InvocationStatus::NoChange ||
         propagated.status == InvocationStatus::Unsat);
  assert(kernel_propagation_fixpoint(executed.kernel) ||
         executed.kernel.inconsistent || executed.kernel.conflict_valid);
  assert(executed_session.keys().size() == 1);

  Fixture candidate(true);
  TseitinCycleBreakingSession session;
  candidate.kernel.tseitin_cycle_breaking_session = &session;
  RandomControllerSelector selector(1);
  auto observation = observe_kernel(candidate.kernel);
  std::vector<std::string> reached;
  bool saw_producer = false;
  for (uint32_t round = 0; round != 2; ++round) {
    ActionRequest selected;
    bool selected_rule = false;
    for (uint32_t attempt = 0; attempt != 32 && !selected_rule; ++attempt) {
      assert(selector.choose(candidate.kernel, observation,
                             random_tseitin_config(), selected, error));
      if (!selected.producer_effort.empty()) {
        assert(selected.producer_effort.size() == 1);
        assert(selected.producer_effort.front().complete);
        assert(selected.producer_effort.front().candidates == 2);
        assert(selected.producer_effort.front().offers == 2);
        assert(selected.producer_effort.front().preparation_work >=
               offered.offers[0].size() + offered.offers[1].size());
        saw_producer = true;
      }
      selected_rule = selected.type == ActionType::Rule;
    }
    assert(selected_rule && saw_producer);
    const std::string identity =
        selected.invocation.tseitin_cycle_instances.front();
    assert(std::find(offered.offers.begin(), offered.offers.end(), identity) !=
           offered.offers.end());
    assert(std::find(reached.begin(), reached.end(), identity) == reached.end());
    reached.push_back(identity);
    ActionResult consumed;
    consumed.sequence = selected.sequence;
    consumed.type = selected.type;
    consumed.invocation.status = InvocationStatus::Completed;
    consumed.invocation.selection_complete = true;
    consumed.invocation.after = state_version(candidate.kernel);
    observation.last_action = consumed;
    observation.sequence = selected.sequence;
  }
  assert(reached.size() == 2);
}

void random_pending_requires_profile_reconfiguration() {
  Fixture fixture;
  TseitinCycleBreakingSession session;
  fixture.kernel.tseitin_cycle_breaking_session = &session;
  auto config = random_tseitin_config();
  RestartController controller(fixture.kernel);
  RandomControllerSelector selector(1);
  auto observation = controller.observe();
  ActionRequest original;
  std::string error;
  bool selected = false;
  for (uint32_t attempt = 0; attempt != 64 && !selected; ++attempt) {
    assert(selector.choose(fixture.kernel, observation, config, original,
                           error));
    selected = original.type == ActionType::Rule;
  }
  assert(selected && original.invocation.tseitin_cycle_instances.size() == 1);
  const auto exact = original.invocation.tseitin_cycle_instances;
  auto constrained = std::get<TseitinCycleBreakingParameters>(
      original.invocation.parameters);
  constrained.maximum_journal_bytes = 1;
  original.invocation.parameters = constrained;
  const auto exhausted =
      controller.apply_rule(original, config.allowance("TseitinCycleBreaking"));
  assert(exhausted.invocation.status == InvocationStatus::ResourceExhausted);
  assert(fixture.kernel.startup_certificate_open);
  observation = controller.observe();
  for (uint32_t attempt = 0; attempt != 32; ++attempt) {
    ActionRequest suspended;
    assert(selector.choose(fixture.kernel, observation, config, suspended,
                           error));
    assert(suspended.type != ActionType::Rule);
  }
  auto reconfigured = config;
  ++reconfigured.parameters.tseitin_cycle_breaking_reference
        .maximum_snapshot_work;
  bool retried = false;
  for (uint32_t attempt = 0; attempt != 64 && !retried; ++attempt) {
    ActionRequest request;
    assert(selector.choose(fixture.kernel, observation, reconfigured, request,
                           error));
    if (request.type != ActionType::Rule) continue;
    assert(request.invocation.tseitin_cycle_instances == exact);
    assert(request.producer_effort.empty());
    retried = true;
  }
  assert(retried && session.keys().empty() && fixture.kernel.trail.empty());
}

void c_v7_is_backward_compatible_and_deep_copies_identity() {
  assert(std::strcmp(tactix_rule_name(38), "TseitinCycleBreaking") == 0);
  const auto reference = reference_inprocess_program();
  const auto reference_step = std::find_if(
      reference.begin(), reference.end(),
      [](const InprocessStep &candidate) { return candidate.id == 27; });
  assert(reference_step != reference.end() &&
         reference_step->rule == "TseitinCycleBreaking" &&
         !reference_step->enabled);
  Kernel source_kernel = parsed_triangle();
  FILE *source_records = std::tmpfile();
  assert(source_records);
  source_kernel.structural_provenance = source_records;
  TseitinCycleBreakingSession source_session;
  const auto offered =
      tseitin_cycle_breaking_offer(source_kernel, source_session, limits());
  assert(offered.status == TseitinCycleBreakingStatus::Applied &&
         offered.offers.size() == 1);

  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  add_triangle(solver);
  tactix_online_step_v7 step{};
  auto &base = step.base.base.base.base.base.base;
  base.step_id = 27;
  base.rule_id = 38;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_TSEITIN_CYCLE_BREAKING;
  const TseitinCycleBreakingParameters parameters;
  assign_c_parameters(base.parameters.value.tseitin_cycle_breaking,
                      parameters);
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  std::string identity = offered.offers.front();
  const tactix_online_tseitin_cycle_instance selected{identity.c_str()};
  step.tseitin_cycle_instances = &selected;
  step.tseitin_cycle_instance_count = 1;
  tactix_online_program_v7 program{
      &step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v7(solver, &program, 90,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  std::fill(identity.begin(), identity.end(), 'x');
  tactix_online_result applied{};
  assert(tactix_online_advance(solver, &applied) == 0);
  assert(applied.rule_id == 38 &&
         applied.status == TACTIX_ONLINE_COMPLETED);
  tactix_release(solver);
  std::fclose(records);

  tactix_c_handle *ordinary = tactix_init();
  tactix_online_step_v7 propagation{};
  auto &plain = propagation.base.base.base.base.base.base;
  plain.step_id = 100;
  plain.rule_id = 14;
  plain.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  plain.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  plain.repeat_limit = 1;
  plain.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
  plain.budget = 100;
  tactix_online_program_v7 ordinary_program{
      &propagation, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v7(ordinary, &ordinary_program, 91,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  propagation.tseitin_cycle_instances = &selected;
  propagation.tseitin_cycle_instance_count = 1;
  assert(tactix_online_configure_v7(ordinary, &ordinary_program, 92,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  propagation.tseitin_cycle_instances = nullptr;
  propagation.tseitin_cycle_instance_count = 0;
  tactix_online_step_v7 pair[2] = {propagation, propagation};
  pair[1].base.base.base.base.base.base.step_id = 101;
  tactix_online_program_v7 pair_program{
      pair, 2, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v7(ordinary, &pair_program, 93,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_step_v6 v6_step = propagation.base;
  tactix_online_program_v6 v6_program{
      &v6_step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v6(ordinary, &v6_program, 94,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_release(ordinary);

  tactix_c_handle *invalid = tactix_init();
  add_triangle(invalid);
  step.tseitin_cycle_instance_count = 0;
  assert(tactix_online_configure_v7(invalid, &program, 93,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.tseitin_cycle_instance_count = -1;
  assert(tactix_online_configure_v7(invalid, &program, 93,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.tseitin_cycle_instance_count = 2;
  assert(tactix_online_configure_v7(invalid, &program, 94,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  step.tseitin_cycle_instance_count = 1;
  step.tseitin_cycle_instances = nullptr;
  assert(tactix_online_configure_v7(invalid, &program, 95,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  const tactix_online_tseitin_cycle_instance null_identity{nullptr};
  step.tseitin_cycle_instances = &null_identity;
  assert(tactix_online_configure_v7(invalid, &program, 95,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  char unterminated[9];
  std::fill(std::begin(unterminated), std::end(unterminated), 'a');
  tactix_online_tseitin_cycle_instance too_long{unterminated};
  step.tseitin_cycle_instances = &too_long;
  base.parameters.value.tseitin_cycle_breaking.maximum_identity_bytes = 8;
  assert(tactix_online_configure_v7(invalid, &program, 96,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  tactix_release(invalid);
  source_kernel.structural_provenance = nullptr;
  std::fclose(source_records);
}

void propagated_root_rebuilds_the_complete_component_census() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 20);
  add_equations(kernel, wide_cycle);
  add_equations(kernel, wide_cycle, 6);
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  char proof_path[] = "/tmp/tseitin-cycle-repeat-dsr-XXXXXX";
  const int proof_descriptor = mkstemp(proof_path);
  assert(proof_descriptor >= 0);
  close(proof_descriptor);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  TseitinCycleBreakingSession session;
  const auto initial = tseitin_cycle_breaking_offer(kernel, session, limits());
  assert(initial.status == TseitinCycleBreakingStatus::Applied &&
         initial.offers.size() == 2);
  const auto first = tseitin_cycle_breaking_apply(
      kernel, session, limits(), initial.offers.front());
  assert(first.status == TseitinCycleBreakingStatus::Applied);
  assert(!kernel_propagation_fixpoint(kernel));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(kernel));
  const auto fresh = tseitin_cycle_breaking_offer(kernel, session, limits());
  assert(fresh.status == TseitinCycleBreakingStatus::Applied &&
         fresh.offers.size() == 2);
  const uint64_t before_stale = kernel.next_clause_id;
  const auto old = tseitin_cycle_breaking_apply(
      kernel, session, limits(), initial.offers.back());
  assert(old.status == TseitinCycleBreakingStatus::Stale &&
         kernel.next_clause_id == before_stale);
  const auto second = tseitin_cycle_breaking_apply(
      kernel, session, limits(), fresh.offers.back());
  assert(second.status == TseitinCycleBreakingStatus::Applied &&
         session.keys().size() == 2);
  const std::string records = contents(journal);
  assert(std::count(records.begin(), records.end(), '\n') == 2);
  assert(records.find("\"external_root_units\":[]") != std::string::npos);
  assert(records.find("\"external_root_units\":[") != std::string::npos);
  if (const char *path = std::getenv("TACTIX_TEST_TSEITIN_REPEAT_JOURNAL")) {
    FILE *copy = std::fopen(path, "wb");
    assert(copy && std::fwrite(records.data(), 1, records.size(), copy) ==
                       records.size());
    assert(!std::fclose(copy));
  }
  proof.flush();
  kernel.proof = nullptr;
  if (const char *path = std::getenv("TACTIX_TEST_TSEITIN_REPEAT_DSR")) {
    FILE *source = std::fopen(proof_path, "rb");
    assert(source);
    const std::string bytes = contents(source);
    assert(!std::fclose(source));
    FILE *copy = std::fopen(path, "wb");
    assert(copy && std::fwrite(bytes.data(), 1, bytes.size(), copy) ==
                       bytes.size());
    assert(!std::fclose(copy));
  }
  unlink(proof_path);
  kernel.structural_provenance = nullptr;
  std::fclose(journal);
}

void assigned_edges_select_an_available_cycle() {
  Kernel probe;
  kernel_reserve_vars(probe, 12);
  add_equations(probe, wide_cycle);
  tseitin_cycle_live::Snapshot snapshot;
  auto configured = limits();
  assert(tseitin_cycle_live::capture(probe, configured.snapshot, snapshot)
             .status == tseitin_cycle_live::Status::Captured);
  std::vector<tseitin_cycle_discovery::Plan> plans;
  assert(tseitin_cycle_discovery::enumerate(snapshot, configured.discovery,
                                            plans)
             .discovered());
  assert(plans.size() == 1);
  tseitin_cycle_live::Prepared prepared;
  const tseitin_cycle_live::Request request{
      configured.snapshot, configured.admission, configured.planning,
      configured.transaction, configured.maximum_validation_ticks};
  assert(tseitin_cycle_discovery::prepare(probe, plans.front(), request,
                                          prepared)
             .status == tseitin_cycle_live::Status::Prepared);
  const auto &cycle = prepared.transaction().identity.cycle_edges;
  assert(!cycle.empty());
  int32_t off_cycle = 0;
  for (const auto &equation : wide_cycle)
    for (int32_t edge : equation.variables)
      if (std::find(cycle.begin(), cycle.end(), edge) == cycle.end())
        off_cycle = edge;
  assert(off_cycle);

  Kernel legal;
  kernel_reserve_vars(legal, 12);
  add_equations(legal, wide_cycle);
  FILE *legal_journal = std::tmpfile();
  assert(legal_journal);
  legal.structural_provenance = legal_journal;
  kernel_assign_unit(legal, internalize(off_cycle));
  assert(propagate_apply(legal) == ApplyResult::Ok);
  TseitinCycleBreakingSession legal_session;
  const auto legal_offer =
      tseitin_cycle_breaking_offer(legal, legal_session, configured);
  assert(legal_offer.status == TseitinCycleBreakingStatus::Applied &&
         legal_offer.offers.size() == 1);
  assert(tseitin_cycle_breaking_apply(legal, legal_session, configured,
                                     legal_offer.offers.front())
             .status == TseitinCycleBreakingStatus::Applied);
  legal.structural_provenance = nullptr;
  std::fclose(legal_journal);

  Kernel blocked;
  kernel_reserve_vars(blocked, 12);
  add_equations(blocked, wide_cycle);
  FILE *blocked_journal = std::tmpfile();
  assert(blocked_journal);
  blocked.structural_provenance = blocked_journal;
  kernel_assign_unit(blocked, internalize(cycle.back()));
  assert(propagate_apply(blocked) == ApplyResult::Ok);
  tseitin_cycle_live::Snapshot blocked_snapshot;
  assert(tseitin_cycle_live::capture(blocked, configured.snapshot,
                                     blocked_snapshot)
             .status == tseitin_cycle_live::Status::Captured);
  std::vector<tseitin_cycle_discovery::Plan> blocked_plans;
  assert(tseitin_cycle_discovery::enumerate(blocked_snapshot,
                                            configured.discovery,
                                            blocked_plans)
             .discovered());
  tseitin_cycle_live::Prepared alternative;
  assert(tseitin_cycle_discovery::prepare(blocked, blocked_plans.front(),
                                          request, alternative)
             .status == tseitin_cycle_live::Status::Prepared);
  const auto &alternative_cycle = alternative.transaction().identity.cycle_edges;
  assert(std::find(alternative_cycle.begin(), alternative_cycle.end(),
                   cycle.back()) == alternative_cycle.end());
  TseitinCycleBreakingSession blocked_session;
  const auto blocked_offer =
      tseitin_cycle_breaking_offer(blocked, blocked_session, configured);
  assert(blocked_offer.status == TseitinCycleBreakingStatus::Applied &&
         blocked_offer.offers.size() == 1);
  assert(tseitin_cycle_breaking_apply(blocked, blocked_session, configured,
                                     blocked_offer.offers.front())
             .status == TseitinCycleBreakingStatus::Applied);
  blocked.structural_provenance = nullptr;
  std::fclose(blocked_journal);
}

void failed_component_encoding_consumes_identity_work() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 20);
  add_equations(kernel, wide_cycle);
  add_equations(kernel, triangle, 6);
  FILE *journal = std::tmpfile();
  assert(journal);
  kernel.structural_provenance = journal;
  TseitinCycleBreakingSession session;
  const auto offered = tseitin_cycle_breaking_offer(kernel, session, limits());
  assert(offered.offers.size() == 2 &&
         offered.offers.front().size() > offered.offers.back().size());
  auto bounded = limits();
  bounded.maximum_identity_bytes = offered.offers.back().size();
  bounded.maximum_identity_work = 32;
  const uint64_t before = kernel.next_clause_id;
  const auto exhausted = tseitin_cycle_breaking_apply(
      kernel, session, bounded, offered.offers.back());
  assert(exhausted.status == TseitinCycleBreakingStatus::ResourceExhausted &&
         exhausted.effort.identity_work > 0 &&
         kernel.next_clause_id == before && kernel.trail.empty() &&
         kernel.startup_certificate_open);
  kernel.structural_provenance = nullptr;
  std::fclose(journal);
}

} // namespace

int main() {
  offers_all_components_and_applies_exact_selection();
  stale_corrupt_rebound_and_limits_are_nonmutating();
  every_precommit_phase_is_interruptible();
  selected_apply_is_independent_of_offer_aggregate_limits();
  malformed_and_residual_staleness_are_nonmutating();
  semantic_duplicate_key_is_storage_order_independent();
  dsr_and_unsupported_proof_contract();
  typed_replay_random_reach_and_separate_propagation();
  random_pending_requires_profile_reconfiguration();
  c_v7_is_backward_compatible_and_deep_copies_identity();
  propagated_root_rebuilds_the_complete_component_census();
  assigned_edges_select_an_available_cycle();
  failed_component_encoding_consumes_identity_work();
  std::puts("test_tseitin_cycle_action: passed");
}

#undef NDEBUG
#include "tseitin_test_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "policies/restart_controller.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_inprocess.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;
using namespace tseitin_test;

namespace {

const std::vector<Equation> wide_cycle = {
    {true, {1, 2, 3}}, {false, {3, 4, 5}},
    {false, {1, 5, 6}}, {false, {2, 4, 6}}};

// A kernel with a structural journal attached.
struct Journaled {
  Kernel kernel;
  FILE *journal = std::tmpfile();
  explicit Journaled(uint32_t variables) {
    kernel_reserve_vars(kernel, variables);
    assert(journal);
    kernel.structural_provenance = journal;
  }
  ~Journaled() {
    kernel.structural_provenance = nullptr;
    assert(!std::fclose(journal));
  }
  Journaled(const Journaled &) = delete;
};

// One triangle (two with `two`), optionally stored in reverse order.
struct Fixture : Journaled {
  explicit Fixture(bool two = false, bool reverse = false) : Journaled(12) {
    add_equations(kernel, tseitin_test::triangle, 0, reverse);
    if (two) add_equations(kernel, tseitin_test::triangle, 3);
  }
};

// Copies bytes to the path named by an environment variable, if set; the
// tseitin_cycle_action_journal CTest checks those files independently.
void export_to(const char *variable, const std::string &bytes) {
  const char *path = std::getenv(variable);
  if (!path) return;
  FILE *copy = std::fopen(path, "wb");
  assert(copy && std::fwrite(bytes.data(), 1, bytes.size(), copy) == bytes.size());
  assert(!std::fclose(copy));
}

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

tseitin_cycle_live::Request request() {
  const auto configured = limits();
  return {configured.snapshot, configured.admission, configured.planning,
          configured.transaction, configured.maximum_validation_ticks};
}

bool interrupted(void *) { return true; }

Kernel parsed_triangle() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  for (const auto &equation : tseitin_test::triangle) {
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
  for (const auto &equation : tseitin_test::triangle) {
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
  const std::string journal = stream_contents(fixture.journal);
  export_to("TACTIX_TEST_TSEITIN_JOURNAL", journal);
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
    Journaled empty(0);
    TseitinCycleBreakingSession empty_session;
    const auto none =
        tseitin_cycle_breaking_offer(empty.kernel, empty_session, limits());
    assert(none.status == TseitinCycleBreakingStatus::NoStructure &&
           none.offers.empty());
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
    InterruptState counter;
    measured.discovery.interrupted = interrupt_after;
    measured.discovery.interrupt_state = &counter;
    assert(tseitin_cycle_breaking_offer(fixture.kernel, session, measured)
               .status == TseitinCycleBreakingStatus::Applied);
    identity_polls = counter.calls;
  }
  {
    Fixture fixture;
    TseitinCycleBreakingSession session;
    auto bounded = limits();
    InterruptState stop{0, identity_polls};
    bounded.discovery.interrupted = interrupt_after;
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
    InterruptState counter;
    measured.discovery.interrupted = interrupt_after;
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
    InterruptState stop{0, threshold};
    bounded.discovery.interrupted = interrupt_after;
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

// Applies the triangle's single offer under a DSR proof and exports the proof
// bytes for the journal CTest, which checks them with the DSR checkers.
struct ProofRun {
  TseitinCycleBreakingResult applied;
  decltype(Kernel::stats) stats;
  std::string journal;
};

ProofRun apply_with_proof(ProofFormat format, const char *export_variable) {
  TemporaryFile file;
  Fixture fixture;
  Proof proof;
  assert(proof.open(file.path.c_str(), format));
  attach(proof, fixture.kernel);
  TseitinCycleBreakingSession session;
  const auto offered =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(offered.offers.size() == 1);
  const auto applied = tseitin_cycle_breaking_apply(
      fixture.kernel, session, limits(), offered.offers.front());
  assert(applied.status == TseitinCycleBreakingStatus::Applied);
  assert(applied.effort.identity_bytes == offered.offers.front().size());
  record_tseitin_cycle_breaking_feedback(fixture.kernel, applied);
  const std::string journal = stream_contents(fixture.journal);
  proof.flush();
  fixture.kernel.proof = nullptr;
  const std::string bytes = read_file(file.path);
  assert(!bytes.empty());
  export_to(export_variable, bytes);
  return {applied, fixture.kernel.stats, journal};
}

void dsr_and_unsupported_proof_contract() {
  const auto run = apply_with_proof(ProofFormat::DsrAscii,
                                    "TACTIX_TEST_TSEITIN_DSR_ASCII");
  const auto &ascii = run.applied;
  const auto &journal = run.journal;
  const auto &stats = run.stats;
  assert(ascii.effort.proof_additions == 3);
  assert(ascii.effort.proof_deletions == 2);
  assert(ascii.effort.proof_literal_occurrences == 5);
  assert(ascii.effort.proof_helpers == 2);
  assert(ascii.effort.proof_only_variables == 1);
  assert(ascii.effort.proof_witness_literals == 5);
  assert(ascii.effort.proof_routes == 1);
  assert(journal.find("\"helper_additions\":[[") != std::string::npos);
  assert(journal.find("\"helper_deletions\":[[") != std::string::npos);
  assert(journal.find("\"deletion_state\":\"helpers-removed\"") !=
         std::string::npos);
  assert(stats.tseitin_cycle_proof_additions == 3);
  assert(stats.tseitin_cycle_proof_deletions == 2);
  assert(stats.tseitin_cycle_proof_literal_occurrences == 5);
  assert(stats.tseitin_cycle_proof_helpers == 2);
  assert(stats.tseitin_cycle_proof_only_variables == 1);
  assert(stats.tseitin_cycle_proof_witness_literals == 5);
  assert(stats.tseitin_cycle_proof_routes == 1);
  const auto binary =
      apply_with_proof(ProofFormat::DsrBinary, "TACTIX_TEST_TSEITIN_DSR_BINARY")
          .applied;
  assert(binary.effort.proof_additions == 3 &&
         binary.effort.proof_deletions == 2 &&
         binary.effort.proof_literal_occurrences == 5);

  TemporaryFile file;
  Fixture fixture;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  fixture.kernel.proof = &proof;
  TseitinCycleBreakingSession session;
  const auto rejected =
      tseitin_cycle_breaking_offer(fixture.kernel, session, limits());
  assert(rejected.status == TseitinCycleBreakingStatus::UnsupportedProof);
  assert(rejected.offers.empty() && fixture.kernel.trail.empty());
  fixture.kernel.proof = nullptr;
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
  Journaled fixture(20);
  Kernel &kernel = fixture.kernel;
  add_equations(kernel, wide_cycle);
  add_equations(kernel, wide_cycle, 6);
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  attach(proof, kernel);
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
  const std::string records = stream_contents(fixture.journal);
  assert(std::count(records.begin(), records.end(), '\n') == 2);
  assert(records.find("\"external_root_units\":[]") != std::string::npos);
  assert(records.find("\"external_root_units\":[") != std::string::npos);
  export_to("TACTIX_TEST_TSEITIN_REPEAT_JOURNAL", records);
  proof.flush();
  kernel.proof = nullptr;
  export_to("TACTIX_TEST_TSEITIN_REPEAT_DSR", read_file(file.path));
}

// The first prepared cycle edge, and an edge on no prepared cycle.
std::pair<int32_t, int32_t> cycle_and_off_cycle_edge() {
  Kernel probe;
  kernel_reserve_vars(probe, 12);
  add_equations(probe, wide_cycle);
  const auto configured = limits();
  tseitin_cycle_live::Snapshot snapshot;
  assert(tseitin_cycle_live::capture(probe, configured.snapshot, snapshot)
             .status == tseitin_cycle_live::Status::Captured);
  std::vector<tseitin_cycle_discovery::Plan> plans;
  assert(tseitin_cycle_discovery::enumerate(snapshot, configured.discovery,
                                            plans)
             .discovered());
  assert(plans.size() == 1);
  tseitin_cycle_live::Prepared prepared;
  assert(tseitin_cycle_discovery::prepare(probe, plans.front(), request(),
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
  return {cycle.back(), off_cycle};
}

// With one edge assigned at the root, the action still offers and applies a
// cycle; an assigned cycle edge moves preparation to an alternative cycle.
void assigned_edges_select_an_available_cycle() {
  const auto [cycle_edge, off_cycle] = cycle_and_off_cycle_edge();
  for (const int32_t assigned : {off_cycle, cycle_edge}) {
    Journaled fixture(12);
    Kernel &kernel = fixture.kernel;
    add_equations(kernel, wide_cycle);
    kernel_assign_unit(kernel, internalize(assigned));
    assert(propagate_apply(kernel) == ApplyResult::Ok);
    if (assigned == cycle_edge) {
      tseitin_cycle_live::Snapshot snapshot;
      assert(tseitin_cycle_live::capture(kernel, limits().snapshot, snapshot)
                 .status == tseitin_cycle_live::Status::Captured);
      std::vector<tseitin_cycle_discovery::Plan> plans;
      assert(tseitin_cycle_discovery::enumerate(snapshot, limits().discovery,
                                                plans)
                 .discovered());
      tseitin_cycle_live::Prepared alternative;
      assert(tseitin_cycle_discovery::prepare(kernel, plans.front(), request(),
                                              alternative)
                 .status == tseitin_cycle_live::Status::Prepared);
      const auto &edges = alternative.transaction().identity.cycle_edges;
      assert(std::find(edges.begin(), edges.end(), cycle_edge) == edges.end());
    }
    TseitinCycleBreakingSession session;
    const auto offer = tseitin_cycle_breaking_offer(kernel, session, limits());
    assert(offer.status == TseitinCycleBreakingStatus::Applied &&
           offer.offers.size() == 1);
    assert(tseitin_cycle_breaking_apply(kernel, session, limits(),
                                       offer.offers.front())
               .status == TseitinCycleBreakingStatus::Applied);
  }
}

void failed_component_encoding_consumes_identity_work() {
  Journaled fixture(20);
  Kernel &kernel = fixture.kernel;
  add_equations(kernel, wide_cycle);
  add_equations(kernel, tseitin_test::triangle, 6);
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
  c_v7_is_backward_compatible_and_deep_copies_identity();
  propagated_root_rebuilds_the_complete_component_census();
  assigned_edges_select_an_available_cycle();
  failed_component_encoding_consumes_identity_work();
  std::puts("test_tseitin_cycle_action: passed");
}

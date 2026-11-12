#include "unit_test.hpp"
#include "tseitin_test_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "tactix_c_api.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/online_inprocess.hpp"

using namespace tactix::strategy;
using namespace tseitin_test;
using CycleStatus = TseitinCycleBreakingStatus;

namespace {

const std::vector<Equation> wide_cycle = {
    {true, {1, 2, 3}}, {false, {3, 4, 5}},
    {false, {1, 5, 6}}, {false, {2, 4, 6}}};

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

// A kernel with a structural journal attached and its own session.
struct Journaled {
  Kernel kernel;
  FILE *journal = std::tmpfile();
  TseitinCycleBreakingSession session;
  uint64_t id = 0, epoch = 0;
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
  TseitinCycleBreakingOfferResult offer(
      const TseitinCycleBreakingLimits &bounds = limits()) {
    return tseitin_cycle_breaking_offer(kernel, session, bounds);
  }
  TseitinCycleBreakingResult apply(
      const std::string &identity,
      const TseitinCycleBreakingLimits &bounds = limits()) {
    return tseitin_cycle_breaking_apply(kernel, session, bounds, identity);
  }
  // The single offer of a one-component formula.
  std::string only_offer() {
    const auto offered = offer();
    assert(offered.status == CycleStatus::Applied && offered.offers.size() == 1);
    return offered.offers.front();
  }
  void mark() {
    id = kernel.next_clause_id;
    epoch = kernel.touch_epoch;
  }
  // Nothing was published or committed since mark().
  void unchanged() const {
    assert(kernel.next_clause_id == id && kernel.touch_epoch == epoch &&
           kernel.trail.empty() && session.keys().empty() &&
           kernel.structural_action_sequence == 0 &&
           kernel.startup_certificate_open);
  }
};

// One triangle (two with `two`), optionally stored in reverse order.
struct Fixture : Journaled {
  explicit Fixture(bool two = false, bool reverse = false) : Journaled(12) {
    add_equations(kernel, tseitin_test::triangle, 0, reverse);
    if (two) add_equations(kernel, tseitin_test::triangle, 3);
    mark();
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

tseitin_cycle_live::Request request() {
  const auto configured = limits();
  return {configured.snapshot, configured.admission, configured.planning,
          configured.transaction, configured.maximum_validation_ticks};
}

bool interrupted(void *) { return true; }

TEST(offers_all_components_and_applies_exact_selection) {
  Fixture fixture(true);
  const auto first = fixture.offer();
  assert(first.status == CycleStatus::Applied);
  assert(first.offers.size() == 2 && first.offers[0] != first.offers[1]);
  assert(first.effort.candidates == 2 && first.effort.offers == 2);
  assert(fixture.offer().offers == first.offers);

  const auto applied = fixture.apply(first.offers[1]);
  assert(applied.status == CycleStatus::Applied && applied.result == ApplyResult::Ok);
  assert(!fixture.kernel.startup_certificate_open);
  assert(applied.effort.identity_bytes == first.offers[1].size());
  assert(!applied.effort.proof_additions && !applied.effort.proof_routes);
  assert(applied.committed_action_id == 1);
  assert(fixture.kernel.next_clause_id == fixture.id + 1);
  assert(fixture.session.retained() &&
         fixture.session.belongs_to(fixture.kernel) &&
         fixture.session.keys().size() == 1);
  record_tseitin_cycle_breaking_feedback(fixture.kernel, applied);
  assert(fixture.kernel.stats.tseitin_cycle_attempts == 1 &&
         fixture.kernel.stats.tseitin_cycle_applied == 1 &&
         fixture.kernel.stats.tseitin_cycle_units == 1 &&
         fixture.kernel.stats.tseitin_cycle_discovery_work > 0 &&
         fixture.kernel.stats.tseitin_cycle_recapture_work > 0);
  // The journal CTest parses this record.
  export_to("TACTIX_TEST_TSEITIN_JOURNAL", stream_contents(fixture.journal));
  assert(fixture.apply(first.offers[0]).status == CycleStatus::Inapplicable);
}

TEST(stale_corrupt_rebound_and_limits_are_nonmutating) {
  {
    Journaled empty(0);
    const auto none = empty.offer();
    assert(none.status == CycleStatus::NoStructure && none.offers.empty());
  }
  Fixture fixture;
  const std::string offered = fixture.only_offer();
  const auto too_large =
      fixture.apply(std::string(limits().maximum_identity_bytes + 1, 'a'));
  assert(too_large.status == CycleStatus::ResourceExhausted &&
         too_large.effort.identity_work == 0 && too_large.identity.empty());
  fixture.unchanged();
  for (const std::string &malformed :
       {std::string(), std::string("tcx1:00"), std::string("tcx2::00"),
        std::string("tcx2:00:"), std::string("tcx2:0:00"),
        std::string("tcx2:00:gg")})
    assert(fixture.apply(malformed).status == CycleStatus::Invalid);
  fixture.unchanged();
  std::string corrupt = offered;
  corrupt.back() = corrupt.back() == '0' ? '1' : '0';
  assert(fixture.apply(corrupt).status == CycleStatus::Stale);
  fixture.unchanged();

  auto small = limits();
  small.maximum_journal_bytes = 32;
  assert(fixture.apply(offered, small).status == CycleStatus::ResourceExhausted);
  fixture.unchanged();
  small = limits();
  small.maximum_identity_bytes = 16;
  const auto identity_limited = fixture.offer(small);
  assert(identity_limited.status == CycleStatus::ResourceExhausted &&
         identity_limited.offers.empty());
  small = limits();
  small.discovery.interrupted = interrupted;
  const auto stopped = fixture.offer(small);
  assert(stopped.status == CycleStatus::Interrupted && stopped.offers.empty());
  fixture.unchanged();

  // A session belongs to the kernel that committed through it.
  assert(fixture.apply(offered).status == CycleStatus::Applied);
  Fixture other;
  const auto rebound = tseitin_cycle_breaking_apply(
      other.kernel, fixture.session, limits(), other.only_offer());
  assert(rebound.status == CycleStatus::Stale);
  other.unchanged();

  // A residual touch on a component edge invalidates its offer.
  Fixture touched;
  const std::string before = touched.only_offer();
  kernel_add_binary_clause(touched.kernel, internalize(1), internalize(8),
                           ++touched.kernel.next_clause_id);
  touched.mark();
  assert(touched.apply(before).status == CycleStatus::Stale);
  touched.unchanged();
}

TEST(every_precommit_phase_is_interruptible) {
  for (int phase = 0; phase < 3; ++phase) {
    Fixture fixture;
    auto bounded = limits();
    (phase == 0   ? bounded.admission.interrupted
     : phase == 1 ? bounded.planning.interrupted
                  : bounded.transaction.interrupted) = interrupted;
    const auto stopped = fixture.offer(bounded);
    assert(stopped.status == CycleStatus::Interrupted && stopped.offers.empty());
    fixture.unchanged();
  }
  // Counted polls through a complete offer or apply; `stop` interrupts at
  // that poll.
  const auto counted = [](uint64_t stop, InterruptState &state) {
    state = {0, stop};
    auto bounded = limits();
    bounded.discovery.interrupted = interrupt_after;
    bounded.discovery.interrupt_state = &state;
    return bounded;
  };
  InterruptState state;
  uint64_t identity_polls = 0, apply_polls = 0;
  {
    Fixture fixture;
    assert(fixture.offer(counted(UINT64_MAX, state)).status == CycleStatus::Applied);
    identity_polls = state.calls;
  }
  {
    // The last offer poll falls inside identity encoding.
    Fixture fixture;
    const auto result = fixture.offer(counted(identity_polls, state));
    assert(result.status == CycleStatus::Interrupted && result.effort.identity_work > 0);
    fixture.unchanged();
  }
  {
    Fixture fixture;
    const std::string offered = fixture.only_offer();
    assert(fixture.apply(offered, counted(UINT64_MAX, state)).status ==
           CycleStatus::Applied);
    apply_polls = state.calls;
  }
  bool journal_stopped = false;
  for (uint64_t threshold = 1; threshold <= apply_polls && !journal_stopped;
       ++threshold) {
    Fixture fixture;
    const std::string offered = fixture.only_offer();
    const auto result = fixture.apply(offered, counted(threshold, state));
    if (result.status == CycleStatus::Interrupted && result.effort.journal_work) {
      fixture.unchanged();
      journal_stopped = true;
    }
  }
  assert(journal_stopped);
}

TEST(selected_apply_is_independent_of_offer_aggregate_limits) {
  Fixture fixture(true);
  const auto offered = fixture.offer();
  assert(offered.offers.size() == 2);
  auto selected_limits = limits();
  selected_limits.maximum_components = 1;
  selected_limits.maximum_total_identity_bytes = 1;
  const auto incomplete_offer = fixture.offer(selected_limits);
  assert(incomplete_offer.status == CycleStatus::ResourceExhausted &&
         incomplete_offer.offers.empty() &&
         incomplete_offer.effort.candidates == 2);
  assert(fixture.apply(offered.offers.back(), selected_limits).status ==
         CycleStatus::Applied);
}

TEST(semantic_duplicate_key_is_storage_order_independent) {
  Fixture ordered(false, false), reversed(false, true);
  assert(ordered.apply(ordered.only_offer()).status == CycleStatus::Applied);
  assert(reversed.apply(reversed.only_offer()).status == CycleStatus::Applied);
  assert(ordered.session.keys().size() == 1 &&
         ordered.session.keys() == reversed.session.keys());
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
  const std::string offered = fixture.only_offer();
  const auto applied = fixture.apply(offered);
  assert(applied.status == CycleStatus::Applied &&
         applied.effort.identity_bytes == offered.size());
  record_tseitin_cycle_breaking_feedback(fixture.kernel, applied);
  const std::string journal = stream_contents(fixture.journal);
  proof.flush();
  fixture.kernel.proof = nullptr;
  const std::string bytes = read_file(file.path);
  assert(!bytes.empty());
  export_to(export_variable, bytes);
  return {applied, fixture.kernel.stats, journal};
}

TEST(dsr_and_unsupported_proof_contract) {
  const auto run = apply_with_proof(ProofFormat::DsrAscii,
                                    "TACTIX_TEST_TSEITIN_DSR_ASCII");
  // The planned accounting matches the helper route the DSR checkers replay.
  for (const auto &effort :
       {run.applied.effort,
        apply_with_proof(ProofFormat::DsrBinary, "TACTIX_TEST_TSEITIN_DSR_BINARY")
            .applied.effort})
    assert(effort.proof_additions == 3 && effort.proof_deletions == 2 &&
           effort.proof_literal_occurrences == 5 && effort.proof_helpers == 2 &&
           effort.proof_only_variables == 1 &&
           effort.proof_witness_literals == 5 && effort.proof_routes == 1);
  assert(run.stats.tseitin_cycle_proof_additions == 3);
  assert(run.journal.find("\"deletion_state\":\"helpers-removed\"") !=
         std::string::npos);

  TemporaryFile file;
  Fixture fixture;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  fixture.kernel.proof = &proof;
  const auto rejected = fixture.offer();
  assert(rejected.status == CycleStatus::UnsupportedProof && rejected.offers.empty());
  assert(fixture.kernel.trail.empty());
  fixture.kernel.proof = nullptr;
}

Kernel parsed_triangle() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  for (const auto &equation : tseitin_test::triangle)
    for (const auto &clause : parity_certificate::xor_equation(
             equation.variables.data(), equation.variables.size(), equation.rhs)) {
      std::vector<Lit> literals;
      for (uint32_t i = 0; i < clause.size; ++i)
        literals.push_back(kernel_internalize(kernel, clause.lits[i]));
      ingest_clause(kernel, literals);
    }
  finish_input(kernel);
  return kernel;
}

void add_triangle(tactix_c_handle *solver) {
  for (const auto &equation : tseitin_test::triangle)
    for (const auto &clause : parity_certificate::xor_equation(
             equation.variables.data(), equation.variables.size(), equation.rhs)) {
      for (uint32_t i = 0; i < clause.size; ++i) tactix_add(solver, clause.lits[i]);
      tactix_add(solver, 0);
    }
}

TEST(c_v7_is_backward_compatible_and_deep_copies_identity) {
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
  assert(offered.status == CycleStatus::Applied && offered.offers.size() == 1);

  tactix_c_handle *solver = tactix_init();
  FILE *records = std::tmpfile();
  assert(records && tactix_bind_structural_provenance(solver, records) == 0);
  add_triangle(solver);
  tactix_online_step_v7 step{};
  auto &base = step.base.base.base.base.base.base;
  base.step_id = 27;
  base.rule_id = 38;
  base.parameters.kind = TACTIX_ONLINE_PARAMETERS_TSEITIN_CYCLE_BREAKING;
  const TseitinCycleBreakingParameters p;
  base.parameters.value.tseitin_cycle_breaking = {
      p.maximum_snapshot_clauses, p.maximum_snapshot_literals,
      p.maximum_snapshot_work, p.maximum_snapshot_variables,
      p.maximum_discovery_work, p.maximum_discovery_clauses,
      p.maximum_discovery_groups, p.maximum_discovery_retained_bytes,
      p.maximum_admission_work, p.maximum_planning_work,
      p.maximum_witness_length, p.maximum_planning_additions,
      p.maximum_transaction_work, p.maximum_transaction_additions,
      p.maximum_transaction_deletions, p.maximum_transaction_helpers,
      p.maximum_transaction_witness_literals, p.maximum_validation_ticks,
      p.maximum_components, p.maximum_identity_bytes,
      p.maximum_total_identity_bytes, p.maximum_identity_work,
      p.maximum_journal_bytes, p.maximum_session_keys, p.maximum_session_work,
      p.maximum_session_identity_bytes};
  base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  base.repeat_limit = 1;
  base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  base.budget = 1;
  std::string identity = offered.offers.front();
  const tactix_online_tseitin_cycle_instance selected{identity.c_str()};
  step.tseitin_cycle_instances = &selected;
  step.tseitin_cycle_instance_count = 1;
  tactix_online_program_v7 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v7(solver, &program, 90,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  std::fill(identity.begin(), identity.end(), 'x');
  tactix_online_result applied{};
  assert(tactix_online_advance(solver, &applied) == 0);
  assert(applied.rule_id == 38 && applied.status == TACTIX_ONLINE_COMPLETED);
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
  tactix_online_program_v7 pair_program{pair, 2, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v7(ordinary, &pair_program, 93,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_online_step_v6 v6_step = propagation.base;
  tactix_online_program_v6 v6_program{&v6_step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure_v6(ordinary, &v6_program, 94,
                                    TACTIX_ONLINE_RECONFIGURE_RESET) == 0);
  tactix_release(ordinary);

  tactix_c_handle *invalid = tactix_init();
  add_triangle(invalid);
  const auto rejected = [&](uint64_t generation) {
    assert(tactix_online_configure_v7(invalid, &program, generation,
                                      TACTIX_ONLINE_RECONFIGURE_RESET) == -1);
  };
  for (const auto &[count, generation] :
       {std::pair{0, 93}, std::pair{-1, 93}, std::pair{2, 94}}) {
    step.tseitin_cycle_instance_count = count;
    rejected(generation);
  }
  step.tseitin_cycle_instance_count = 1;
  step.tseitin_cycle_instances = nullptr;
  rejected(95);
  const tactix_online_tseitin_cycle_instance null_identity{nullptr};
  step.tseitin_cycle_instances = &null_identity;
  rejected(95);
  char unterminated[9];
  std::fill(std::begin(unterminated), std::end(unterminated), 'a');
  tactix_online_tseitin_cycle_instance too_long{unterminated};
  step.tseitin_cycle_instances = &too_long;
  base.parameters.value.tseitin_cycle_breaking.maximum_identity_bytes = 8;
  rejected(96);
  tactix_release(invalid);
  source_kernel.structural_provenance = nullptr;
  std::fclose(source_records);
}

TEST(propagated_root_rebuilds_the_complete_component_census) {
  Journaled fixture(20);
  Kernel &kernel = fixture.kernel;
  add_equations(kernel, wide_cycle);
  add_equations(kernel, wide_cycle, 6);
  TemporaryFile file;
  Proof proof;
  assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
  attach(proof, kernel);
  const auto initial = fixture.offer();
  assert(initial.status == CycleStatus::Applied && initial.offers.size() == 2);
  assert(fixture.apply(initial.offers.front()).status == CycleStatus::Applied);
  assert(!kernel_propagation_fixpoint(kernel));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(kernel));
  const auto fresh = fixture.offer();
  assert(fresh.status == CycleStatus::Applied && fresh.offers.size() == 2);
  const uint64_t before_stale = kernel.next_clause_id;
  assert(fixture.apply(initial.offers.back()).status == CycleStatus::Stale &&
         kernel.next_clause_id == before_stale);
  assert(fixture.apply(fresh.offers.back()).status == CycleStatus::Applied &&
         fixture.session.keys().size() == 2);
  const std::string records = stream_contents(fixture.journal);
  assert(std::count(records.begin(), records.end(), '\n') == 2);
  export_to("TACTIX_TEST_TSEITIN_REPEAT_JOURNAL", records);
  proof.flush();
  kernel.proof = nullptr;
  export_to("TACTIX_TEST_TSEITIN_REPEAT_DSR", read_file(file.path));
}

// The cycle the live adapter prepares for the single component of `kernel`.
std::vector<int32_t> prepared_cycle(Kernel &kernel) {
  tseitin_cycle_live::Snapshot snapshot;
  assert(tseitin_cycle_live::capture(kernel, limits().snapshot, snapshot).status ==
         tseitin_cycle_live::Status::Captured);
  std::vector<tseitin_cycle_discovery::Plan> plans;
  assert(tseitin_cycle_discovery::enumerate(snapshot, limits().discovery, plans)
             .discovered() &&
         plans.size() == 1);
  tseitin_cycle_live::Prepared prepared;
  assert(tseitin_cycle_discovery::prepare(kernel, plans.front(), request(),
                                          prepared)
             .status == tseitin_cycle_live::Status::Prepared);
  return prepared.transaction().identity.cycle_edges;
}

// With one edge assigned at the root, the action still offers and applies a
// cycle; an assigned cycle edge moves preparation to an alternative cycle.
TEST(assigned_edges_select_an_available_cycle) {
  Kernel probe;
  kernel_reserve_vars(probe, 12);
  add_equations(probe, wide_cycle);
  const auto cycle = prepared_cycle(probe);
  int32_t off_cycle = 0;
  for (const auto &equation : wide_cycle)
    for (int32_t edge : equation.variables)
      if (std::find(cycle.begin(), cycle.end(), edge) == cycle.end())
        off_cycle = edge;
  assert(off_cycle && !cycle.empty());
  for (const int32_t assigned : {off_cycle, cycle.back()}) {
    Journaled fixture(12);
    add_equations(fixture.kernel, wide_cycle);
    kernel_assign_unit(fixture.kernel, internalize(assigned));
    assert(propagate_apply(fixture.kernel) == ApplyResult::Ok);
    if (assigned == cycle.back()) {
      const auto alternative = prepared_cycle(fixture.kernel);
      assert(std::find(alternative.begin(), alternative.end(), assigned) ==
             alternative.end());
    }
    assert(fixture.apply(fixture.only_offer()).status == CycleStatus::Applied);
  }
}

TEST(failed_component_encoding_consumes_identity_work) {
  Journaled fixture(20);
  add_equations(fixture.kernel, wide_cycle);
  add_equations(fixture.kernel, tseitin_test::triangle, 6);
  fixture.mark();
  const auto offered = fixture.offer();
  assert(offered.offers.size() == 2 &&
         offered.offers.front().size() > offered.offers.back().size());
  auto bounded = limits();
  bounded.maximum_identity_bytes = offered.offers.back().size();
  bounded.maximum_identity_work = 32;
  const auto exhausted = fixture.apply(offered.offers.back(), bounded);
  assert(exhausted.status == CycleStatus::ResourceExhausted &&
         exhausted.effort.identity_work > 0);
  fixture.unchanged();
}

} // namespace

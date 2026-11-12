#undef NDEBUG
#include "checker_certificate.hpp"
#include "dimacs_text.hpp"
#include "kernel_image.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> clauses() {
  return {{1, 2, 3},  {-1, -2}, {-1, -3}, {-2, -3},
          {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6},
          {-5, -7}, {-6, -7}, {-1, -7}, {-2, -4}, {-2, -6},
          {-2, -7}, {-3, -5}};
}

std::string dimacs(bool checker_suffix = false) {
  auto formula = clauses();
  if (checker_suffix)
    formula.insert(formula.end(), {{20, 21}, {-20, 21}, {20, -21}, {-20, -21}});
  return dimacs_text(checker_suffix ? 21 : 7, formula);
}

Kernel parse() { return parse_dimacs_text(dimacs(), ParsedRoot::EmptyTrail); }
DirectCspSupportActionSelection selection(uint32_t row) {
  return {{1, 2, 3}, {4, 5, 6, 7}, row};
}

DirectCspSupportActionSelection reversed_selection(uint32_t row) {
  return {{4, 5, 6, 7}, {1, 2, 3}, row};
}

DirectCspSupportActionKey action_key(
    const DirectCspSupportActionSelection &selected) {
  return {selected.left_values, selected.right_values,
          selected.selected_left_value,
          selected.left_values[selected.selected_left_value]};
}

DirectCspSupportActionLimits limits(bool extension = false) {
  DirectCspSupportActionLimits result;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_domain_values = 16;
  result.maximum_direct_clause_literals = extension ? 2 : 8;
  result.maximum_definition_clause_literals = 16;
  result.maximum_proof_work = 1000;
  result.maximum_proof_additions = 20;
  result.maximum_proof_literal_occurrences = 100;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = extension ? 1 : 0;
  result.maximum_external_extent = 100;
  result.maximum_session_keys = 16;
  result.maximum_session_work = 10000;
  result.allow_extension = extension;
  assert(result.valid());
  return result;
}
void attach(Kernel &kernel, FILE *records) {
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 40;
}

// A declined action also leaves the action sequence and candidate flag alone.
struct State {
  Image kernel;
  uint64_t action_sequence = 0;
  bool candidate_pending = false;
  bool operator==(const State &) const = default;
};

State state(const Kernel &kernel) {
  return {image(kernel), kernel.structural_action_sequence,
          kernel.semantic_variable_candidate_pending};
}

// A parsed fixture with attached records and a fresh action session.
struct Fixture {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  DirectCspSupportActionSession session;
  Fixture() {
    assert(records);
    attach(kernel, records);
  }
  ~Fixture() { std::fclose(records); }
  DirectCspSupportActionResult apply(const DirectCspSupportActionLimits &bounds,
                                     const DirectCspSupportActionSelection &selected) {
    return direct_csp_support_action_apply(kernel, session, bounds, selected);
  }
};

void direct_then_extension_and_durable_replay() {
  Fixture f;
  Kernel &kernel = f.kernel;
  DirectCspSupportActionSession &session = f.session;
  const auto first = f.apply(limits(), selection(1));
  assert(first.status == DirectCspSupportActionStatus::Applied &&
         first.result == ApplyResult::Ok);
  assert(first.proposed_action_id == 41 && first.committed_action_id == 41 &&
         first.semantic_id == 41 && first.key == action_key(selection(1)));
  assert(first.owner_retained && session.retained() &&
         session.belongs_to(kernel) && session.committed_keys() == 1 &&
         session.retained_owners() == 1 &&
         session.snapshot(selection(1).left_values,
                          selection(1).right_values) &&
         session.snapshot(selection(1).left_values,
                          selection(1).right_values)
                 ->source.size() == clauses().size());
  assert(first.effort.owner_capture_work && first.effort.recognition_work &&
         first.effort.emitted_additions == 1 &&
         first.live.published_support == std::vector<int32_t>({-2, 5}));
  assert(first.semantic_candidate_ready &&
         first.semantic_variables.size() == 2 &&
         !first.projection.extension_variable);
  for (const auto &variable : first.semantic_variables)
    assert(variable.role == strategy::SemanticVariableRole::ExistingTheory &&
           variable.priority == 1);
  assert(!kernel.semantic_variable_candidate_pending);

  const auto second = f.apply(limits(true), selection(0));
  assert(second.status == DirectCspSupportActionStatus::Applied &&
         second.result == ApplyResult::Ok &&
         second.proposed_action_id == 42 && second.committed_action_id == 42 &&
         second.effort.owner_capture_work == 0 &&
         second.effort.extent_growth == 1 && kernel.nvars == 8 &&
         kernel_num_output_vars(kernel) == 7);
  assert(session.committed_keys() == 2 &&
         session.retained_owners() == 1 &&
         session.keys()[0] == action_key(selection(0)) &&
         session.keys()[1] == action_key(selection(1)));
  assert(second.live.published_support == std::vector<int32_t>({-1, 8}) &&
         second.semantic_variables.size() == 5);
  assert(second.semantic_variables.back().external == 8 &&
         second.semantic_variables.back().role ==
             strategy::SemanticVariableRole::IntroducedTheory);
  assert(second.projection.extension_variable == 8 &&
         second.projection.definition_inputs ==
             std::vector<int32_t>({4, 5, 6}) &&
         second.projection.exact_or_definition &&
         second.projection.project_from_external_model &&
         !second.projection.reconstruction_witness_required);

  // The support clause can disappear without reopening its semantic action.
  assert(kernel_delete_binary(kernel, kernel_internalize(kernel, -1),
                              kernel_internalize(kernel, 8)));
  const State before = state(kernel);
  const long journal = position(f.records);
  const auto repeated = f.apply(limits(true), selection(0));
  assert(repeated.status ==
             DirectCspSupportActionStatus::AlreadyPublished &&
         repeated.result == ApplyResult::NoOp &&
         repeated.key == action_key(selection(0)) &&
         !repeated.proposed_action_id && !repeated.committed_action_id &&
         repeated.committed_keys == 2 && state(kernel) == before &&
         position(f.records) == journal);
}

void ordered_domain_pairs_have_distinct_durable_keys() {
  Fixture f;
  const auto forward = selection(1);
  const auto reverse = reversed_selection(1);
  const auto first = f.apply(limits(), forward);
  assert(first.status == DirectCspSupportActionStatus::Applied &&
         first.live.published_support == std::vector<int32_t>({-2, 5}) &&
         first.key == action_key(forward) && first.retained_owners == 1);

  // The same row ordinal in the reverse direction selects external literal 5
  // from a different left domain and publishes a different support clause.
  const auto second = f.apply(limits(), reverse);
  assert(second.status == DirectCspSupportActionStatus::Applied &&
         second.live.published_support ==
             std::vector<int32_t>({-5, 1, 2}) &&
         second.key == action_key(reverse) && second.key != first.key &&
         second.effort.owner_capture_work && second.retained_owners == 2 &&
         f.session.retained_owners() == 2 && f.session.committed_keys() == 2);
  assert(f.session.snapshot(forward.left_values, forward.right_values) &&
         f.session.snapshot(reverse.left_values, reverse.right_values));

  const State before = state(f.kernel);
  const long journal = position(f.records);
  for (const auto &selected : {reverse, forward}) {
    const auto replay = f.apply(limits(), selected);
    assert(replay.status == DirectCspSupportActionStatus::AlreadyPublished &&
           replay.key == action_key(selected) && state(f.kernel) == before &&
           position(f.records) == journal);
  }
}

// Each fresh-session decline leaves the kernel, records and session untouched.
DirectCspSupportActionResult fresh_declines_are_atomic(
    Fixture &f, const DirectCspSupportActionLimits &bounds,
    const DirectCspSupportActionSelection &selected,
    DirectCspSupportActionStatus expected) {
  const State before = state(f.kernel);
  const auto result = f.apply(bounds, selected);
  assert(result.status == expected && result.result == ApplyResult::NoOp &&
         !f.session.retained() && state(f.kernel) == before &&
         !position(f.records));
  return result;
}

void declines_are_atomic_and_sessions_are_kernel_local() {
  {
    Fixture f;
    fresh_declines_are_atomic(f, limits(), selection(3),
                              DirectCspSupportActionStatus::Invalid);
    auto tiny = limits();
    tiny.maximum_session_work = 1;
    fresh_declines_are_atomic(f, tiny, selection(1),
                              DirectCspSupportActionStatus::ResourceExhausted);
    // Fresh-session owner capture: source caps and recognition work.
    for (int mode = 0; mode < 3; ++mode) {
      auto bounded = limits();
      if (mode == 0) bounded.maximum_source_clauses = clauses().size() - 1;
      if (mode == 1) bounded.maximum_source_literals = 34;
      if (mode == 2) bounded.maximum_recognition_work = 1;
      const auto declined = fresh_declines_are_atomic(
          f, bounded, selection(1), DirectCspSupportActionStatus::ResourceExhausted);
      assert(mode == 2 ||
             declined.live.status == DirectCspSupportLiveStatus::ResourceExhausted);
    }
  }
  {
    Fixture f;
    volatile sig_atomic_t stop = 1;
    f.kernel.stop = &stop;
    fresh_declines_are_atomic(f, limits(), selection(1),
                              DirectCspSupportActionStatus::Interrupted);
    f.kernel.stop = nullptr;
  }
  {
    // A missing domain AMO leaves no recognizable relation.
    Fixture f;
    assert(kernel_delete_binary(f.kernel, kernel_internalize(f.kernel, -1),
                                kernel_internalize(f.kernel, -2)));
    fresh_declines_are_atomic(f, limits(), selection(1),
                              DirectCspSupportActionStatus::NoStructure);
  }
  {
    Fixture first;
    assert(first.apply(limits(), selection(1)).status ==
           DirectCspSupportActionStatus::Applied);
    Fixture second;
    const State before = state(second.kernel);
    const auto stale = direct_csp_support_action_apply(
        second.kernel, first.session, limits(), selection(2));
    assert(stale.status == DirectCspSupportActionStatus::Stale &&
           stale.result == ApplyResult::NoOp && state(second.kernel) == before &&
           first.session.belongs_to(first.kernel) &&
           first.session.committed_keys() == 1 && !position(second.records));
  }
}

void retained_payload_and_source_are_exact() {
  Fixture f;
  assert(f.apply(limits(), selection(1)).status ==
         DirectCspSupportActionStatus::Applied);

  auto changed = selection(2);
  std::swap(changed.right_values[0], changed.right_values[1]);
  const State before_mismatch = state(f.kernel);
  const long before_journal = position(f.records);
  const auto mismatch = f.apply(limits(), changed);
  assert(mismatch.status == DirectCspSupportActionStatus::Invalid &&
         mismatch.result == ApplyResult::NoOp &&
         state(f.kernel) == before_mismatch &&
         position(f.records) == before_journal &&
         f.session.committed_keys() == 1);

  assert(kernel_delete_binary(f.kernel, kernel_internalize(f.kernel, -1),
                              kernel_internalize(f.kernel, -2)));
  const State before_stale = state(f.kernel);
  const auto stale = f.apply(limits(), selection(2));
  assert(stale.status == DirectCspSupportActionStatus::Stale &&
         stale.result == ApplyResult::NoOp && state(f.kernel) == before_stale &&
         position(f.records) == before_journal &&
         f.session.committed_keys() == 1);
}

void smaller_later_limits_exhaust_valid_retained_state() {
  Fixture f;
  assert(f.apply(limits(), selection(1)).status ==
         DirectCspSupportActionStatus::Applied);
  assert(f.apply(limits(), selection(2)).status ==
         DirectCspSupportActionStatus::Applied);
  assert(f.session.committed_keys() == 2 && f.session.retained_owners() == 1);

  const State before = state(f.kernel);
  const long journal = position(f.records);
  const auto *owner = f.session.snapshot(selection(0).left_values,
                                         selection(0).right_values);
  assert(owner && owner->source_literals > 1);
  for (int mode = 0; mode < 5; ++mode) {
    auto bounded = limits();
    switch (mode) {
    case 0: bounded.maximum_source_clauses = clauses().size() - 1; break;
    case 1: bounded.maximum_source_literals = owner->source_literals - 1; break;
    case 2: bounded.maximum_domain_values = 3; break;
    case 3: bounded.maximum_session_keys = 1; break;
    case 4: bounded.maximum_session_work = 1; break;
    }
    const auto result = f.apply(bounded, selection(0));
    assert(result.status == DirectCspSupportActionStatus::ResourceExhausted &&
           result.live.status ==
               DirectCspSupportLiveStatus::ResourceExhausted &&
           result.result == ApplyResult::NoOp && state(f.kernel) == before &&
           position(f.records) == journal && f.session.committed_keys() == 2 &&
           f.session.retained_owners() == 1);
  }
}

void malformed_retained_key_is_rejected_before_live_work() {
  Fixture f;
  assert(f.apply(limits(), selection(1)).status ==
         DirectCspSupportActionStatus::Applied);

  // Deliberate corruption: apply validates retained identities rather than
  // trusting their order or the redundant selected-literal field.
  auto &corrupted = const_cast<std::vector<DirectCspSupportActionKey> &>(
      f.session.keys());
  corrupted[0].selected_left_literal = 99;
  const State before = state(f.kernel);
  const long journal = position(f.records);
  auto bounded = limits();
  bounded.maximum_source_clauses = 1;
  bounded.maximum_source_literals = 1;
  bounded.maximum_domain_values = 2;
  const auto rejected = f.apply(bounded, selection(2));
  assert(rejected.status == DirectCspSupportActionStatus::Invalid &&
         rejected.live.status == DirectCspSupportLiveStatus::Invalid &&
         rejected.result == ApplyResult::NoOp && state(f.kernel) == before &&
         position(f.records) == journal && f.session.committed_keys() == 1);
}

void unsupported_proof_is_inapplicable() {
  const char *const path = "/tmp/direct-csp-action-decline.pbp";
  Fixture f;
  Proof proof;
  std::remove(path);
  assert(proof.open(path, ProofFormat::PbpStreaming));
  proof.finish_input_transaction();
  assert(proof.begin_pb());
  proof.bind_variable_map(&f.kernel.internal_to_external);
  proof.bind_external_variable_map(&f.kernel.external_to_internal);
  f.kernel.proof = &proof;
  fresh_declines_are_atomic(f, limits(), selection(1),
                            DirectCspSupportActionStatus::Inapplicable);
  f.kernel.proof = nullptr;
}

void projection_preserves_an_original_model() {
  Fixture f;
  const auto result = f.apply(limits(true), selection(0));
  assert(result.status == DirectCspSupportActionStatus::Applied &&
         result.projection.exact_or_definition);

  // left=1 and right=4 is an original-formula model.  The exact-OR extension
  // is true because right=4 is true, and forgetting it recovers that model.
  const std::vector<bool> value{false, true, false, false, true,
                                false, false, false, true};
  const auto satisfied = [&](const std::vector<int32_t> &clause) {
    return std::any_of(clause.begin(), clause.end(), [&](int32_t literal) {
      const bool assigned = value[static_cast<size_t>(
          literal < 0 ? -int64_t(literal) : int64_t(literal))];
      return literal > 0 ? assigned : !assigned;
    });
  };
  for (const auto &clause : clauses()) assert(satisfied(clause));
  assert(satisfied(result.live.published_support));
  assert(value[result.projection.extension_variable] ==
         std::any_of(result.projection.definition_inputs.begin(),
                     result.projection.definition_inputs.end(),
                     [&](int32_t literal) {
                       assert(literal > 0);
                       return value[static_cast<size_t>(literal)];
                     }));
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  Kernel kernel = parse();
  kernel.next_clause_id = clauses().size() + 4;
  DirectCspSupportActionSession session;
  write_checker_certificate(cnf_path, dimacs(true), proof_path, format, kernel, 20,
                            [&](FILE *records) {
    attach(kernel, records);
    const auto result = direct_csp_support_action_apply(
        kernel, session, limits(true), selection(0));
    assert(result.status == DirectCspSupportActionStatus::Applied &&
           result.effort.emitted_additions == 5);
  });
}

} // namespace

// CTest direct_csp_support_proof_model passes UNSAT.cnf UNSAT.dsr SAT.cnf
// SAT.model to check the emitted certificate and a model of the formula.
int main(int argc, char **argv) {
  direct_then_extension_and_durable_replay();
  ordered_domain_pairs_have_distinct_durable_keys();
  declines_are_atomic_and_sessions_are_kernel_local();
  retained_payload_and_source_are_exact();
  smaller_later_limits_exhaust_valid_retained_state();
  malformed_retained_key_is_rejected_before_live_work();
  unsupported_proof_is_inapplicable();
  projection_preserves_an_original_model();
  if (argc == 5) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_file(argv[3], dimacs(false));
    write_file(argv[4], "s SATISFIABLE\nv 1 -2 -3 4 -5 -6 -7 0\n");
  }
  std::puts("test_direct_csp_support_action: passed");
}

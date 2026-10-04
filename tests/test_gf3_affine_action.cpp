#undef NDEBUG
#include "rules/semantic_extension/gf3_affine_action.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> formula(bool complete = true) {
  std::vector<std::vector<int32_t>> result;
  std::vector<std::vector<int32_t>> values;
  for (int32_t domain = 0; domain < 7; ++domain) {
    const int32_t first = 3 * domain + 1;
    values.push_back({first, first + 1, first + 2});
    result.push_back(values.back());
    for (uint8_t left = 0; left < 3; ++left)
      for (uint8_t right = left + 1; right < 3; ++right)
        result.push_back({-values.back()[left], -values.back()[right]});
  }
  const auto equation = [&](const std::vector<uint32_t> &domains) {
    for (uint64_t tuple = 0; tuple < 27; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (uint32_t domain : domains) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>((sum + value) % 3);
        clause.push_back(-values[domain][value]);
      }
      if (sum) result.push_back(std::move(clause));
    }
  };
  equation({0, 1, 2});
  equation({2, 3, 4});
  equation({4, 5, 6});
  if (!complete) result.pop_back();
  return result;
}

std::string dimacs(bool complete = true, bool checker_suffix = false) {
  const auto clauses = formula(complete);
  std::ostringstream output;
  output << "p cnf " << (checker_suffix ? 31 : 21) << ' '
         << clauses.size() + (checker_suffix ? 4 : 0) << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  if (checker_suffix)
    output << "30 31 0\n-30 31 0\n30 -31 0\n-30 -31 0\n";
  return output.str();
}

Kernel parse(bool complete = true) {
  const std::string text = dimacs(complete);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

Gf3AffineActionLimits limits() {
  Gf3AffineActionLimits result;
  result.recognition = {2000, 20000, 100, 100, 6, 1000};
  result.elimination = {8, 10000, 1000, 10000};
  result.maximum_recognition_work = 1000000;
  result.maximum_planning_work = 1000000;
  result.maximum_duplicate_scan_work = 1000000;
  result.maximum_revalidation_work = 1000000;
  result.maximum_proof_work = 100000;
  result.maximum_proof_additions = 1000;
  result.maximum_proof_literal_occurrences = 10000;
  result.maximum_arena_words = 10000;
  result.maximum_journal_bytes = 1000000;
  result.maximum_external_extent = 100;
  result.maximum_session_keys = 100;
  result.maximum_session_work = 10000;
  assert(result.valid());
  return result;
}

void attach(Kernel &kernel, FILE *records) {
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 80;
}

struct Image {
  uint64_t next_clause_id = 0;
  uint64_t action_sequence = 0;
  size_t arena_words = 0;
  size_t trail = 0;
  uint64_t arena_generation = 0;
  bool candidate_pending = false;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.next_clause_id = kernel.next_clause_id;
  result.action_sequence = kernel.structural_action_sequence;
  result.arena_words = kernel.arena.size();
  result.trail = kernel.trail.size();
  result.arena_generation = kernel.arena_generation;
  result.candidate_pending = kernel.semantic_variable_candidate_pending;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &first, const Image &second) {
  return first.next_clause_id == second.next_clause_id &&
         first.action_sequence == second.action_sequence &&
         first.arena_words == second.arena_words &&
         first.trail == second.trail &&
         first.arena_generation == second.arena_generation &&
         first.candidate_pending == second.candidate_pending &&
         first.watches == second.watches;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

void delete_published(Kernel &kernel, const std::vector<uint64_t> &ids) {
  uint64_t deleted = 0;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!clause->garbage() &&
        std::find(ids.begin(), ids.end(), clause->id()) != ids.end()) {
      kernel_delete_clause(kernel, clause);
      ++deleted;
    }
  }
  assert(deleted == ids.size());
}

void success_replay_advice_and_repeated_pairs() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;

  const auto first =
      gf3_affine_action_apply(kernel, session, limits(), 0, 1);
  assert(first.status == Gf3AffineActionStatus::Applied &&
         first.result == ApplyResult::Ok);
  assert(first.proposed_action_id == 81 && first.committed_action_id == 81 &&
         first.semantic_id == 81);
  assert((first.key == Gf3AffineActionKey{0, 1}));
  assert(first.owner_retained && session.retained() &&
         session.belongs_to(kernel) && session.committed_keys() == 1);
  assert(session.snapshot() &&
         session.snapshot()->sources.size() == formula().size() &&
         session.snapshot()->theory.equations.size() == 3);
  assert(first.effort.owner_capture_work && first.effort.recognition_work &&
         first.effort.emitted_additions == 54 &&
         first.live.consequence.domains ==
             std::vector<uint32_t>({0, 1, 3, 4}));
  assert(first.semantic_candidate_ready &&
         first.semantic_candidates.size() == 12 &&
         !kernel.semantic_variable_candidate_pending);
  for (const auto &variable : first.semantic_candidates)
    assert(variable.role ==
               strategy::SemanticVariableRole::ExistingTheory &&
           variable.priority == 1);

  // Publication and propagation remain separate controller actions.
  for (int32_t selected : {1, 4, 10}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, selected));
    assert(propagate_service(kernel) == ApplyResult::Ok);
  }
  assert(kernel.value(kernel_internalize(kernel, 13)) > 0);
  while (kernel.level) kernel_backtrack(kernel, kernel.level - 1);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));

  // A different caller-selected pair reuses and revalidates the exact owner.
  const auto second =
      gf3_affine_action_apply(kernel, session, limits(), 1, 2);
  assert(second.status == Gf3AffineActionStatus::Applied &&
         second.committed_action_id == 82 && second.owner_retained &&
         second.effort.owner_capture_work == 0 &&
         second.live.consequence.domains ==
             std::vector<uint32_t>({2, 3, 5, 6}));
  assert((session.committed_keys() == 2 &&
          session.keys()[0] == Gf3AffineActionKey{0, 1} &&
          session.keys()[1] == Gf3AffineActionKey{1, 2}));

  // Retained identity, rather than incidental clause liveness, suppresses the
  // replay after every clause published by this pair has been deleted.
  delete_published(kernel, second.live.published_clause_ids);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  // Reverse parent order is the same durable semantic selection.  It consumes
  // neither an action id nor provenance bytes and never enters the live layer.
  const Image before = image(kernel);
  const long journal = position(records);
  const auto repeated =
      gf3_affine_action_apply(kernel, session, limits(), 2, 1);
  assert((repeated.status == Gf3AffineActionStatus::AlreadyPublished &&
          repeated.result == ApplyResult::NoOp &&
          repeated.key == Gf3AffineActionKey{1, 2}));
  assert(repeated.proposed_action_id == 83 && repeated.semantic_id == 83 &&
         !repeated.committed_action_id && repeated.committed_keys == 2 &&
         image(kernel) == before && position(records) == journal);
  std::fclose(records);
}

void declines_are_atomic_and_sessions_are_kernel_local() {
  {
    Kernel kernel = parse(false);
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto declined =
        gf3_affine_action_apply(kernel, session, limits(), 0, 1);
    assert(declined.status == Gf3AffineActionStatus::NoStructure &&
           declined.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
  {
    Kernel first_kernel = parse();
    FILE *first_records = std::tmpfile();
    assert(first_records);
    attach(first_kernel, first_records);
    Gf3AffineActionSession session;
    assert(gf3_affine_action_apply(first_kernel, session, limits(), 0, 1)
               .status == Gf3AffineActionStatus::Applied);

    Kernel second_kernel = parse();
    FILE *second_records = std::tmpfile();
    assert(second_records);
    attach(second_kernel, second_records);
    const Image before = image(second_kernel);
    const auto stale =
        gf3_affine_action_apply(second_kernel, session, limits(), 1, 2);
    assert(stale.status == Gf3AffineActionStatus::Stale &&
           stale.result == ApplyResult::NoOp && image(second_kernel) == before &&
           session.belongs_to(first_kernel) && session.committed_keys() == 1 &&
           !position(second_records));
    std::fclose(second_records);
    std::fclose(first_records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto invalid =
        gf3_affine_action_apply(kernel, session, limits(), 1, 1);
    assert(invalid.status == Gf3AffineActionStatus::Invalid &&
           image(kernel) == before && !session.retained() &&
           !position(records));
    auto tiny = limits();
    tiny.maximum_session_work = 1;
    const auto exhausted =
        gf3_affine_action_apply(kernel, session, tiny, 0, 1);
    assert(exhausted.status == Gf3AffineActionStatus::ResourceExhausted &&
           image(kernel) == before && !session.retained() &&
           !position(records));
    std::fclose(records);
  }
}

void retained_owner_is_revalidated_before_a_novel_pair() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  assert(gf3_affine_action_apply(kernel, session, limits(), 0, 1).status ==
         Gf3AffineActionStatus::Applied);
  const auto source = std::find_if(
      session.snapshot()->sources.begin(), session.snapshot()->sources.end(),
      [](const Gf3AffineLiveSource &item) {
        return item.kind == Gf3AffineSourceKind::Large;
      });
  assert(source != session.snapshot()->sources.end());
  Clause *retained = clause_at(kernel.arena, source->ref);
  assert(!retained->garbage() && retained->id() == source->native_proof_id);
  kernel_delete_clause(kernel, retained);
  const Image before = image(kernel);
  const long journal = position(records);
  const auto stale =
      gf3_affine_action_apply(kernel, session, limits(), 1, 2);
  assert(stale.status == Gf3AffineActionStatus::Stale &&
         stale.result == ApplyResult::NoOp && session.committed_keys() == 1 &&
         image(kernel) == before && position(records) == journal);
  std::fclose(records);
}

void proof_modes_and_guard() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/gf3-action-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/gf3-action-decline.pbp"}}) {
    Kernel kernel = parse();
    Proof proof;
    std::remove(path);
    if (format == ProofFormat::Pbp) {
      assert(proof.open(path, ProofFormat::Automatic));
      proof.finish_input_transaction();
      assert(proof.begin_pb());
    } else {
      assert(proof.open(path, format));
    }
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto result =
        gf3_affine_action_apply(kernel, session, limits(), 0, 1);
    assert(result.status == Gf3AffineActionStatus::Inapplicable &&
           result.result == ApplyResult::NoOp && image(kernel) == before &&
           !session.retained() && !position(records));
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const std::string checker = dimacs(true, true);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) == checker.size());
  std::fclose(cnf);

  Kernel kernel = parse();
  kernel.next_clause_id = formula().size() + 4;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  const auto result =
      gf3_affine_action_apply(kernel, session, limits(), 0, 1);
  assert(result.status == Gf3AffineActionStatus::Applied &&
         result.effort.emitted_additions == 54);
  const int32_t unit = 30;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  success_replay_advice_and_repeated_pairs();
  declines_are_atomic_and_sessions_are_kernel_local();
  retained_owner_is_revalidated_before_a_novel_pair();
  proof_modes_and_guard();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_gf3_affine_action: passed");
}

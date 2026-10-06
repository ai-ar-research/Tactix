#undef NDEBUG
#include "kernel/clause.hpp"
#include "rules/semantic_extension/affine_matrix_transaction.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>

using namespace tactix::affine_semantic_extension;

static Equation equation(std::initializer_list<External> variables, bool rhs) {
  Equation result;
  result.variables = variables;
  result.rhs = rhs;
  return result;
}

static Equation authenticated_equation(
    std::initializer_list<External> variables, bool rhs,
    uint64_t first_occurrence) {
  Equation result = equation(variables, rhs);
  result.premise_clauses =
      tactix::affine_semantic_extension::clausal_form(result.variables, rhs);
  for (size_t i = 0; i < result.premise_clauses.size(); ++i)
    result.premises.push_back(first_occurrence + i);
  return result;
}

static MatrixLimits matrix_limits() {
  return {1000000, 64, 64, 64, 64, 64};
}

static MatrixTransactionLimits transaction_limits() {
  return {10000000, 100000, 100000, 10000, 10000};
}

static std::vector<Equation> sources() {
  return {authenticated_equation({1, 2, 3, 4, 9, 10}, false, 100),
          authenticated_equation({5, 6, 7, 8, 9, 10}, true, 200)};
}

static bool same_equation(const Equation &first, const Equation &second) {
  return first.variables == second.variables && first.rhs == second.rhs &&
         first.premises == second.premises &&
         first.premise_clauses == second.premise_clauses;
}

static void event_fixture(AffineMatrix &matrix, MatrixEvent &event) {
  const auto input = sources();
  assert(build_affine_matrix(input, 991, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  std::vector<MatrixAssignment> trail;
  for (External variable = 1; variable <= 8; ++variable)
    trail.push_back({variable, false});
  assert(matrix.synchronize(trail, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict);
  assert((event.row.source_equations == std::vector<size_t>{0, 1}));
  assert((event.reason ==
          std::vector<External>{1, 2, 3, 4, 5, 6, 7, 8}));
}

static AffineMatrixTransaction prepared() {
  AffineMatrix matrix;
  MatrixEvent event;
  event_fixture(matrix, event);
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, transaction_limits(),
             [](uint64_t) { return true; }, transaction) ==
         MatrixTransactionStatus::Prepared);
  return transaction;
}

static void complete_plan_accounts_and_retains_only_reason() {
  const auto transaction = prepared();
  assert(transaction.ready());
  assert(transaction.owner_identity() == 991);
  assert(transaction.additions() && transaction.deletions());
  assert(transaction.helper_extent());
  assert(transaction.arena_words() == 8 + tactix::CLAUSE_HEADER_WORDS);
  assert(transaction.work());
  assert((transaction.source_equations() == std::vector<size_t>{0, 1}));
  assert(transaction.source_identity().size() == 2);
  uint64_t additions = 0, deletions = 0, installs = 0;
  for (const MatrixProofEvent &event : transaction.proof_events()) {
    if (event.kind == MatrixProofEvent::Kind::Delete) {
      assert(event.id >= 1000 && event.deleted_id > 1000);
      ++deletions;
    } else
      ++additions;
    if (event.install) {
      ++installs;
      assert(event.kind == MatrixProofEvent::Kind::Rup);
      assert(event.clause == transaction.reason());
    }
  }
  assert(additions == transaction.additions());
  assert(deletions == transaction.deletions());
  assert(installs == 1);
}

static void implication_event_uses_the_same_general_bridge() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 3, 4}, false, 300),
      authenticated_equation({2, 3, 4, 5}, false, 400)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 17, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 17, input, event, 5, 2000, transaction_limits(),
             [](uint64_t) { return true; }, transaction) ==
         MatrixTransactionStatus::Prepared);
  assert((transaction.reason() == std::vector<External>{1, 2, -5}));
  uint64_t installs = 0;
  for (const MatrixProofEvent &proof : transaction.proof_events())
    if (proof.install) {
      ++installs;
      assert(proof.kind == MatrixProofEvent::Kind::Rup);
      assert(proof.clause == transaction.reason());
    }
  assert(installs == 1);
}

static void root_unit_reuses_the_accumulated_clause() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 2, 3}, false, 500),
      authenticated_equation({1, 2, 4}, false, 504),
      authenticated_equation({3, 4, 5}, true, 508)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 18, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize({}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication &&
         (event.reason == std::vector<External>{5}));

  AffineMatrixProofCache cache;
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 18, input, event, 5, 3000, transaction_limits(),
             [](uint64_t) { return true; }, transaction, &cache, 0) ==
         MatrixTransactionStatus::Prepared);
  uint64_t installs = 0;
  for (const MatrixProofEvent &proof : transaction.proof_events())
    if (proof.install) {
      ++installs;
      assert(proof.clause == event.reason);
    }
  assert(installs == 1);
}

static void empty_root_conflict_is_a_publishable_reason() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 2, 3}, false, 600),
      authenticated_equation({1, 2, 3}, true, 604)};
  AffineMatrix matrix;
  assert(build_affine_matrix(input, 19, matrix_limits(),
                             [](uint64_t) { return true; }, matrix) ==
         MatrixStatus::Complete);
  MatrixEvent event;
  assert(matrix.synchronize({}, [](uint64_t) { return true; }, event) ==
         MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Conflict && event.reason.empty());

  AffineMatrixProofCache cache;
  MatrixProofCacheEntry retained;
  retained.entry_id = 1;
  retained.owner_identity = 19;
  retained.proof_epoch = 7;
  retained.support_ids = {3999};
  assert(cache.commit(std::move(retained), 0));
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 19, input, event, 3, 4000, transaction_limits(),
             [](uint64_t) { return true; }, transaction, &cache, 7) ==
         MatrixTransactionStatus::Prepared);
  uint64_t installs = 0;
  for (const MatrixProofEvent &proof : transaction.proof_events())
    if (proof.install) {
      ++installs;
      assert(proof.clause.empty());
    }
  assert(installs == 1 && transaction.reason().empty() &&
         !transaction.proof_cache_insert() &&
         transaction.proof_cache_generation() == cache.generation() &&
         transaction.proof_cache_entry().proof_epoch == 7);
}

static void exact_identity_and_atomic_failure() {
  AffineMatrix matrix;
  MatrixEvent event;
  event_fixture(matrix, event);
  const AffineMatrixTransaction sentinel = prepared();
  const auto unchanged = [&](const AffineMatrixTransaction &candidate) {
    assert(candidate.ready() == sentinel.ready());
    assert(candidate.owner_identity() == sentinel.owner_identity());
    assert(candidate.additions() == sentinel.additions());
    assert(candidate.deletions() == sentinel.deletions());
    assert(candidate.helper_extent() == sentinel.helper_extent());
    assert(candidate.arena_words() == sentinel.arena_words());
    assert(candidate.reason() == sentinel.reason());
    assert(candidate.source_equations() == sentinel.source_equations());
    assert(candidate.source_identity().size() ==
           sentinel.source_identity().size());
    for (size_t i = 0; i < candidate.source_identity().size(); ++i)
      assert(same_equation(candidate.source_identity()[i],
                           sentinel.source_identity()[i]));
    assert(candidate.proof_events() == sentinel.proof_events());
  };

  auto exact = sources();
  exact[0].rhs = !exact[0].rhs;
  auto output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, exact, event, 10, 1000, transaction_limits(),
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::Stale);
  unchanged(output);
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 992, sources(), event, 10, 1000, transaction_limits(),
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::Stale);
  unchanged(output);
  MatrixEvent tampered = event;
  tampered.reason[0] = -tampered.reason[0];
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), tampered, 10, 1000,
             transaction_limits(), [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::Stale);
  unchanged(output);

  const auto successful = prepared();
  const std::vector<uint64_t> allowances = {
      0, successful.work() / 2, successful.work() - 1};
  for (uint64_t allowance : allowances) {
    uint64_t used = 0;
    output = sentinel;
    const auto status = prepare_affine_matrix_transaction(
        matrix, 991, sources(), event, 10, 1000, transaction_limits(),
        [&](uint64_t amount) {
          if (used > allowance || amount > allowance - used) return false;
          used += amount;
          return true;
        },
        output);
    assert(status == MatrixTransactionStatus::Interrupted);
    unchanged(output);
  }

  auto bounded = transaction_limits();
  bounded.maximum_proof_additions = successful.additions() - 1;
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, bounded,
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::ResourceExhausted);
  unchanged(output);
  bounded = transaction_limits();
  bounded.maximum_proof_deletions = successful.deletions() - 1;
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, bounded,
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::ResourceExhausted);
  unchanged(output);
  bounded = transaction_limits();
  bounded.maximum_helper_extent = successful.helper_extent() - 1;
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, bounded,
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::ResourceExhausted);
  unchanged(output);
  bounded = transaction_limits();
  bounded.maximum_arena_words = successful.arena_words() - 1;
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, bounded,
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::ResourceExhausted);
  unchanged(output);

  // This bound passes owner/current-event authentication and the fixed request
  // charges, then stops while accounting the first selected source's full CNF.
  // Premise canonicalization must therefore participate in the same atomic
  // work contract as certificate construction.
  bounded = transaction_limits();
  bounded.maximum_work = 650;
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, bounded,
             [](uint64_t) { return true; }, output) ==
         MatrixTransactionStatus::ResourceExhausted);
  unchanged(output);
}

static void incomplete_or_forged_premises_are_unsupported() {
  auto reject = [](std::vector<Equation> input) {
    AffineMatrix matrix;
    assert(build_affine_matrix(input, 818, matrix_limits(),
                               [](uint64_t) { return true; }, matrix) ==
           MatrixStatus::Complete);
    std::vector<MatrixAssignment> trail;
    for (External variable = 1; variable <= 8; ++variable)
      trail.push_back({variable, false});
    MatrixEvent event;
    assert(matrix.synchronize(trail, [](uint64_t) { return true; }, event) ==
           MatrixStatus::Complete);
    AffineMatrixTransaction output = prepared();
    const auto before = output.proof_events();
    assert(prepare_affine_matrix_transaction(
               matrix, 818, input, event, 10, 1000, transaction_limits(),
               [](uint64_t) { return true; }, output) ==
           MatrixTransactionStatus::UnsupportedProof);
    assert(output.proof_events() == before && output.ready());
  };

  auto missing = sources();
  missing[0].premise_clauses.clear();
  reject(std::move(missing));
  auto tampered = sources();
  tampered[0].premise_clauses[0][0] =
      -tampered[0].premise_clauses[0][0];
  reject(std::move(tampered));
  auto duplicate = sources();
  duplicate[1].premises[0] = duplicate[0].premises[0];
  reject(std::move(duplicate));
}

static int write_proof(const char *cnf_path, const char *proof_path) {
  const auto transaction = prepared();
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fprintf(cnf, "p cnf %u 72\n", 10 + transaction.helper_extent());
  for (const Equation &source : sources()) {
    const auto clauses = tactix::parity_certificate::xor_equation(
        source.variables.data(), source.variables.size(), source.rhs);
    for (const auto &clause : clauses) {
      for (uint32_t i = 0; i < clause.size; ++i)
        std::fprintf(cnf, "%d ", clause.lits[i]);
      std::fprintf(cnf, "0\n");
    }
  }
  for (External variable = 1; variable <= 8; ++variable)
    std::fprintf(cnf, "-%d 0\n", variable);
  std::fclose(cnf);

  FILE *proof = std::fopen(proof_path, "w");
  assert(proof);
  for (const MatrixProofEvent &event : transaction.proof_events()) {
    if (event.kind == MatrixProofEvent::Kind::Delete) std::fprintf(proof, "d ");
    for (External literal : event.clause) std::fprintf(proof, "%d ", literal);
    std::fprintf(proof, "0\n");
  }
  std::fprintf(proof, "0\n");
  std::fclose(proof);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4 && std::strcmp(argv[1], "--proof") == 0)
    return write_proof(argv[2], argv[3]);
  complete_plan_accounts_and_retains_only_reason();
  implication_event_uses_the_same_general_bridge();
  root_unit_reuses_the_accumulated_clause();
  empty_root_conflict_is_a_publishable_reason();
  exact_identity_and_atomic_failure();
  incomplete_or_forged_premises_are_unsupported();
  std::puts("test_affine_matrix_transaction: passed");
}

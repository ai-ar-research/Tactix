#undef NDEBUG
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "kernel/clause.hpp"
#include "rules/semantic_extension/affine_matrix_transaction.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

using namespace tactix::affine_semantic_extension;

// A charge callback that admits all work.
static bool ample(uint64_t) { return true; }

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

// The matrix of input under matrix_limits(), which must admit it completely.
static AffineMatrix built(const std::vector<Equation> &input, uint64_t owner) {
  AffineMatrix matrix;
  assert(build_affine_matrix(input, owner, matrix_limits(), ample, matrix) ==
         MatrixStatus::Complete);
  return matrix;
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

// x1 = .. = x8 = 0, under which sources() conflict.
static std::vector<MatrixAssignment> zero_trail() {
  std::vector<MatrixAssignment> trail;
  for (External variable = 1; variable <= 8; ++variable)
    trail.push_back({variable, false});
  return trail;
}

static void event_fixture(AffineMatrix &matrix, MatrixEvent &event) {
  const auto input = sources();
  assert(build_affine_matrix(input, 991, matrix_limits(),
                             ample, matrix) == MatrixStatus::Complete);
  const auto trail = zero_trail();
  assert(matrix.synchronize(trail, ample, event) == MatrixStatus::Complete);
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
             ample, transaction) == MatrixTransactionStatus::Prepared);
  return transaction;
}

// Exactly one proof event installs the reason, as a RUP addition when rup.
static void single_install(const AffineMatrixTransaction &transaction,
                           const std::vector<External> &reason, bool rup = true) {
  uint64_t installs = 0;
  for (const MatrixProofEvent &proof : transaction.proof_events())
    if (proof.install) {
      ++installs;
      assert(!rup || proof.kind == MatrixProofEvent::Kind::Rup);
      assert(proof.clause == reason);
    }
  assert(installs == 1);
}

static void invalid_request_reports_failure_stage() {
  AffineMatrix matrix;
  MatrixEvent event;
  AffineMatrixTransaction transaction;
  MatrixTransactionFailureStage stage =
      MatrixTransactionFailureStage::ProofPopulation;
  assert(prepare_affine_matrix_transaction(
             matrix, 0, {}, event, 0, 0, {}, {}, transaction, nullptr, 0,
             nullptr, true, false, false, &stage) ==
         MatrixTransactionStatus::Invalid);
  assert(stage == MatrixTransactionFailureStage::Request);
}

static void historical_event_requires_explicit_authentication_mode() {
  AffineMatrix matrix;
  MatrixEvent historical;
  event_fixture(matrix, historical);
  MatrixEvent current;
  assert(matrix.synchronize({}, ample, current) == MatrixStatus::Complete);
  assert(current.kind == MatrixEventKind::None);
  AffineMatrixTransaction ordinary;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), historical, 10, 1000,
             transaction_limits(), ample, ordinary) ==
         MatrixTransactionStatus::Stale);
  AffineMatrixTransaction retained;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), historical, 10, 1000,
             transaction_limits(), ample, retained,
             nullptr, 0, nullptr, true, false, true) ==
         MatrixTransactionStatus::Prepared);
  assert(retained.reason() == historical.reason && retained.ready());
}

static void compact_one_shot_may_borrow_source_identity() {
  AffineMatrix matrix;
  MatrixEvent event;
  event_fixture(matrix, event);
  tactix::PbpParityReasonPlan plan;
  plan.valid = true;
  plan.starting_external_extent = 10;
  plan.proof_constraints = 1;
  AffineMatrixTransaction borrowed;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, transaction_limits(),
             ample, borrowed, nullptr, 0, &plan,
             false) == MatrixTransactionStatus::Prepared);
  assert(borrowed.compact_pbp() && borrowed.source_identity().empty());
  AffineMatrixTransaction retained;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), event, 10, 1000, transaction_limits(),
             ample, retained, nullptr, 0, &plan) ==
         MatrixTransactionStatus::Prepared);
  assert(retained.compact_pbp() &&
         retained.source_identity().size() == event.row.source_equations.size());
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
  uint64_t additions = 0, deletions = 0;
  for (const MatrixProofEvent &event : transaction.proof_events()) {
    if (event.kind == MatrixProofEvent::Kind::Delete) {
      assert(event.id >= 1000 && event.deleted_id > 1000);
      ++deletions;
    } else
      ++additions;
  }
  assert(additions == transaction.additions());
  assert(deletions == transaction.deletions());
  single_install(transaction, transaction.reason());
}

static void retained_source_authentication_preserves_work() {
  AffineMatrix matrix;
  MatrixEvent event;
  event_fixture(matrix, event);
  AffineMatrixTransaction copied, retained;
  uint64_t copied_charges = 0, retained_charges = 0;
  const auto input = sources();
  assert(prepare_affine_matrix_transaction(
             matrix, 991, input, event, 10, 1000, transaction_limits(),
             [&](uint64_t) { ++copied_charges; return true; }, copied) ==
         MatrixTransactionStatus::Prepared);
  assert(prepare_affine_matrix_transaction(
             matrix, 991, matrix.authenticated_sources(), event, 10, 1000,
             transaction_limits(),
             [&](uint64_t) { ++retained_charges; return true; }, retained,
             nullptr, 0, nullptr, true, true) ==
         MatrixTransactionStatus::Prepared);
  uint64_t audited_authentication = 0;
  assert(matrix.authenticates(
      991, matrix.authenticated_sources(), event,
      [&](uint64_t amount) { audited_authentication += amount; return true; }));
  assert(retained.authentication_work() == audited_authentication);
  assert(retained.authentication_work() == copied.authentication_work());
  assert(retained.proof_events() == copied.proof_events());
  assert(retained_charges < copied_charges);
}

static void implication_event_uses_the_same_general_bridge() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 3, 4}, false, 300),
      authenticated_equation({2, 3, 4, 5}, false, 400)};
  AffineMatrix matrix = built(input, 17);
  MatrixEvent event;
  assert(matrix.synchronize({{1, false}, {2, false}},
                            ample, event) == MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication);
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 17, input, event, 5, 2000, transaction_limits(),
             ample, transaction) == MatrixTransactionStatus::Prepared);
  assert((transaction.reason() == std::vector<External>{1, 2, -5}));
  single_install(transaction, transaction.reason());
}

static void root_unit_reuses_the_accumulated_clause() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 2, 3}, false, 500),
      authenticated_equation({1, 2, 4}, false, 504),
      authenticated_equation({3, 4, 5}, true, 508)};
  AffineMatrix matrix = built(input, 18);
  MatrixEvent event;
  assert(matrix.synchronize({}, ample, event) == MatrixStatus::Complete);
  assert(event.kind == MatrixEventKind::Implication &&
         (event.reason == std::vector<External>{5}));

  AffineMatrixProofCache cache;
  AffineMatrixTransaction transaction;
  assert(prepare_affine_matrix_transaction(
             matrix, 18, input, event, 5, 3000, transaction_limits(),
             ample, transaction, &cache, 0) ==
         MatrixTransactionStatus::Prepared);
  single_install(transaction, event.reason, false);
}

// A cached plan resumes the next event's row from the committed tree.
static void isolated_symmetric_difference_resume(
    AffineMatrixTransaction *first_out = nullptr,
    AffineMatrixTransaction *second_out = nullptr) {
  AffineMatrix matrix;
  const auto input = sources();
  assert(build_affine_matrix(input, 991, matrix_limits(),
                             ample, matrix) == MatrixStatus::Complete);
  MatrixEvent first_event;
  assert(matrix.synchronize({{1,false},{2,false},{3,false},{4,false},{9,false}},
                            ample, first_event) == MatrixStatus::Complete);
  AffineMatrixProofCache cache;
  AffineMatrixTransaction first;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, input, first_event, 10, 1000,
             transaction_limits(), ample,
             first, &cache, 0) == MatrixTransactionStatus::Prepared);
  assert(first.proof_cache_insert());
  assert(cache.commit(MatrixProofCacheEntry(first.proof_cache_entry()), 0));
  MatrixEvent second_event;
  const auto trail = zero_trail();
  assert(matrix.synchronize(trail, ample,
                            second_event) == MatrixStatus::Complete);
  AffineMatrixTransaction second;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, input, second_event, 10+first.helper_extent(),
             1000+first.additions(), transaction_limits(),
             ample, second, &cache, 0) == MatrixTransactionStatus::Prepared);
  AffineMatrixTransaction fresh;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, input, second_event, 10+first.helper_extent(),
             1000+first.additions(), transaction_limits(),
             ample, fresh) == MatrixTransactionStatus::Prepared);
  assert(second.proof_cache_resume());
  assert(second.additions() < fresh.additions());
  if (first_out) *first_out = std::move(first);
  if (second_out) *second_out = std::move(second);
}

static std::vector<Equation> removal_sources() {
  return {authenticated_equation({2,3,4,7},false,1),
          authenticated_equation({5,6,7,9},true,9),
          authenticated_equation({2,3,6,8},true,17),
          authenticated_equation({2,4,7,8},false,25),
          authenticated_equation({1,2,5,9},true,33),
          authenticated_equation({1,3,6,7},false,41)};
}

// A resumed plan drops the cached row's source the next row lacks.
static void isolated_removed_source_resume(
    AffineMatrixTransaction *old_out = nullptr,
    AffineMatrixTransaction *new_out = nullptr) {
  const auto input=removal_sources();
  AffineMatrix matrix;
  assert(build_affine_matrix(input,991,matrix_limits(),
                             [](uint64_t){return true;},matrix)==
         MatrixStatus::Complete);
  MatrixEvent old_event;
  assert(matrix.synchronize({{1,true},{2,true},{5,false},{6,true},
                             {8,true},{9,true}},
                            [](uint64_t){return true;},old_event)==
         MatrixStatus::Complete);
  assert((old_event.row.source_equations==std::vector<size_t>{0,2,3}));
  AffineMatrixProofCache cache;
  AffineMatrixTransaction old_tx;
  assert(prepare_affine_matrix_transaction(
             matrix,991,input,old_event,9,52,transaction_limits(),
             [](uint64_t){return true;},old_tx,&cache,0)==
         MatrixTransactionStatus::Prepared);
  assert(old_tx.proof_cache_insert());
  assert(cache.commit(MatrixProofCacheEntry(old_tx.proof_cache_entry()),0));
  MatrixEvent new_event;
  assert(matrix.synchronize({{4,false},{5,true},{8,false},{9,false}},
                            [](uint64_t){return true;},new_event)==
         MatrixStatus::Complete);
  assert((new_event.row.source_equations==std::vector<size_t>{0,1,2}));
  AffineMatrixTransaction new_tx;
  assert(prepare_affine_matrix_transaction(
             matrix,991,input,new_event,9+old_tx.helper_extent(),
             52+old_tx.additions(),transaction_limits(),
             [](uint64_t){return true;},new_tx,&cache,0)==
         MatrixTransactionStatus::Prepared);
  AffineMatrixTransaction fresh;
  assert(prepare_affine_matrix_transaction(
             matrix,991,input,new_event,9+old_tx.helper_extent(),
             52+old_tx.additions(),transaction_limits(),
             [](uint64_t){return true;},fresh)==
         MatrixTransactionStatus::Prepared);
  assert(new_tx.proof_cache_resume() &&
         (new_tx.resume_source_equations()==std::vector<size_t>{3}));
  if (old_out) *old_out = std::move(old_tx);
  if (new_out) *new_out = std::move(new_tx);
}

static void empty_root_conflict_is_a_publishable_reason() {
  const std::vector<Equation> input = {
      authenticated_equation({1, 2, 3}, false, 600),
      authenticated_equation({1, 2, 3}, true, 604)};
  AffineMatrix matrix = built(input, 19);
  MatrixEvent event;
  assert(matrix.synchronize({}, ample, event) == MatrixStatus::Complete);
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
             ample, transaction, &cache, 7) ==
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
             ample, output) == MatrixTransactionStatus::Stale);
  unchanged(output);
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 992, sources(), event, 10, 1000, transaction_limits(),
             ample, output) == MatrixTransactionStatus::Stale);
  unchanged(output);
  MatrixEvent tampered = event;
  tampered.reason[0] = -tampered.reason[0];
  output = sentinel;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, sources(), tampered, 10, 1000,
             transaction_limits(), ample, output) ==
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

  // maximum_work 650 passes owner/current-event authentication and the fixed
  // request charges, then stops while accounting the first source's CNF:
  // premise canonicalization is inside the same atomic work contract.
  for (int bound = 0; bound < 5; ++bound) {
    auto bounded = transaction_limits();
    switch (bound) {
    case 0: bounded.maximum_proof_additions = successful.additions() - 1; break;
    case 1: bounded.maximum_proof_deletions = successful.deletions() - 1; break;
    case 2: bounded.maximum_helper_extent = successful.helper_extent() - 1; break;
    case 3: bounded.maximum_arena_words = successful.arena_words() - 1; break;
    case 4: bounded.maximum_work = 650; break;
    }
    output = sentinel;
    assert(prepare_affine_matrix_transaction(
               matrix, 991, sources(), event, 10, 1000, bounded,
               ample, output) == MatrixTransactionStatus::ResourceExhausted);
    unchanged(output);
  }
}

static void incomplete_or_forged_premises_are_unsupported() {
  auto reject = [](std::vector<Equation> input) {
    AffineMatrix matrix = built(input, 818);
    const auto trail = zero_trail();
    MatrixEvent event;
    assert(matrix.synchronize(trail, ample, event) == MatrixStatus::Complete);
    AffineMatrixTransaction output = prepared();
    const auto before = output.proof_events();
    assert(prepare_affine_matrix_transaction(
               matrix, 818, input, event, 10, 1000, transaction_limits(),
               ample, output) == MatrixTransactionStatus::UnsupportedProof);
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
  auto duplicate_clause = sources();
  duplicate_clause[0].premise_clauses[1] =
      duplicate_clause[0].premise_clauses[0];
  reject(std::move(duplicate_clause));
  auto duplicate_coordinate = sources();
  duplicate_coordinate[0].premise_clauses[0][1] =
      duplicate_coordinate[0].premise_clauses[0][0];
  reject(std::move(duplicate_coordinate));

  // Authentication is set-based: arbitrary clause and literal order remains
  // accepted exactly as it was by the canonical-copy implementation.
  auto reordered = sources();
  for (auto &source : reordered) {
    std::reverse(source.premises.begin(), source.premises.end());
    std::reverse(source.premise_clauses.begin(),
                 source.premise_clauses.end());
    for (auto &clause : source.premise_clauses)
      std::reverse(clause.begin(), clause.end());
  }
  AffineMatrix matrix = built(reordered, 818);
  const auto trail = zero_trail();
  MatrixEvent event;
  assert(matrix.synchronize(trail, ample, event) == MatrixStatus::Complete);
  AffineMatrixTransaction output;
  assert(prepare_affine_matrix_transaction(
             matrix, 818, reordered, event, 10, 1000, transaction_limits(),
             ample, output) == MatrixTransactionStatus::Prepared);
}

static void matrix_source_authentication_certificate_is_exact() {
  const auto valid = sources();
  AffineMatrix matrix = built(valid, 991);
  assert(matrix.proof_sources_authenticated());
  const auto trail = zero_trail();
  MatrixEvent event;
  assert(matrix.synchronize(trail, ample, event) == MatrixStatus::Complete);
  AffineMatrixTransaction copied, retained;
  assert(prepare_affine_matrix_transaction(
             matrix, 991, valid, event, 10, 1000, transaction_limits(),
             ample, copied) == MatrixTransactionStatus::Prepared);
  assert(prepare_affine_matrix_transaction(
             matrix, 991, matrix.authenticated_sources(), event, 10, 1000,
             transaction_limits(), ample, retained) ==
         MatrixTransactionStatus::Prepared);
  assert(copied.work() == retained.work());
  assert(copied.proof_events() == retained.proof_events());

  auto malformed = valid;
  malformed[0].premise_clauses[0][0] =
      -malformed[0].premise_clauses[0][0];
  AffineMatrix invalid = built(malformed, 992);
  assert(!invalid.proof_sources_authenticated());
  MatrixEvent invalid_event;
  assert(invalid.synchronize(trail, ample,
                             invalid_event) == MatrixStatus::Complete);
  AffineMatrixTransaction rejected;
  assert(prepare_affine_matrix_transaction(
             invalid, 992, invalid.authenticated_sources(), invalid_event, 10,
             1000, transaction_limits(), ample,
             rejected) == MatrixTransactionStatus::UnsupportedProof);
}

// The CNF of input's premise clauses and the root units over variables, and
// the transactions' proof lines in order, closed by the empty clause.
static void write_pair(const std::string &cnf_path, const std::string &proof_path,
                       uint32_t variables, const std::vector<Equation> &input,
                       const std::vector<External> &units,
                       std::initializer_list<const AffineMatrixTransaction *> transactions) {
  std::vector<std::vector<External>> clauses;
  for (const Equation &source : input)
    clauses.insert(clauses.end(), source.premise_clauses.begin(),
                   source.premise_clauses.end());
  for (External unit : units) clauses.push_back({unit});
  write_file(cnf_path.c_str(), dimacs_text(variables, clauses));
  std::string proof;
  for (const AffineMatrixTransaction *transaction : transactions)
    for (const MatrixProofEvent &event : transaction->proof_events()) {
      if (event.kind == MatrixProofEvent::Kind::Delete) proof += "d ";
      for (External literal : event.clause) proof += std::to_string(literal) + ' ';
      proof += "0\n";
    }
  write_file(proof_path.c_str(), proof + "0\n");
}

static const std::vector<External> zero_units{-1, -2, -3, -4, -5, -6, -7, -8};

// The UNSAT x1..x8 = 0 instance of sources() with prepared()'s proof lines.
static int write_proof(const char *cnf_path, const char *proof_path) {
  const AffineMatrixTransaction transaction = prepared();
  write_pair(cnf_path, proof_path, 10 + transaction.helper_extent(), sources(),
             zero_units, {&transaction});
  return 0;
}

// DIR/resume and DIR/removed: each resume fixture's cached and resumed
// transactions streamed as one refutation of its sources and root units.
static int write_resume_proofs(const std::string &directory) {
  AffineMatrixTransaction first, second;
  isolated_symmetric_difference_resume(&first, &second);
  write_pair(directory + "/resume.cnf", directory + "/resume.dsr",
             10 + first.helper_extent() + second.helper_extent(), sources(),
             zero_units, {&first, &second});
  AffineMatrixTransaction old_tx, new_tx;
  isolated_removed_source_resume(&old_tx, &new_tx);
  write_pair(directory + "/removed.cnf", directory + "/removed.dsr",
             9 + old_tx.helper_extent() + new_tx.helper_extent(),
             removal_sources(), {-4, 5, -8, -9}, {&old_tx, &new_tx});
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4 && std::strcmp(argv[1], "--proof") == 0)
    return write_proof(argv[2], argv[3]);
  if (argc == 3 && std::strcmp(argv[1], "--resume-proofs") == 0)
    return write_resume_proofs(argv[2]);
  invalid_request_reports_failure_stage();
  complete_plan_accounts_and_retains_only_reason();
  historical_event_requires_explicit_authentication_mode();
  retained_source_authentication_preserves_work();
  compact_one_shot_may_borrow_source_identity();
  isolated_symmetric_difference_resume();
  isolated_removed_source_resume();
  {
    AffineMatrix matrix;
    MatrixEvent event;
    event_fixture(matrix, event);
    AffineMatrixProofCache cache;
    AffineMatrixTransaction transaction;
    assert(prepare_affine_matrix_transaction(
               matrix, 991, sources(), event, 10, 1000,
               transaction_limits(), ample,
               transaction, &cache, 0) == MatrixTransactionStatus::Prepared);
    assert(transaction.proof_cache_insert());
    assert(transaction.proof_cache_entry().tree.variable_count > 0);
    assert(!transaction.proof_cache_entry().tree.variables.empty());
  }
  implication_event_uses_the_same_general_bridge();
  root_unit_reuses_the_accumulated_clause();
  empty_root_conflict_is_a_publishable_reason();
  exact_identity_and_atomic_failure();
  incomplete_or_forged_premises_are_unsupported();
  matrix_source_authentication_certificate_is_exact();
  std::puts("test_affine_matrix_transaction: passed");
}

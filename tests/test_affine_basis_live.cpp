#include "unit_test.hpp"
#include "affine_formula_support.hpp"
#include "clause_oracle.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/affine_basis_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <map>

using namespace tactix::affine_semantic_extension;

static Kernel cancellation_fixture() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 6);
  add_equation(kernel, {1, 2, 3, 4, 5}, false);
  add_equation(kernel, {1, 2, 3, 4, 6}, true);
  return kernel;
}

static AffineBasisLiveRequest request(FILE *records) {
  AffineBasisLiveRequest result;
  result.image = {10000, 1000, 10000, 10000000, 8, 32, 32, 4};
  result.basis = {10000000, 32, 32, 10000, 10000, 3};
  result.maximum_snapshot_literals = 100000;
  result.maximum_snapshot_work = 1000000;
  result.maximum_proof_additions = 1000000;
  result.maximum_proof_deletions = 1000000;
  result.maximum_journal_bytes = 10000000;
  result.maximum_external_extent = 100000;
  result.action_id = 41;
  result.semantic_id = 7;
  result.records = records;
  return result;
}

// Bind `configured` to the owner snapshot and its planned row over
// `variables`.
static void select(Kernel &kernel, AffineBasisLiveRequest &configured,
                   AffineLiveSnapshot &owner, AffineBasisRowSelection &selection,
                   const std::vector<int32_t> &variables) {
  selection = planned_row(
      planned_rows(kernel, configured.image, configured.basis,
                   configured.maximum_snapshot_literals,
                   configured.maximum_snapshot_work, configured.records, owner),
      variables);
  configured.owner = &owner;
  configured.owner_version = 1;
  configured.selection = &selection;
}

using ProofClause = std::vector<int32_t>;

static ProofClause normalized(ProofClause clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

static bool tautological(const ProofClause &clause) {
  for (int32_t literal : clause)
    if (std::find(clause.begin(), clause.end(), -literal) != clause.end())
      return true;
  return false;
}

static bool rat(const std::vector<ProofClause> &database, const ProofClause &candidate) {
  if (candidate.empty()) return rup_implied(database, candidate);
  if (rup_implied(database, candidate)) return true;
  const int32_t pivot = candidate.front();
  for (const ProofClause &partner : database) {
    if (std::find(partner.begin(), partner.end(), -pivot) == partner.end())
      continue;
    ProofClause resolvent;
    for (int32_t literal : candidate)
      if (literal != pivot) resolvent.push_back(literal);
    for (int32_t literal : partner)
      if (literal != -pivot) resolvent.push_back(literal);
    std::sort(resolvent.begin(), resolvent.end());
    resolvent.erase(std::unique(resolvent.begin(), resolvent.end()),
                    resolvent.end());
    if (!tautological(resolvent) && !rup_implied(database, resolvent)) return false;
  }
  return true;
}

static std::vector<ProofClause> fixture_clauses(bool contradictory) {
  return xor_cnf({{{1, 2, 3, 4, 5}, false},
                  {{1, 2, 3, 4, contradictory ? 5 : 6}, true}});
}

static void verify_transaction_proof(
    const AffineBasisLiveTransaction &transaction,
    std::vector<ProofClause> database) {
  std::map<uint64_t, ProofClause> active;
  for (const auto &event : transaction.proof_events()) {
    if (event.kind == AffineBasisLiveTransaction::Event::Kind::Delete) {
      const auto found = active.find(event.deleted_id);
      assert(found != active.end());
      assert(normalized(found->second) == normalized(event.clause));
      const auto population = std::find_if(
          database.begin(), database.end(), [&](const ProofClause &clause) {
            return normalized(clause) == normalized(event.clause);
          });
      assert(population != database.end());
      database.erase(population);
      active.erase(found);
      continue;
    }
    assert(event.kind == AffineBasisLiveTransaction::Event::Kind::Rat
               ? rat(database, event.clause)
               : rup_implied(database, event.clause));
    database.push_back(event.clause);
    active.emplace(event.id, event.clause);
  }
}

// Resource declines are atomic; a prepared transaction is stale after a new
// clause id or a retired source.
TEST(atomic_declines) {
  for (int mode = 0; mode < 4; ++mode) {
    Kernel kernel = cancellation_fixture();
    FILE *records = temporary_records();
    auto configured = request(records);
    AffineLiveSnapshot owner;
    AffineBasisRowSelection selection;
    select(kernel, configured, owner, selection, {5, 6});
    if (mode == 0) configured.maximum_proof_additions = 1;
    if (mode == 1) configured.maximum_journal_bytes = 1;
    const auto mapping = kernel.external_to_internal;
    const auto ids = kernel.next_clause_id;
    const auto arena = kernel.arena.size();
    AffineBasisLiveTransaction transaction;
    auto result = prepare_affine_basis_live(kernel, configured, transaction);
    if (mode < 2) {
      assert(result.status == AffineBasisLiveStatus::ResourceExhausted &&
             !transaction.ready());
    } else {
      assert(result.status == AffineBasisLiveStatus::Prepared);
      if (mode == 3) clause_at(kernel.arena, 0)->flags |= tactix::Clause::GARBAGE;
      else ++kernel.next_clause_id;
      result = publish_affine_basis_live(kernel, configured, transaction);
      assert(result.status == AffineBasisLiveStatus::Stale);
    }
    assert(kernel.external_to_internal == mapping &&
           kernel.next_clause_id == ids + (mode == 2) &&
           kernel.arena.size() == arena && std::ftell(records) == 0);
    std::fclose(records);
  }
}

// A selected direct row whose source equations span more than the bounded
// proof scope (16 variables) declines atomically.
TEST(bounded_proof_scope) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 17);
  // Ascending pivots chain all four sources into the binary row {16, 17}.
  add_equation(kernel, {1, 2, 3, 4, 5, 6, 7, 8}, false);
  add_equation(kernel, {1, 2, 3, 4, 9, 10, 11, 12}, false);
  add_equation(kernel, {5, 6, 7, 8, 13, 14, 15, 16}, false);
  add_equation(kernel, {9, 10, 11, 12, 13, 14, 15, 17}, true);
  FILE *records = temporary_records();
  auto configured = request(records);
  AffineLiveSnapshot owner;
  AffineBasisRowSelection selection;
  select(kernel, configured, owner, selection, {16, 17});
  assert(selection.kind == BasisRowKind::DirectBinary &&
         selection.source_equation_ordinals.size() == 4);
  const auto mapping = kernel.external_to_internal;
  const auto ids = kernel.next_clause_id;
  const auto arena = kernel.arena.size();
  AffineBasisLiveTransaction transaction;
  const auto result = prepare_affine_basis_live(kernel, configured, transaction);
  assert(result.status == AffineBasisLiveStatus::UnsupportedProof &&
         !transaction.ready());
  assert(kernel.external_to_internal == mapping &&
         kernel.next_clause_id == ids && kernel.arena.size() == arena &&
         std::ftell(records) == 0);
  std::fclose(records);
}

TEST(exact_owned_row_selection) {
  Kernel kernel = cancellation_fixture();
  FILE *records = temporary_records();
  auto configured = request(records);
  AffineLiveSnapshot owner;
  AffineBasisRowSelection selection;
  select(kernel, configured, owner, selection, {5, 6});
  assert(selection.rhs);
  AffineBasisLiveTransaction transaction;
  const auto prepared =
      prepare_affine_basis_live(kernel, configured, transaction);
  assert(prepared.status == AffineBasisLiveStatus::Prepared);
  assert(prepared.rows == 1 && prepared.definitions == 0 &&
         prepared.extent_growth == 0 && prepared.selection == selection);
  verify_transaction_proof(transaction, fixture_clauses(false));

  auto forged = selection;
  forged.variables = {5, 7};
  configured.selection = &forged;
  AffineBasisLiveTransaction rejected;
  assert(prepare_affine_basis_live(kernel, configured, rejected).status ==
         AffineBasisLiveStatus::Stale);
  assert(!rejected.ready() && std::ftell(records) == 0);
  forged = selection;
  forged.source_equation_ordinals = {0};
  configured.selection = &forged;
  assert(prepare_affine_basis_live(kernel, configured, rejected).status ==
         AffineBasisLiveStatus::Stale);
  configured.selection = &selection;
  configured.owner_version = 2;
  assert(prepare_affine_basis_live(kernel, configured, rejected).status ==
         AffineBasisLiveStatus::Invalid);
  std::fclose(records);
}

static void exact_owned_wide_row_selection(const char *proof_path) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 15);
  const std::vector<int32_t> first{1, 2, 3, 4, 5, 6, 7, 8};
  const std::vector<int32_t> second{1, 9, 10, 11, 12, 13, 14, 15};
  add_equation(kernel, first, false);
  add_equation(kernel, second, true);
  Proof proof;
  bind_proof(proof, kernel, proof_path);
  FILE *records = temporary_records();
  auto configured = request(records);
  AffineLiveSnapshot owner;
  AffineBasisRowSelection selection;
  const std::vector<External> expected{2, 3, 4, 5, 6, 7, 8,
                                       9, 10, 11, 12, 13, 14, 15};
  select(kernel, configured, owner, selection, expected);
  assert(selection.kind == BasisRowKind::Extended && selection.rhs);

  AffineBasisLiveTransaction transaction;
  const auto prepared =
      prepare_affine_basis_live(kernel, configured, transaction);
  assert(prepared.status == AffineBasisLiveStatus::Prepared);
  assert(prepared.rows == 1 && prepared.definitions == expected.size() - 1);
  assert(prepared.extent_growth > prepared.definitions);
  assert(prepared.introduced_theory_variables.size() == prepared.definitions);
  std::map<int32_t, int32_t> retained_units;
  for (const auto &event : transaction.proof_events()) {
    if (event.kind == AffineBasisLiveTransaction::Event::Kind::Delete ||
        event.clause.size() != 1)
      continue;
    const int32_t literal = event.clause.front();
    const int32_t variable = std::abs(literal);
    const auto [found, inserted] = retained_units.emplace(variable, literal);
    assert(inserted || found->second == literal);
  }
  verify_transaction_proof(transaction, xor_cnf({{first, false}, {second, true}}));

  auto too_tight = configured;
  too_tight.basis.maximum_nodes = prepared.extent_growth - 1;
  AffineBasisLiveTransaction declined;
  assert(prepare_affine_basis_live(kernel, too_tight, declined).status ==
         AffineBasisLiveStatus::ResourceExhausted);
  assert(!declined.ready());
  auto work_tight = configured;
  work_tight.basis.maximum_work = 1000;
  assert(prepare_affine_basis_live(kernel, work_tight, declined).status ==
         AffineBasisLiveStatus::ResourceExhausted);
  assert(!declined.ready());

  const auto applied =
      publish_affine_basis_live(kernel, configured, transaction);
  assert(applied.status == AffineBasisLiveStatus::Applied &&
         applied.result == ApplyResult::Ok && applied.propagation_pending);
  assert(kernel.nvars == 15 + prepared.extent_growth &&
         kernel_num_output_vars(kernel) == 15);
  assert(propagate_service(kernel) == ApplyResult::Ok);
  const std::string journal = stream_contents(records);
  assert(journal.find("\"definitions\":13") != std::string::npos);
  assert(journal.find("affine-basis-helper-fix-rat") != std::string::npos);
  assert(journal.find("affine-basis-proof-rat") != std::string::npos);
  assert(journal.find("affine-basis-proof-helper") != std::string::npos);
  assert(journal.find("\"projection\":\"omit-extension\"") !=
         std::string::npos);
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

static void contradiction(const char *proof_path) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 5);
  add_equation(kernel, {1, 2, 3, 4, 5}, false);
  add_equation(kernel, {1, 2, 3, 4, 5}, true);
  Proof proof;
  bind_proof(proof, kernel, proof_path);
  FILE *records = temporary_records();
  auto configured = request(records);
  AffineLiveSnapshot owner;
  AffineBasisRowSelection selection;
  select(kernel, configured, owner, selection, {});
  AffineBasisLiveTransaction transaction;
  assert(prepare_affine_basis_live(kernel, configured, transaction).status ==
         AffineBasisLiveStatus::Prepared);
  verify_transaction_proof(transaction, fixture_clauses(true));
  const auto result = publish_affine_basis_live(kernel, configured, transaction);
  assert(result.status == AffineBasisLiveStatus::Applied);
  assert(result.result == ApplyResult::Unsat && result.contradiction);
  assert(kernel.inconsistent);
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

int main() {
  run_tests();
  exact_owned_wide_row_selection("/tmp/affine-basis-wide.dsr");
  contradiction("/tmp/affine-basis-unsat.dsr");
  std::puts("affine basis live: selected rows, wide definitions, stale/resource "
            "declines, and contradiction PASS");
}

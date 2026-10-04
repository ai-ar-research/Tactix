#undef NDEBUG
#include "rules/semantic_extension/affine_basis_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <string>

using namespace tactix;
using namespace tactix::affine_semantic_extension;

static void add_equation(Kernel &kernel, const std::vector<External> &variables,
                         bool rhs) {
  for (const auto &external : clausal_form(variables, rhs)) {
    std::vector<Lit> internal;
    for (External literal : external) internal.push_back(internalize(literal));
    if (internal.size() == 1) {
      kernel.unit_ids[var_of(internal[0])] = ++kernel.next_clause_id;
      kernel_assign_unit(kernel, internal[0]);
    } else if (internal.size() == 2) {
      kernel_add_binary_clause(kernel, internal[0], internal[1],
                               ++kernel.next_clause_id);
    } else {
      kernel_add_large_clause(kernel, internal.data(), internal.size(), false,
                              0, ++kernel.next_clause_id);
    }
  }
}

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

static std::string contents(FILE *file) {
  std::fflush(file);
  std::rewind(file);
  std::string result;
  char buffer[4096];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, count);
  return result;
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

static bool rup(const std::vector<ProofClause> &database, const ProofClause &candidate) {
  std::map<int32_t, int8_t> values;
  for (int32_t literal : candidate) {
    const int32_t variable = std::abs(literal);
    const int8_t value = literal > 0 ? -1 : 1;
    if (values[variable] && values[variable] != value) return true;
    values[variable] = value;
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const ProofClause &clause : database) {
      bool satisfied = false;
      uint32_t open = 0;
      int32_t unit = 0;
      for (int32_t literal : clause) {
        const int8_t value = values[std::abs(literal)];
        if (value && (value > 0) == (literal > 0)) {
          satisfied = true;
          break;
        }
        if (!value) {
          ++open;
          unit = literal;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        const int32_t variable = std::abs(unit);
        const int8_t value = unit > 0 ? 1 : -1;
        if (values[variable] && values[variable] != value) return true;
        if (!values[variable]) {
          values[variable] = value;
          changed = true;
        }
      }
    }
  }
  return false;
}

static bool rat(const std::vector<ProofClause> &database, const ProofClause &candidate) {
  if (candidate.empty()) return rup(database, candidate);
  if (rup(database, candidate)) return true;
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
    if (!tautological(resolvent) && !rup(database, resolvent)) return false;
  }
  return true;
}

static std::vector<ProofClause> fixture_clauses(bool contradictory) {
  std::vector<ProofClause> result = clausal_form({1, 2, 3, 4, 5}, false);
  auto second = clausal_form(
      contradictory ? std::vector<External>{1, 2, 3, 4, 5}
                    : std::vector<External>{1, 2, 3, 4, 6},
      true);
  result.insert(result.end(), second.begin(), second.end());
  return result;
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
               : rup(database, event.clause));
    database.push_back(event.clause);
    active.emplace(event.id, event.clause);
  }
}

static void successful_basis(bool with_proof, const char *proof_path) {
  Kernel kernel = cancellation_fixture();
  Proof proof;
  if (with_proof) {
    assert(proof.open(proof_path, ProofFormat::DratAscii));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
  }
  FILE *records = std::tmpfile();
  assert(records);
  const auto applied = apply_affine_basis_live(kernel, request(records));
  assert(applied.status == AffineBasisLiveStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.extent_growth == 4);
  assert(applied.definitions == 4);
  assert(applied.existing_theory_variables ==
         std::vector<int32_t>({1, 2, 3, 4, 5, 6}));
  assert(applied.introduced_theory_variables ==
         std::vector<int32_t>({7, 8, 9, 10}));
  assert(kernel.nvars == 10 && kernel_num_output_vars(kernel) == 6);
  assert(kernel_binary_live(kernel, internalize(5), internalize(6)));
  assert(kernel_binary_live(kernel, internalize(-5), internalize(-6)));
  assert(applied.propagation_pending);
  const std::string journal = contents(records);
  assert(journal.find("affine-basis-definition-rat") != std::string::npos);
  assert(journal.find("\"projection\":\"omit-extension\"") !=
         std::string::npos);
  assert(journal.find("\"sources\":[0,1]") != std::string::npos);
  std::fclose(records);

  assert(propagate_service(kernel) == ApplyResult::Ok);
  kernel_assign_decision(kernel, internalize(1));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  if (with_proof) {
    proof.flush();
    kernel.proof = nullptr;
  }
}

static void independent_proof_microcheck() {
  Kernel kernel = cancellation_fixture();
  FILE *records = std::tmpfile();
  assert(records);
  AffineBasisLiveTransaction transaction;
  assert(prepare_affine_basis_live(kernel, request(records), transaction).status ==
         AffineBasisLiveStatus::Prepared);
  verify_transaction_proof(transaction, fixture_clauses(false));
  std::fclose(records);
}

static void atomic_declines() {
  {
    Kernel kernel = cancellation_fixture();
    FILE *records = std::tmpfile();
    assert(records);
    auto limited = request(records);
    limited.maximum_proof_additions = 1;
    const auto mapping = kernel.external_to_internal;
    const auto ids = kernel.next_clause_id;
    const auto arena = kernel.arena.size();
    const auto result = apply_affine_basis_live(kernel, limited);
    assert(result.status == AffineBasisLiveStatus::ResourceExhausted);
    assert(kernel.external_to_internal == mapping &&
           kernel.next_clause_id == ids && kernel.arena.size() == arena &&
           std::ftell(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = cancellation_fixture();
    FILE *records = std::tmpfile();
    assert(records);
    auto tiny = request(records);
    tiny.maximum_journal_bytes = 1;
    const auto mapping = kernel.external_to_internal;
    const auto ids = kernel.next_clause_id;
    AffineBasisLiveTransaction transaction;
    const auto result = prepare_affine_basis_live(kernel, tiny, transaction);
    assert(result.status == AffineBasisLiveStatus::ResourceExhausted);
    assert(!transaction.ready() && kernel.external_to_internal == mapping &&
           kernel.next_clause_id == ids && std::ftell(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = cancellation_fixture();
    FILE *records = std::tmpfile();
    assert(records);
    const auto configured = request(records);
    AffineBasisLiveTransaction transaction;
    assert(prepare_affine_basis_live(kernel, configured, transaction).status ==
           AffineBasisLiveStatus::Prepared);
    const auto mapping = kernel.external_to_internal;
    ++kernel.next_clause_id;
    const auto result = publish_affine_basis_live(kernel, configured,
                                                   transaction);
    assert(result.status == AffineBasisLiveStatus::Stale);
    assert(kernel.external_to_internal == mapping && std::ftell(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = cancellation_fixture();
    FILE *records = std::tmpfile();
    assert(records);
    const auto configured = request(records);
    AffineBasisLiveTransaction transaction;
    assert(prepare_affine_basis_live(kernel, configured, transaction).status ==
           AffineBasisLiveStatus::Prepared);
    const auto mapping = kernel.external_to_internal;
    clause_at(kernel.arena, 0)->flags |= tactix::Clause::GARBAGE;
    const auto result = publish_affine_basis_live(kernel, configured,
                                                   transaction);
    assert(result.status == AffineBasisLiveStatus::Stale);
    assert(kernel.external_to_internal == mapping && std::ftell(records) == 0);
    std::fclose(records);
  }
}

static void no_new_information() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 3);
  add_equation(kernel, {1, 2, 3}, true);
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const auto result = apply_affine_basis_live(kernel, request(records));
  assert(result.status == AffineBasisLiveStatus::NoStructure);
  assert(kernel.external_to_internal == mapping && std::ftell(records) == 0);
  std::fclose(records);
}

static void bounded_proof_scope() {
  Kernel kernel;
  kernel_reserve_vars(kernel, 15);
  add_equation(kernel, {1, 2, 3, 4, 5, 6, 7, 8}, false);
  add_equation(kernel, {1, 9, 10, 11, 12, 13, 14, 15}, true);
  FILE *records = std::tmpfile();
  assert(records);
  const auto mapping = kernel.external_to_internal;
  const auto result = apply_affine_basis_live(kernel, request(records));
  assert(result.status == AffineBasisLiveStatus::UnsupportedProof);
  assert(kernel.external_to_internal == mapping && std::ftell(records) == 0);
  std::fclose(records);
}

static void contradiction(const char *proof_path) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 5);
  add_equation(kernel, {1, 2, 3, 4, 5}, false);
  add_equation(kernel, {1, 2, 3, 4, 5}, true);
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DratAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const auto configured = request(records);
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

int main(int argc, char **argv) {
  const char *sat_proof = argc > 1 ? argv[1] : "/tmp/affine-basis-sat.drat";
  const char *unsat_proof = argc > 2 ? argv[2] : "/tmp/affine-basis-unsat.drat";
  successful_basis(false, sat_proof);
  successful_basis(true, sat_proof);
  independent_proof_microcheck();
  atomic_declines();
  no_new_information();
  bounded_proof_scope();
  contradiction(unsat_proof);
  std::puts("affine basis live: atomic definitions, consequences, projection, "
            "stale/resource declines, and proof scope PASS");
}

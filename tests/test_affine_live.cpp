#undef NDEBUG
#include "rules/semantic_extension/affine_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace tactix;
using namespace tactix::affine_semantic_extension;

static std::vector<std::vector<int32_t>> affine_formula(bool complete = true) {
  auto first = clausal_form({1, 2, 3, 4, 5}, false);
  auto second = clausal_form({1, 2, 3, 4, 6}, true);
  if (!complete) first.pop_back();
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

static void add_clause(Kernel &k, const std::vector<int32_t> &external) {
  std::vector<Lit> literals;
  for (int32_t literal : external)
    literals.push_back(kernel_internalize(k, literal));
  const uint64_t id = ++k.next_clause_id;
  if (literals.size() == 2)
    kernel_add_binary_clause(k, literals[0], literals[1], id);
  else
    kernel_add_large_clause(k, literals.data(), literals.size(), false, 0, id);
}

static void install(Kernel &k, const std::vector<std::vector<int32_t>> &cnf,
                    uint32_t variables = 6) {
  kernel_reserve_vars(k, variables);
  for (const auto &clause : cnf) add_clause(k, clause);
  assert(kernel_propagation_fixpoint(k));
}

static AffineLiveRequest request(FILE *records) {
  return {{1000, 100, 1000, 1000000, 8, 16, 16, 4},
          10000, 100000, 100000, 1000000, 0, 1, 7, records, nullptr};
}

static long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

static void assert_model(const Kernel &k) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage()) continue;
    bool satisfied = false;
    for (Lit literal : *clause) satisfied |= k.value(literal) > 0;
    assert(satisfied);
  }
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first))
      if (watch.binary() && first < watch.other())
        assert(k.value(first) > 0 || k.value(watch.other()) > 0);
}

static void positive_and_continuing() {
  Kernel k;
  install(k, affine_formula());
  FILE *records = std::tmpfile(); assert(records);
  auto r = request(records);
  const auto result = derive_affine_live(k, r);
  assert(result.result == ApplyResult::Ok);
  assert(result.equations == 2 && result.consequences == 1);
  assert(result.selected_sources == 2 && result.selected_premises == 32);
  assert((result.semantic_variables == std::vector<int32_t>{5, 6}));
  assert(result.rhs && !result.contradiction);
  assert(result.published == 2 && result.additions > 2);
  assert(kernel_binary_live(k, kernel_internalize(k, 5),
                            kernel_internalize(k, 6)));
  assert(kernel_binary_live(k, kernel_internalize(k, -5),
                            kernel_internalize(k, -6)));
  assert(k.nvars == 6 && kernel_num_output_vars(k) == 6);
  assert(position(records) > 0);

  FILE *repeat = std::tmpfile(); assert(repeat);
  r.records = repeat;
  const auto before_id = k.next_clause_id;
  const auto before_arena = k.arena.size();
  assert(derive_affine_live(k, r).result == ApplyResult::NoOp);
  assert(k.next_clause_id == before_id && k.arena.size() == before_arena);
  assert(position(repeat) == 0);

  for (int32_t external : {-1, -2, -3, -4}) {
    if (!k.value(kernel_internalize(k, external)))
      kernel_assign_decision(k, kernel_internalize(k, external));
    assert(propagate_service(k) == ApplyResult::Ok);
  }
  assert(k.value(kernel_internalize(k, -5)) > 0);
  assert(k.value(kernel_internalize(k, 6)) > 0);
  kernel_extend_model(k);
  assert_model(k);
  std::fclose(repeat);
  std::fclose(records);
}

static void clean_declines() {
  {
    Kernel k;
    install(k, affine_formula(false));
    FILE *records = std::tmpfile(); assert(records);
    const auto id = k.next_clause_id;
    const auto arena = k.arena.size();
    assert(derive_affine_live(k, request(records)).result == ApplyResult::NoOp);
    assert(k.next_clause_id == id && k.arena.size() == arena);
    assert(position(records) == 0);
    std::fclose(records);
  }
  {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    volatile sig_atomic_t stop = 1;
    k.stop = &stop;
    const auto id = k.next_clause_id;
    assert(derive_affine_live(k, request(records)).result == ApplyResult::NoOp);
    assert(k.next_clause_id == id && position(records) == 0);
    std::fclose(records);
  }
  for (int mode : {0, 1}) {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    auto r = request(records);
    if (!mode) r.maximum_proof_additions = 1;
    else r.maximum_journal_bytes = 1;
    const auto id = k.next_clause_id;
    const auto binaries = k.watches.binary_clauses();
    assert(derive_affine_live(k, r).result == ApplyResult::NoOp);
    assert(k.next_clause_id == id && k.watches.binary_clauses() == binaries);
    assert(position(records) == 0);
    std::fclose(records);
  }
}

static void interruption_boundaries() {
  bool saw_after_snapshot = false, saw_after_preflight = false;
  for (uint64_t seed = 1; seed <= 512 && !saw_after_preflight; ++seed) {
    Kernel k;
    install(k, affine_formula());
    k.stop_fuzz = 127;
    k.stop_fuzz_state = seed;
    FILE *records = std::tmpfile(); assert(records);
    const auto id = k.next_clause_id;
    const auto arena = k.arena.size();
    const auto binaries = k.watches.binary_clauses();
    const auto result = derive_affine_live(k, request(records));
    if (result.result == ApplyResult::NoOp) {
      assert(k.next_clause_id == id && k.arena.size() == arena);
      assert(k.watches.binary_clauses() == binaries && position(records) == 0);
      saw_after_snapshot |= result.snapshot_clauses != 0;
      saw_after_preflight |= result.preflight_additions != 0;
    } else {
      assert(result.result == ApplyResult::Ok && result.published == 2);
    }
    std::fclose(records);
  }
  assert(saw_after_snapshot && saw_after_preflight);
}

static void stale_snapshot() {
  Kernel k;
  std::vector<std::vector<int32_t>> cnf;
  auto first = clausal_form({1, 2}, false);
  auto second = clausal_form({1, 3}, true);
  cnf.insert(cnf.end(), first.begin(), first.end());
  cnf.insert(cnf.end(), second.begin(), second.end());
  install(k, cnf, 3);
  FILE *records = std::tmpfile(); assert(records);
  auto r = request(records);
  AffineLiveSnapshot snapshot;
  assert(capture_affine_live_snapshot(k, r, snapshot));
  assert(snapshot.sources.size() == 4);
  const auto &lost = snapshot.sources[0];
  assert(lost.kind == AffineSourceKind::Binary);
  assert(kernel_delete_binary(k, lost.first, lost.second));
  assert(!affine_live_sources_valid(k, snapshot, {lost.occurrence}));
  r.snapshot = &snapshot;
  const auto id = k.next_clause_id;
  const auto binaries = k.watches.binary_clauses();
  assert(derive_affine_live(k, r).result == ApplyResult::NoOp);
  assert(k.next_clause_id == id && k.watches.binary_clauses() == binaries);
  assert(position(records) == 0);
  std::fclose(records);
}

static void contradiction_and_false_unit() {
  {
    Kernel k;
    auto even = clausal_form({1, 2, 3}, false);
    auto odd = clausal_form({1, 2, 3}, true);
    even.insert(even.end(), odd.begin(), odd.end());
    install(k, even, 3);
    FILE *records = std::tmpfile(); assert(records);
    const auto result = derive_affine_live(k, request(records));
    assert(result.result == ApplyResult::Unsat && result.contradiction);
    assert(result.published == 0 && result.additions > 0 && k.inconsistent);
    assert(position(records) > 0);
    std::fclose(records);
  }
  {
    Kernel k;
    auto first = clausal_form({1, 2}, false);
    auto second = clausal_form({1, 2, 3}, true);
    first.insert(first.end(), second.begin(), second.end());
    install(k, first, 3);
    kernel_assign_unit(k, kernel_internalize(k, -3));
    assert(propagate_service(k) == ApplyResult::Ok);
    FILE *records = std::tmpfile(); assert(records);
    const auto result = derive_affine_live(k, request(records));
    assert(result.result == ApplyResult::Unsat && !result.contradiction);
    assert((result.semantic_variables == std::vector<int32_t>{3}));
    assert(result.rhs && result.published == 0 && k.inconsistent);
    assert(result.preflight_additions == result.additions);
    std::fclose(records);
  }
}

static void write_cnf(const char *path,
                      const std::vector<std::vector<int32_t>> &cnf,
                      uint32_t variables) {
  FILE *file = std::fopen(path, "w"); assert(file);
  std::fprintf(file, "p cnf %u %zu\n", variables, cnf.size());
  for (const auto &clause : cnf) {
    for (int32_t literal : clause) std::fprintf(file, "%d ", literal);
    std::fputs("0\n", file);
  }
  assert(!std::fclose(file));
}

static void proof_run(const char *proof_path, const char *cnf_path,
                      ProofFormat format) {
  auto cnf = affine_formula();
  for (const auto &clause : std::vector<std::vector<int32_t>>{
           {7, 8}, {7, -8}, {-7, 8}, {-7, -8}})
    cnf.push_back(clause);
  if (cnf_path) write_cnf(cnf_path, cnf, 8);
  Kernel k;
  kernel_reserve_vars(k, 8);
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  k.proof = &proof;
  for (const auto &clause : cnf) add_clause(k, clause);
  FILE *records = std::tmpfile(); assert(records);
  const auto result = derive_affine_live(k, request(records));
  assert(result.result == ApplyResult::Ok && result.published == 2);
  const int32_t seven = 7;
  assert(proof.add_external_rup(++k.next_clause_id, &seven, 1));
  assert(proof.add_external_rup(++k.next_clause_id, nullptr, 0));
  proof.flush();
  k.proof = nullptr;
  std::fclose(records);
}

static void unsupported_format() {
  Kernel k;
  install(k, affine_formula());
  Proof proof;
  assert(proof.open("/tmp/tactix-affine-live-declined.lrat", ProofFormat::Lrat));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  k.proof = &proof;
  FILE *records = std::tmpfile(); assert(records);
  const auto id = k.next_clause_id;
  assert(derive_affine_live(k, request(records)).result == ApplyResult::NoOp);
  assert(k.next_clause_id == id && position(records) == 0);
  k.proof = nullptr;
  std::fclose(records);
}

int main(int argc, char **argv) {
  positive_and_continuing();
  clean_declines();
  interruption_boundaries();
  stale_snapshot();
  contradiction_and_false_unit();
  unsupported_format();
  proof_run("/tmp/tactix-affine-live.dsr", nullptr, ProofFormat::DsrAscii);
  if (argc == 3) proof_run(argv[2], argv[1], ProofFormat::DratAscii);
  std::puts("affine live adapter: atomic recognition, proof, liveness, and continuing CDCL PASS");
}

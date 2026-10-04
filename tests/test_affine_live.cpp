#undef NDEBUG
#include "rules/semantic_extension/affine_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
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
  AffineLiveRequest result;
  result.image = {1000, 100, 1000, 1000000, 8, 16, 16, 4};
  result.maximum_snapshot_literals = 10000;
  result.maximum_snapshot_work = 100000;
  result.maximum_proof_additions = 100000;
  result.maximum_proof_deletions = 100000;
  result.maximum_journal_bytes = 1000000;
  result.action_id = 1;
  result.semantic_id = 7;
  result.records = records;
  return result;
}

static long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

static std::string read_all(FILE *file) {
  assert(!std::fflush(file));
  const long end = position(file);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, end, SEEK_SET));
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
  assert(!result.propagation_pending);
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
  for (int mode : {0, 1, 2}) {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    auto r = request(records);
    if (!mode) r.maximum_proof_additions = 1;
    else if (mode == 1) r.maximum_journal_bytes = 1;
    else r.maximum_proof_deletions = 0;
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

static void stale_large_snapshot_and_cached_counters() {
  {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    const auto large = std::find_if(
        snapshot.sources.begin(), snapshot.sources.end(), [](const auto &source) {
          return source.kind == AffineSourceKind::Large;
        });
    assert(large != snapshot.sources.end());
    const size_t index = static_cast<size_t>(large - snapshot.sources.begin());

    AffineLiveSnapshot interior = snapshot;
    ++interior.sources[index].ref;
    assert(!affine_live_sources_valid(
        k, interior, {interior.sources[index].occurrence}));

    const uint64_t generation = k.arena_generation;
    kernel_collect_garbage(k);
    assert(k.arena_generation == generation + 1);
    assert(!affine_live_sources_valid(
        k, snapshot, {snapshot.sources[index].occurrence}));
    r.snapshot = &snapshot;
    const uint64_t id = k.next_clause_id;
    assert(derive_affine_live(k, r).result == ApplyResult::NoOp);
    assert(k.next_clause_id == id && position(records) == 0);
    std::fclose(records);
  }
  {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    uint64_t literals = 0, payload_work = 0;
    for (const auto &premise : snapshot.premises) {
      literals += premise.literals.size();
      payload_work += premise.literals.size() + 1;
    }
    snapshot.literals = 0;
    snapshot.work = 0;
    r.snapshot = &snapshot;
    const auto result = derive_affine_live(k, r);
    assert(result.result == ApplyResult::Ok);
    assert(result.snapshot_literals == literals &&
           result.snapshot_work == payload_work);
    std::fclose(records);
  }
  {
    Kernel k;
    install(k, affine_formula());
    FILE *records = std::tmpfile(); assert(records);
    auto r = request(records);
    AffineLiveSnapshot snapshot;
    assert(capture_affine_live_snapshot(k, r, snapshot));
    uint64_t literals = 0;
    for (const auto &premise : snapshot.premises)
      literals += premise.literals.size();
    snapshot.literals = 0;
    snapshot.work = 0;
    r.snapshot = &snapshot;
    r.maximum_snapshot_literals = literals - 1;
    const uint64_t id = k.next_clause_id;
    assert(derive_affine_live(k, r).result == ApplyResult::NoOp);
    assert(k.next_clause_id == id && position(records) == 0);
    std::fclose(records);
  }
}

static void uninstalled_target_cleanup_and_deferred_propagation() {
  {
    Kernel k;
    install(k, affine_formula());
    add_clause(k, {5, 6});
    FILE *records = std::tmpfile(); assert(records);
    const auto result = derive_affine_live(k, request(records));
    assert(result.result == ApplyResult::Ok && result.published == 1);
    assert(result.preflight_deletions == result.deletions);
    assert(read_all(records).find("affine-proof-delete-uninstalled") !=
           std::string::npos);
    assert(kernel_binary_live(k, kernel_internalize(k, 5),
                              kernel_internalize(k, 6)));
    assert(kernel_binary_live(k, kernel_internalize(k, -5),
                              kernel_internalize(k, -6)));
    std::fclose(records);
  }
  {
    Kernel k;
    install(k, affine_formula());
    kernel_assign_unit(k, kernel_internalize(k, -5));
    assert(propagate_service(k) == ApplyResult::Ok);
    assert(!k.value(kernel_internalize(k, 6)) &&
           kernel_propagation_fixpoint(k));
    FILE *records = std::tmpfile(); assert(records);
    const uint64_t before_id = k.next_clause_id;
    const auto result = derive_affine_live(k, request(records));
    assert(result.result == ApplyResult::Ok && result.published == 1);
    assert(result.propagation_pending &&
           !kernel_propagation_fixpoint(k));
    assert(!k.value(kernel_internalize(k, 6)) &&
           k.next_clause_id == before_id + result.additions);
    const std::string journal = read_all(records);
    assert(journal.find("\"propagation\":\"deferred-controller\"") !=
               std::string::npos &&
           journal.find("\"propagation_pending\":true") !=
               std::string::npos);
    assert(propagate_service(k) == ApplyResult::Ok);
    assert(k.value(kernel_internalize(k, 6)) > 0 &&
           kernel_propagation_fixpoint(k));
    std::fclose(records);
  }
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
                      ProofFormat format, bool covered_target = false) {
  auto cnf = affine_formula();
  if (covered_target) cnf.push_back({5, 6});
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
  assert(result.result == ApplyResult::Ok &&
         result.published == (covered_target ? 1 : 2));
  if (covered_target)
    assert(read_all(records).find("affine-proof-delete-uninstalled") !=
           std::string::npos);
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
  stale_large_snapshot_and_cached_counters();
  uninstalled_target_cleanup_and_deferred_propagation();
  contradiction_and_false_unit();
  unsupported_format();
  proof_run("/tmp/tactix-affine-live.dsr", nullptr, ProofFormat::DsrAscii,
            true);
  if (argc == 3)
    proof_run(argv[2], argv[1], ProofFormat::DratAscii, true);
  std::puts("affine live adapter: atomic recognition, proof, liveness, and continuing CDCL PASS");
}

#undef NDEBUG
#include "proof/direct_php_live.hpp"

#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <unistd.h>

using namespace tactix;
using namespace tactix::direct_php_live;

namespace {

Kernel fixture(uint32_t holes) {
  assert(holes >= 2);
  const uint32_t pigeons = holes + 1;
  Kernel kernel;
  kernel_reserve_vars(kernel, pigeons * holes);
  const auto variable = [holes](uint32_t pigeon, uint32_t hole) {
    return int32_t(pigeon * holes + hole + 1);
  };
  for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<Lit> alo;
    for (uint32_t hole = 0; hole < holes; ++hole)
      alo.push_back(internalize(variable(pigeon, hole)));
    const uint64_t id = ++kernel.next_clause_id;
    if (alo.size() == 2)
      kernel_add_binary_clause(kernel, alo[0], alo[1], id);
    else
      kernel_add_large_clause(kernel, alo.data(), alo.size(), false, 0, id);
  }
  for (uint32_t hole = 0; hole < holes; ++hole)
    for (uint32_t first = 0; first < pigeons; ++first)
      for (uint32_t second = first + 1; second < pigeons; ++second)
        kernel_add_binary_clause(
            kernel, internalize(-variable(first, hole)),
            internalize(-variable(second, hole)), ++kernel.next_clause_id);
  return kernel;
}

Request request() {
  Request result;
  result.snapshot = {100000, 1000000, 10000000};
  result.recognition = {100000, 1000000, 100000000, 10000,
                        1000000, 1000000, 20};
  result.certificate = {1000000, 1000000, 100000000, 1000000, 2000000};
  return result;
}

std::string temporary_path() {
  char pattern[] = "/tmp/direct-php-live-XXXXXX";
  const int fd = mkstemp(pattern);
  assert(fd >= 0);
  close(fd);
  return pattern;
}

std::string read_file(const std::string &path) {
  FILE *file = std::fopen(path.c_str(), "rb");
  assert(file);
  std::string result;
  char buffer[4096];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, count);
  assert(!std::ferror(file));
  assert(!std::fclose(file));
  return result;
}

void exact_admission_and_replay() {
  Kernel kernel = fixture(3);
  const uint64_t input_ids = kernel.next_clause_id;
  Admission admission;
  const Result admitted = admit(kernel, request(), admission);
  assert(admitted.status == Status::Admitted && admission.ready());
  assert(admission.snapshot().sources.size() == 22);
  assert(admission.snapshot().premises.size() == 22);
  assert(admission.image().alos.size() == 4);
  assert(admission.image().amos.size() == 18);
  assert(admission.plan().accounting.additions == admitted.additions);
  assert(admission.plan().accounting.deletions == admitted.deletions);
  assert(admission.plan().first_fresh == 13);
  std::set<uint64_t> source_ids;
  for (const Source &source : admission.snapshot().sources) {
    assert(source.occurrence >= 1 && source.occurrence <= 22);
    assert(source.native_proof_id >= 1 && source.native_proof_id <= input_ids);
    assert(source_ids.insert(source.native_proof_id).second);
  }

  Transaction transaction;
  const Result prepared = prepare(kernel, admission, transaction);
  assert(prepared.status == Status::Prepared && transaction.ready());
  assert(transaction.events().size() == transaction.accounting().events);
  uint64_t additions = 0, deletions = 0, original_deletions = 0;
  for (const ReplayEvent &event : transaction.events()) {
    if (event.kind == direct_php_certificate::EventKind::Delete) {
      ++deletions;
      assert(event.deleted_id);
      if (event.role == direct_php_certificate::EventRole::RetireOriginalAmo) {
        ++original_deletions;
        assert(event.source_occurrence);
        const Source &source =
            admission.snapshot().sources[event.source_occurrence - 1];
        assert(source.native_proof_id == event.deleted_id);
      }
    } else {
      ++additions;
      assert(event.id == input_ids + additions);
    }
  }
  assert(additions == transaction.accounting().additions);
  assert(deletions == transaction.accounting().deletions);
  assert(original_deletions == 18);
  assert(transaction.events().back().clause.empty());
}

void normalized_sizes() {
  for (uint32_t holes : {2u, 3u, 4u}) {
    Kernel kernel = fixture(holes);
    Admission admission;
    assert(admit(kernel, request(), admission).status == Status::Admitted);
    assert(admission.image().variables.size() == holes + 1);
    for (const auto &row : admission.image().variables)
      assert(row.size() == holes);
    Transaction transaction;
    assert(prepare(kernel, admission, transaction).status == Status::Prepared);
    assert(transaction.accounting() == admission.plan().accounting);
  }
}

void proofless_commit_preserves_solver_state() {
  Kernel kernel = fixture(3);
  const auto internal_to_external = kernel.internal_to_external;
  const auto reconstruction = kernel.reconstruction;
  const auto arena = kernel.arena;
  const uint32_t nvars = kernel.nvars;
  const uint32_t extent = kernel.external_to_internal.size();
  const uint64_t first_id = kernel.next_clause_id;
  Admission admission;
  assert(admit(kernel, request(), admission).status == Status::Admitted);
  Transaction transaction;
  assert(prepare(kernel, admission, transaction).status == Status::Prepared);
  const Result applied = publish(kernel, transaction);
  assert(applied.status == Status::Applied && kernel.inconsistent);
  assert(kernel.nvars == nvars);
  assert(kernel.internal_to_external == internal_to_external);
  assert(kernel.reconstruction == reconstruction);
  assert(kernel.arena == arena);
  assert(kernel.next_clause_id == first_id + applied.additions);
  assert(kernel.external_to_internal.size() ==
         extent + applied.fresh_variables);
  for (size_t i = extent; i < kernel.external_to_internal.size(); ++i)
    assert(kernel.external_to_internal[i] == INVALID_VAR);
}

void ascii_publication(ProofFormat format) {
  const std::string path = temporary_path();
  {
    Kernel kernel = fixture(3);
    Proof proof;
    assert(proof.open(path.c_str(), format));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    const Result applied = apply(kernel, request());
    assert(applied.status == Status::Applied && kernel.inconsistent);
    proof.flush();
    kernel.proof = nullptr;
  }
  const std::string bytes = read_file(path);
  assert(!bytes.empty());
  assert(bytes.size() >= 2 && bytes.substr(bytes.size() - 2) == "0\n");
  unlink(path.c_str());
}

void atomic_declines_and_staleness() {
  {
    Kernel kernel = fixture(3);
    auto limited = request();
    limited.snapshot.maximum_clauses = 1;
    Admission output;
    const auto ids = kernel.next_clause_id;
    const auto extent = kernel.external_to_internal;
    assert(admit(kernel, limited, output).status == Status::ResourceExhausted);
    assert(!output.ready() && kernel.next_clause_id == ids &&
           kernel.external_to_internal == extent);
  }
  {
    Kernel kernel = fixture(3);
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    Admission output;
    assert(admit(kernel, request(), output).status == Status::Interrupted);
    assert(!output.ready());
  }
  {
    Kernel kernel = fixture(3);
    Admission admission;
    assert(admit(kernel, request(), admission).status == Status::Admitted);
    ++kernel.next_clause_id;
    Transaction output;
    assert(prepare(kernel, admission, output).status == Status::Stale);
    assert(!output.ready());
  }
  {
    Kernel kernel = fixture(3);
    Admission admission;
    assert(admit(kernel, request(), admission).status == Status::Admitted);
    Transaction transaction;
    assert(prepare(kernel, admission, transaction).status == Status::Prepared);
    const auto extent = kernel.external_to_internal;
    ++kernel.next_clause_id;
    assert(publish(kernel, transaction).status == Status::Stale);
    assert(!kernel.inconsistent && kernel.external_to_internal == extent);
  }
  {
    const std::string path = temporary_path();
    Kernel kernel = fixture(3);
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::Lrat));
    kernel.proof = &proof;
    const auto extent = kernel.external_to_internal;
    Admission output;
    assert(admit(kernel, request(), output).status == Status::UnsupportedProof);
    assert(!output.ready() && kernel.external_to_internal == extent);
    kernel.proof = nullptr;
    unlink(path.c_str());
  }
}

} // namespace

int main() {
  exact_admission_and_replay();
  normalized_sizes();
  proofless_commit_preserves_solver_state();
  ascii_publication(ProofFormat::DratAscii);
  ascii_publication(ProofFormat::DsrAscii);
  atomic_declines_and_staleness();
  std::puts("test_direct_php_live: passed");
}

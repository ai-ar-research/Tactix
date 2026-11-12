#undef NDEBUG
#include "gf3_formula_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "clause_oracle.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_gaussian_live.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace tactix;
namespace live = tactix::gf3_gaussian_live;

namespace {

Kernel install(const Gf3Formula &formula) {
  Kernel kernel = gf3_native_kernel(formula, 9);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

live::Request request(FILE *records) {
  live::Request result;
  result.recognition = {1000, 10000, 100, 100, 8, 10000};
  result.certificate.bridge = bridge_limits();
  result.certificate.maximum_work = 10000000;
  result.certificate.maximum_events = 100000;
  result.certificate.maximum_event_literals = 1000000;
  result.certificate.maximum_active_clauses = 100000;
  result.certificate.maximum_retained_source_references = 10000;
  result.certificate.maximum_event_width = 32;
  result.certificate.maximum_branch_domains = 32;
  result.certificate.maximum_consequences = 32;
  result.maximum_capture_work = 1000000;
  result.maximum_revalidation_work = 1000000;
  result.maximum_proof_additions = 100000;
  result.maximum_proof_literal_occurrences = 1000000;
  result.maximum_journal_bytes = 1000000;
  result.maximum_identity_bytes = 4000000;
  result.maximum_external_extent = 100;
  result.action_id = 401;
  result.semantic_id = 503;
  result.records = records;
  return result;
}

struct Image {
  uint64_t next_clause_id;
  size_t arena;
  size_t trail;
  bool inconsistent;
  uint64_t generation;
  std::vector<int8_t> values;
  std::vector<uint64_t> unit_ids;

  friend bool operator==(const Image &, const Image &) = default;
};

Image image(const Kernel &kernel) {
  return {kernel.next_clause_id, kernel.arena.size(), kernel.trail.size(),
          kernel.inconsistent, kernel.arena_generation, kernel.values,
          kernel.unit_ids};
}

// The fixture ranges over variables 1..9.
bool rup(const std::vector<std::vector<int32_t>> &active,
         const std::vector<int32_t> &candidate) {
  return rup_implied(active, candidate, ClauseLiterals{}, 9);
}

void independently_replay(const live::Prepared &prepared) {
  const auto &owner = prepared.owner();
  const auto &bridge = prepared.bridge();
  const auto &certificate = prepared.certificate();
  for (size_t consequence_index = 0;
       consequence_index < certificate.consequences.size();
       ++consequence_index) {
    const auto &consequence = certificate.consequences[consequence_index];
    std::set<uint64_t> used;
    std::vector<std::vector<int32_t>> active;
    const auto take = [&](const gf3_affine::SourceReference &reference) {
      assert(reference.occurrence &&
             reference.occurrence <= owner.theory.source.size());
      const auto &source = owner.theory.source[reference.occurrence - 1];
      assert(source.occurrence == reference.occurrence);
      assert(source.native_proof_id == reference.native_proof_id);
      if (used.insert(reference.occurrence).second)
        active.push_back(source.literals);
    };
    for (const auto &source : consequence.sources) {
      assert(source.authentication_index < bridge.authentication.size());
      const auto &authentication =
          bridge.authentication[source.authentication_index];
      assert(authentication.source_id == source.combination.source_id);
      for (const auto &domain : authentication.domains) {
        take(domain.at_least_one);
        for (const auto &amo : domain.at_most_one) take(amo);
      }
      for (const auto &tuple : authentication.forbidden_tuples)
        take(tuple.premise);
    }
    for (uint64_t offset = 0; offset < consequence.event_count; ++offset) {
      const auto &event =
          certificate.events[consequence.first_event + offset];
      assert(event.consequence == consequence_index);
      assert(rup(active, event.clause));
      active.push_back(event.clause);
    }
  }
}

void chained_positive_and_fresh_transaction() {
  Kernel kernel = install(gf3_chained_units());
  FILE *records = std::tmpfile();
  assert(records);
  auto config = request(records);
  live::Prepared prepared;
  const auto planned = live::prepare(kernel, config, prepared);
  assert(planned.status == live::Status::Prepared && prepared.ready());
  assert(prepared.bridge().gaussian.derivations.size() > 3);
  bool chained = false;
  for (const auto &node : prepared.bridge().gaussian.derivations)
    chained |= node.kind == gf3_gaussian::DerivationKind::Add &&
               node.first_parent > 3;
  assert(chained);
  assert(prepared.certificate().consequences.size() == 3);
  independently_replay(prepared);

  const Image before_publish = image(kernel);
  auto changed_limit = config;
  --changed_limit.maximum_proof_additions;
  assert(live::apply(kernel, changed_limit, prepared).status ==
         live::Status::Invalid);
  assert(image(kernel) == before_publish && position(records) == 0);
  FILE *other_records = std::tmpfile();
  assert(other_records);
  auto changed_sink = config;
  changed_sink.records = other_records;
  assert(live::apply(kernel, changed_sink, prepared).status ==
         live::Status::Invalid);
  assert(image(kernel) == before_publish && position(records) == 0 &&
         position(other_records) == 0);
  std::fclose(other_records);

  const auto applied = live::apply(kernel, config, prepared);
  assert(applied.status == live::Status::Applied &&
         applied.result == ApplyResult::Ok && !applied.contradiction);
  auto units = applied.published_units;
  std::sort(units.begin(), units.end());
  assert((units == std::vector<int32_t>{-8, -7, -6, -4, -2, -1, 3, 5, 9}));
  assert(applied.effort.proof_additions ==
         prepared.certificate().events.size());
  assert(applied.effort.proof_literal_occurrences ==
         prepared.certificate().statistics.event_literals);
  assert(applied.effort.capture_work && applied.effort.bridge_work &&
         applied.effort.certificate_work && applied.effort.validation_work &&
         applied.effort.revalidation_work);
  assert(applied.effort.journal_work == applied.effort.journal_bytes);
  assert(position(records) == long(applied.effort.journal_bytes));
  const std::string journal = written_bytes(records);
  assert(journal.find("\"kind\":\"gf3-gaussian-authentication\"") !=
             std::string::npos &&
         journal.find("\"forbidden_tuples\":[") != std::string::npos &&
         journal.find("\"kind\":\"gf3-gaussian-consequence\"") !=
             std::string::npos &&
         journal.find("\"derivation\":") != std::string::npos &&
         journal.find("\"source_combination\":[") != std::string::npos &&
         journal.find("\"consequence\":") != std::string::npos);
  const Image after = image(kernel);
  const auto repeated = live::apply(kernel, config, prepared);
  assert(repeated.status == live::Status::AlreadyApplied &&
         image(kernel) == after && position(records) ==
                                       long(applied.effort.journal_bytes));
  std::fclose(records);
}

// A chained-units kernel with a fresh journal.
struct Fixture {
  Kernel kernel = install(gf3_chained_units());
  FILE *records = std::tmpfile();
  Fixture() { assert(records); }
  ~Fixture() { std::fclose(records); }

  // `run` declines with `expected` and leaves the kernel and journal untouched.
  template <class Run> void declines(live::Status expected, Run run) {
    const Image before = image(kernel);
    assert(run().status == expected);
    assert(image(kernel) == before && position(records) == 0);
  }
};

void attach(Proof &proof, Kernel &kernel) {
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
}

// Prepares, lets `mutate` change the prepared action or the kernel, and
// requires apply to decline with `expected`.
template <class Mutate>
void prepared_then_declines(live::Status expected, Mutate mutate) {
  Fixture f;
  const auto config = request(f.records);
  live::Prepared prepared;
  assert(live::prepare(f.kernel, config, prepared).status ==
         live::Status::Prepared);
  mutate(f.kernel, prepared);
  f.declines(expected, [&] { return live::apply(f.kernel, config, prepared); });
}

const Gf3AffineLiveSource &large_source(const live::Prepared &prepared) {
  const auto &sources = prepared.owner().sources;
  const auto source =
      std::find_if(sources.begin(), sources.end(), [](const auto &item) {
        return item.kind == Gf3AffineSourceKind::Large;
      });
  assert(source != sources.end());
  return *source;
}

void corruption_stale_rebinding_and_no_mutation() {
  prepared_then_declines(live::Status::Invalid, [](Kernel &, live::Prepared &p) {
    auto &corrupt = const_cast<gf3_gaussian_certificate::Plan &>(p.certificate());
    assert(!corrupt.events.empty());
    corrupt.events.front().clause.push_back(INT32_MAX);
  });
  prepared_then_declines(live::Status::Invalid, [](Kernel &, live::Prepared &p) {
    auto &owner = const_cast<Gf3AffineOwnerSnapshot &>(p.owner());
    assert(!owner.sources.empty() && !owner.sources.front().clause.empty());
    owner.sources.front().clause.front() *= -1;
  });
  prepared_then_declines(live::Status::Stale, [](Kernel &k, live::Prepared &p) {
    kernel_delete_clause(k, clause_at(k.arena, large_source(p).ref));
  });
  prepared_then_declines(live::Status::Stale, [](Kernel &k, live::Prepared &p) {
    const auto &source = large_source(p);
    clause_at(k.arena, source.ref)->set_id(source.native_proof_id + 1000);
  });
  bool saw_late_interrupt = false;
  for (uint64_t seed = 1; seed <= 2048 && !saw_late_interrupt; ++seed) {
    Fixture f;
    f.kernel.stop_fuzz = 127;
    f.kernel.stop_fuzz_state = seed;
    const Image before = image(f.kernel);
    const auto result = live::apply(f.kernel, request(f.records));
    if (result.status == live::Status::Interrupted) {
      assert(image(f.kernel) == before && position(f.records) == 0);
      saw_late_interrupt = result.effort.certificate_work != 0;
    } else {
      assert(result.status == live::Status::Applied);
    }
  }
  assert(saw_late_interrupt);
}

void budgets_interruptions_and_formats() {
  uint64_t events = 0;
  {
    Fixture baseline;
    const auto complete = live::apply(baseline.kernel, request(baseline.records));
    assert(complete.status == live::Status::Applied);
    events = complete.effort.proof_additions;
  }
  {
    Fixture f;
    auto bounded = request(f.records);
    bounded.maximum_proof_additions = events - 1;
    f.declines(live::Status::ResourceExhausted,
               [&] { return live::apply(f.kernel, bounded); });
  }
  {
    Fixture f;
    volatile sig_atomic_t stop = 1;
    f.kernel.stop = &stop;
    f.declines(live::Status::Interrupted,
               [&] { return live::apply(f.kernel, request(f.records)); });
  }
  {
    Fixture f;
    TemporaryFile file;
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::PbpStreaming));
    proof.finish_input_transaction();
    assert(proof.begin_pb());
    attach(proof, f.kernel);
    f.declines(live::Status::UnsupportedProof,
               [&] { return live::apply(f.kernel, request(f.records)); });
    f.kernel.proof = nullptr;
  }
}

void proof_modes_and_conflict() {
  {
    TemporaryFile file;
    Kernel kernel = install(gf3_conflict());
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
    attach(proof, kernel);
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = live::apply(kernel, request(records));
    assert(result.status == live::Status::Applied && result.contradiction &&
           result.result == ApplyResult::Unsat && kernel.inconsistent &&
           result.published_units.empty());
    proof.flush();
    kernel.proof = nullptr;
    assert(!read_file(file.path).empty());
    std::fclose(records);
  }
  {
    Fixture f;
    TemporaryFile file;
    Proof proof;
    assert(proof.open(file.path.c_str(), ProofFormat::DsrAscii));
    attach(proof, f.kernel);
    live::Prepared prepared;
    const auto declined = live::prepare(f.kernel, request(f.records), prepared);
    assert(declined.status == live::Status::UnsupportedProofSplit &&
           !prepared.ready() && position(f.records) == 0);
    f.kernel.proof = nullptr;
  }
}

} // namespace

int main() {
  chained_positive_and_fresh_transaction();
  corruption_stale_rebinding_and_no_mutation();
  budgets_interruptions_and_formats();
  proof_modes_and_conflict();
  std::puts("test_gf3_gaussian_live: passed");
}

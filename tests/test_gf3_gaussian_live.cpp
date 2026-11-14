#include "unit_test.hpp"
#include "gf3_formula_support.hpp"
#include "temporary_file.hpp"
#include "test_io.hpp"
#include "kernel_image.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_gaussian_live.hpp"

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

// The shared no-mutation image plus the root assignment state.
struct State {
  Image image;
  bool inconsistent;
  std::vector<int8_t> values;
  std::vector<uint64_t> unit_ids;
  bool operator==(const State &) const = default;
};

State state(const Kernel &kernel) {
  return {image(kernel), kernel.inconsistent, kernel.values, kernel.unit_ids};
}

// A chained-units kernel with a fresh journal.
struct Fixture {
  Kernel kernel = install(gf3_chained_units());
  FILE *records = std::tmpfile();
  Fixture() { assert(records); }
  ~Fixture() { std::fclose(records); }

  // `run` declines with `expected` and leaves the kernel and journal untouched.
  template <class Run> void declines(live::Status expected, Run run) {
    const State before = state(kernel);
    assert(run().status == expected);
    assert(state(kernel) == before && position(records) == 0);
  }
};

void attach(Proof &proof, Kernel &kernel) {
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
}

// Source/Scale/Add closure publishes every unit of x=2, y=1, z=2 at once.
TEST(chained_units_and_journal) {
  Fixture f;
  // inspect() is mutation-free; its certificate fixes the proof accounting.
  live::Prepared prepared;
  assert(live::inspect(f.kernel, request(f.records), prepared).status ==
         live::Status::Prepared);
  const auto applied = live::apply(f.kernel, request(f.records));
  assert(applied.status == live::Status::Applied &&
         applied.result == ApplyResult::Ok && !applied.contradiction);
  assert(applied.effort.proof_additions ==
             prepared.certificate().events.size() &&
         applied.effort.proof_literal_occurrences ==
             prepared.certificate().statistics.event_literals);
  auto units = applied.published_units;
  std::sort(units.begin(), units.end());
  assert((units == std::vector<int32_t>{-8, -7, -6, -4, -2, -1, 3, 5, 9}));
  assert(applied.effort.capture_work && applied.effort.bridge_work &&
         applied.effort.certificate_work && applied.effort.validation_work &&
         applied.effort.revalidation_work);
  assert(applied.effort.journal_work == applied.effort.journal_bytes);
  assert(position(f.records) == long(applied.effort.journal_bytes));
  const std::string journal = written_bytes(f.records);
  for (const char *field :
       {"\"kind\":\"gf3-gaussian-authentication\"", "\"forbidden_tuples\":[",
        "\"kind\":\"gf3-gaussian-consequence\"", "\"derivation\":",
        "\"source_combination\":[", "\"consequence\":"})
    assert(journal.find(field) != std::string::npos);
}

TEST(budgets_interruptions_and_formats) {
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
  bool saw_late_interrupt = false;
  for (uint64_t seed = 1; seed <= 2048 && !saw_late_interrupt; ++seed) {
    Fixture f;
    f.kernel.stop_fuzz = 127;
    f.kernel.stop_fuzz_state = seed;
    const State before = state(f.kernel);
    const auto result = live::apply(f.kernel, request(f.records));
    if (result.status == live::Status::Interrupted) {
      assert(state(f.kernel) == before && position(f.records) == 0);
      saw_late_interrupt = result.effort.certificate_work != 0;
    } else {
      assert(result.status == live::Status::Applied);
    }
  }
  assert(saw_late_interrupt);
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

TEST(dsr_conflict) {
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

} // namespace

#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/semantic_extension/gf3_gaussian_live.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

using namespace tactix;
namespace live = tactix::gf3_gaussian_live;

namespace {

struct Formula {
  std::vector<std::vector<int32_t>> clauses;
  std::vector<std::vector<int32_t>> values;

  void domain(std::vector<int32_t> literals) {
    assert(literals.size() == 3);
    values.push_back(literals);
    clauses.push_back(literals);
    for (uint8_t first = 0; first < 3; ++first)
      for (uint8_t second = first + 1; second < 3; ++second)
        clauses.push_back({-literals[first], -literals[second]});
  }

  void equation(std::vector<uint32_t> domains,
                std::vector<uint8_t> coefficients, uint8_t rhs) {
    uint64_t tuples = 1;
    for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t index = 0; index < domains.size(); ++index) {
        const uint8_t value = rest % 3;
        rest /= 3;
        sum = (sum + coefficients[index] * value) % 3;
        clause.push_back(-values[domains[index]][value]);
      }
      if (sum != rhs) clauses.push_back(std::move(clause));
    }
  }
};

Formula chained_units() {
  Formula formula;
  formula.domain({1, 2, 3});
  formula.domain({4, 5, 6});
  formula.domain({7, 8, 9});
  // x+y=0, y+z=0 first derive x-z=0.  Combining that derived row
  // with x+z=1 yields 2x=1 and hence x=2, y=1, z=2.  No original pair
  // alone has a unit result, so this pins genuine Source/Scale/Add closure.
  formula.equation({0, 1}, {1, 1}, 0);
  formula.equation({1, 2}, {1, 1}, 0);
  formula.equation({0, 2}, {1, 1}, 1);
  return formula;
}

Formula conflict() {
  Formula formula;
  formula.domain({1, 2, 3});
  formula.domain({4, 5, 6});
  formula.domain({7, 8, 9});
  formula.equation({0, 1}, {1, 1}, 0);
  formula.equation({1, 2}, {1, 1}, 0);
  formula.equation({0, 2}, {1, 2}, 1);
  return formula;
}

Kernel install(const Formula &formula) {
  Kernel kernel;
  kernel_reserve_vars(kernel, 9);
  for (const auto &external : formula.clauses) {
    std::vector<Lit> clause;
    for (int32_t literal : external)
      clause.push_back(kernel_internalize(kernel, literal));
    const uint64_t id = ++kernel.next_clause_id;
    if (clause.size() == 2)
      kernel_add_binary_clause(kernel, clause[0], clause[1], id);
    else
      kernel_add_large_clause(kernel, clause.data(), clause.size(), false, 0,
                              id);
  }
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

live::Request request(FILE *records) {
  live::Request result;
  result.recognition = {1000, 10000, 100, 100, 8, 10000};
  auto &bridge = result.certificate.bridge;
  bridge.maximum_work = 1000000;
  bridge.maximum_source_clauses = 10000;
  bridge.maximum_source_literals = 100000;
  bridge.maximum_domains = 100;
  bridge.maximum_equations = 100;
  bridge.maximum_terms = 1000;
  bridge.maximum_authentication_references = 10000;
  bridge.maximum_references_per_equation = 1000;
  bridge.gaussian = {100000, 100000, 32, 100, 1000, 1000000};
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

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

std::string contents(FILE *file) {
  const long end = position(file);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, end, SEEK_SET));
  return result;
}

uint32_t variable(int32_t literal) {
  return literal < 0 ? uint32_t(-int64_t(literal)) : uint32_t(literal);
}

bool assign(std::vector<int8_t> &assignment, int32_t literal) {
  const uint32_t atom = variable(literal);
  const int8_t value = literal > 0 ? 1 : -1;
  if (assignment[atom] == -value) return false;
  assignment[atom] = value;
  return true;
}

bool rup(const std::vector<std::vector<int32_t>> &active,
         const std::vector<int32_t> &candidate) {
  std::vector<int8_t> assignment(10, 0);
  for (int32_t literal : candidate)
    if (!assign(assignment, -literal)) return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &clause : active) {
      bool satisfied = false;
      unsigned open = 0;
      int32_t last = 0;
      for (int32_t literal : clause) {
        const int8_t value = assignment[variable(literal)];
        if (value && (literal > 0) == (value > 0)) {
          satisfied = true;
          break;
        }
        if (!value) {
          ++open;
          last = literal;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        const int8_t before = assignment[variable(last)];
        if (!assign(assignment, last)) return true;
        changed |= !before;
      }
    }
  }
  return false;
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
  Kernel kernel = install(chained_units());
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
  const std::string journal = contents(records);
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

void corruption_stale_rebinding_and_no_mutation() {
  {
    Kernel kernel = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    auto config = request(records);
    live::Prepared prepared;
    assert(live::prepare(kernel, config, prepared).status ==
           live::Status::Prepared);
    auto &corrupt = const_cast<gf3_gaussian_certificate::Plan &>(
        prepared.certificate());
    assert(!corrupt.events.empty());
    corrupt.events.front().clause.push_back(INT32_MAX);
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, config, prepared);
    assert(declined.status == live::Status::Invalid &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    auto config = request(records);
    live::Prepared prepared;
    assert(live::prepare(kernel, config, prepared).status ==
           live::Status::Prepared);
    auto &owner =
        const_cast<Gf3AffineOwnerSnapshot &>(prepared.owner());
    assert(!owner.sources.empty() && !owner.sources.front().clause.empty());
    owner.sources.front().clause.front() *= -1;
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, config, prepared);
    assert(declined.status == live::Status::Invalid &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    auto config = request(records);
    live::Prepared prepared;
    assert(live::prepare(kernel, config, prepared).status ==
           live::Status::Prepared);
    const auto source = std::find_if(
        prepared.owner().sources.begin(), prepared.owner().sources.end(),
        [](const Gf3AffineLiveSource &item) {
          return item.kind == Gf3AffineSourceKind::Large;
        });
    assert(source != prepared.owner().sources.end());
    kernel_delete_clause(kernel, clause_at(kernel.arena, source->ref));
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, config, prepared);
    assert(declined.status == live::Status::Stale &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
  bool saw_late_interrupt = false;
  for (uint64_t seed = 1; seed <= 2048 && !saw_late_interrupt; ++seed) {
    Kernel kernel = install(chained_units());
    kernel.stop_fuzz = 127;
    kernel.stop_fuzz_state = seed;
    FILE *records = std::tmpfile();
    assert(records);
    const Image before = image(kernel);
    const auto result = live::apply(kernel, request(records));
    if (result.status == live::Status::Interrupted) {
      assert(image(kernel) == before && position(records) == 0);
      saw_late_interrupt = result.effort.certificate_work != 0;
    } else {
      assert(result.status == live::Status::Applied);
    }
    std::fclose(records);
  }
  assert(saw_late_interrupt);
  {
    Kernel kernel = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    auto config = request(records);
    live::Prepared prepared;
    assert(live::prepare(kernel, config, prepared).status ==
           live::Status::Prepared);
    const auto source = std::find_if(
        prepared.owner().sources.begin(), prepared.owner().sources.end(),
        [](const Gf3AffineLiveSource &item) {
          return item.kind == Gf3AffineSourceKind::Large;
        });
    assert(source != prepared.owner().sources.end());
    clause_at(kernel.arena, source->ref)->set_id(source->native_proof_id + 1000);
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, config, prepared);
    assert(declined.status == live::Status::Stale &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
}

void budgets_interruptions_and_formats() {
  {
    Kernel baseline = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    const auto complete = live::apply(baseline, request(records));
    assert(complete.status == live::Status::Applied);
    const uint64_t events = complete.effort.proof_additions;
    std::fclose(records);

    Kernel kernel = install(chained_units());
    records = std::tmpfile();
    assert(records);
    auto bounded = request(records);
    bounded.maximum_proof_additions = events - 1;
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, bounded);
    assert(declined.status == live::Status::ResourceExhausted &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
  {
    Kernel kernel = install(chained_units());
    FILE *records = std::tmpfile();
    assert(records);
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, request(records));
    assert(declined.status == live::Status::Interrupted &&
           image(kernel) == before && position(records) == 0);
    std::fclose(records);
  }
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/gf3-gaussian-live.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/gf3-gaussian-live.pbp"}}) {
    Kernel kernel = install(chained_units());
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
    const Image before = image(kernel);
    const auto declined = live::apply(kernel, request(records));
    assert(declined.status == live::Status::UnsupportedProof &&
           image(kernel) == before && position(records) == 0);
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

std::string temporary_path() {
  char pattern[] = "/tmp/gf3-gaussian-live-XXXXXX";
  const int descriptor = mkstemp(pattern);
  assert(descriptor >= 0);
  close(descriptor);
  return pattern;
}

void proof_modes_and_conflict() {
  for (ProofFormat format : {ProofFormat::DsrAscii, ProofFormat::DratAscii}) {
    const std::string path = temporary_path();
    Kernel kernel = install(conflict());
    Proof proof;
    assert(proof.open(path.c_str(), format));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = live::apply(kernel, request(records));
    assert(result.status == live::Status::Applied && result.contradiction &&
           result.result == ApplyResult::Unsat && kernel.inconsistent &&
           result.published_units.empty());
    proof.flush();
    kernel.proof = nullptr;
    FILE *stream = std::fopen(path.c_str(), "rb");
    assert(stream);
    assert(!std::fseek(stream, 0, SEEK_END));
    assert(std::ftell(stream) > 0);
    assert(!std::fclose(stream));
    unlink(path.c_str());
    std::fclose(records);
  }
  {
    Kernel kernel = install(chained_units());
    const std::string path = temporary_path();
    Proof proof;
    assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    live::Prepared prepared;
    const auto declined = live::prepare(kernel, request(records), prepared);
    assert(declined.status == live::Status::UnsupportedProofSplit &&
           !prepared.ready() && position(records) == 0);
    kernel.proof = nullptr;
    unlink(path.c_str());
    std::fclose(records);
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

#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/ttf_truth_table_live.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

namespace {

struct Built {
  TtfTruthTableLiveSnapshot snapshot;
  TtfTruthTableAmoWitness amo;
  std::vector<std::vector<int32_t>> stars;
  std::vector<uint64_t> forced_definition;
  uint64_t next = 1001;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t id = next++;
    snapshot.clauses.push_back({id, std::move(clause)});
    return id;
  }

  std::vector<uint64_t> gate(int32_t root,
                             const std::vector<int32_t> &inputs) {
    std::vector<uint64_t> ids;
    for (int32_t input : inputs) ids.push_back(add({-input, root}));
    std::vector<int32_t> backward = inputs;
    backward.push_back(-root);
    ids.push_back(add(std::move(backward)));
    return ids;
  }

  TtfTruthTableLiveSource source(uint64_t id) const {
    const auto found = std::find_if(
        snapshot.clauses.begin(), snapshot.clauses.end(),
        [id](const TtfTruthTableLiveSource &source) {
          return source.native_proof_id == id;
        });
    assert(found != snapshot.clauses.end());
    return *found;
  }
};

Built build(bool contradiction = false) {
  Built built;
  built.snapshot.generation = 17;
  built.snapshot.external_variables = 1000;
  built.stars = {
      {1, 10, 11, 12, 13}, {2, 10, 11, 14, 15},
      {3, 12, 13, 14, 15}, {101, 110, 111, 112, 113},
      {102, 110, 111, 114, 115}, {103, 112, 113, 114, 115},
  };
  for (uint32_t domain = 0; domain < 3; ++domain) {
    const auto ids = built.gate(500 + domain,
                                {600 + int32_t(2 * domain),
                                 601 + int32_t(2 * domain)});
    if (!domain) built.forced_definition = ids;
    built.add({500 + int32_t(domain)});
  }
  for (uint32_t coordinate = 0; coordinate < 2; ++coordinate)
    for (uint32_t domain = 0; domain < 3; ++domain) {
      const uint32_t star = coordinate * 3 + domain;
      const int32_t forced = 500 + domain;
      const int32_t x = 700 + int32_t(2 * domain), y = x + 1;
      const bool participation = domain < 2;
      const int32_t p = participation ? 800 + int32_t(star) : 0;
      if (participation) built.gate(p, built.stars[star]);
      built.add({-built.stars[star][0]});
      const auto nonzero = [&](int32_t atom) {
        auto clause = built.stars[star];
        clause.push_back(-atom);
        built.add(std::move(clause));
      };
      const auto zero = [&](int32_t atom) {
        if (participation) {
          built.add({-p, -atom});
        } else {
          for (int32_t variable : built.stars[star])
            built.add({-atom, -variable});
        }
      };
      if (coordinate == 0 || domain != 1)
        nonzero(forced);
      else
        zero(forced);
      nonzero(x);
      if (coordinate == 1 && domain == 2)
        nonzero(y);
      else
        zero(y);
    }

  // Exact AMO witnesses for all six reduced B stars.
  for (uint32_t star = 0; star < built.stars.size(); ++star) {
    TtfTruthTableAmoComponent component;
    component.leaves.assign(built.stars[star].begin() + 1,
                            built.stars[star].end());
    const int32_t root = 900 + int32_t(star);
    std::vector<uint64_t> ids{built.add({root})};
    for (uint32_t i = 0; i < component.leaves.size(); ++i)
      for (uint32_t j = i + 1; j < component.leaves.size(); ++j)
        ids.push_back(
            built.add({-root, -component.leaves[i], -component.leaves[j]}));
    for (uint64_t id : ids) component.sources.push_back(built.source(id));
    built.amo.components.push_back(std::move(component));
  }

  // One AMO uses forced root 500 as a nested OR. Its definition and guarded
  // exclusions exercise exact shared ownership and complete root census.
  TtfTruthTableAmoComponent shared;
  shared.leaves = {600, 601, 910, 911};
  std::vector<uint64_t> shared_ids = built.forced_definition;
  const int32_t root = 950;
  shared_ids.push_back(built.add({root}));
  shared_ids.push_back(built.add({-root, -600, -601}));
  shared_ids.push_back(built.add({-root, -500, -910}));
  shared_ids.push_back(built.add({-root, -500, -911}));
  shared_ids.push_back(built.add({-root, -910, -911}));
  for (uint64_t id : shared_ids) shared.sources.push_back(built.source(id));
  built.amo.components.push_back(std::move(shared));

  if (contradiction) {
    built.add({990, 991});
    built.add({-990, 991});
    built.add({990, -991});
    built.add({-990, -991});
  }
  return built;
}

Kernel active_kernel(uint32_t variables) {
  Kernel kernel;
  kernel_reserve_vars(kernel, variables);
  return kernel;
}

struct Provider {
  TtfTruthTableLiveSnapshot current;
  bool fail = false;
};

bool refresh(void *context, TtfTruthTableLiveSnapshot &output) {
  Provider &provider = *static_cast<Provider *>(context);
  if (provider.fail) return false;
  output = provider.current;
  return true;
}

TtfTruthTableLiveRequest request(const Built &built, Provider &provider,
                                 TtfTruthTableLiveSession &session,
                                 FILE *records) {
  TtfTruthTableLiveRequest request;
  request.owner = &built.snapshot;
  request.refresh = refresh;
  request.refresh_context = &provider;
  request.amo = &built.amo;
  request.session = &session;
  request.theory_limits = {1000, 20000, 100000, 1000, 20, 10, 20, 20};
  request.amo_limits = {100, 1000, 10000, 20, 20};
  request.maximum_source_clauses = 1000;
  request.maximum_source_literals = 20000;
  request.maximum_snapshot_work = 30000;
  request.maximum_recognition_work = 200000;
  request.maximum_proof_work = 1000;
  request.maximum_proof_additions = 10;
  request.maximum_proof_literal_occurrences = 1000;
  request.maximum_formula_additions = 10;
  request.maximum_formula_literal_occurrences = 1000;
  request.maximum_arena_words = 1000;
  request.maximum_journal_bytes = 100000;
  request.action_id = 51;
  request.semantic_id = 61;
  request.records = records;
  return request;
}

void applies_atomically_and_session_is_durable() {
  const Built built = build();
  Provider provider{built.snapshot};
  TtfTruthTableLiveSession session;
  Kernel kernel = active_kernel(built.snapshot.external_variables);
  kernel.next_clause_id = built.next - 1;
  FILE *records = std::tmpfile();
  assert(records);
  const auto applied = ttf_truth_table_live_apply(
      kernel, request(built, provider, session, records));
  assert(applied.status == TtfTruthTableLiveStatus::Applied);
  assert(applied.result == ApplyResult::Ok && applied.additions == 3);
  assert(applied.literal_occurrences == 12 && applied.journal_bytes > 0);
  assert(applied.propagation_pending && !applied.complete_refutation);
  assert(applied.published_clause_ids.size() == 3);
  assert(applied.semantic_candidate_variables ==
         std::vector<int32_t>({10, 11, 12, 13, 14, 15}));
  assert(session.committed_keys() == 1);
  assert(std::ftell(records) == long(applied.journal_bytes));
  std::rewind(records);
  std::string journal(applied.journal_bytes, '\0');
  assert(std::fread(journal.data(), 1, journal.size(), records) ==
         journal.size());
  assert(journal.find("\"native_id\":1004,") != std::string::npos);
  assert(journal.find("\"native_id\":1,") == std::string::npos);
  assert(journal.find("\"kind\":\"ttf-frontier-amo-obligation\"") !=
         std::string::npos);

  FILE *repeat_records = std::tmpfile();
  assert(repeat_records);
  auto repeated_request = request(built, provider, session, repeat_records);
  repeated_request.action_id = 52;
  const uint64_t before_id = kernel.next_clause_id;
  const auto repeated =
      ttf_truth_table_live_apply(kernel, repeated_request);
  assert(repeated.status == TtfTruthTableLiveStatus::AlreadyPublished);
  assert(repeated.result == ApplyResult::NoOp);
  assert(kernel.next_clause_id == before_id && std::ftell(repeat_records) == 0);
  std::fclose(repeat_records);
  std::fclose(records);
}

void declines_are_mutation_free() {
  const Built built = build();
  Provider provider{built.snapshot};
  TtfTruthTableLiveSession session;
  Kernel kernel = active_kernel(built.snapshot.external_variables);
  kernel.next_clause_id = built.next - 1;
  const uint64_t before = kernel.next_clause_id;

  FILE *records = std::tmpfile();
  assert(records);
  provider.current.generation++;
  const auto stale = ttf_truth_table_live_apply(
      kernel, request(built, provider, session, records));
  assert(stale.status == TtfTruthTableLiveStatus::Stale);
  assert(kernel.next_clause_id == before && std::ftell(records) == 0);

  provider.current = built.snapshot;
  auto tiny = request(built, provider, session, records);
  tiny.maximum_journal_bytes = 1;
  const auto exhausted = ttf_truth_table_live_apply(kernel, tiny);
  assert(exhausted.status == TtfTruthTableLiveStatus::ResourceExhausted);
  assert(kernel.next_clause_id == before && std::ftell(records) == 0);

  Built missing = built;
  missing.amo.components.erase(missing.amo.components.begin());
  provider.current = missing.snapshot;
  const auto no_amo = ttf_truth_table_live_apply(
      kernel, request(missing, provider, session, records));
  assert(no_amo.status == TtfTruthTableLiveStatus::MissingAmoWitness);
  assert(kernel.next_clause_id == before && std::ftell(records) == 0);

  Built invalid = built;
  invalid.snapshot.clauses.front().native_proof_id = 0;
  provider.current = invalid.snapshot;
  const auto zero_id = ttf_truth_table_live_apply(
      kernel, request(invalid, provider, session, records));
  assert(zero_id.status == TtfTruthTableLiveStatus::Invalid);
  assert(kernel.next_clause_id == before && std::ftell(records) == 0);

  invalid = built;
  invalid.snapshot.clauses.front().native_proof_id =
      invalid.snapshot.clauses.back().native_proof_id;
  provider.current = invalid.snapshot;
  const auto duplicate_id = ttf_truth_table_live_apply(
      kernel, request(invalid, provider, session, records));
  assert(duplicate_id.status == TtfTruthTableLiveStatus::Invalid);
  assert(kernel.next_clause_id == before && std::ftell(records) == 0);
  std::fclose(records);
}

std::string dimacs(const Built &built) {
  std::vector<TtfTruthTableLiveSource> clauses = built.snapshot.clauses;
  std::sort(clauses.begin(), clauses.end(),
            [](const auto &a, const auto &b) {
              return a.native_proof_id < b.native_proof_id;
            });
  std::ostringstream out;
  out << "p cnf " << built.snapshot.external_variables << ' '
      << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause.literals) out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const Built built = build(true);
  const std::string checker = dimacs(built);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) ==
         checker.size());
  std::fclose(cnf);

  Provider provider{built.snapshot};
  TtfTruthTableLiveSession session;
  Kernel kernel = active_kernel(built.snapshot.external_variables);
  kernel.next_clause_id = built.next - 1;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const auto applied = ttf_truth_table_live_apply(
      kernel, request(built, provider, session, records));
  assert(applied.status == TtfTruthTableLiveStatus::Applied);
  const int32_t unit = 990;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  applies_atomically_and_session_is_durable();
  declines_are_mutation_free();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_ttf_truth_table_live: passed");
}

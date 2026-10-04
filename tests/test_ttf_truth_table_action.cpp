#undef NDEBUG
#include "rules/semantic_extension/ttf_truth_table_action.hpp"

#include "proof/proof.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

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
        [id](const TtfTruthTableLiveSource &candidate) {
          return candidate.native_proof_id == id;
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
    const auto ids = built.gate(500 + int32_t(domain),
                                {600 + int32_t(2 * domain),
                                 601 + int32_t(2 * domain)});
    if (!domain) built.forced_definition = ids;
    built.add({500 + int32_t(domain)});
  }
  for (uint32_t coordinate = 0; coordinate < 2; ++coordinate)
    for (uint32_t domain = 0; domain < 3; ++domain) {
      const uint32_t star = coordinate * 3 + domain;
      const int32_t forced = 500 + int32_t(domain);
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
      if (!coordinate || domain != 1)
        nonzero(forced);
      else
        zero(forced);
      nonzero(x);
      if (coordinate == 1 && domain == 2)
        nonzero(y);
      else
        zero(y);
    }

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

TtfTruthTableActionParameters parameters() {
  TtfTruthTableActionParameters result;
  result.selection = TtfTruthTableFrontierSelection::FirstEligible;
  result.limits.theory = {1000, 20000, 100000, 1000, 20, 10, 20, 20};
  result.limits.amo = {100, 1000, 10000, 20, 20};
  result.limits.maximum_source_clauses = 1000;
  result.limits.maximum_source_literals = 20000;
  result.limits.maximum_snapshot_work = 30000;
  result.limits.maximum_recognition_work = 200000;
  result.limits.maximum_proof_work = 1000;
  result.limits.maximum_proof_additions = 10;
  result.limits.maximum_proof_literal_occurrences = 1000;
  result.limits.maximum_formula_additions = 10;
  result.limits.maximum_formula_literal_occurrences = 1000;
  result.limits.maximum_arena_words = 1000;
  result.limits.maximum_journal_bytes = 100000;
  result.limits.maximum_amo_components = 20;
  result.limits.maximum_amo_sources = 200;
  result.limits.maximum_amo_source_literals = 2000;
  result.limits.maximum_session_commits = 10;
  result.limits.maximum_session_work = 100000;
  assert(result.valid());
  return result;
}

Kernel active_kernel(uint32_t variables) {
  Kernel kernel;
  kernel_reserve_vars(kernel, variables);
  return kernel;
}

void attach(Kernel &kernel, FILE *records, uint64_t action = 50) {
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = action;
}

struct Image {
  uint64_t next_clause_id = 0;
  uint64_t action_sequence = 0;
  size_t arena_words = 0;
  size_t trail = 0;
  uint64_t arena_generation = 0;
  bool candidate_pending = false;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.next_clause_id = kernel.next_clause_id;
  result.action_sequence = kernel.structural_action_sequence;
  result.arena_words = kernel.arena.size();
  result.trail = kernel.trail.size();
  result.arena_generation = kernel.arena_generation;
  result.candidate_pending = kernel.semantic_variable_candidate_pending;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &first, const Image &second) {
  return first.next_clause_id == second.next_clause_id &&
         first.action_sequence == second.action_sequence &&
         first.arena_words == second.arena_words &&
         first.trail == second.trail &&
         first.arena_generation == second.arena_generation &&
         first.candidate_pending == second.candidate_pending &&
         first.watches == second.watches;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

void success_retains_replay_state_and_neutral_advice() {
  Built built = build();
  const size_t source_size = built.snapshot.clauses.size();
  const size_t amo_size = built.amo.components.size();
  Kernel kernel = active_kernel(built.snapshot.external_variables);
  kernel.next_clause_id = built.next - 1;
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  TtfTruthTableActionSession session;
  const auto configured = ttf_truth_table_action_configure(
      kernel, session, built.snapshot, built.amo, parameters());
  assert(configured.status == TtfTruthTableSessionStatus::Configured &&
         configured.source_clauses == source_size &&
         configured.amo_components == amo_size && configured.session_work);
  assert(session.configured() && session.belongs_to(kernel) &&
         session.parameters() && *session.parameters() == parameters());

  // External owner mutation cannot change the retained replay source.
  built.snapshot.clauses.clear();
  built.amo.components.clear();
  assert(session.snapshot()->clauses.size() == source_size &&
         session.amo()->components.size() == amo_size);

  const auto applied = ttf_truth_table_action_apply(kernel, session);
  assert(applied.status == TtfTruthTableActionStatus::Applied &&
         applied.result == ApplyResult::Ok);
  assert(applied.proposed_action_id == 51 &&
         applied.committed_action_id == 51 && applied.semantic_id == 51);
  assert(applied.session_work == 1);
  assert(applied.live.coordinate == 0 && applied.live.additions == 3 &&
         applied.live.literal_occurrences == 12);
  assert(applied.identity_projection && applied.propagation_pending &&
         !applied.complete_refutation && applied.advice_ready);
  assert(session.committed_keys() == 1 &&
         session.committed_identities() == 1);
  assert((session.identities().front() ==
          TtfTruthTableActionIdentity{51, 51, 17, 0}));
  assert(kernel.structural_action_sequence == 51 && kernel.trail.empty());
  assert(!kernel.semantic_variable_candidate_pending);
  assert(applied.existing_variable_advice.size() == 6);
  const std::vector<uint32_t> expected{10, 11, 12, 13, 14, 15};
  for (size_t i = 0; i < expected.size(); ++i) {
    assert(applied.existing_variable_advice[i].external == expected[i]);
    assert(applied.existing_variable_advice[i].role ==
           strategy::SemanticVariableRole::ExistingTheory);
    assert(applied.existing_variable_advice[i].priority == 1);
  }

  // Duplicate identity survives deletion of every published frontier clause.
  for (uint64_t id : applied.live.published_clause_ids)
    for (Ref ref = 0; ref < kernel.arena.size();) {
      Clause *clause = clause_at(kernel.arena, ref);
      ref += clause_actual_words(clause);
      if (!clause->garbage() && clause->id() == id)
        kernel_delete_clause(kernel, clause);
    }
  const Image before = image(kernel);
  const long journal = position(records);
  const auto repeated = ttf_truth_table_action_apply(kernel, session);
  assert(repeated.status == TtfTruthTableActionStatus::AlreadyPublished &&
         repeated.result == ApplyResult::NoOp);
  assert(!repeated.proposed_action_id && !repeated.committed_action_id &&
         repeated.committed_keys == 1 &&
         repeated.committed_identities == 1);
  assert(image(kernel) == before && position(records) == journal);
  std::fclose(records);
}

void configuration_and_action_declines_are_atomic() {
  const Built built = build();
  {
    Kernel kernel = active_kernel(built.snapshot.external_variables);
    TtfTruthTableActionSession session;
    auto tiny = parameters();
    tiny.limits.maximum_session_work = 1;
    const auto declined = ttf_truth_table_action_configure(
        kernel, session, built.snapshot, built.amo, tiny);
    assert(declined.status ==
               TtfTruthTableSessionStatus::ResourceExhausted &&
           !session.configured());
  }
  {
    Kernel kernel = active_kernel(built.snapshot.external_variables);
    TtfTruthTableActionSession session;
    TtfTruthTableAmoWitness forged = built.amo;
    forged.components.front().sources.front().native_proof_id = UINT64_MAX;
    const auto rejected = ttf_truth_table_action_configure(
        kernel, session, built.snapshot, forged, parameters());
    assert(rejected.status == TtfTruthTableSessionStatus::Invalid &&
           !session.configured());
  }
  {
    Kernel kernel = active_kernel(built.snapshot.external_variables);
    kernel.next_clause_id = built.next - 1;
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    TtfTruthTableActionSession session;
    auto limited = parameters();
    limited.limits.maximum_journal_bytes = 1;
    assert(ttf_truth_table_action_configure(
               kernel, session, built.snapshot, built.amo, limited)
               .status == TtfTruthTableSessionStatus::Configured);
    const Image before = image(kernel);
    const auto declined = ttf_truth_table_action_apply(kernel, session);
    assert(declined.status ==
               TtfTruthTableActionStatus::ResourceExhausted &&
           declined.result == ApplyResult::NoOp && image(kernel) == before &&
           !session.committed_keys() && !session.committed_identities() &&
           !position(records));
    std::fclose(records);
  }
  {
    Kernel first = active_kernel(built.snapshot.external_variables);
    Kernel second = active_kernel(built.snapshot.external_variables);
    FILE *records = std::tmpfile();
    assert(records);
    attach(second, records);
    TtfTruthTableActionSession session;
    assert(ttf_truth_table_action_configure(
               first, session, built.snapshot, built.amo, parameters())
               .status == TtfTruthTableSessionStatus::Configured);
    const Image before = image(second);
    const auto stale = ttf_truth_table_action_apply(second, session);
    assert(stale.status == TtfTruthTableActionStatus::Stale &&
           image(second) == before && !position(records));
    std::fclose(records);
  }
  {
    Built missing = built;
    missing.amo.components.erase(missing.amo.components.begin());
    Kernel kernel = active_kernel(missing.snapshot.external_variables);
    kernel.next_clause_id = missing.next - 1;
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    TtfTruthTableActionSession session;
    assert(ttf_truth_table_action_configure(
               kernel, session, missing.snapshot, missing.amo, parameters())
               .status == TtfTruthTableSessionStatus::Configured);
    const Image before = image(kernel);
    const auto declined = ttf_truth_table_action_apply(kernel, session);
    assert(declined.status ==
               TtfTruthTableActionStatus::MissingAmoWitness &&
           image(kernel) == before && !session.committed_keys() &&
           !position(records));
    std::fclose(records);
  }
}

void guard_preserves_assignment_and_proof_boundaries() {
  const Built built = build();
  {
    Kernel kernel = active_kernel(built.snapshot.external_variables);
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    assert(ttf_truth_table_action_guard(kernel));
    kernel_assign_decision(kernel, kernel_internalize(kernel, 1));
    assert(!ttf_truth_table_action_guard(kernel));
    std::fclose(records);
  }
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/ttf-action-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/ttf-action-decline.pbp"}}) {
    Kernel kernel = active_kernel(built.snapshot.external_variables);
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
    attach(kernel, records);
    TtfTruthTableActionSession session;
    assert(ttf_truth_table_action_configure(
               kernel, session, built.snapshot, built.amo, parameters())
               .status == TtfTruthTableSessionStatus::Configured);
    const Image before = image(kernel);
    const auto declined = ttf_truth_table_action_apply(kernel, session);
    assert(declined.status == TtfTruthTableActionStatus::Inapplicable &&
           image(kernel) == before && !position(records));
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

std::string dimacs(const Built &built) {
  std::vector<TtfTruthTableLiveSource> clauses = built.snapshot.clauses;
  std::sort(clauses.begin(), clauses.end(),
            [](const auto &left, const auto &right) {
              return left.native_proof_id < right.native_proof_id;
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

  Kernel kernel = active_kernel(built.snapshot.external_variables);
  kernel.next_clause_id = built.next - 1;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  TtfTruthTableActionSession session;
  assert(ttf_truth_table_action_configure(
             kernel, session, built.snapshot, built.amo, parameters())
             .status == TtfTruthTableSessionStatus::Configured);
  const auto applied = ttf_truth_table_action_apply(kernel, session);
  assert(applied.status == TtfTruthTableActionStatus::Applied &&
         applied.live.additions == 3 && !applied.complete_refutation);
  const int32_t unit = 990;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  success_retains_replay_state_and_neutral_advice();
  configuration_and_action_declines_are_atomic();
  guard_preserves_assignment_and_proof_boundaries();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_ttf_truth_table_action: passed");
}

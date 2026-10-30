#include "rules/semantic_extension/ttf_truth_table_live.hpp"

#include "kernel/types.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace tactix {

struct TtfTruthTableLiveSessionAccess {
  using Key = TtfTruthTableLiveSession::Key;
  static const std::vector<Key> &keys(const TtfTruthTableLiveSession &session) {
    return session.committed_keys_;
  }
  static void commit(TtfTruthTableLiveSession &session,
                     std::vector<Key> &replacement) {
    session.committed_keys_.swap(replacement);
  }
};

namespace {

namespace truth = ttf_truth_table;
using SessionKey = TtfTruthTableLiveSessionAccess::Key;

struct PreparedSource {
  TtfTruthTableLiveSnapshot snapshot;
  std::vector<truth::Premise> premises;
  uint64_t literals = 0;
  uint64_t work = 0;
};

bool request_valid(const TtfTruthTableLiveRequest &request) {
  return request.owner && request.refresh && request.amo && request.session &&
         request.records && request.action_id && request.semantic_id &&
         request.maximum_source_clauses && request.maximum_source_literals &&
         request.maximum_snapshot_work && request.maximum_recognition_work &&
         request.maximum_proof_work &&
         request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences &&
         request.maximum_formula_additions &&
         request.maximum_formula_literal_occurrences &&
         request.maximum_arena_words && request.maximum_journal_bytes &&
         request.theory_limits.maximum_clauses &&
         request.theory_limits.maximum_literals &&
         request.theory_limits.maximum_work &&
         request.theory_limits.maximum_plan_literals &&
         request.theory_limits.maximum_stars &&
         request.theory_limits.maximum_domains &&
         request.theory_limits.maximum_domain_leaves &&
         request.theory_limits.maximum_plan_additions &&
         request.amo_limits.maximum_clauses &&
         request.amo_limits.maximum_literals &&
         request.amo_limits.maximum_work &&
         request.amo_limits.maximum_leaves && request.amo_limits.maximum_gates;
}

bool state_applicable(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && !kernel.dense && !kernel.probing.recording &&
         kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

enum class PrepareStatus { Complete, Invalid, ResourceExhausted };

PrepareStatus prepare_source(const TtfTruthTableLiveSnapshot &input,
                             const TtfTruthTableLiveRequest &request,
                             PreparedSource &output) {
  if (!input.generation || !input.external_variables ||
      input.external_variables > INT32_MAX ||
      input.clauses.empty())
    return PrepareStatus::Invalid;
  if (input.clauses.size() > request.maximum_source_clauses)
    return PrepareStatus::ResourceExhausted;
  PreparedSource prepared;
  prepared.snapshot = input;
  std::set<TtfTruthTableLiveSource> identities;
  std::set<uint64_t> native_proof_ids;
  for (TtfTruthTableLiveSource &source : prepared.snapshot.clauses) {
    if (!source.native_proof_id ||
        !native_proof_ids.insert(source.native_proof_id).second ||
        source.literals.empty())
      return PrepareStatus::Invalid;
    if (source.literals.size() >
        request.maximum_source_literals - prepared.literals)
      return PrepareStatus::ResourceExhausted;
    const uint64_t charge = uint64_t{1} + source.literals.size();
    if (charge > request.maximum_snapshot_work - prepared.work)
      return PrepareStatus::ResourceExhausted;
    prepared.work += charge;
    prepared.literals += source.literals.size();
    std::sort(source.literals.begin(), source.literals.end());
    if (std::adjacent_find(source.literals.begin(), source.literals.end()) !=
        source.literals.end())
      return PrepareStatus::Invalid;
    for (size_t i = 0; i < source.literals.size(); ++i) {
      const int32_t literal = source.literals[i];
      if (!literal || literal == INT32_MIN ||
          external_variable_of(literal) > input.external_variables)
        return PrepareStatus::Invalid;
      if (i && external_variable_of(source.literals[i - 1]) ==
                   external_variable_of(literal))
        return PrepareStatus::Invalid;
    }
    if (!identities.insert(source).second) return PrepareStatus::Invalid;
  }
  std::sort(prepared.snapshot.clauses.begin(),
            prepared.snapshot.clauses.end());
  prepared.premises.reserve(prepared.snapshot.clauses.size());
  for (size_t i = 0; i < prepared.snapshot.clauses.size(); ++i)
    prepared.premises.push_back(
        {i + 1, prepared.snapshot.clauses[i].literals,
         prepared.snapshot.clauses[i].native_proof_id});
  output = std::move(prepared);
  return PrepareStatus::Complete;
}

bool resolve_amo(const PreparedSource &source,
                 const TtfTruthTableAmoWitness &witness,
                 std::vector<uint64_t> &occurrences,
                 std::map<std::vector<int32_t>, size_t> &leaf_components,
                 const z3_amo_semantic::Limits &limits,
                 z3_amo_semantic::Charge charge, void *charge_context) {
  std::map<TtfTruthTableLiveSource, uint64_t> source_occurrence;
  for (size_t i = 0; i < source.snapshot.clauses.size(); ++i)
    source_occurrence.emplace(source.snapshot.clauses[i], i + 1);
  std::set<TtfTruthTableLiveSource> owned;
  for (size_t component = 0; component < witness.components.size();
       ++component) {
    auto leaves = witness.components[component].leaves;
    std::sort(leaves.begin(), leaves.end());
    if (leaves.size() < 2 ||
        std::adjacent_find(leaves.begin(), leaves.end()) != leaves.end() ||
        !leaf_components.emplace(leaves, component).second ||
        witness.components[component].sources.empty())
      return false;
    std::vector<z3_amo_semantic::Premise> component_source;
    for (const TtfTruthTableLiveSource &raw :
         witness.components[component].sources) {
      TtfTruthTableLiveSource identity = raw;
      std::sort(identity.literals.begin(), identity.literals.end());
      const auto found = source_occurrence.find(identity);
      if (found == source_occurrence.end() || !owned.insert(identity).second)
        return false;
      occurrences.push_back(found->second);
      component_source.push_back(
          {found->second, identity.literals, identity.native_proof_id});
    }
    z3_amo_semantic::Image image;
    if (z3_amo_semantic::recognize(component_source, limits, charge,
                                   charge_context, image) !=
            z3_amo_semantic::Status::Recognized ||
        image.leaves != leaves)
      return false;
  }
  std::sort(occurrences.begin(), occurrences.end());
  return !occurrences.empty();
}

const TtfTruthTableLiveSource &source_at(const PreparedSource &source,
                                        uint64_t occurrence) {
  if (!occurrence || occurrence > source.snapshot.clauses.size()) std::abort();
  return source.snapshot.clauses[occurrence - 1];
}

bool clause_live(const Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<Lit> internal;
  internal.reserve(external.size());
  for (int32_t literal : external) {
    const Lit converted = kernel_internalize(kernel, literal);
    if (converted == INVALID_LIT ||
        kernel_var_retired(kernel, var_of(converted)))
      return false;
    internal.push_back(converted);
  }
  if (internal.size() == 2)
    return kernel_binary_live(kernel, internal[0], internal[1]);
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *candidate = clause_at(kernel.arena, ref);
    ref += clause_actual_words(candidate);
    if (candidate->garbage() || candidate->size != external.size()) continue;
    std::vector<int32_t> found;
    for (Lit literal : *candidate)
      found.push_back(kernel_externalize(kernel, literal));
    std::sort(found.begin(), found.end());
    if (found == external) return true;
  }
  return false;
}

bool append(std::string &journal, uint64_t maximum, const std::string &line,
            uint64_t &work) {
  if (line.size() > UINT64_MAX - work) {
    work = UINT64_MAX;
    return false;
  }
  work += line.size();
  if (journal.size() > maximum || line.size() > maximum - journal.size())
    return false;
  journal += line;
  return true;
}

std::string integers(const std::vector<int32_t> &values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i)
    result += (i ? "," : "") + std::to_string(values[i]);
  return result;
}

bool render_journal(const TtfTruthTableLiveRequest &request,
                    const PreparedSource &source,
                    const truth::ParityFrontierPlan &plan,
                    const TtfTruthTableAmoWitness &amo,
                    const std::map<std::vector<int32_t>, size_t> &amo_leaves,
                    const std::vector<uint64_t> &proof_ids,
                    std::string &journal, uint64_t &work) {
  const std::string prefix = "\"action\":" +
      std::to_string(request.action_id) + ",\"semantic\":" +
      std::to_string(request.semantic_id);
  if (!append(journal, request.maximum_journal_bytes,
              "{" + prefix +
                  ",\"kind\":\"ttf-frontier-begin\",\"coordinate\":" +
                  std::to_string(plan.coordinate) +
                  ",\"projection\":\"identity-existing-variables\","
                  "\"propagation\":\"deferred-controller\","
                  "\"complete_refutation\":false}\n",
              work))
    return false;
  for (size_t i = 0; i < plan.additions.size(); ++i) {
    const auto &addition = plan.additions[i];
    std::string line = "{" + prefix +
        ",\"kind\":\"ttf-frontier-rup\",\"id\":" +
        std::to_string(proof_ids[i]) + ",\"domain\":" +
        std::to_string(addition.domain) + ",\"star\":" +
        std::to_string(addition.star) + ",\"clause\":[" +
        integers(addition.clause) + "],\"premises\":[";
    for (size_t p = 0; p < addition.rup_premises.size(); ++p) {
      const uint64_t occurrence = addition.rup_premises[p];
      const auto &identity = source_at(source, occurrence);
      line += (p ? "," : "") +
          std::string("{\"occurrence\":") +
          std::to_string(occurrence) + ",\"native_id\":" +
          std::to_string(identity.native_proof_id) + ",\"clause\":[" +
          integers(identity.literals) + "]}";
    }
    line += "]}\n";
    if (!append(journal, request.maximum_journal_bytes, line, work))
      return false;
  }
  for (const auto &leaves : plan.required_amo_leaf_sets) {
    const auto found = amo_leaves.find(leaves);
    if (found == amo_leaves.end()) return false;
    auto sources = amo.components[found->second].sources;
    for (auto &identity : sources)
      std::sort(identity.literals.begin(), identity.literals.end());
    std::sort(sources.begin(), sources.end());
    std::string line = "{" + prefix +
        ",\"kind\":\"ttf-frontier-amo-obligation\",\"leaves\":[" +
        integers(leaves) + "],\"sources\":[";
    for (size_t i = 0; i < sources.size(); ++i) {
      const auto location = std::lower_bound(source.snapshot.clauses.begin(),
                                             source.snapshot.clauses.end(),
                                             sources[i]);
      if (location == source.snapshot.clauses.end() || *location != sources[i])
        return false;
      line += (i ? "," : "") +
          std::string("{\"occurrence\":") +
          std::to_string(1 + std::distance(source.snapshot.clauses.begin(),
                                           location)) +
          ",\"native_id\":" + std::to_string(sources[i].native_proof_id) +
          ",\"clause\":[" + integers(sources[i].literals) + "]}";
    }
    line += "]}\n";
    if (!append(journal, request.maximum_journal_bytes, line, work))
      return false;
  }
  return append(journal, request.maximum_journal_bytes,
                "{" + prefix +
                    ",\"kind\":\"ttf-frontier-complete\","
                    "\"additions\":" +
                    std::to_string(plan.additions.size()) +
                    ",\"complete_refutation\":false}\n",
                work);
}

} // namespace

TtfTruthTableLiveResult ttf_truth_table_live_apply(
    Kernel &kernel, const TtfTruthTableLiveRequest &request) {
  TtfTruthTableLiveResult result;
  if (!request_valid(request)) return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = TtfTruthTableLiveStatus::UnsupportedProof;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = TtfTruthTableLiveStatus::Interrupted;
    return result;
  }
  if (!state_applicable(kernel)) {
    result.status = TtfTruthTableLiveStatus::Inapplicable;
    return result;
  }

  PreparedSource source;
  const PrepareStatus prepared = prepare_source(*request.owner, request, source);
  if (prepared != PrepareStatus::Complete) {
    result.status = prepared == PrepareStatus::ResourceExhausted
                        ? TtfTruthTableLiveStatus::ResourceExhausted
                        : TtfTruthTableLiveStatus::Invalid;
    return result;
  }
  result.source_clauses = source.snapshot.clauses.size();
  result.source_literals = source.literals;
  result.snapshot_work = source.work;
  if (source.snapshot.external_variables > kernel_num_output_vars(kernel)) {
    result.status = TtfTruthTableLiveStatus::Stale;
    return result;
  }

  std::vector<uint64_t> amo_occurrences;
  std::map<std::vector<int32_t>, size_t> amo_leaves;
  struct RecognitionMeter {
    Kernel &kernel;
    uint64_t used = 0;
    uint64_t limit = 0;
    bool interrupted = false;
    bool exhausted = false;
    static bool charge(void *context, uint64_t amount) {
      auto &self = *static_cast<RecognitionMeter *>(context);
      if (kernel_terminating(self.kernel)) {
        self.interrupted = true;
        return false;
      }
      if (self.used > self.limit || amount > self.limit - self.used) {
        self.exhausted = true;
        return false;
      }
      self.used += amount;
      return true;
    }
  } meter{kernel, 0, request.maximum_recognition_work};
  if (!resolve_amo(source, *request.amo, amo_occurrences, amo_leaves,
                   request.amo_limits, RecognitionMeter::charge, &meter)) {
    result.recognition_work = meter.used;
    result.status = meter.interrupted ? TtfTruthTableLiveStatus::Interrupted
                    : meter.exhausted
                        ? TtfTruthTableLiveStatus::ResourceExhausted
                        : TtfTruthTableLiveStatus::MissingAmoWitness;
    return result;
  }
  truth::Image image;
  const truth::Status recognized = truth::recognize(
      source.premises, amo_occurrences, request.theory_limits,
      RecognitionMeter::charge, &meter, image);
  result.recognition_work = meter.used;
  if (recognized != truth::Status::Recognized) {
    result.status = meter.interrupted ? TtfTruthTableLiveStatus::Interrupted
                    : recognized == truth::Status::LimitExceeded ||
                              meter.exhausted
                        ? TtfTruthTableLiveStatus::ResourceExhausted
                        : TtfTruthTableLiveStatus::NoStructure;
    return result;
  }
  meter.used = 0;
  meter.exhausted = meter.interrupted = false;
  truth::ParityFrontierPlan plan;
  const truth::Status planned = truth::plan_parity_frontier(
      image, request.theory_limits, RecognitionMeter::charge, &meter, plan);
  result.plan_work = meter.used;
  if (planned != truth::Status::Recognized) {
    result.status = meter.interrupted ? TtfTruthTableLiveStatus::Interrupted
                    : planned == truth::Status::LimitExceeded || meter.exhausted
                        ? TtfTruthTableLiveStatus::ResourceExhausted
                        : TtfTruthTableLiveStatus::NoParityFrontier;
    return result;
  }
  result.coordinate = plan.coordinate;
  for (const auto &leaves : plan.required_amo_leaf_sets)
    if (!amo_leaves.count(leaves)) {
      result.status = TtfTruthTableLiveStatus::MissingAmoWitness;
      return result;
    }

  SessionKey key;
  key.coordinate_stars = plan.required_amo_leaf_sets;
  std::sort(key.coordinate_stars.begin(), key.coordinate_stars.end());
  for (const auto &addition : plan.additions)
    for (uint64_t occurrence : addition.rup_premises)
      key.proof_sources.push_back(source_at(source, occurrence));
  for (const auto &leaves : plan.required_amo_leaf_sets) {
    const auto &component = request.amo->components[amo_leaves.at(leaves)];
    for (auto identity : component.sources) {
      std::sort(identity.literals.begin(), identity.literals.end());
      key.proof_sources.push_back(std::move(identity));
    }
  }
  std::sort(key.proof_sources.begin(), key.proof_sources.end());
  key.proof_sources.erase(
      std::unique(key.proof_sources.begin(), key.proof_sources.end()),
      key.proof_sources.end());
  std::vector<SessionKey> replacement =
      TtfTruthTableLiveSessionAccess::keys(*request.session);
  if (!std::is_sorted(replacement.begin(), replacement.end()) ||
      std::adjacent_find(replacement.begin(), replacement.end()) !=
          replacement.end())
    return result;
  if (std::binary_search(replacement.begin(), replacement.end(), key)) {
    result.status = TtfTruthTableLiveStatus::AlreadyPublished;
    return result;
  }
  replacement.push_back(key);
  std::sort(replacement.begin(), replacement.end());

  uint64_t literal_occurrences = 0, arena_words = 0;
  std::set<int32_t> semantic_variables;
  if (plan.additions.size() > request.maximum_proof_additions ||
      plan.additions.size() > request.maximum_formula_additions)
    goto exhausted;
  for (const auto &addition : plan.additions) {
    if (addition.clause.size() >
        request.maximum_proof_literal_occurrences - literal_occurrences ||
        addition.clause.size() >
        request.maximum_formula_literal_occurrences - literal_occurrences)
      goto exhausted;
    literal_occurrences += addition.clause.size();
    if (addition.clause.size() > 2) {
      const uint64_t words = addition.clause.size() + CLAUSE_HEADER_WORDS;
      if (words > request.maximum_arena_words - arena_words)
        goto exhausted;
      arena_words += words;
    }
    if (clause_live(kernel, addition.clause)) {
      result.status = TtfTruthTableLiveStatus::AlreadyPublished;
      return result;
    }
    for (int32_t external : addition.clause) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)) ||
          kernel.value(literal)) {
        result.status = TtfTruthTableLiveStatus::Stale;
        return result;
      }
      semantic_variables.insert(external);
    }
  }
  if (literal_occurrences > request.maximum_proof_work)
    goto exhausted;
  result.proof_work = literal_occurrences;
  result.literal_occurrences = literal_occurrences;
  if (arena_words > request.maximum_arena_words ||
      arena_words > INVALID_REF - kernel.arena.size())
    goto exhausted;
  if (kernel.next_clause_id > uint64_t(INT64_MAX) ||
      plan.additions.size() > uint64_t(INT64_MAX) - kernel.next_clause_id)
    goto exhausted;

  {
    PreparedSource current;
    TtfTruthTableLiveSnapshot refreshed;
    if (!request.refresh(request.refresh_context, refreshed)) {
      result.status = kernel_terminating(kernel)
                          ? TtfTruthTableLiveStatus::Interrupted
                          : TtfTruthTableLiveStatus::Stale;
      return result;
    }
    const PrepareStatus refreshed_status =
        prepare_source(refreshed, request, current);
    if (refreshed_status != PrepareStatus::Complete) {
      result.status = refreshed_status == PrepareStatus::ResourceExhausted
                          ? TtfTruthTableLiveStatus::ResourceExhausted
                          : TtfTruthTableLiveStatus::Stale;
      return result;
    }
    result.revalidation_work = current.work;
    if (current.snapshot != source.snapshot) {
      result.status = TtfTruthTableLiveStatus::Stale;
      return result;
    }
  }

  {
    std::vector<uint64_t> proof_ids(plan.additions.size());
    for (size_t i = 0; i < proof_ids.size(); ++i)
      proof_ids[i] = kernel.next_clause_id + i + 1;
    std::string journal;
    uint64_t journal_work = 0;
    if (!render_journal(request, source, plan, *request.amo, amo_leaves,
                        proof_ids, journal, journal_work))
      goto exhausted;
    if (kernel_terminating(kernel)) {
      result.status = TtfTruthTableLiveStatus::Interrupted;
      return result;
    }

    kernel.close_startup_certificate();
    for (size_t i = 0; i < plan.additions.size(); ++i) {
      const auto &addition = plan.additions[i];
      const uint64_t id = ++kernel.next_clause_id;
      if (id != proof_ids[i]) std::abort();
      if (kernel.proof &&
          !kernel.proof->add_external_rup(
              id, addition.clause.data(), addition.clause.size()))
        std::abort();
      std::vector<Lit> internal;
      for (int32_t external : addition.clause)
        internal.push_back(kernel_internalize(kernel, external));
      if (internal.size() == 2)
        kernel_add_binary_clause(kernel, internal[0], internal[1], id);
      else
        kernel_add_large_clause(kernel, internal.data(), internal.size(), true,
                                internal.size(), id);
    }
    if (std::fwrite(journal.data(), 1, journal.size(), request.records) !=
            journal.size() ||
        std::fflush(request.records) || std::ferror(request.records))
      std::abort();
    TtfTruthTableLiveSessionAccess::commit(*request.session, replacement);
    result.status = TtfTruthTableLiveStatus::Applied;
    result.result = ApplyResult::Ok;
    result.additions = plan.additions.size();
    result.journal_bytes = journal.size();
    result.propagation_pending = true;
    result.complete_refutation = false;
    result.published_clause_ids = std::move(proof_ids);
    result.semantic_candidate_variables.assign(semantic_variables.begin(),
                                               semantic_variables.end());
    return result;
  }

exhausted:
  result.status = TtfTruthTableLiveStatus::ResourceExhausted;
  return result;
}

} // namespace tactix

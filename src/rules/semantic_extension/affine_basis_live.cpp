#include "rules/semantic_extension/affine_basis_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace tactix {
namespace {

namespace affine = affine_semantic_extension;
using Event = AffineBasisLiveTransaction::Event;
using SmallClause = parity_certificate::SmallClause;

bool state_applicable(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel);
}

bool valid_request(const AffineBasisLiveRequest &request) {
  return request.records && request.action_id && request.semantic_id &&
         request.maximum_snapshot_literals && request.maximum_snapshot_work &&
         request.maximum_proof_additions && request.maximum_journal_bytes &&
         request.maximum_external_extent && request.image.maximum_clauses &&
         request.image.maximum_groups && request.image.maximum_patterns &&
         request.image.maximum_elimination_work &&
         request.image.maximum_arity && request.image.maximum_arity <= 8 &&
         request.image.maximum_intermediate_width &&
         request.image.maximum_sources;
}

std::vector<int32_t> clause_key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool to_small(const std::vector<int32_t> &clause, SmallClause &small) {
  if (clause.size() > small.lits.size()) return false;
  small.size = static_cast<uint8_t>(clause.size());
  std::copy(clause.begin(), clause.end(), small.lits.begin());
  return true;
}

int32_t external_atom(const affine::BasisAtom &atom, uint32_t first_extension) {
  if (atom.kind == affine::BasisAtomKind::Existing)
    return static_cast<int32_t>(atom.value);
  const uint64_t external = uint64_t{first_extension} + atom.value - 1;
  return external <= INT32_MAX ? static_cast<int32_t>(external) : 0;
}

bool external_clause(const affine::BasisClause &symbolic,
                     uint32_t first_extension, std::vector<int32_t> &output) {
  output.clear();
  output.reserve(symbolic.size());
  std::set<int32_t> variables;
  for (const affine::BasisLiteral &literal : symbolic) {
    const int32_t variable = external_atom(literal.atom, first_extension);
    if (!variable || !variables.insert(variable).second) return false;
    output.push_back(literal.positive ? variable : -variable);
  }
  return true;
}

struct PlanningSink {
  std::vector<Event> &events;
  uint64_t &next_id;
  uint64_t maximum_additions;
  uint64_t maximum_deletions;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  std::map<std::vector<int32_t>, std::vector<uint64_t>> active;
  bool exhausted = false;
  bool invalid = false;

  PlanningSink(std::vector<Event> &events_, uint64_t &next_id_,
               uint64_t maximum_additions_, uint64_t maximum_deletions_)
      : events(events_), next_id(next_id_),
        maximum_additions(maximum_additions_),
        maximum_deletions(maximum_deletions_) {}

  bool add(const SmallClause &small, bool rat) {
    if (additions == maximum_additions || next_id == UINT64_MAX) {
      exhausted = true;
      return false;
    }
    Event event;
    event.kind = rat ? Event::Kind::Rat : Event::Kind::Rup;
    event.clause.assign(small.lits.begin(), small.lits.begin() + small.size);
    event.id = ++next_id;
    events.push_back(event);
    active[clause_key(event.clause)].push_back(event.id);
    ++additions;
    return true;
  }

  bool erase(const SmallClause &small) {
    if (deletions == maximum_deletions) {
      exhausted = true;
      return false;
    }
    std::vector<int32_t> clause(small.lits.begin(),
                                small.lits.begin() + small.size);
    auto found = active.find(clause_key(clause));
    if (found == active.end() || found->second.empty()) {
      invalid = true;
      return false;
    }
    Event event;
    event.kind = Event::Kind::Delete;
    event.clause = std::move(clause);
    event.deleted_id = found->second.back();
    found->second.pop_back();
    events.push_back(std::move(event));
    ++deletions;
    return true;
  }
};

bool append_bounded(std::string &journal, uint64_t maximum,
                    const std::string &text) {
  if (journal.size() > maximum || text.size() > maximum - journal.size())
    return false;
  journal += text;
  return true;
}

std::string integers(const std::vector<int32_t> &values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i)
    result += (i ? "," : "") + std::to_string(values[i]);
  return result;
}

std::string unsigned_integers(const std::vector<uint64_t> &values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i)
    result += (i ? "," : "") + std::to_string(values[i]);
  return result;
}

bool add_arena_words(uint64_t &words, const std::vector<int32_t> &clause) {
  if (clause.size() <= 2) return true;
  const uint64_t amount = clause.size() + CLAUSE_HEADER_WORDS;
  if (amount > UINT64_MAX - words) return false;
  words += amount;
  return true;
}

} // namespace

bool make_affine_basis_journal(
    const AffineBasisLiveRequest &request, const affine::Image &image,
    const affine::BasisPlan &plan,
    AffineBasisLiveTransaction &transaction) {
  std::string journal;
  const std::string identity = "\"action\":" +
      std::to_string(request.action_id) + ",\"semantic\":" +
      std::to_string(request.semantic_id);
  if (!append_bounded(journal, request.maximum_journal_bytes,
          "{" + identity +
          ",\"kind\":\"affine-basis-begin\",\"equations\":" +
          std::to_string(image.equations.size()) + ",\"rows\":" +
          std::to_string(plan.rows.size()) + ",\"definitions\":" +
          std::to_string(plan.definitions.size()) +
          ",\"propagation\":\"deferred-controller\"}\n"))
    return false;
  for (uint64_t occurrence : transaction.source_occurrences_) {
    if (!occurrence || occurrence > transaction.snapshot_.sources.size())
      return false;
    const auto &source = transaction.snapshot_.sources[occurrence - 1];
    if (!append_bounded(journal, request.maximum_journal_bytes,
            "{" + identity +
            ",\"kind\":\"affine-basis-source\",\"occurrence\":" +
            std::to_string(occurrence) + ",\"source_id\":" +
            std::to_string(source.clause_id) + ",\"clause\":[" +
            integers(source.clause) + "]}\n"))
      return false;
  }
  for (size_t row = 0; row < plan.rows.size(); ++row) {
    const auto &planned = plan.rows[row];
    if (!append_bounded(journal, request.maximum_journal_bytes,
            "{" + identity +
            ",\"kind\":\"affine-basis-row\",\"row\":" +
            std::to_string(row) + ",\"rhs\":" +
            (planned.rhs ? "true" : "false") + ",\"variables\":[" +
            integers(planned.variables) + "],\"sources\":[" +
            [&] {
              std::vector<uint64_t> sources(planned.source_equations.begin(),
                                            planned.source_equations.end());
              return unsigned_integers(sources);
            }() + "],\"premises\":[" +
            unsigned_integers(planned.premises) + "]}\n"))
      return false;
  }
  for (const auto &event : transaction.events_) {
    const char *kind = event.kind == Event::Kind::Rat
                           ? "affine-basis-definition-rat"
                       : event.kind == Event::Kind::Rup
                           ? "affine-basis-proof-rup"
                           : "affine-basis-proof-delete";
    if (!append_bounded(journal, request.maximum_journal_bytes,
            "{" + identity + ",\"kind\":\"" + kind +
            "\",\"id\":" +
            std::to_string(event.kind == Event::Kind::Delete
                               ? event.deleted_id : event.id) +
            ",\"install\":" + (event.install ? "true" : "false") +
            ",\"clause\":[" + integers(event.clause) + "]}\n"))
      return false;
  }
  for (size_t i = 0; i < transaction.introduced_.size(); ++i) {
    const auto &projection = plan.projection[i];
    const int32_t first = external_atom(projection.first,
                                        transaction.starting_extent_ + 1);
    const int32_t second = external_atom(projection.second,
                                         transaction.starting_extent_ + 1);
    if (!first || !second || !append_bounded(
            journal, request.maximum_journal_bytes,
            "{" + identity +
            ",\"kind\":\"affine-basis-theory-variable\",\"output\":" +
            std::to_string(transaction.introduced_[i]) +
            ",\"meaning\":\"output iff first XOR second\",\"first\":" +
            std::to_string(first) + ",\"second\":" +
            std::to_string(second) +
            ",\"projection\":\"omit-extension\","
            "\"reconstruction\":\"evaluate-operand-xor\"}\n"))
      return false;
  }
  if (!append_bounded(journal, request.maximum_journal_bytes,
          "{" + identity +
          ",\"kind\":\"affine-basis-complete\",\"additions\":" +
          std::to_string(transaction.additions_) + ",\"deletions\":" +
          std::to_string(transaction.deletions_) +
          ",\"published\":" + std::to_string(transaction.published_) +
          ",\"extent_growth\":" +
          std::to_string(transaction.extent_growth_) + "}\n"))
    return false;
  transaction.journal_ = std::move(journal);
  return true;
}

namespace {

bool install_clause(Kernel &kernel, const Event &event) {
  std::vector<Lit> clause;
  clause.reserve(event.clause.size());
  for (int32_t external : event.clause) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
      return false;
    clause.push_back(literal);
  }
  if (clause.empty()) {
    kernel.inconsistent = true;
  } else if (clause.size() == 1) {
    if (kernel.value(clause[0]) < 0) return false;
    if (!kernel.value(clause[0])) {
      kernel.unit_ids[var_of(clause[0])] = event.id;
      kernel_assign_unit(kernel, clause[0]);
    }
  } else if (clause.size() == 2) {
    kernel_add_binary_clause(kernel, clause[0], clause[1], event.id);
  } else {
    kernel_add_large_clause(kernel, clause.data(), clause.size(),
                            !event.definition, 0, event.id);
  }
  return true;
}

bool consumed_key(const affine::Image &image,
                  const AffineLiveSnapshot &snapshot,
                  const affine::BasisRow &row,
                  AffineBasisConsumedKey &output) {
  AffineBasisConsumedKey key;
  key.variables = row.variables;
  key.rhs = row.rhs;
  for (size_t source_index : row.source_equations) {
    if (source_index >= image.equations.size()) return false;
    const affine::Equation &source = image.equations[source_index];
    AffineBasisEquationIdentity equation;
    equation.variables = source.variables;
    equation.rhs = source.rhs;
    for (uint64_t occurrence : source.premises) {
      if (!occurrence || occurrence > snapshot.sources.size()) return false;
      const AffineLiveSource &premise = snapshot.sources[occurrence - 1];
      if (premise.occurrence != occurrence || !premise.clause_id)
        return false;
      AffineBasisSourceIdentity identity{premise.clause_id, premise.clause};
      std::sort(identity.clause.begin(), identity.clause.end());
      equation.premises.push_back(std::move(identity));
    }
    std::sort(equation.premises.begin(), equation.premises.end());
    if (std::adjacent_find(equation.premises.begin(),
                           equation.premises.end()) !=
        equation.premises.end())
      return false;
    key.source_equations.push_back(std::move(equation));
  }
  std::sort(key.source_equations.begin(), key.source_equations.end());
  if (std::adjacent_find(key.source_equations.begin(),
                         key.source_equations.end()) !=
      key.source_equations.end())
    return false;
  output = std::move(key);
  return true;
}

} // namespace

AffineBasisLiveResult prepare_affine_basis_live(
    Kernel &kernel, const AffineBasisLiveRequest &request,
    AffineBasisLiveTransaction &output) {
  AffineBasisLiveResult result;
  if (!valid_request(request)) return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = AffineBasisLiveStatus::UnsupportedProof;
    return result;
  }
  if (!state_applicable(kernel)) {
    result.status = AffineBasisLiveStatus::Inapplicable;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = AffineBasisLiveStatus::Interrupted;
    return result;
  }

  AffineBasisLiveTransaction transaction;
  transaction.action_id_ = request.action_id;
  transaction.semantic_id_ = request.semantic_id;
  transaction.starting_clause_id_ = kernel.next_clause_id;
  transaction.arena_generation_ = kernel.arena_generation;
  transaction.starting_nvars_ = kernel.nvars;
  transaction.starting_extent_ = kernel.external_to_internal.size();
  AffineLiveRequest snapshot_request;
  snapshot_request.image = request.image;
  snapshot_request.maximum_snapshot_literals = request.maximum_snapshot_literals;
  snapshot_request.maximum_snapshot_work = request.maximum_snapshot_work;
  snapshot_request.records = request.records;
  snapshot_request.action_id = request.action_id;
  snapshot_request.semantic_id = request.semantic_id;
  if (!capture_affine_live_snapshot(kernel, snapshot_request,
                                    transaction.snapshot_)) {
    result.status = kernel_terminating(kernel)
                        ? AffineBasisLiveStatus::Interrupted
                        : AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }
  result.snapshot_clauses = transaction.snapshot_.premises.size();
  result.snapshot_literals = transaction.snapshot_.literals;

  affine::Image image;
  uint64_t recognition_work = 0;
  bool recognition_interrupted = false;
  bool recognition_exhausted = false;
  const auto recognize_charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      recognition_interrupted = true;
      return false;
    }
    if (amount > request.image.maximum_elimination_work - recognition_work) {
      recognition_exhausted = true;
      return false;
    }
    recognition_work += amount;
    return true;
  };
  const affine::Status recognized = affine::recognize(
      transaction.snapshot_.premises, request.image, recognize_charge, image);
  result.recognition_work = recognition_work;
  if (recognized != affine::Status::Complete) {
    result.status = recognition_exhausted
                        ? AffineBasisLiveStatus::ResourceExhausted
                    : recognition_interrupted
                        ? AffineBasisLiveStatus::Interrupted
                    : recognized == affine::Status::NoStructure
                        ? AffineBasisLiveStatus::NoStructure
                    : recognized == affine::Status::Interrupted
                        ? AffineBasisLiveStatus::Interrupted
                    : recognized == affine::Status::Exhausted
                        ? AffineBasisLiveStatus::ResourceExhausted
                        : AffineBasisLiveStatus::Invalid;
    return result;
  }
  result.equations = image.equations.size();

  affine::BasisPlan basis;
  const affine::BasisPlanStatus planned = affine::build_basis_plan(
      image.equations, request.basis,
      [&](uint64_t) { return !kernel_terminating(kernel); }, basis);
  result.basis_work = basis.planning_work;
  if (planned != affine::BasisPlanStatus::Complete) {
    result.status = planned == affine::BasisPlanStatus::NoStructure
                        ? AffineBasisLiveStatus::NoStructure
                    : planned == affine::BasisPlanStatus::Interrupted
                        ? AffineBasisLiveStatus::Interrupted
                    : planned == affine::BasisPlanStatus::Exhausted
                        ? AffineBasisLiveStatus::ResourceExhausted
                        : AffineBasisLiveStatus::Invalid;
    return result;
  }
  result.rows = basis.rows.size();
  result.definitions = basis.definitions.size();
  if (transaction.starting_extent_ > request.maximum_external_extent ||
      transaction.starting_extent_ > INT32_MAX ||
      basis.definitions.size() > UINT32_MAX ||
      basis.definitions.size() > UINT32_MAX - kernel.nvars ||
      basis.definitions.size() >
          uint64_t(request.maximum_external_extent) -
              transaction.starting_extent_ ||
      transaction.starting_extent_ + basis.definitions.size() > INT32_MAX) {
    result.status = AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }
  transaction.extent_growth_ = basis.definitions.size();
  const uint32_t first_extension = transaction.starting_extent_ + 1;
  for (uint32_t node = 0; node < transaction.extent_growth_; ++node)
    transaction.introduced_.push_back(first_extension + node);

  std::set<int32_t> existing;
  std::set<uint64_t> occurrences;
  for (const affine::BasisRow &row : basis.rows) {
    existing.insert(row.variables.begin(), row.variables.end());
    occurrences.insert(row.premises.begin(), row.premises.end());
  }
  transaction.existing_.assign(existing.begin(), existing.end());
  transaction.source_occurrences_.assign(occurrences.begin(), occurrences.end());
  if (!affine_live_sources_valid(kernel, transaction.snapshot_,
                                 transaction.source_occurrences_)) {
    result.status = AffineBasisLiveStatus::Stale;
    return result;
  }

  uint64_t next_id = kernel.next_clause_id;
  PlanningSink sink{transaction.events_, next_id,
                    request.maximum_proof_additions,
                    request.maximum_proof_deletions};
  for (const affine::XorDefinition &definition : basis.definitions) {
    for (const affine::BasisClause &symbolic : definition.clauses) {
      std::vector<int32_t> clause;
      if (!external_clause(symbolic, first_extension, clause)) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      const int32_t pivot = external_atom(definition.output, first_extension);
      const auto found = std::find_if(clause.begin(), clause.end(),
          [&](int32_t literal) { return std::abs(literal) == pivot; });
      if (found == clause.end()) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      std::iter_swap(clause.begin(), found);
      SmallClause small;
      if (!to_small(clause, small) || !sink.add(small, true)) {
        result.status = AffineBasisLiveStatus::ResourceExhausted;
        return result;
      }
      transaction.events_.back().install = true;
      transaction.events_.back().definition = true;
      if (!add_arena_words(transaction.arena_words_, clause)) {
        result.status = AffineBasisLiveStatus::ResourceExhausted;
        return result;
      }
      ++transaction.published_;
    }
  }

  bool terminal = false;
  for (size_t row_index = 0;
       row_index < basis.rows.size() && !terminal; ++row_index) {
    const affine::BasisRow &row = basis.rows[row_index];
    // A one-source direct row is precisely the already-live complete CNF that
    // recognition consumed.  Re-adding it creates no Boolean information.
    if (row.source_equations.size() == 1 &&
        row.kind != affine::BasisRowKind::Extended)
      continue;
    AffineBasisConsumedKey row_key;
    if (!consumed_key(image, transaction.snapshot_, row, row_key)) {
      result.status = AffineBasisLiveStatus::Invalid;
      return result;
    }
    parity_certificate::Clauses premises;
    std::set<int32_t> proof_variables;
    for (size_t source : row.source_equations) {
      if (source >= image.equations.size()) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      const affine::Equation &equation = image.equations[source];
      if (equation.premises.size() != equation.premise_clauses.size()) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      for (const auto &clause : equation.premise_clauses) {
        SmallClause small;
        if (!to_small(clause, small)) {
          result.status = AffineBasisLiveStatus::UnsupportedProof;
          return result;
        }
        premises.push_back(small);
        for (int32_t literal : clause) proof_variables.insert(std::abs(literal));
      }
    }
    for (const affine::XorDefinition &definition : basis.definitions) {
      if (!std::binary_search(definition.used_by_rows.begin(),
                              definition.used_by_rows.end(), row_index))
        continue;
      for (const affine::BasisClause &symbolic : definition.clauses) {
        std::vector<int32_t> clause;
        SmallClause small;
        if (!external_clause(symbolic, first_extension, clause) ||
            !to_small(clause, small)) {
          result.status = AffineBasisLiveStatus::Invalid;
          return result;
        }
        premises.push_back(small);
        for (int32_t literal : clause) proof_variables.insert(std::abs(literal));
      }
    }
    if (proof_variables.size() > 16) {
      result.status = AffineBasisLiveStatus::UnsupportedProof;
      return result;
    }
    for (const affine::BasisClause &symbolic : row.clauses) {
      std::vector<int32_t> target;
      SmallClause small, kept;
      if (!external_clause(symbolic, first_extension, target) ||
          !to_small(target, small)) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      const size_t before = transaction.events_.size();
      parity_certificate::Deriver<PlanningSink> deriver(sink);
      if (!deriver.derive(small, premises, row.variables, kept)) {
        result.status = sink.exhausted
                            ? AffineBasisLiveStatus::ResourceExhausted
                        : sink.invalid ? AffineBasisLiveStatus::Invalid
                            : AffineBasisLiveStatus::UnsupportedProof;
        return result;
      }
      size_t final = transaction.events_.size();
      while (final > before) {
        --final;
        if (transaction.events_[final].kind != Event::Kind::Delete &&
            clause_key(transaction.events_[final].clause) == clause_key(target))
          break;
      }
      if (final == before &&
          (transaction.events_[final].kind == Event::Kind::Delete ||
           clause_key(transaction.events_[final].clause) != clause_key(target))) {
        result.status = AffineBasisLiveStatus::Invalid;
        return result;
      }
      transaction.events_[final].install = true;
      if (!add_arena_words(transaction.arena_words_, target)) {
        result.status = AffineBasisLiveStatus::ResourceExhausted;
        return result;
      }
      ++transaction.published_;
      if (target.empty()) {
        transaction.contradiction_ = true;
        terminal = true;
        break;
      }
    }
    transaction.consumed_keys_.push_back(std::move(row_key));
  }
  transaction.additions_ = sink.additions;
  transaction.deletions_ = sink.deletions;
  if (!transaction.published_) {
    result.status = AffineBasisLiveStatus::NoStructure;
    return result;
  }
  std::sort(transaction.consumed_keys_.begin(),
            transaction.consumed_keys_.end());
  if (transaction.consumed_keys_.empty() ||
      std::adjacent_find(transaction.consumed_keys_.begin(),
                         transaction.consumed_keys_.end()) !=
          transaction.consumed_keys_.end()) {
    result.status = AffineBasisLiveStatus::Invalid;
    return result;
  }
  if (transaction.additions_ > request.maximum_proof_additions ||
      transaction.deletions_ > request.maximum_proof_deletions ||
      next_id > uint64_t(INT64_MAX) ||
      transaction.arena_words_ >= INVALID_REF - kernel.arena.size()) {
    result.status = AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }
  if (!make_affine_basis_journal(request, image, basis, transaction)) {
    result.status = AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = AffineBasisLiveStatus::Interrupted;
    return result;
  }
  if (!affine_live_sources_valid(kernel, transaction.snapshot_,
                                 transaction.source_occurrences_)) {
    result.status = AffineBasisLiveStatus::Stale;
    return result;
  }
  transaction.ready_ = true;
  result.status = AffineBasisLiveStatus::Prepared;
  result.preflight_additions = transaction.additions_;
  result.preflight_deletions = transaction.deletions_;
  result.published_clauses = transaction.published_;
  result.extent_growth = transaction.extent_growth_;
  result.journal_bytes = transaction.journal_.size();
  result.contradiction = transaction.contradiction_;
  result.existing_theory_variables = transaction.existing_;
  result.introduced_theory_variables = transaction.introduced_;
  transaction.prepared_ = result;
  output = std::move(transaction);
  return result;
}

AffineBasisLiveResult publish_affine_basis_live(
    Kernel &kernel, const AffineBasisLiveRequest &request,
    const AffineBasisLiveTransaction &transaction) {
  AffineBasisLiveResult result = transaction.prepared_;
  result.result = ApplyResult::NoOp;
  if (!transaction.ready_ || !valid_request(request) ||
      request.action_id != transaction.action_id_ ||
      request.semantic_id != transaction.semantic_id_) {
    result.status = AffineBasisLiveStatus::Invalid;
    return result;
  }
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = AffineBasisLiveStatus::UnsupportedProof;
    return result;
  }
  if (!state_applicable(kernel)) {
    result.status = AffineBasisLiveStatus::Inapplicable;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = AffineBasisLiveStatus::Interrupted;
    return result;
  }
  if (kernel.next_clause_id != transaction.starting_clause_id_ ||
      kernel.arena_generation != transaction.arena_generation_ ||
      kernel.nvars != transaction.starting_nvars_ ||
      kernel.external_to_internal.size() != transaction.starting_extent_ ||
      !affine_live_sources_valid(kernel, transaction.snapshot_,
                                 transaction.source_occurrences_)) {
    result.status = AffineBasisLiveStatus::Stale;
    return result;
  }
  if (transaction.journal_.size() > request.maximum_journal_bytes ||
      transaction.additions_ > request.maximum_proof_additions ||
      transaction.deletions_ > request.maximum_proof_deletions ||
      uint64_t(transaction.starting_extent_) + transaction.extent_growth_ >
          request.maximum_external_extent ||
      transaction.arena_words_ >= INVALID_REF - kernel.arena.size()) {
    result.status = AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }

  kernel.close_startup_certificate();
  if (transaction.extent_growth_) {
    const Var first = kernel_new_extension_vars(kernel,
                                                 transaction.extent_growth_);
    if (first != transaction.starting_nvars_) std::abort();
    for (uint32_t i = 0; i < transaction.extent_growth_; ++i)
      if (kernel_externalize(kernel, make_lit(first + i, false)) !=
          transaction.introduced_[i])
        std::abort();
  }
  for (const Event &event : transaction.events_) {
    if (event.kind == Event::Kind::Delete) {
      if (kernel.proof && !kernel.proof->del_external(
              kernel.next_clause_id, event.deleted_id,
              event.clause.empty() ? nullptr : event.clause.data(),
              event.clause.size()))
        std::abort();
      continue;
    }
    if (++kernel.next_clause_id != event.id) std::abort();
    if (kernel.proof) {
      const bool emitted = event.kind == Event::Kind::Rat
          ? kernel.proof->add_external_rat(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                event.clause.size())
          : kernel.proof->add_external_rup(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                event.clause.size());
      if (!emitted) std::abort();
    }
    if (event.install && !install_clause(kernel, event)) std::abort();
  }
  if (std::fwrite(transaction.journal_.data(), 1, transaction.journal_.size(),
                  request.records) != transaction.journal_.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  result.status = AffineBasisLiveStatus::Applied;
  result.result = kernel.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
  result.additions = transaction.additions_;
  result.deletions = transaction.deletions_;
  result.propagation_pending =
      !kernel.inconsistent && !kernel_propagation_fixpoint(kernel);
  return result;
}

AffineBasisLiveResult apply_affine_basis_live(
    Kernel &kernel, const AffineBasisLiveRequest &request) {
  AffineBasisLiveTransaction transaction;
  AffineBasisLiveResult prepared =
      prepare_affine_basis_live(kernel, request, transaction);
  if (prepared.status != AffineBasisLiveStatus::Prepared) return prepared;
  return publish_affine_basis_live(kernel, request, transaction);
}

} // namespace tactix

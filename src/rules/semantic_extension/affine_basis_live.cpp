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
         !kernel.pending.valid && !kernel.dense &&
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
         request.image.maximum_sources &&
         (!request.excluded_keys || request.maximum_exclusion_work) &&
         (!request.owner || request.owner_version) &&
         (!request.selection ||
          (request.owner_version &&
           request.selection->owner_version == request.owner_version &&
           request.selection->elimination_profile == 1));
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

  bool mark_install(const SmallClause &small, bool definition) {
    std::vector<int32_t> clause(small.lits.begin(),
                                small.lits.begin() + small.size);
    const auto found = active.find(clause_key(std::move(clause)));
    if (found == active.end() || found->second.empty()) return false;
    const uint64_t id = found->second.back();
    for (auto event = events.rbegin(); event != events.rend(); ++event)
      if (event->kind != Event::Kind::Delete && event->id == id) {
        if (event->install || event->definition) return false;
        event->install = true;
        event->definition = definition;
        return true;
      }
    return false;
  }

  bool exact_active_population(const std::vector<SmallClause> &expected) const {
    std::map<std::vector<int32_t>, size_t> wanted;
    for (const SmallClause &small : expected) {
      std::vector<int32_t> clause(small.lits.begin(),
                                  small.lits.begin() + small.size);
      ++wanted[clause_key(std::move(clause))];
    }
    size_t populated = 0;
    for (const auto &[clause, ids] : active) {
      if (ids.empty()) continue;
      ++populated;
      const auto found = wanted.find(clause);
      if (found == wanted.end() || found->second != ids.size()) return false;
    }
    return populated == wanted.size();
  }

  std::set<int32_t> active_variables() const {
    std::set<int32_t> result;
    for (const auto &[clause, ids] : active) {
      if (ids.empty()) continue;
      for (int32_t literal : clause) result.insert(std::abs(literal));
    }
    return result;
  }
};

// Run the scalable parity certificate over dense theory-variable identities,
// while every proof event names the kernel's stable external identity.  Dense
// leaves avoid allocating a segment tree proportional to a sparse/high input
// variable number.  Helpers form one contiguous external namespace after the
// current kernel extent so proof-on and proof-off executions stay identical.
struct DensePlanningSink {
  PlanningSink &sink;
  const std::vector<int32_t> &external_variables;
  uint32_t starting_extent;
  uint32_t fresh;
  uint32_t maximum_helpers;

  int32_t external_literal(int32_t literal) const {
    if (!literal || literal == INT32_MIN) return 0;
    const uint32_t variable = static_cast<uint32_t>(std::abs(literal));
    int32_t external = 0;
    if (variable <= external_variables.size())
      external = external_variables[variable - 1];
    else {
      const uint64_t offset = variable - external_variables.size();
      const uint64_t mapped = uint64_t{starting_extent} + offset;
      if (offset > maximum_helpers || mapped > INT32_MAX) return 0;
      external = static_cast<int32_t>(mapped);
    }
    return literal < 0 ? -external : external;
  }

  int32_t helper_external(uint32_t dense) const {
    if (dense <= external_variables.size()) return 0;
    const uint64_t offset = dense - external_variables.size();
    const uint64_t external = uint64_t{starting_extent} + offset;
    return offset <= maximum_helpers && external <= INT32_MAX
               ? static_cast<int32_t>(external)
               : 0;
  }

  bool map(const SmallClause &input, SmallClause &output) const {
    output.size = input.size;
    for (uint32_t i = 0; i < input.size; ++i) {
      output.lits[i] = external_literal(input.lits[i]);
      if (!output.lits[i]) return false;
    }
    return true;
  }

  bool add(const SmallClause &clause, bool rat) {
    SmallClause external;
    if (!map(clause, external)) {
      sink.invalid = true;
      return false;
    }
    return sink.add(external, rat);
  }

  bool erase(const SmallClause &clause) {
    SmallClause external;
    if (!map(clause, external)) {
      sink.invalid = true;
      return false;
    }
    return sink.erase(external);
  }

  bool new_variable(int32_t &variable) {
    const uint64_t used = uint64_t{fresh} - external_variables.size();
    if (used == maximum_helpers || fresh == INT32_MAX) {
      sink.exhausted = true;
      return false;
    }
    variable = static_cast<int32_t>(++fresh);
    return true;
  }

  bool mark_install(const SmallClause &clause, bool definition) {
    SmallClause external;
    return map(clause, external) && sink.mark_install(external, definition);
  }

  uint32_t helpers() const {
    return fresh - static_cast<uint32_t>(external_variables.size());
  }
};

bool add_arena_words(uint64_t &, const std::vector<int32_t> &);

enum class WideProofStatus : uint8_t {
  Complete,
  Invalid,
  Exhausted,
  Interrupted,
  Unsupported,
};

struct WideProofPlan {
  uint32_t helpers = 0;
  uint64_t published = 0;
  uint64_t arena_words = 0;
  std::vector<int32_t> introduced;
  std::vector<AffineBasisProjectionRecord> projection;
  std::vector<int32_t> proof_only_helpers;
};

WideProofStatus prepare_wide_proof(
    Kernel &kernel, const affine::Image &image, const affine::BasisRow &row,
    uint32_t starting_extent, uint32_t maximum_helpers,
    uint32_t maximum_live_nodes, uint64_t maximum_work,
    uint64_t &proof_work,
    PlanningSink &sink, WideProofPlan &output) {
  proof_work = 0;
  bool interrupted = false;
  bool work_exhausted = false;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (amount > maximum_work - proof_work) {
      work_exhausted = true;
      return false;
    }
    proof_work += amount;
    return true;
  };
  if (row.kind != affine::BasisRowKind::Extended ||
      row.variables.size() < 2 || row.source_equations.empty())
    return WideProofStatus::Invalid;
  if (row.variables.size() - 1 > maximum_live_nodes)
    return WideProofStatus::Exhausted;

  std::vector<int32_t> variables;
  for (size_t source : row.source_equations) {
    if (source >= image.equations.size()) return WideProofStatus::Invalid;
    const auto &equation = image.equations[source];
    if (equation.variables.empty() ||
        equation.variables.size() >
            parity_certificate::Equation{}.variables.size())
      return WideProofStatus::Unsupported;
    variables.insert(variables.end(), equation.variables.begin(),
                     equation.variables.end());
  }
  std::sort(variables.begin(), variables.end());
  variables.erase(std::unique(variables.begin(), variables.end()),
                  variables.end());
  if (variables.empty() || variables.size() > INT32_MAX)
    return WideProofStatus::Invalid;

  std::vector<parity_certificate::Equation> equations;
  equations.reserve(row.source_equations.size());
  for (size_t source : row.source_equations) {
    const auto &recognized = image.equations[source];
    parity_certificate::Equation equation;
    equation.size = static_cast<uint8_t>(recognized.variables.size());
    equation.rhs = recognized.rhs;
    for (size_t i = 0; i < recognized.variables.size(); ++i) {
      const auto found = std::lower_bound(variables.begin(), variables.end(),
                                          recognized.variables[i]);
      if (found == variables.end() || *found != recognized.variables[i])
        return WideProofStatus::Invalid;
      equation.variables[i] =
          static_cast<int32_t>(found - variables.begin() + 1);
    }
    equations.push_back(equation);
  }

  DensePlanningSink mapped{sink, variables, starting_extent,
                           static_cast<uint32_t>(variables.size()),
                           maximum_helpers};
  parity_certificate::Certificate<DensePlanningSink> certificate(
      mapped, static_cast<uint32_t>(variables.size()), UINT32_MAX, charge);
  if (!certificate.accumulate(equations))
    return interrupted ? WideProofStatus::Interrupted
         : work_exhausted || sink.exhausted ? WideProofStatus::Exhausted
         : sink.invalid ? WideProofStatus::Invalid
                                            : WideProofStatus::Unsupported;
  if (!charge(1))
    return interrupted ? WideProofStatus::Interrupted
                       : WideProofStatus::Exhausted;
  typename parity_certificate::Certificate<DensePlanningSink>::ActiveTree tree;
  if (!certificate.active_tree(tree)) return WideProofStatus::Invalid;

  std::vector<int32_t> expected_dense;
  expected_dense.reserve(row.variables.size());
  for (int32_t external : row.variables) {
    const auto found =
        std::lower_bound(variables.begin(), variables.end(), external);
    if (found == variables.end() || *found != external)
      return WideProofStatus::Invalid;
    expected_dense.push_back(
        static_cast<int32_t>(found - variables.begin() + 1));
  }
  if (tree.variables != expected_dense || tree.rhs != row.rhs ||
      tree.definitions.size() + 1 != tree.variables.size())
    return WideProofStatus::Invalid;

  std::vector<SmallClause> expected_active;
  for (const auto &definition : tree.definitions)
    for (const SmallClause &clause : definition.clauses) {
      SmallClause external;
      if (!mapped.map(clause, external)) return WideProofStatus::Invalid;
      expected_active.push_back(external);
    }
  SmallClause external_root;
  if (!mapped.map(tree.root_unit, external_root))
    return WideProofStatus::Invalid;
  expected_active.push_back(external_root);
  if (!sink.exact_active_population(expected_active))
    return WideProofStatus::Invalid;

  WideProofPlan plan;
  std::set<int32_t> live_helpers;
  for (const auto &definition : tree.definitions) {
    const int32_t external_output = mapped.helper_external(definition.output);
    const int32_t external_first = mapped.external_literal(definition.first);
    const int32_t external_second = mapped.external_literal(definition.second);
    if (!external_output || !external_first || !external_second ||
        !live_helpers.insert(external_output).second)
      return WideProofStatus::Invalid;
    plan.introduced.push_back(external_output);
    plan.projection.push_back(
        {external_output, external_first, external_second});
    for (const SmallClause &clause : definition.clauses) {
      SmallClause external;
      if (!mapped.map(clause, external) ||
          !mapped.mark_install(clause, true))
        return WideProofStatus::Invalid;
      std::vector<int32_t> material(external.lits.begin(),
                                    external.lits.begin() + external.size);
      if (!add_arena_words(plan.arena_words, material))
        return WideProofStatus::Exhausted;
      ++plan.published;
    }
  }
  if (!mapped.mark_install(tree.root_unit, false))
    return WideProofStatus::Invalid;
  ++plan.published;

  const std::set<int32_t> active_variables = sink.active_variables();
  if (!live_helpers.count(std::abs(external_root.lits[0])))
    return WideProofStatus::Invalid;
  plan.helpers = mapped.helpers();
  for (uint32_t helper = 1; helper <= plan.helpers; ++helper) {
    const uint64_t candidate = uint64_t{starting_extent} + helper;
    if (candidate > INT32_MAX) return WideProofStatus::Exhausted;
    const int32_t external = static_cast<int32_t>(candidate);
    if (live_helpers.count(external)) {
      if (!active_variables.count(external)) return WideProofStatus::Invalid;
      continue;
    }
    if (active_variables.count(external)) return WideProofStatus::Invalid;
    SmallClause fixed;
    fixed.size = 1;
    fixed.lits[0] = external;
    if (!sink.add(fixed, true))
      return sink.exhausted ? WideProofStatus::Exhausted
                            : WideProofStatus::Invalid;
    if (!sink.mark_install(fixed, false)) return WideProofStatus::Invalid;
    plan.proof_only_helpers.push_back(external);
    ++plan.published;
  }
  output = std::move(plan);
  return WideProofStatus::Complete;
}

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

bool clause_covered(const Kernel &kernel, const std::vector<int32_t> &clause) {
  for (int32_t external : clause) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
      return false;
    if (kernel.value(literal) > 0) return true;
  }
  if (clause.empty()) return false;
  if (clause.size() == 1)
    return kernel.value(kernel_internalize(kernel, clause.front())) > 0;
  if (clause.size() == 2)
    return kernel_binary_live(kernel, kernel_internalize(kernel, clause[0]),
                              kernel_internalize(kernel, clause[1]));
  std::vector<int32_t> wanted = clause_key(clause);
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *candidate = clause_at(kernel.arena, ref);
    ref += clause_actual_words(candidate);
    if (candidate->garbage() || candidate->size != clause.size()) continue;
    std::vector<int32_t> current;
    current.reserve(candidate->size);
    for (Lit literal : *candidate)
      current.push_back(kernel_externalize(kernel, literal));
    if (clause_key(std::move(current)) == wanted) return true;
  }
  return false;
}

bool valid_owned_snapshot(const Kernel &kernel,
                          const AffineBasisLiveRequest &request,
                          const AffineLiveSnapshot &snapshot,
                          uint64_t &literals, uint64_t &work) {
  literals = work = 0;
  if (snapshot.arena_generation != kernel.arena_generation ||
      snapshot.premises.size() != snapshot.sources.size() ||
      snapshot.premises.size() > request.image.maximum_clauses)
    return false;
  for (size_t i = 0; i < snapshot.premises.size(); ++i) {
    const auto &premise = snapshot.premises[i];
    const auto &source = snapshot.sources[i];
    if (premise.occurrence != i + 1 || source.occurrence != i + 1 ||
        premise.literals != source.clause || !source.clause_id)
      return false;
    if (premise.literals.size() > request.maximum_snapshot_literals - literals)
      return false;
    const uint64_t amount = premise.literals.size() + 1;
    if (amount > request.maximum_snapshot_work - work) return false;
    literals += premise.literals.size();
    work += amount;
  }
  return true;
}

} // namespace

bool AffineBasisRowSelection::operator<(
    const AffineBasisRowSelection &other) const {
  const auto priority = [](affine::BasisRowKind kind) {
    switch (kind) {
    case affine::BasisRowKind::DirectContradiction: return 0u;
    case affine::BasisRowKind::DirectUnit: return 1u;
    case affine::BasisRowKind::DirectBinary: return 2u;
    case affine::BasisRowKind::DirectSmall: return 3u;
    case affine::BasisRowKind::Extended: return 4u;
    }
    return 5u;
  };
  const auto left = priority(kind), right = priority(other.kind);
  if (left != right) return left < right;
  if (variables.size() != other.variables.size())
    return variables.size() < other.variables.size();
  if (variables != other.variables) return variables < other.variables;
  if (rhs != other.rhs) return rhs < other.rhs;
  if (source_equation_ordinals != other.source_equation_ordinals)
    return source_equation_ordinals < other.source_equation_ordinals;
  if (source_equations != other.source_equations)
    return source_equations < other.source_equations;
  if (owner_version != other.owner_version)
    return owner_version < other.owner_version;
  return elimination_profile < other.elimination_profile;
}

bool make_affine_basis_journal(
    const AffineBasisLiveRequest &request, const affine::Image &image,
    const affine::BasisPlan &plan,
    AffineBasisLiveTransaction &transaction) {
  std::string journal;
  const std::string identity = "\"action\":" +
      std::to_string(request.action_id) + ",\"semantic\":" +
      std::to_string(request.semantic_id) + ",\"owner\":" +
      std::to_string(request.owner_version);
  if (!append_bounded(journal, request.maximum_journal_bytes,
          "{" + identity +
          ",\"kind\":\"affine-basis-begin\",\"equations\":" +
          std::to_string(image.equations.size()) + ",\"rows\":" +
          std::to_string(plan.rows.size()) + ",\"definitions\":" +
          std::to_string(transaction.projection_.size()) +
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
    const char *kind =
        event.kind == Event::Kind::Rat
            ? (event.definition ? "affine-basis-definition-rat"
               : event.install && event.clause.size() == 1
                   ? "affine-basis-helper-fix-rat"
                   : "affine-basis-proof-rat")
        : event.kind == Event::Kind::Rup ? "affine-basis-proof-rup"
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
  if (transaction.introduced_.size() != transaction.projection_.size() ||
      transaction.introduced_.size() +
              transaction.proof_only_helpers_.size() !=
          transaction.extent_growth_)
    return false;
  for (size_t i = 0; i < transaction.projection_.size(); ++i) {
    const auto &projection = transaction.projection_[i];
    if (projection.output != transaction.introduced_[i] ||
        !projection.output || !projection.first || !projection.second ||
        !append_bounded(
            journal, request.maximum_journal_bytes,
            "{" + identity +
            ",\"kind\":\"affine-basis-theory-variable\",\"output\":" +
            std::to_string(projection.output) +
            ",\"meaning\":\"output iff first XOR second\",\"first\":" +
            std::to_string(projection.first) + ",\"second\":" +
            std::to_string(projection.second) +
            ",\"projection\":\"omit-extension\","
            "\"reconstruction\":\"evaluate-operand-xor\"}\n"))
      return false;
  }
  for (size_t i = 0; i < transaction.proof_only_helpers_.size(); ++i) {
    const int32_t helper = transaction.proof_only_helpers_[i];
    if (helper <= static_cast<int32_t>(transaction.starting_extent_) ||
        uint64_t(helper) > uint64_t(transaction.starting_extent_) +
                               transaction.extent_growth_ ||
        (i && transaction.proof_only_helpers_[i - 1] >= helper) ||
        std::binary_search(transaction.introduced_.begin(),
                           transaction.introduced_.end(), helper) ||
        !append_bounded(
            journal, request.maximum_journal_bytes,
            "{" + identity +
            ",\"kind\":\"affine-basis-proof-helper\",\"output\":" +
            std::to_string(helper) +
            ",\"meaning\":\"retired transition identity fixed true\","
            "\"projection\":\"omit-extension\","
            "\"reconstruction\":\"assign-true\"}\n"))
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

bool make_equation_identity(const affine::Image &image,
                            const AffineLiveSnapshot &snapshot,
                            size_t source_index,
                            AffineBasisEquationIdentity &output) {
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
                         equation.premises.end()) != equation.premises.end())
    return false;
  output = std::move(equation);
  return true;
}

bool make_consumed_key(const affine::Image &image,
                       const AffineLiveSnapshot &snapshot,
                       const affine::BasisRow &row,
                       AffineBasisConsumedKey &output) {
  AffineBasisConsumedKey key;
  key.variables = row.variables;
  key.rhs = row.rhs;
  for (size_t source_index : row.source_equations) {
    AffineBasisEquationIdentity equation;
    if (!make_equation_identity(image, snapshot, source_index, equation))
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

enum class FilterStatus : uint8_t {
  Complete,
  AllExcluded,
  Invalid,
  Exhausted,
};

bool exclusion_charge(uint64_t amount, uint64_t maximum, uint64_t &work) {
  if (amount > maximum - work) return false;
  work += amount;
  return true;
}

bool measure_excluded_key(const AffineBasisConsumedKey &key,
                          uint64_t maximum, uint64_t &work) {
  if (!exclusion_charge(1, maximum, work) ||
      !exclusion_charge(key.variables.size(), maximum, work))
    return false;
  for (const auto &equation : key.source_equations) {
    if (!exclusion_charge(1, maximum, work) ||
        !exclusion_charge(equation.variables.size(), maximum, work))
      return false;
    for (const auto &premise : equation.premises)
      if (!exclusion_charge(1, maximum, work) ||
          !exclusion_charge(premise.clause.size(), maximum, work))
        return false;
  }
  return true;
}

FilterStatus excluded_key(const std::vector<AffineBasisConsumedKey> &excluded,
                          const AffineBasisConsumedKey &key,
                          uint64_t maximum, uint64_t &work, bool &found) {
  size_t begin = 0, end = excluded.size();
  while (begin < end) {
    if (!exclusion_charge(1, maximum, work))
      return FilterStatus::Exhausted;
    const size_t middle = begin + (end - begin) / 2;
    if (excluded[middle] < key)
      begin = middle + 1;
    else
      end = middle;
  }
  found = begin < excluded.size() && !(key < excluded[begin]);
  return FilterStatus::Complete;
}

bool remap_atom(affine::BasisAtom &atom,
                const std::vector<uint32_t> &node_map) {
  if (atom.kind == affine::BasisAtomKind::Existing) return atom.value != 0;
  if (!atom.value || atom.value > node_map.size() ||
      !node_map[atom.value - 1])
    return false;
  atom.value = node_map[atom.value - 1];
  return true;
}

bool remap_clauses(std::vector<affine::BasisClause> &clauses,
                   const std::vector<uint32_t> &node_map) {
  for (auto &clause : clauses)
    for (auto &literal : clause)
      if (!remap_atom(literal.atom, node_map)) return false;
  return true;
}

FilterStatus filter_excluded_rows(
    const affine::Image &image, const AffineLiveSnapshot &snapshot,
    const std::vector<AffineBasisConsumedKey> &excluded, uint64_t maximum,
    affine::BasisPlan &plan, uint64_t &work, uint64_t &excluded_rows) {
  if (excluded.empty()) return FilterStatus::Complete;
  for (size_t i = 0; i < excluded.size(); ++i) {
    if (!measure_excluded_key(excluded[i], maximum, work))
      return FilterStatus::Exhausted;
    if (i) {
      if (!exclusion_charge(1, maximum, work))
        return FilterStatus::Exhausted;
      if (!(excluded[i - 1] < excluded[i])) return FilterStatus::Invalid;
    }
  }

  std::vector<size_t> retained;
  retained.reserve(plan.rows.size());
  for (size_t row_index = 0; row_index < plan.rows.size(); ++row_index) {
    const affine::BasisRow &row = plan.rows[row_index];
    // One-source direct rows are the source CNF itself and have never been
    // publication/session entries.  They are removed from a filtered plan so
    // its journal and row count describe only novel output.
    if (row.source_equations.size() == 1 &&
        row.kind != affine::BasisRowKind::Extended)
      continue;
    AffineBasisConsumedKey key;
    if (!make_consumed_key(image, snapshot, row, key))
      return FilterStatus::Invalid;
    if (!measure_excluded_key(key, maximum, work))
      return FilterStatus::Exhausted;
    bool found = false;
    const FilterStatus lookup =
        excluded_key(excluded, key, maximum, work, found);
    if (lookup != FilterStatus::Complete) return lookup;
    if (found)
      ++excluded_rows;
    else
      retained.push_back(row_index);
  }
  if (retained.empty())
    return excluded_rows ? FilterStatus::AllExcluded
                         : FilterStatus::Complete;

  if (plan.projection.size() != plan.definitions.size())
    return FilterStatus::Invalid;
  std::vector<size_t> row_map(plan.rows.size(), SIZE_MAX);
  for (size_t i = 0; i < retained.size(); ++i) row_map[retained[i]] = i;

  std::vector<uint8_t> needed(plan.definitions.size(), 0);
  for (size_t old_node = 0; old_node < plan.definitions.size(); ++old_node)
    for (size_t old_row : plan.definitions[old_node].used_by_rows)
      if (old_row < row_map.size() && row_map[old_row] != SIZE_MAX) {
        needed[old_node] = 1;
        break;
      }
  // Definitions are topological; a needed node's extension operands precede
  // it, so the reverse closure visits each dependency after marking it.
  for (size_t cursor = plan.definitions.size(); cursor; --cursor) {
    const size_t node = cursor - 1;
    if (!needed[node]) continue;
    for (const affine::BasisAtom operand :
         {plan.definitions[node].first, plan.definitions[node].second}) {
      if (operand.kind != affine::BasisAtomKind::Extension) continue;
      if (!operand.value || operand.value > node)
        return FilterStatus::Invalid;
      needed[operand.value - 1] = 1;
    }
  }

  std::vector<uint32_t> node_map(plan.definitions.size(), 0);
  uint32_t next_node = 0;
  for (size_t node = 0; node < needed.size(); ++node)
    if (needed[node]) node_map[node] = ++next_node;

  std::vector<affine::XorDefinition> definitions;
  std::vector<affine::ProjectionObligation> projection;
  definitions.reserve(next_node);
  projection.reserve(next_node);
  for (size_t node = 0; node < plan.definitions.size(); ++node) {
    if (!needed[node]) continue;
    affine::XorDefinition definition = plan.definitions[node];
    if (!remap_atom(definition.output, node_map) ||
        !remap_atom(definition.first, node_map) ||
        !remap_atom(definition.second, node_map) ||
        !remap_clauses(definition.clauses, node_map))
      return FilterStatus::Invalid;
    std::vector<size_t> uses;
    for (size_t old_row : definition.used_by_rows)
      if (old_row < row_map.size() && row_map[old_row] != SIZE_MAX)
        uses.push_back(row_map[old_row]);
    if (uses.empty()) return FilterStatus::Invalid;
    definition.used_by_rows = std::move(uses);
    definitions.push_back(std::move(definition));

    affine::ProjectionObligation obligation = plan.projection[node];
    if (!remap_atom(obligation.extension, node_map) ||
        !remap_atom(obligation.first, node_map) ||
        !remap_atom(obligation.second, node_map))
      return FilterStatus::Invalid;
    projection.push_back(std::move(obligation));
  }

  std::vector<affine::BasisRow> rows;
  rows.reserve(retained.size());
  for (size_t old_row : retained) {
    affine::BasisRow row = plan.rows[old_row];
    if (!remap_clauses(row.clauses, node_map))
      return FilterStatus::Invalid;
    if ((row.kind == affine::BasisRowKind::DirectUnit ||
         row.kind == affine::BasisRowKind::Extended) &&
        !remap_atom(row.root, node_map))
      return FilterStatus::Invalid;
    rows.push_back(std::move(row));
  }
  plan.rows = std::move(rows);
  plan.definitions = std::move(definitions);
  plan.projection = std::move(projection);
  return FilterStatus::Complete;
}

FilterStatus filter_selected_row(
    const AffineBasisRowSelection &selection, const affine::Image &image,
    const AffineLiveSnapshot &snapshot, affine::BasisPlan &plan) {
  size_t selected = SIZE_MAX;
  for (size_t i = 0; i < plan.rows.size(); ++i) {
    const affine::BasisRow &row = plan.rows[i];
    AffineBasisRowSelection actual;
    if (!affine_basis_row_selection(selection.owner_version, image, snapshot,
                                    row, actual))
      return FilterStatus::Invalid;
    if (actual == selection) {
      if (selected != SIZE_MAX) return FilterStatus::Invalid;
      selected = i;
    }
  }
  if (selected == SIZE_MAX) return FilterStatus::AllExcluded;
  // A selected wide row is rebuilt by the scalable parity certificate.  Drop
  // the whole-basis planner's local definition graph here so unrelated rows
  // cannot consume namespace or proof allowance.
  plan.rows = {plan.rows[selected]};
  plan.definitions.clear();
  plan.projection.clear();
  return FilterStatus::Complete;
}

} // namespace

bool affine_basis_consumed_key(const affine::Image &image,
                               const AffineLiveSnapshot &snapshot,
                               const affine::BasisRow &row,
                               AffineBasisConsumedKey &output) {
  return make_consumed_key(image, snapshot, row, output);
}

bool affine_basis_row_selection(uint64_t owner_version,
                                const affine::Image &image,
                                const AffineLiveSnapshot &snapshot,
                                const affine::BasisRow &row,
                                AffineBasisRowSelection &output) {
  if (!owner_version) return false;
  AffineBasisRowSelection selection;
  selection.owner_version = owner_version;
  selection.kind = row.kind;
  selection.variables = row.variables;
  selection.rhs = row.rhs;
  selection.source_equation_ordinals = row.source_equations;
  for (size_t source_index : row.source_equations) {
    AffineBasisEquationIdentity equation;
    if (!make_equation_identity(image, snapshot, source_index, equation))
      return false;
    selection.source_equations.push_back(std::move(equation));
  }
  output = std::move(selection);
  return true;
}

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
  if (request.owner) {
    uint64_t payload_work = 0;
    if (!valid_owned_snapshot(kernel, request, *request.owner,
                              result.snapshot_literals, payload_work)) {
      result.status = request.owner->arena_generation != kernel.arena_generation
                          ? AffineBasisLiveStatus::Stale
                          : AffineBasisLiveStatus::Invalid;
      return result;
    }
    transaction.snapshot_ = *request.owner;
  } else if (!capture_affine_live_snapshot(kernel, snapshot_request,
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
  const affine::BasisPlanStatus planned = request.selection
      ? affine::build_selectable_basis_plan(
            image.equations, request.basis,
            [&](uint64_t) { return !kernel_terminating(kernel); }, basis)
      : affine::build_basis_plan(
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
  if (request.selection) {
    const FilterStatus selected = filter_selected_row(
        *request.selection, image, transaction.snapshot_, basis);
    if (selected != FilterStatus::Complete) {
      result.status = selected == FilterStatus::AllExcluded
                          ? AffineBasisLiveStatus::Stale
                          : AffineBasisLiveStatus::Invalid;
      return result;
    }
    result.selection = *request.selection;
  }
  if (request.excluded_keys) {
    const FilterStatus filtered = filter_excluded_rows(
        image, transaction.snapshot_, *request.excluded_keys,
        request.maximum_exclusion_work, basis, result.exclusion_work,
        result.excluded_rows);
    if (filtered != FilterStatus::Complete) {
      result.status = filtered == FilterStatus::AllExcluded
                          ? AffineBasisLiveStatus::NoStructure
                      : filtered == FilterStatus::Exhausted
                          ? AffineBasisLiveStatus::ResourceExhausted
                          : AffineBasisLiveStatus::Invalid;
      return result;
    }
  }
  result.rows = basis.rows.size();
  const bool selected_wide = request.selection && basis.rows.size() == 1 &&
      basis.rows.front().kind == affine::BasisRowKind::Extended;
  result.definitions = basis.definitions.size();
  if (transaction.starting_extent_ > request.maximum_external_extent ||
      transaction.starting_extent_ > INT32_MAX ||
      (!selected_wide &&
       (basis.definitions.size() > UINT32_MAX ||
        basis.definitions.size() > UINT32_MAX - kernel.nvars ||
        basis.definitions.size() >
            uint64_t(request.maximum_external_extent) -
                transaction.starting_extent_ ||
        transaction.starting_extent_ + basis.definitions.size() >
            INT32_MAX))) {
    result.status = AffineBasisLiveStatus::ResourceExhausted;
    return result;
  }
  transaction.extent_growth_ = basis.definitions.size();
  const uint32_t first_extension = transaction.starting_extent_ + 1;
  for (const affine::ProjectionObligation &projection : basis.projection) {
    const int32_t output = external_atom(projection.extension, first_extension);
    const int32_t first = external_atom(projection.first, first_extension);
    const int32_t second = external_atom(projection.second, first_extension);
    if (!output || !first || !second) {
      result.status = AffineBasisLiveStatus::Invalid;
      return result;
    }
    transaction.introduced_.push_back(output);
    transaction.projection_.push_back({output, first, second});
  }
  if (transaction.projection_.size() != basis.definitions.size()) {
    result.status = AffineBasisLiveStatus::Invalid;
    return result;
  }

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
  if (selected_wide) {
    const uint64_t extent_allowance =
        uint64_t(request.maximum_external_extent) -
        transaction.starting_extent_;
    const uint64_t internal_allowance = UINT32_MAX - kernel.nvars;
    const uint64_t signed_allowance =
        uint64_t(INT32_MAX) - transaction.starting_extent_;
    const uint64_t allowed =
        std::min({extent_allowance, internal_allowance, signed_allowance,
                  uint64_t(request.basis.maximum_nodes),
                  uint64_t(UINT32_MAX)});
    if (!allowed) {
      result.status = AffineBasisLiveStatus::ResourceExhausted;
      return result;
    }
    if (result.basis_work >= request.basis.maximum_work) {
      result.status = AffineBasisLiveStatus::ResourceExhausted;
      return result;
    }
    WideProofPlan wide;
    uint64_t proof_work = 0;
    const WideProofStatus prepared = prepare_wide_proof(
        kernel, image, basis.rows.front(), transaction.starting_extent_,
        static_cast<uint32_t>(allowed), request.basis.maximum_nodes,
        request.basis.maximum_work - result.basis_work, proof_work, sink,
        wide);
    result.basis_work += proof_work;
    if (prepared != WideProofStatus::Complete) {
      result.status = prepared == WideProofStatus::Exhausted
                          ? AffineBasisLiveStatus::ResourceExhausted
                      : prepared == WideProofStatus::Interrupted
                          ? AffineBasisLiveStatus::Interrupted
                      : prepared == WideProofStatus::Unsupported
                          ? AffineBasisLiveStatus::UnsupportedProof
                          : AffineBasisLiveStatus::Invalid;
      return result;
    }
    transaction.extent_growth_ = wide.helpers;
    transaction.published_ = wide.published;
    transaction.arena_words_ = wide.arena_words;
    transaction.introduced_ = std::move(wide.introduced);
    transaction.projection_ = std::move(wide.projection);
    transaction.proof_only_helpers_ = std::move(wide.proof_only_helpers);
    AffineBasisConsumedKey row_key;
    if (!make_consumed_key(image, transaction.snapshot_, basis.rows.front(),
                           row_key)) {
      result.status = AffineBasisLiveStatus::Invalid;
      return result;
    }
    transaction.consumed_keys_.push_back(std::move(row_key));
  }
  if (!selected_wide) {
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
    if (!make_consumed_key(image, transaction.snapshot_, row, row_key)) {
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
      if (clause_covered(kernel, target)) continue;
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
  result.definitions = transaction.projection_.size();
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
          static_cast<int32_t>(transaction.starting_extent_ + i + 1))
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

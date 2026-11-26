#include "rules/analyze/conflict_er.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "rules/reduce/reduce.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <unordered_map>

namespace tactix {
namespace {

using Retention = ConflictEr::Retention;

bool supports_pair(const Kernel &k, Lit a, const ConflictEr::Definition &d) {
  return !kernel_var_retired(k, var_of(d.output)) &&
         kernel_binary_live(k, d.output, negate(a)) &&
         kernel_binary_live(k, d.output, negate(d.right));
}

// The author requires more than two nonasserting literals for discovery.
constexpr size_t minimum_discovery_size = 4;

void reset_learned_window(ConflictEr &er) {
  er.previous_has_reference = false;
  er.previous.clear();
  er.older_heads.clear();
}

bool same_tail(ConflictEr &er, const std::vector<Lit> &clause) {
  assert(er.previous.size() == clause.size());
  if (std::equal(clause.begin() + 1, clause.end(), er.previous.begin() + 1))
    return true;
  for (size_t i = 1; i < clause.size(); ++i)
    er.positions[clause[i]] = 1;
  const bool equal = std::all_of(er.previous.begin() + 1, er.previous.end(),
                                [&](Lit l) { return er.positions[l] != 0; });
  for (size_t i = 1; i < clause.size(); ++i)
    er.positions[clause[i]] = 0;
  return equal;
}

Lit publish_pair(Kernel &k, ConflictEr &er, Lit a, Lit b, bool search_support) {
  if (var_of(a) == var_of(b))
    return INVALID_LIT;
  if (a > b)
    std::swap(a, b);
  auto &row = er.definitions[a];
  auto existing = std::find_if(row.begin(), row.end(), [b](const auto &d) {
    return d.right == b;
  });
  if (existing != row.end() && supports_pair(k, a, *existing))
    return INVALID_LIT;
  const Lit output = extended_resolution_define_search_or(k, a, b, search_support);
  if (output == INVALID_LIT)
    return INVALID_LIT;
  const Var output_var = var_of(output);
  if (k.value(output) && !k.var_level(output_var))
    heuristics_deactivate_var(k, output_var);
  er.retained.resize(k.nvars);
  er.retained[output_var] = Retention::Kept;
  if (er.factor_learned) {
    const Var external = k.internal_to_external[output_var];
    assert(er.automatic_outputs_external.empty() ||
           er.automatic_outputs_external.back() < external);
    er.automatic_outputs_external.push_back(external);
    er.use_epoch.resize(k.nvars);
    er.use_epoch[output_var] = k.stats.reductions;
  }
  if (existing == row.end()) {
    row.push_back({b, output});
  } else {
    er.retained[var_of(existing->output)] = Retention::Released;
    existing->output = output;
  }
  ++k.stats.conflict_er_definitions;
  return output;
}

// A lower bound on the fresh variable's mandatory state.
constexpr size_t variable_bytes = sizeof(Assigned) + 2 * sizeof(WatchHead) +
                                  2 * sizeof(int8_t) + 2 * sizeof(Var);
constexpr size_t variable_words =
    (variable_bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t);
// After cancelling one old large watch pair and clause header against the
// reverse OR clause, charge its three literals, the binary watch words, and
// the fresh variable. Leave smaller source clauses in the native database.
constexpr size_t minimum_replacement_tail_words = 3 + 4 + variable_words;

void queue_learned_pair(Kernel &k, ConflictEr &er, const std::vector<Lit> &clause) {
  if (clause.size() <= minimum_replacement_tail_words || k.pending.ref == INVALID_REF) {
    er.previous_has_reference = false;
    er.previous.clear();
    return;
  }
  if (er.previous_has_reference && er.previous_generation == k.arena_generation &&
      er.previous.size() == clause.size() &&
      var_of(er.previous.front()) != var_of(clause.front()) && same_tail(er, clause)) {
    const int first = kernel_externalize(k, er.previous.front());
    const int second = kernel_externalize(k, clause.front());
    er.learned_pair_heads.emplace_back(std::minmax(first, second));
  }
  er.previous_generation = k.arena_generation;
  er.previous_has_reference = true;
  er.previous = clause;
}

const Clause *live_learned_source(const Kernel &k, const ConflictEr::LearnedOrigin &origin) {
  if (origin.ref >= k.arena.size())
    return nullptr;
  const Clause *clause = clause_at(k.arena, origin.ref);
  if (clause->garbage() || !clause->redundant() || clause->id() != origin.id)
    return nullptr;
  return clause;
}

bool extract_tail(const Kernel &k, const ConflictEr::LearnedOrigin &origin,
                  std::vector<Lit> &tail) {
  const Clause *clause = live_learned_source(k, origin);
  if (!clause)
    return false;
  tail.clear();
  for (const Lit lit : *clause) {
    if (k.value(lit))
      return false;
    if (lit != origin.head)
      tail.push_back(lit);
  }
  if (tail.size() + 1 != clause->size)
    return false;
  std::sort(tail.begin(), tail.end());
  return true;
}

bool learned_pair_fits(const Kernel &k, size_t tail_size) {
  return k.next_clause_id <= uint64_t(INT64_MAX) - 4 &&
         uint64_t(k.arena.size()) + clause_words(tail_size + 1) +
             clause_words(3) < INVALID_REF;
}

using LearnedRows = std::unordered_map<Lit, std::map<std::vector<Lit>, ConflictEr::LearnedOrigin>>;
struct IndexedLearnedPair {
  ConflictEr::LearnedPair sources;
  const std::vector<Lit> &tail; // Borrowed from the rows for this batch.
};
using LearnedGroups = std::map<std::pair<Lit, Lit>, std::vector<IndexedLearnedPair>>;

LearnedGroups complete_live_sources(Kernel &k, std::vector<std::pair<int, int>> &seeds,
                                    LearnedRows &rows) {
  std::sort(seeds.begin(), seeds.end());
  seeds.erase(std::unique(seeds.begin(), seeds.end()), seeds.end());

  using Origin = ConflictEr::LearnedOrigin;
  std::vector<std::pair<Lit, Lit>> candidates;
  for (const auto &[first, second] : seeds) {
    const Lit left = kernel_internalize(k, first);
    const Lit right = kernel_internalize(k, second);
    if (left == INVALID_LIT || right == INVALID_LIT ||
        kernel_var_retired(k, var_of(left)) || kernel_var_retired(k, var_of(right)) ||
        k.value(left) || k.value(right))
      continue;
    candidates.emplace_back(left, right);
    rows.try_emplace(left);
    rows.try_emplace(right);
  }
  if (candidates.empty())
    return {};

  std::vector<Lit> ordered, tail;
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (kernel_terminating(k))
      return {};
    if (clause->garbage() || clause->detached_reason() || !clause->redundant() ||
        clause->size <= minimum_replacement_tail_words)
      continue;
    ordered.clear();
    for (const Lit head : *clause) {
      const auto row = rows.find(head);
      if (row == rows.end())
        continue;
      const Origin origin{ref, clause->id(), head};
      if (ordered.empty()) {
        if (std::any_of(clause->begin(), clause->end(), [&](Lit l) { return k.value(l); }))
          break;
        ordered.assign(clause->begin(), clause->end());
        std::sort(ordered.begin(), ordered.end());
      }
      const auto position = std::lower_bound(ordered.begin(), ordered.end(), head);
      assert(position != ordered.end() && *position == head);
      const auto next = position + 1;
      if (next != ordered.end() && *next == head)
        continue;
      tail.assign(ordered.begin(), position);
      tail.insert(tail.end(), next, ordered.end());
      const auto [entry, inserted] = row->second.try_emplace(tail, origin);
      if (!inserted) {
        const Clause *saved = clause_at(k.arena, entry->second.ref);
        if (clause->glue() < saved->glue() ||
            (clause->glue() == saved->glue() && clause->used() > saved->used()))
          entry->second = origin;
      }
    }
  }

  LearnedGroups groups;
  for (const auto &[left, right] : candidates) {
    const auto &other = rows.at(right);
    for (const auto &[remainder, first] : rows.at(left)) {
      const auto second = other.find(remainder);
      if (second != other.end())
        groups[std::minmax(left, right)].push_back({{first, second->second}, remainder});
    }
  }
  return groups;
}

std::pair<uint32_t, uint32_t>
learned_pair_quotient_priority(const Clause &first, const Clause &second) {
  return {std::min(first.glue(), second.glue()),
          std::max(first.used(), second.used())};
}

template <typename Publish>
Lit commit_learned_pair(Kernel &k, const ConflictEr::LearnedPair &pair,
                        const std::vector<Lit> &tail, std::vector<Lit> &quotient,
                        Publish publish) {
  const auto [glue, used] = learned_pair_quotient_priority(
      *clause_at(k.arena, pair.first.ref), *clause_at(k.arena, pair.second.ref));
  const Lit output = publish(negate(pair.first.head), negate(pair.second.head));
  if (output == INVALID_LIT)
    return INVALID_LIT;

  // With the tail false the sources imply both heads. The reverse OR
  // then implies the negated output, proving the quotient by RUP.
  quotient.assign(1, negate(output));
  quotient.insert(quotient.end(), tail.begin(), tail.end());
  const auto size = static_cast<uint32_t>(quotient.size());
  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(id, quotient.data(), size);
  const Ref ref = kernel_add_large_clause(k, quotient.data(), size, true, glue, id);
  clause_at(k.arena, ref)->set_used(used);
  kernel_delete_clause(k, clause_at(k.arena, pair.first.ref));
  kernel_delete_clause(k, clause_at(k.arena, pair.second.ref));
  ++k.stats.conflict_er_factored_pairs;
  return output;
}

} // namespace

const std::vector<Lit> &conflict_er_rewrite(Kernel &k,
                                           const std::vector<Lit> &clause) {
  auto &er = *k.conflict_er;
  er.rewritten.clear();
  auto &candidates = er.candidates;
  candidates.clear();
  if (clause.size() < 3 || er.definitions.empty() ||
      k.next_clause_id > uint64_t(INT64_MAX) - 3)
    return er.rewritten;
  er.positions.resize(size_t{2} * k.nvars);
  for (uint32_t i = 1; i < clause.size(); ++i)
    er.positions[clause[i]] = i;

  const uint32_t head_level = k.var_level(var_of(clause.front()));
  for (uint32_t i = 1; i < clause.size(); ++i) {
    const Lit a = clause[i];
    const auto row = er.definitions.find(a);
    if (row == er.definitions.end())
      continue;
    for (const auto &d : row->second) {
      const uint32_t j = er.positions[d.right];
      if (!j || k.value(d.output) >= 0)
        continue;
      const Var output = var_of(d.output);
      const uint32_t level = k.var_level(output);
      if (level && level < head_level && conflict_er_retained(er, output) &&
          supports_pair(k, a, d))
        candidates.push_back({a, d.right, d.output, i, j,
                              k.h ? k.h->scores.score[output] : 0.0});
    }
  }

  if (!candidates.empty()) {
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
      if (a.score != b.score)
        return a.score > b.score;
      const auto a_positions = std::minmax(a.left_position, a.right_position);
      const auto b_positions = std::minmax(b.left_position, b.right_position);
      return a_positions < b_positions;
    });
    er.rewritten = clause;
    size_t applied = 0;
    for (const auto &c : candidates) {
      if (er.rewritten[c.left_position] != c.left ||
          er.rewritten[c.right_position] != c.right)
        continue;
      const uint32_t i = std::min(c.left_position, c.right_position);
      const uint32_t j = std::max(c.left_position, c.right_position);
      er.positions[c.left] = er.positions[c.right] = 0;
      er.rewritten[i] = er.positions[c.output] ? INVALID_LIT : c.output;
      er.rewritten[j] = INVALID_LIT;
      if (er.rewritten[i] != INVALID_LIT)
        er.positions[c.output] = i;
      candidates[applied++] = c;
    }
    candidates.resize(applied);
    for (const auto &c : candidates)
      er.positions[c.output] = 0;
    std::erase(er.rewritten, INVALID_LIT);
  }
  for (const Lit l : clause)
    er.positions[l] = 0;
  return er.rewritten;
}

// A definition that has paid for itself can earn protection through reuse
// while the solver is still waiting for the first LBD improvement.
void conflict_er_refresh_applied_outputs(Kernel &k, bool improved_lbd) {
  auto &er = *k.conflict_er;
  if (!er.factor_learned || (!improved_lbd && er.observed_lbd_gain))
    return;
  assert(!er.candidates.empty());
  er.observed_lbd_gain |= improved_lbd;
  for (const auto &candidate : er.candidates) {
    const Var output = var_of(candidate.output);
    if (improved_lbd || er.retained[output] == Retention::Paid)
      er.use_epoch[output] = k.stats.reductions;
  }
}

void conflict_er_observe(Kernel &k, const std::vector<Lit> &clause, bool factor_learned) {
  if (!k.conflict_er)
    k.conflict_er.emplace();
  auto &er = *k.conflict_er;
  if (er.factor_learned != factor_learned) {
    er.factor_learned = factor_learned;
    er.observed_lbd_gain = false;
    er.learned_pair_heads.clear();
    reset_learned_window(er);
    if (factor_learned)
      er.use_epoch.resize(k.nvars, k.stats.reductions);
  }
  if (er.last_learned + 1 != k.stats.learned ||
      er.last_restart != k.stats.restarts) {
    reset_learned_window(er);
  }
  er.last_learned = k.stats.learned;
  er.last_restart = k.stats.restarts;
  if (er.last_reduction != k.stats.reductions) {
    for (auto it = er.definitions.begin(); it != er.definitions.end();) {
      std::erase_if(it->second, [&](const auto &d) {
        if (supports_pair(k, it->first, d))
          return false;
        er.retained[var_of(d.output)] = Retention::Released;
        return true;
      });
      if (it->second.empty())
        it = er.definitions.erase(it);
      else
        ++it;
    }
    er.last_reduction = k.stats.reductions;
  }
  if (clause.size() < minimum_discovery_size) {
    reset_learned_window(er);
    return;
  }
  er.positions.resize(size_t{2} * k.nvars);
  if (factor_learned) {
    queue_learned_pair(k, er, clause);
    return;
  }
  const Lit head = clause.front();
  if (er.previous.size() == clause.size() &&
      var_of(er.previous.front()) != var_of(head) && same_tail(er, clause)) {
    const auto publish = [&](Lit left) {
      const Lit output = publish_pair(k, er, left, negate(head), false);
      if (output != INVALID_LIT)
        strategy::finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
      return output != INVALID_LIT;
    };
    const Lit previous = negate(er.previous.front());
    const bool published = publish(previous);
    for (const Lit older : er.older_heads)
      publish(older);
    if (published)
      er.older_heads.push_back(previous);
    else
      er.older_heads.clear();
  } else {
    er.older_heads.clear();
  }
  er.previous = clause;
}

DeferredConflictErBatch::DeferredConflictErBatch(Kernel &k) {
  if (!k.conflict_er)
    return;
  er_ = &*k.conflict_er;
  er_->previous_has_reference = false;
}

DeferredConflictErBatch::~DeferredConflictErBatch() {
  if (er_)
    er_->learned_pair_heads.clear();
}

bool conflict_er_factor_learned_pairs(Kernel &k, bool consume_seeds) {
  if (!k.conflict_er || k.level || k.dense || k.pending.valid || k.conflict_valid ||
      k.inconsistent || !k.large_watches_connected || k.probing.recording ||
      !kernel_propagation_fixpoint(k))
    return false;
  auto &er = *k.conflict_er;
  if (er.learned_pair_heads.empty())
    return false;
  LearnedRows rows;
  const LearnedGroups groups = complete_live_sources(k, er.learned_pair_heads, rows);
  if (consume_seeds)
    er.learned_pair_heads.clear();
  er.previous_has_reference = false;
  std::vector<Lit> quotient;

  // Each pair removes one clause after paying for its quotient. Charge
  // three definition clauses and one variable against those savings.
  constexpr size_t support_clauses = 3;
  constexpr size_t definition_variables = 1;
  constexpr size_t definition_size = support_clauses + definition_variables;
  constexpr uint64_t large_watch_words = 4;
  constexpr uint64_t two_binary_watch_words = 4;
  const uint64_t support_words = clause_words(3) + large_watch_words +
                                 two_binary_watch_words + variable_words;
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("ExtendedResolution")];
  for (const auto &[heads, pairs] : groups) {
    if (kernel_terminating(k))
      break;
    if (pairs.size() <= definition_size)
      continue;

    // A previous group can delete shared sources. Surviving sources and
    // their root assignments stay unchanged throughout this batch.
    std::vector<const IndexedLearnedPair *> selected;
    uint64_t quotient_words = 0;
    size_t qualifying_pairs = 0;
    int64_t protected_savings = 0;
    // A definition should retain at least one quotient that Reduce protects.
    bool has_protected_quotient = false;
    const uint32_t tier2 = std::max(k.tier1, k.tier2);
    for (const auto &pair : pairs) {
      const Clause *first = live_learned_source(k, pair.sources.first);
      const Clause *second = live_learned_source(k, pair.sources.second);
      if (!first || !second || !learned_pair_fits(k, pair.tail.size()))
        continue;
      selected.push_back(&pair);
      quotient_words += clause_words(pair.tail.size() + 1);
      const auto [glue, used] = learned_pair_quotient_priority(*first, *second);
      qualifying_pairs += glue < pair.tail.size();
      const bool protected_quotient = reduce_protects_by_recency(glue, used, k.tier1, tier2);
      has_protected_quotient |= protected_quotient;
      protected_savings += reduce_protects_by_recency(first->glue(), first->used(), k.tier1, tier2);
      protected_savings += reduce_protects_by_recency(second->glue(), second->used(), k.tier1, tier2);
      protected_savings -= protected_quotient;
    }
    const uint64_t removed_words = quotient_words + selected.size() * large_watch_words;
    // Require enough lower-LBD quotients to pay for the definition until
    // an applied substitution has demonstrated an LBD improvement.
    const size_t pairs_covering_cost = er.observed_lbd_gain ? selected.size() : qualifying_pairs;
    if (!has_protected_quotient || pairs_covering_cost <= definition_size ||
        removed_words <= support_words ||
        selected.size() > uint64_t(INT64_MAX) - k.next_clause_id - support_clauses ||
        uint64_t(k.arena.size()) + clause_words(3) + quotient_words >= INVALID_REF)
      continue;

    apply_rule_structured_preserve_startup(rule, k, "factor-learned-pair-batch", [&] {
      const Lit output = publish_pair(k, er, negate(heads.first), negate(heads.second), true);
      if (output == INVALID_LIT)
        return ApplyResult::NoOp;
      if (protected_savings >= static_cast<int64_t>(definition_size))
        er.retained[var_of(output)] = Retention::Paid;
      for (const auto *pair : selected)
        commit_learned_pair(k, pair->sources, pair->tail, quotient,
                            [&](Lit, Lit) { return output; });
      k.stats.conflict_er_factored_words += removed_words - support_words;
      return ApplyResult::Ok;
    });
  }
  return true;
}

bool conflict_er_factor_policy_batch(Kernel &k, bool consume_seeds,
                                     bool before_first_elimination) {
  if (!before_first_elimination && !k.stats.conflict_er_definitions)
    return false;
  const Var first_output = k.nvars;
  const bool attempted = conflict_er_factor_learned_pairs(k, consume_seeds);
  if (first_output < k.nvars)
    heuristics_demote_fresh_vars(k, first_output, k.nvars);
  return attempted;
}

ConflictEr::LearnedPair conflict_er_find_learned_pair(
    const Kernel &k, Lit first_head, Lit second_head, std::vector<Lit> tail) {
  ConflictEr::LearnedPair pair;
  if (first_head == INVALID_LIT || second_head == INVALID_LIT ||
      var_of(first_head) == var_of(second_head) || tail.size() < 2 ||
      !extended_resolution_guard(k))
    return pair;
  std::sort(tail.begin(), tail.end());
  std::vector<Lit> candidate_tail;
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (clause->garbage() || !clause->redundant() || clause->size != tail.size() + 1)
      continue;
    const auto match = [&](Lit head, ConflictEr::LearnedOrigin &source) {
      if (source.ref == INVALID_REF && extract_tail(k, {ref, clause->id(), head},
                                                   candidate_tail) &&
          candidate_tail == tail)
        source = {ref, clause->id(), head};
    };
    match(first_head, pair.first);
    match(second_head, pair.second);
    if (pair.first.ref != INVALID_REF && pair.second.ref != INVALID_REF)
      return pair;
  }
  return {};
}

Lit conflict_er_define_extension(Kernel &k, Lit first_head, Lit second_head) {
  if (!extended_resolution_guard(k) || first_head == INVALID_LIT ||
      second_head == INVALID_LIT || var_of(first_head) == var_of(second_head))
    return INVALID_LIT;
  if (!k.conflict_er)
    k.conflict_er.emplace();
  return publish_pair(k, *k.conflict_er, negate(first_head), negate(second_head), true);
}

Lit conflict_er_factor_learned_pair(Kernel &k, const ConflictEr::LearnedPair &pair,
                                   Lit extension) {
  if (pair.first.ref == pair.second.ref || pair.first.head == INVALID_LIT ||
      pair.second.head == INVALID_LIT || var_of(pair.first.head) == var_of(pair.second.head) ||
      !extended_resolution_guard(k))
    return INVALID_LIT;
  std::vector<Lit> tail, other_tail;
  if (!extract_tail(k, pair.first, tail) || !extract_tail(k, pair.second, other_tail) ||
      tail != other_tail || tail.size() < 2)
    return INVALID_LIT;
  if (extension != INVALID_LIT) {
    const Lit first = pair.first.head, second = pair.second.head;
    const Var output = var_of(extension);
    if (output == var_of(first) || output == var_of(second) ||
        kernel_var_retired(k, output) || k.value(extension) ||
        std::any_of(tail.begin(), tail.end(), [&](Lit lit) { return var_of(lit) == output; }) ||
        !kernel_binary_live(k, extension, first) || !kernel_binary_live(k, extension, second) ||
        k.next_clause_id >= uint64_t(INT64_MAX) ||
        uint64_t(k.arena.size()) + clause_words(tail.size() + 1) >= INVALID_REF)
      return INVALID_LIT;

    // The two heads imply !extension through the reverse definition clause.
    // The forward binaries alone only justify the conflict substitution.
    const Lit reverse[] = {negate(extension), negate(first), negate(second)};
    for (const auto [ref, clause] : arena_records(k.arena)) {
      if (kernel_terminating(k))
        return INVALID_LIT;
      if (!clause->garbage() && !clause->detached_reason() && clause->size == 3 &&
          std::is_permutation(clause->begin(), clause->end(), std::begin(reverse)))
        return commit_learned_pair(k, pair, tail, other_tail,
                                   [&](Lit, Lit) { return extension; });
    }
    return INVALID_LIT;
  }
  if (!learned_pair_fits(k, tail.size()))
    return INVALID_LIT;
  return commit_learned_pair(k, pair, tail, other_tail, [&](Lit left, Lit right) {
    return extended_resolution_define_search_or(k, left, right, true);
  });
}

void conflict_er_release_cold_outputs(Kernel &k) {
  assert(!k.level);
  auto &er = *k.conflict_er;
  if (er.last_retirement == k.stats.reductions)
    return;
  er.last_retirement = k.stats.reductions;
  std::vector<Lit> outputs;
  std::vector<uint8_t> dependency(k.nvars);
  for (const auto &[a, row] : er.definitions)
    for (const auto &d : row) {
      const Var output = var_of(d.output);
      if (!supports_pair(k, a, d)) {
        er.retained[output] = Retention::Released;
        continue;
      }
      dependency[var_of(a)] = dependency[var_of(d.right)] = 1;
      if (conflict_er_retained(er, output))
        outputs.push_back(d.output);
    }
  if (!er.factor_learned) {
    const auto score = [&](Lit l) {
      return k.h ? k.h->scores.score[var_of(l)] : 0.0;
    };
    std::sort(outputs.begin(), outputs.end(), [&](Lit a, Lit b) {
      return std::pair{score(a), a} < std::pair{score(b), b};
    });
    // GlucosER considers the lowest-activity half after a reduction.
    outputs.resize(outputs.size() / 2);
  }
  // Native BVE projects the output and all clauses that still use it.
  for (const Lit output : outputs) {
    const Var v = var_of(output);
    if (k.value(output) || dependency[v] ||
        (er.factor_learned && k.stats.reductions - er.use_epoch[v] <= 1))
      continue;
    er.retained[v] = Retention::Released;
    kernel_mark_bve_retry(k, v);
    ++k.stats.conflict_er_released;
  }
}

void conflict_er_compact(ConflictEr &er, const std::vector<Var> &old_to_new) {
  const auto remap = [&](Lit literal) {
    const Var next = old_to_new[var_of(literal)];
    return next == INVALID_VAR ? INVALID_LIT : make_lit(next, negated(literal));
  };
  decltype(er.definitions) remapped;
  decltype(er.retained) retained;
  std::vector<uint64_t> use_epoch;
  for (auto &[a, definitions] : er.definitions) {
    const Lit left = remap(a);
    if (left == INVALID_LIT)
      continue;
    std::erase_if(definitions, [&](auto &d) {
      const Var old_output = var_of(d.output);
      const auto keep = conflict_er_retention(er, old_output);
      d.right = remap(d.right);
      d.output = remap(d.output);
      if (d.right == INVALID_LIT || d.output == INVALID_LIT)
        return true;
      const Var output = var_of(d.output);
      if (output >= retained.size())
        retained.resize(size_t{output} + 1);
      retained[output] = keep;
      if (er.factor_learned) {
        if (output >= use_epoch.size())
          use_epoch.resize(size_t{output} + 1);
        use_epoch[output] = er.use_epoch[old_output];
      }
      return false;
    });
    if (!definitions.empty())
      remapped.emplace(left, std::move(definitions));
  }
  er.definitions.swap(remapped);
  er.retained.swap(retained);
  er.use_epoch.swap(use_epoch);
  reset_learned_window(er);
}

} // namespace tactix

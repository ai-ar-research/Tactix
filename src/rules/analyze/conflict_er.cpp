#include "rules/analyze/conflict_er.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <climits>
#include <map>

namespace tactix {
namespace {

bool supports_pair(const Kernel &k, Lit a, const ConflictEr::Definition &d) {
  return !kernel_var_retired(k, var_of(d.output)) &&
         kernel_binary_live(k, d.output, negate(a)) &&
         kernel_binary_live(k, d.output, negate(d.right));
}

// The author requires more than two nonasserting literals for discovery.
constexpr size_t minimum_discovery_size = 4;

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

Lit publish_pair(Kernel &k, ConflictEr &er, Lit a, Lit b) {
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
  const Lit output = extended_resolution_define_search_or(k, a, b, er.factor_learned);
  if (output == INVALID_LIT)
    return INVALID_LIT;
  const Var output_var = var_of(output);
  if (k.value(output) && !k.var_level(output_var))
    heuristics_deactivate_var(k, output_var);
  er.retained.resize(k.nvars);
  er.retained[output_var] = 1;
  if (er.factor_learned) {
    er.use_epoch.resize(k.nvars);
    er.use_epoch[output_var] = k.stats.reductions;
  }
  if (existing == row.end()) {
    row.push_back({b, output});
  } else {
    er.retained[var_of(existing->output)] = 0;
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
  if (er.origin_generation != k.arena_generation) {
    er.learned_pairs.clear();
    er.previous_origin = {};
    er.origin_generation = k.arena_generation;
  }
  if (clause.size() > minimum_replacement_tail_words && er.previous_origin.ref != INVALID_REF &&
      k.pending.ref != INVALID_REF && er.previous.size() == clause.size() &&
      var_of(er.previous.front()) != var_of(clause.front()) && same_tail(er, clause))
    er.learned_pairs.push_back({er.previous_origin,
                               {k.pending.ref, k.pending.id, clause.front()}});
  er.previous_origin = {k.pending.ref, k.pending.id, clause.front()};
  er.previous = clause;
}

bool extract_tail(const Kernel &k, const ConflictEr::LearnedOrigin &origin,
                  std::vector<Lit> &tail) {
  if (origin.ref >= k.arena.size())
    return false;
  const Clause *clause = clause_at(k.arena, origin.ref);
  if (clause->garbage() || !clause->redundant() || clause->id() != origin.id)
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

bool match_learned_pair(const Kernel &k, const ConflictEr::LearnedPair &pair,
                        std::vector<Lit> &tail, std::vector<Lit> &other_tail) {
  return extract_tail(k, pair.first, tail) &&
         extract_tail(k, pair.second, other_tail) && tail == other_tail &&
         k.next_clause_id <= uint64_t(INT64_MAX) - 4 &&
         uint64_t(k.arena.size()) + clause_words(tail.size() + 1) +
             clause_words(3) < INVALID_REF;
}

template <typename Publish>
Lit commit_learned_pair(Kernel &k, const ConflictEr::LearnedPair &pair,
                        std::vector<Lit> &tail, Publish publish) {
  const Clause *first = clause_at(k.arena, pair.first.ref);
  const Clause *second = clause_at(k.arena, pair.second.ref);
  const uint32_t glue = std::min(first->glue(), second->glue());
  const uint32_t used = std::max(first->used(), second->used());
  const Lit output = publish(negate(pair.first.head), negate(pair.second.head));
  if (output == INVALID_LIT)
    return INVALID_LIT;

  // With the tail false the sources imply both heads. The reverse OR
  // then implies the negated output, proving the quotient by RUP.
  tail.insert(tail.begin(), negate(output));
  const uint64_t id = ++k.next_clause_id;
  if (k.proof)
    k.proof->add_rup(id, tail.data(), static_cast<uint32_t>(tail.size()));
  const Ref quotient = kernel_add_large_clause(k, tail.data(),
      static_cast<uint32_t>(tail.size()), true, glue, id);
  clause_at(k.arena, quotient)->set_used(used);
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
  if (clause.size() < 3 || er.definitions.empty() ||
      k.next_clause_id > uint64_t(INT64_MAX) - 3)
    return er.rewritten;
  er.positions.resize(size_t{2} * k.nvars);
  for (uint32_t i = 1; i < clause.size(); ++i)
    er.positions[clause[i]] = i;

  auto &candidates = er.candidates;
  candidates.clear();
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
    std::sort(candidates.begin(), candidates.end(), [](auto a, auto b) {
      if (a.score != b.score)
        return a.score > b.score;
      const auto a_positions = std::minmax(a.left_position, a.right_position);
      const auto b_positions = std::minmax(b.left_position, b.right_position);
      return a_positions < b_positions;
    });
    er.rewritten = clause;
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
      if (er.factor_learned)
        er.use_epoch[var_of(c.output)] = k.stats.reductions;
    }
    for (const auto &c : candidates)
      er.positions[c.output] = 0;
    std::erase(er.rewritten, INVALID_LIT);
  }
  for (const Lit l : clause)
    er.positions[l] = 0;
  return er.rewritten;
}

void conflict_er_observe(Kernel &k, const std::vector<Lit> &clause, bool factor_learned) {
  if (!k.conflict_er)
    k.conflict_er.emplace();
  auto &er = *k.conflict_er;
  if (er.factor_learned != factor_learned) {
    er.factor_learned = factor_learned;
    er.learned_pairs.clear();
    er.previous_origin = {};
    er.previous.clear();
    er.older_heads.clear();
    if (factor_learned)
      er.use_epoch.resize(k.nvars, k.stats.reductions);
  }
  if (er.last_learned + 1 != k.stats.learned ||
      er.last_restart != k.stats.restarts) {
    er.previous.clear();
    er.previous_origin = {};
    er.older_heads.clear();
  }
  er.last_learned = k.stats.learned;
  er.last_restart = k.stats.restarts;
  if (er.last_reduction != k.stats.reductions) {
    for (auto it = er.definitions.begin(); it != er.definitions.end();) {
      std::erase_if(it->second, [&](const auto &d) {
        if (supports_pair(k, it->first, d))
          return false;
        er.retained[var_of(d.output)] = 0;
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
    er.previous.clear();
    er.previous_origin = {};
    er.older_heads.clear();
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
      const Lit output = publish_pair(k, er, left, negate(head));
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

void conflict_er_factor_learned_pairs(Kernel &k) {
  if (!k.conflict_er || k.level || k.dense || k.pending.valid || k.conflict_valid ||
      k.inconsistent || !k.large_watches_connected)
    return;
  auto &er = *k.conflict_er;
  if (er.learned_pairs.empty())
    return;
  std::vector<Lit> tail, other_tail;
  if (er.origin_generation == k.arena_generation) {
    using LearnedPair = ConflictEr::LearnedPair;
    std::map<std::pair<Lit, Lit>, std::vector<const LearnedPair *>> groups;
    for (const auto &pair : er.learned_pairs)
      groups[std::minmax(pair.first.head, pair.second.head)].push_back(&pair);

    // The two binaries and reverse ternary cost three new clauses. Each
    // replaced pair removes one clause after paying for its quotient.
    constexpr size_t support_clauses = 3;
    constexpr uint64_t large_watch_words = 4;
    constexpr uint64_t two_binary_watch_words = 4;
    const uint64_t support_words = clause_words(3) + large_watch_words +
                                   two_binary_watch_words + variable_words;
    const RuleVTable &rule = tactix_rules()[tactix_rule_id("ExtendedResolution")];
    for (const auto &[heads, pairs] : groups) {
      if (kernel_terminating(k))
        break;
      if (pairs.size() <= support_clauses)
        continue;

      // Revalidate after previous groups may have removed shared sources.
      // For fixed distinct heads, distinct tails identify disjoint sources.
      std::map<std::vector<Lit>, const LearnedPair *> selected;
      uint64_t quotient_words = 0;
      for (const LearnedPair *pair : pairs) {
        if (match_learned_pair(k, *pair, tail, other_tail) &&
            tail.size() >= minimum_replacement_tail_words &&
            selected.emplace(tail, pair).second)
          quotient_words += clause_words(tail.size() + 1);
      }
      const uint64_t removed_words = quotient_words + selected.size() * large_watch_words;
      if (selected.size() <= support_clauses || removed_words <= support_words ||
          selected.size() > uint64_t(INT64_MAX) - k.next_clause_id - support_clauses ||
          uint64_t(k.arena.size()) + clause_words(3) + quotient_words >= INVALID_REF)
        continue;

      apply_rule_structured_preserve_startup(rule, k, "factor-learned-pair-batch", [&] {
        const Lit output = publish_pair(k, er, negate(heads.first), negate(heads.second));
        if (output == INVALID_LIT)
          return ApplyResult::NoOp;
        for (const auto &[remainder, pair] : selected) {
          tail = remainder;
          commit_learned_pair(k, *pair, tail, [&](Lit, Lit) { return output; });
        }
        k.stats.conflict_er_factored_words += removed_words - support_words;
        return ApplyResult::Ok;
      });
    }
  }
  er.learned_pairs.clear();
  er.previous_origin = {};
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

Lit conflict_er_factor_learned_pair(Kernel &k, const ConflictEr::LearnedPair &pair) {
  if (pair.first.ref == pair.second.ref || pair.first.head == INVALID_LIT ||
      pair.second.head == INVALID_LIT || var_of(pair.first.head) == var_of(pair.second.head) ||
      !extended_resolution_guard(k))
    return INVALID_LIT;
  std::vector<Lit> tail, other_tail;
  if (!match_learned_pair(k, pair, tail, other_tail) || tail.size() < 2)
    return INVALID_LIT;
  return commit_learned_pair(k, pair, tail, [&](Lit left, Lit right) {
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
        er.retained[output] = 0;
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
    er.retained[v] = 0;
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
  std::vector<uint8_t> retained;
  std::vector<uint64_t> use_epoch;
  for (auto &[a, definitions] : er.definitions) {
    const Lit left = remap(a);
    if (left == INVALID_LIT)
      continue;
    std::erase_if(definitions, [&](auto &d) {
      const Var old_output = var_of(d.output);
      const bool keep = conflict_er_retained(er, old_output);
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
  er.learned_pairs.clear();
  er.previous_origin = {};
  er.previous.clear();
  er.older_heads.clear();
}

} // namespace tactix

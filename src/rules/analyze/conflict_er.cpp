#include "rules/analyze/conflict_er.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <climits>

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

bool publish_pair(Kernel &k, ConflictEr &er, Lit a, Lit b) {
  if (var_of(a) == var_of(b))
    return false;
  if (a > b)
    std::swap(a, b);
  auto &row = er.definitions[a];
  auto existing = std::find_if(row.begin(), row.end(), [b](const auto &d) {
    return d.right == b;
  });
  if (existing != row.end() && supports_pair(k, a, *existing))
    return false;
  const Lit output = extended_resolution_define_search_or(k, a, b);
  if (output == INVALID_LIT)
    return false;
  const Var output_var = var_of(output);
  if (k.value(output) && !k.var_level(output_var))
    heuristics_deactivate_var(k, output_var);
  er.retained.resize(k.nvars);
  er.retained[output_var] = 1;
  if (existing == row.end()) {
    row.push_back({b, output});
  } else {
    er.retained[var_of(existing->output)] = 0;
    existing->output = output;
  }
  strategy::finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
  ++k.stats.conflict_er_definitions;
  return true;
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
    }
    for (const auto &c : candidates)
      er.positions[c.output] = 0;
    std::erase(er.rewritten, INVALID_LIT);
  }
  for (const Lit l : clause)
    er.positions[l] = 0;
  return er.rewritten;
}

void conflict_er_observe(Kernel &k, const std::vector<Lit> &clause) {
  if (!k.conflict_er)
    k.conflict_er.emplace();
  auto &er = *k.conflict_er;
  if (er.last_learned + 1 != k.stats.learned ||
      er.last_restart != k.stats.restarts) {
    er.previous.clear();
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
    er.older_heads.clear();
    return;
  }
  er.positions.resize(size_t{2} * k.nvars);
  const Lit head = clause.front();
  if (er.previous.size() == clause.size() &&
      var_of(er.previous.front()) != var_of(head) && same_tail(er, clause)) {
    const Lit previous = negate(er.previous.front());
    const bool created = publish_pair(k, er, previous, negate(head));
    for (const Lit older : er.older_heads)
      publish_pair(k, er, older, negate(head));
    if (created)
      er.older_heads.push_back(previous);
    else
      er.older_heads.clear();
  } else {
    er.older_heads.clear();
  }
  er.previous = clause;
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
  const auto score = [&](Lit l) {
    return k.h ? k.h->scores.score[var_of(l)] : 0.0;
  };
  std::sort(outputs.begin(), outputs.end(), [&](Lit a, Lit b) {
    return std::pair{score(a), a} < std::pair{score(b), b};
  });
  // GlucosER considers the lowest-activity half after a reduction. The
  // native BVE scheduler owns projection, including all non-definition uses.
  for (size_t i = 0; i < outputs.size() / 2; ++i) {
    const Lit output = outputs[i];
    const Var v = var_of(output);
    if (k.value(output) || dependency[v])
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
  for (auto &[a, definitions] : er.definitions) {
    const Lit left = remap(a);
    if (left == INVALID_LIT)
      continue;
    std::erase_if(definitions, [&](auto &d) {
      const bool keep = conflict_er_retained(er, var_of(d.output));
      d.right = remap(d.right);
      d.output = remap(d.output);
      if (d.right == INVALID_LIT || d.output == INVALID_LIT)
        return true;
      const Var output = var_of(d.output);
      if (output >= retained.size())
        retained.resize(size_t{output} + 1);
      retained[output] = keep;
      return false;
    });
    if (!definitions.empty())
      remapped.emplace(left, std::move(definitions));
  }
  er.definitions.swap(remapped);
  er.retained.swap(retained);
  er.previous.clear();
  er.older_heads.clear();
}

} // namespace tactix

#pragma once

#include "rules/lucky/model.hpp"

namespace tactix::graph_labeling {

enum class CompletionStatus { Model, NoModel, Limited };

// Complete a constructor's signed partial assignment without touching Kernel.
// Residual clauses and private occurrence queues propagate auxiliary variables;
// remaining unknowns take the reference's false completion. Every proposed model
// still passes the whole live/root validator, including redundant clauses and
// retired-variable semantics. Publication/reconstruction belong to the action.
// maximum_bytes bounds logical payload, excluding allocator/spare capacity and
// the caller's input/output storage. Every scan/queue/validation visit is charged.
template<class Charge>
CompletionStatus complete_model(const Kernel &k, const std::vector<int8_t> &partial,
                                uint64_t maximum_bytes, Charge &&charge,
                                std::vector<uint8_t> &out) {
  out.clear();
  if (partial.size() != k.nvars) return CompletionStatus::NoModel;
  bool limited = false;
  uint64_t bytes = 0;
  const auto admit = [&](uint64_t amount) {
    if (amount > maximum_bytes - bytes) { limited = true; return false; }
    bytes += amount;
    return true;
  };
  const auto work = [&](uint64_t amount) {
    if (!charge(amount)) { limited = true; return false; }
    return true;
  };
  struct Residual {
    size_t begin;
    uint32_t size;
    uint32_t remaining;
    bool satisfied = false;
  };
  // Values, eventual Boolean model, queue capacity and occurrence-list headers.
  const uint64_t variable_bytes = sizeof(int8_t) + sizeof(uint8_t) + sizeof(Lit) +
                                  2 * sizeof(std::vector<size_t>);
  if (!admit(uint64_t(k.nvars) * variable_bytes) || !work(k.nvars))
    return CompletionStatus::Limited;
  std::vector<int8_t> values = partial;
  for (Var v = 0; v < k.nvars; ++v) {
    if (!work(1)) return CompletionStatus::Limited;
    if (values[v] < -1 || values[v] > 1) return CompletionStatus::NoModel;
    const int root = k.value(make_lit(v, false));
    if (root) {
      const int8_t sign = root > 0 ? 1 : -1;
      if (values[v] && values[v] != sign) return CompletionStatus::NoModel;
      values[v] = sign;
    }
  }
  std::vector<Residual> clauses;
  std::vector<Lit> literals;
  const auto append = [&](const Lit *begin, const Lit *end) {
    if (!work(1)) return false;
    const size_t start = literals.size();
    const uint64_t bytes_before = bytes;
    for (auto it = begin; it != end; ++it) {
      if (!work(1)) return false;
      const Lit lit = *it;
      const int8_t value = values[var_of(lit)];
      if (value && (value > 0) != negated(lit)) {
        literals.resize(start);
        bytes = bytes_before;
        return true;
      }
      if (value) continue;
      // Reserve logical storage for the literal and its occurrence entry.
      if (!admit(sizeof(Lit) + sizeof(size_t))) return false;
      literals.push_back(lit);
    }
    const size_t size = literals.size() - start;
    if (!size) return false;
    if (!admit(sizeof(Residual))) return false;
    clauses.push_back({start, static_cast<uint32_t>(size), static_cast<uint32_t>(size), false});
    return true;
  };
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!work(1)) return CompletionStatus::Limited;
    if (!clause->garbage() && !append(clause->begin(), clause->end()))
      return limited ? CompletionStatus::Limited : CompletionStatus::NoModel;
    ref += clause_actual_words(clause);
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    if (!work(1)) return CompletionStatus::Limited;
    for (WatchEntry watch : k.watches.entries(lit)) {
      if (!work(1)) return CompletionStatus::Limited;
      if (!watch.binary() || lit > watch.other()) continue;
      const Lit pair[] = {lit, watch.other()};
      if (!append(pair, pair + 2))
        return limited ? CompletionStatus::Limited : CompletionStatus::NoModel;
    }
  }
  if (!work(2 * uint64_t(k.nvars))) return CompletionStatus::Limited;
  std::vector<std::vector<size_t>> occurrences(2 * size_t(k.nvars));
  std::vector<Lit> queue;
  queue.reserve(k.nvars);
  for (size_t id = 0; id < clauses.size(); ++id) {
    const auto &clause = clauses[id];
    if (!work(1)) return CompletionStatus::Limited;
    for (size_t i = clause.begin; i < clause.begin + clause.size; ++i) {
      if (!work(1)) return CompletionStatus::Limited;
      occurrences[literals[i]].push_back(id);
    }
  }
  const auto enqueue = [&](Lit lit) {
    if (!work(1)) return false;
    int8_t &value = values[var_of(lit)];
    const int8_t sign = negated(lit) ? -1 : 1;
    if (value) return value == sign;
    value = sign;
    queue.push_back(lit);
    return true;
  };
  for (const auto &clause : clauses) {
    if (!work(1)) return CompletionStatus::Limited;
    if (clause.size == 1 && !enqueue(literals[clause.begin]))
      return limited ? CompletionStatus::Limited : CompletionStatus::NoModel;
  }
  for (size_t head = 0; head < queue.size(); ++head) {
    if (!work(1)) return CompletionStatus::Limited;
    const Lit lit = queue[head];
    for (size_t id : occurrences[lit]) {
      if (!work(1)) return CompletionStatus::Limited;
      clauses[id].satisfied = true;
    }
    for (size_t id : occurrences[negate(lit)]) {
      if (!work(1)) return CompletionStatus::Limited;
      auto &clause = clauses[id];
      if (clause.satisfied) continue;
      if (!--clause.remaining) return CompletionStatus::NoModel;
      if (clause.remaining != 1) continue;
      Lit unit = INVALID_LIT;
      for (size_t i = clause.begin; i < clause.begin + clause.size; ++i) {
        if (!work(1)) return CompletionStatus::Limited;
        const Lit candidate = literals[i];
        const int8_t value = values[var_of(candidate)];
        if (!value) unit = candidate;
        else if ((value > 0) != negated(candidate)) {
          clause.satisfied = true;
          break;
        }
      }
      if (!clause.satisfied &&
          (unit == INVALID_LIT || !enqueue(unit)))
        return limited ? CompletionStatus::Limited : CompletionStatus::NoModel;
    }
  }
  if (!work(k.nvars)) return CompletionStatus::Limited;
  std::vector<uint8_t> model(k.nvars, 0);
  for (Var v = 0; v < k.nvars; ++v) model[v] = values[v] > 0;
  if (!lucky_model_valid(k, model, work))
    return limited ? CompletionStatus::Limited : CompletionStatus::NoModel;
  out = std::move(model);
  return CompletionStatus::Model;
}

} // namespace tactix::graph_labeling

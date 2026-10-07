#include "rules/semantic_extension/direct_csp_guarded_cover_proposals.hpp"
#include "rules/semantic_extension/direct_csp_guarded_cover_work.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace tactix::direct_csp_guarded_cover::proposals {
namespace {
struct Move {
  uint32_t domain = 0;
  uint32_t value = 0;
  bool operator<(const Move &other) const {
    return domain < other.domain ||
           (domain == other.domain && value < other.value);
  }
};
struct Meter {
  const Limits &limits;
  const std::function<bool()> &interrupted;
  Effort &effort;
  Status stopped = Status::NoProgress;
  bool charge(uint64_t amount) {
    if (interrupted && interrupted()) {
      stopped = Status::Interrupted;
      return false;
    }
    if (amount > limits.maximum_work - effort.work) {
      stopped = Status::ResourceExhausted;
      return false;
    }
    effort.work += amount;
    return true;
  }
  bool increment(uint64_t &counter, uint64_t maximum) {
    if (!charge(1)) return false;
    if (counter == maximum) {
      stopped = Status::ResourceExhausted;
      return false;
    }
    ++counter;
    return true;
  }
};
uint64_t shape_cost(const Selection &owner) {
  uint64_t cost = detail::add(owner.cover.size(), owner.selected_values.size());
  cost = detail::add(cost, detail::multiply(owner.blockers.size(), 4));
  for (const auto &domain : owner.domains)
    cost = detail::add(cost, detail::add(1, domain.size()));
  for (const auto &definition : owner.exact_or)
    cost = detail::add(cost, detail::add(1, definition.inputs.size()));
  return detail::add(1, cost);
}
Status planner_status(direct_csp_guarded_cover::Status status) {
  switch (status) {
  case direct_csp_guarded_cover::Status::IncompleteTheory:
    return Status::IncompleteTheory;
  case direct_csp_guarded_cover::Status::ResourceExhausted:
    return Status::ResourceExhausted;
  case direct_csp_guarded_cover::Status::Interrupted:
    return Status::Interrupted;
  default:
    return Status::Invalid;
  }
}
} // namespace

bool Limits::valid() const {
  return maximum_work && maximum_proposals && maximum_single_moves &&
         maximum_pair_moves && maximum_repair_steps && maximum_visited_keys &&
         maximum_nominations;
}

Result propose(const std::vector<Premise> &source, const Selection &owner,
               const std::vector<uint32_t> &preferences, const Bounds &bounds,
               const Limits &limits, std::function<bool()> interrupted) {
  Result result;
  if (!bounds.valid() || !limits.valid() || owner.domains.empty() ||
      preferences.size() != owner.domains.size())
    return result;
  Meter meter{limits, interrupted, result.effort};
  const auto stop = [&](Status status) {
    result.status = status;
    result.nominations.clear();
    result.neighborhood_complete = false;
    return result;
  };
  if (owner.domains.size() > bounds.maximum_domains ||
      owner.domains.size() > bounds.maximum_output_literals ||
      source.size() > bounds.maximum_source_clauses ||
      owner.cover.size() > bounds.maximum_cover_literals ||
      owner.exact_or.size() > bounds.maximum_or_definitions)
    return stop(Status::ResourceExhausted);
  if (!meter.charge(detail::add(preferences.size(), detail::add(owner.domains.size(), owner.exact_or.size()))))
    return stop(meter.stopped);
  for (size_t d = 0; d < preferences.size(); ++d) {
    if (preferences[d] >= owner.domains[d].size()) return stop(Status::Invalid);
    if (owner.domains[d].size() > bounds.maximum_domain_values)
      return stop(Status::ResourceExhausted);
  }
  for (const auto &definition : owner.exact_or)
    if (definition.inputs.size() > bounds.maximum_or_inputs)
      return stop(Status::ResourceExhausted);
  if (!meter.charge(shape_cost(owner))) return stop(meter.stopped);
  Selection candidate = owner;
  candidate.selected_values = preferences;
  const auto authenticate = [&]() {
    const uint64_t before = result.effort.work;
    auto planned = direct_csp_guarded_cover::plan(
        source, candidate, bounds,
        [&](uint64_t amount) { return meter.charge(amount); });
    result.effort.planner_work += result.effort.work - before;
    return planned;
  };
  // Validate the entire source even when the seed does not discharge a cover.
  Plan initial = authenticate();
  if (initial.status != direct_csp_guarded_cover::Status::Planned &&
      initial.status != direct_csp_guarded_cover::Status::NoConsequence) {
    if (meter.stopped != Status::NoProgress) return stop(meter.stopped);
    return stop(planner_status(initial.status));
  }

  std::set<std::vector<uint32_t>> visited;
  std::vector<uint32_t> current = preferences;
  std::vector<uint32_t> best;
  uint64_t best_uncovered = 0, best_distance = 0;
  bool have_best = false;
  const auto evaluate = [&](const std::vector<uint32_t> &tuple,
                            uint64_t &uncovered, uint64_t &distance,
                            std::vector<uint32_t> *unresolved) {
    uncovered = distance = 0;
    for (size_t d = 0; d < tuple.size(); ++d) {
      if (!meter.charge(1)) return false;
      distance += tuple[d] != preferences[d];
    }
    for (size_t b = 0; b < owner.blockers.size(); ++b) {
      if (!meter.charge(1)) return false;
      const auto &blocker = owner.blockers[b];
      if (tuple[blocker.domain] != blocker.value) continue;
      bool discharged = false;
      for (int32_t input : owner.exact_or[blocker.or_definition].inputs) {
        for (size_t d = 0; d < tuple.size(); ++d) {
          if (!meter.charge(1)) return false;
          if (input == owner.domains[d][tuple[d]]) {
            discharged = true;
            break;
          }
        }
        if (discharged) break;
      }
      if (!discharged) {
        ++uncovered;
        if (unresolved) {
          if (!meter.charge(1)) return false;
          unresolved->push_back(static_cast<uint32_t>(b));
        }
      }
    }
    return true;
  };
  const auto consider = [&](const std::vector<uint32_t> &tuple,
                            Plan *already_authenticated = nullptr) {
    if (!meter.increment(result.effort.proposals, limits.maximum_proposals))
      return false;
    if (!meter.charge(detail::multiply(tuple.size(), detail::add(visited.size(), 1))))
      return false;
    if (visited.contains(tuple)) return true;
    if (!meter.increment(result.effort.visited_keys, limits.maximum_visited_keys))
      return false;
    visited.insert(tuple);
    uint64_t uncovered, distance;
    if (!evaluate(tuple, uncovered, distance, nullptr)) return false;
    if (!meter.charge(tuple.size())) return false;
    if (!have_best || uncovered < best_uncovered ||
        (uncovered == best_uncovered && distance < best_distance) ||
        (uncovered == best_uncovered && distance == best_distance && tuple < best)) {
      if (!meter.charge(tuple.size())) return false;
      best = tuple;
      best_uncovered = uncovered;
      best_distance = distance;
      have_best = true;
    }
    if (uncovered) return true;
    if (!meter.charge(detail::add(shape_cost(owner), tuple.size()))) return false;
    candidate.selected_values = tuple;
    Plan planned = already_authenticated ? std::move(*already_authenticated) : authenticate();
    if (planned.status != direct_csp_guarded_cover::Status::Planned) {
      if (meter.stopped == Status::NoProgress)
        meter.stopped = planner_status(planned.status);
      return false;
    }
    result.nominations.push_back({candidate, std::move(planned.additions.front())});
    result.nomination_limit_reached = result.nominations.size() == limits.maximum_nominations;
    return true;
  };
  const auto finish = [&]() {
    if (!meter.charge(0)) return stop(meter.stopped);
    result.status = result.nominations.empty() ? Status::NoProgress : Status::Nominated;
    return result;
  };
  if (!consider(current, &initial)) return stop(meter.stopped);
  if (result.nomination_limit_reached) {
    return finish();
  }

  while (true) {
    if (!meter.increment(result.effort.repair_steps, limits.maximum_repair_steps))
      return stop(meter.stopped);
    uint64_t current_uncovered, current_distance;
    std::vector<uint32_t> unresolved;
    if (!evaluate(current, current_uncovered, current_distance, &unresolved))
      return stop(meter.stopped);
    std::set<Move> moves;
    const auto add_move = [&](uint32_t d, uint32_t value) {
      if (!meter.charge(detail::multiply(2, detail::add(moves.size(), 1)))) return false;
      if (value != current[d] && !moves.contains({d, value})) {
        if (moves.size() >= limits.maximum_single_moves - result.effort.single_moves) {
          meter.stopped = Status::ResourceExhausted;
          return false;
        }
        moves.insert({d, value});
      }
      return true;
    };
    if (!current_uncovered) {
      // Only a bounded one-coordinate neighborhood of an authenticated tuple.
      for (size_t d = 0; d < owner.domains.size(); ++d)
        for (size_t v = 0; v < owner.domains[d].size(); ++v)
          if (!add_move(static_cast<uint32_t>(d), static_cast<uint32_t>(v)))
            return stop(meter.stopped);
    } else {
      for (uint32_t b : unresolved) {
        const auto &blocker = owner.blockers[b];
        for (size_t v = 0; v < owner.domains[blocker.domain].size(); ++v)
          if (!add_move(blocker.domain, static_cast<uint32_t>(v)))
            return stop(meter.stopped);
        for (int32_t input : owner.exact_or[blocker.or_definition].inputs)
          for (size_t d = 0; d < owner.domains.size(); ++d)
            for (size_t v = 0; v < owner.domains[d].size(); ++v) {
              if (!meter.charge(1)) return stop(meter.stopped);
              if (input == owner.domains[d][v] &&
                  !add_move(static_cast<uint32_t>(d), static_cast<uint32_t>(v)))
                return stop(meter.stopped);
            }
      }
    }
    have_best = false;
    for (const Move &move : moves) {
      if (!meter.increment(result.effort.single_moves, limits.maximum_single_moves) ||
          !meter.charge(current.size()))
        return stop(meter.stopped);
      auto tuple = current;
      tuple[move.domain] = move.value;
      if (!consider(tuple)) return stop(meter.stopped);
      if (result.nomination_limit_reached) break;
    }
    if (result.nomination_limit_reached) {
      return finish();
    }
    // Pair repairs are explored only when no single repair lowers the deficit.
    if (current_uncovered && (!have_best || best_uncovered >= current_uncovered)) {
      for (auto first = moves.begin(); first != moves.end(); ++first) {
        auto second = first;
        for (++second; second != moves.end(); ++second) {
          if (!meter.charge(1)) return stop(meter.stopped);
          if (first->domain == second->domain) continue;
          if (!meter.increment(result.effort.pair_moves, limits.maximum_pair_moves) ||
              !meter.charge(current.size()))
            return stop(meter.stopped);
          auto tuple = current;
          tuple[first->domain] = first->value;
          tuple[second->domain] = second->value;
          if (!consider(tuple)) return stop(meter.stopped);
          if (result.nomination_limit_reached) break;
        }
        if (result.nomination_limit_reached) break;
      }
    }
    if (result.nomination_limit_reached) {
      return finish();
    }
    result.neighborhood_complete = true;
    if (!have_best || (current_uncovered && best_uncovered >= current_uncovered) ||
        (!current_uncovered && best_uncovered)) {
      return finish();
    }
    if (!meter.charge(current.size())) return stop(meter.stopped);
    current = std::move(best);
  }
}
} // namespace tactix::direct_csp_guarded_cover::proposals

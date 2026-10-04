#include "rules/semantic_extension/balanced_signature_recognition.hpp"

#include <algorithm>
#include <climits>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace tactix::balanced_signature_semantic {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

struct Meter {
  const Limits &limits;
  Charge charge;
  void *context;
  uint64_t work = 0;
  Status status = Status::Recognized;

  bool take(uint64_t amount) {
    if (amount > limits.maximum_work - work) {
      status = Status::LimitExceeded;
      return false;
    }
    if (charge && !charge(context, amount)) {
      status = Status::Interrupted;
      return false;
    }
    work += amount;
    return true;
  }
};

bool power3(uint32_t width, uint64_t &result) {
  result = 1;
  for (uint32_t i = 0; i < width; ++i) {
    if (result > UINT64_MAX / 3) return false;
    result *= 3;
  }
  return true;
}

} // namespace

Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out) {
  if (!limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || !limits.maximum_balance_tuples ||
      !limits.maximum_alignment_trials || !limits.maximum_balls ||
      !limits.maximum_width)
    return Status::LimitExceeded;
  if (source.empty()) return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;

  Meter meter{limits, charge, charge_context};
  std::set<uint64_t> occurrences;
  std::map<std::vector<int32_t>, const Premise *> clauses;
  uint64_t literal_count = 0;
  for (const Premise &premise : source) {
    if (premise.literals.empty() || !premise.occurrence ||
        !occurrences.insert(premise.occurrence).second)
      return Status::NotRecognized;
    if (premise.literals.size() > limits.maximum_literals - literal_count)
      return Status::LimitExceeded;
    literal_count += premise.literals.size();
    if (!meter.take(uint64_t{1} + premise.literals.size())) return meter.status;
    std::vector<int32_t> normalized = premise.literals;
    for (int32_t literal : normalized)
      if (!literal || literal == INT32_MIN) return Status::NotRecognized;
    std::sort(normalized.begin(), normalized.end());
    if (std::adjacent_find(normalized.begin(), normalized.end()) !=
        normalized.end())
      return Status::NotRecognized;
    for (size_t i = 1; i < normalized.size(); ++i)
      if (variable(normalized[i - 1]) == variable(normalized[i]))
        return Status::NotRecognized;
    if (!clauses.emplace(std::move(normalized), &premise).second)
      return Status::NotRecognized;
  }

  struct DomainWork {
    const Premise *cover = nullptr;
    std::vector<int32_t> selectors;
    std::map<int32_t, uint32_t> index;
    std::vector<uint32_t> state_variables;
    std::vector<std::pair<uint32_t, uint32_t>> state_pairs;
    std::map<std::pair<int32_t, int32_t>, uint64_t> definitions;
  };
  std::vector<DomainWork> domains;
  std::map<uint32_t, uint32_t> selector_domain;

  // All-positive clauses are the domain covers.  Requiring disjoint covers
  // prevents a convenient long clause from being misread as metadata.
  for (const auto &[normalized, premise] : clauses) {
    if (!meter.take(1)) return meter.status;
    if (normalized.size() < 2 || normalized.front() <= 0) continue;
    if (domains.size() >= limits.maximum_balls) return Status::LimitExceeded;
    DomainWork domain;
    domain.cover = premise;
    domain.selectors = normalized;
    for (uint32_t i = 0; i < domain.selectors.size(); ++i) {
      const uint32_t v = variable(domain.selectors[i]);
      if (!selector_domain.emplace(v, domains.size()).second)
        return Status::NotRecognized;
      domain.index.emplace(domain.selectors[i], i);
    }
    domains.push_back(std::move(domain));
  }
  if (domains.size() < 2 || domains.size() > limits.maximum_balls)
    return Status::NotRecognized;
  const size_t signatures = domains.front().selectors.size();
  if (signatures < 2) return Status::NotRecognized;
  for (const DomainWork &domain : domains)
    if (domain.selectors.size() != signatures) return Status::NotRecognized;

  std::set<uint32_t> state_variables;
  std::map<uint32_t, uint32_t> state_owner;
  for (const auto &[normalized, premise] : clauses) {
    if (!meter.take(1)) return meter.status;
    if (normalized.size() != 2) continue;
    int selector_pos = -1;
    uint32_t domain_index = 0;
    for (int i = 0; i < 2; ++i) {
      const auto found = selector_domain.find(variable(normalized[i]));
      if (found != selector_domain.end()) {
        if (selector_pos >= 0) {
          selector_pos = -2;
          break;
        }
        selector_pos = i;
        domain_index = found->second;
      }
    }
    if (selector_pos < 0) continue;
    const int32_t selector_literal = normalized[selector_pos];
    if (selector_literal >= 0) return Status::NotRecognized;
    const int32_t state_literal = normalized[1 - selector_pos];
    const uint32_t state = variable(state_literal);
    if (selector_domain.count(state)) return Status::NotRecognized;
    const auto [owner, inserted] = state_owner.emplace(state, domain_index);
    if (!inserted && owner->second != domain_index)
      return Status::NotRecognized;
    domains[domain_index].definitions.emplace(
        std::make_pair(-selector_literal, state_literal), premise->occurrence);
    domains[domain_index].state_variables.push_back(state);
    state_variables.insert(state);
  }
  for (DomainWork &domain : domains) {
    std::sort(domain.state_variables.begin(), domain.state_variables.end());
    domain.state_variables.erase(
        std::unique(domain.state_variables.begin(), domain.state_variables.end()),
        domain.state_variables.end());
    if (domain.state_variables.empty() || domain.state_variables.size() % 2)
      return Status::NotRecognized;
    if (domain.state_variables.size() / 2 > limits.maximum_width ||
        domain.state_variables.size() / 2 >= 31)
      return Status::LimitExceeded;
  }
  const uint32_t width = domains.front().state_variables.size() / 2;
  for (const DomainWork &domain : domains)
    if (domain.state_variables.size() != size_t{2} * width)
      return Status::NotRecognized;
  uint64_t ternary = 0;
  if (!power3(width, ternary) || ternary - 1 != signatures)
    return Status::NotRecognized;
  if (signatures > UINT32_MAX ||
      signatures > std::numeric_limits<size_t>::max() / signatures)
    return Status::LimitExceeded;

  // State-pair negative binaries form a perfect matching within each ball.
  for (uint32_t d = 0; d < domains.size(); ++d) {
    DomainWork &domain = domains[d];
    std::set<uint32_t> local(domain.state_variables.begin(),
                             domain.state_variables.end());
    std::map<uint32_t, uint32_t> mate;
    for (const auto &[normalized, premise] : clauses) {
      if (!meter.take(1)) return meter.status;
      if (normalized.size() != 2 || normalized[0] >= 0 || normalized[1] >= 0)
        continue;
      const uint32_t a = variable(normalized[0]), b = variable(normalized[1]);
      if (selector_domain.count(a) || selector_domain.count(b)) continue;
      if (!local.count(a) && !local.count(b)) continue;
      if (!local.count(a) || !local.count(b) || a == b || mate.count(a) ||
          mate.count(b))
        return Status::NotRecognized;
      mate[a] = b;
      mate[b] = a;
      domain.state_pairs.emplace_back(std::min(a, b), std::max(a, b));
      (void)premise;
    }
    if (mate.size() != domain.state_variables.size())
      return Status::NotRecognized;
    std::sort(domain.state_pairs.begin(), domain.state_pairs.end());
  }

  // Decode each domain in its local frame and retain exact implication IDs.
  std::vector<std::vector<std::vector<int8_t>>> local_values(domains.size());
  std::vector<std::vector<std::vector<uint64_t>>> definition_ids(domains.size());
  for (uint32_t d = 0; d < domains.size(); ++d) {
    std::set<std::vector<int8_t>> unique;
    for (int32_t selector : domains[d].selectors) {
      if (!meter.take(uint64_t{1} + width)) return meter.status;
      std::vector<int8_t> value;
      std::vector<uint64_t> ids;
      for (auto [negative, positive] : domains[d].state_pairs) {
        int bits[2] = {-1, -1};
        for (int side = 0; side < 2; ++side) {
          const uint32_t state = side ? positive : negative;
          uint64_t found_id = 0;
          for (int sign : {-1, 1}) {
            const int32_t literal = sign * static_cast<int32_t>(state);
            auto found = domains[d].definitions.find({selector, literal});
            if (found == domains[d].definitions.end()) continue;
            if (found_id) return Status::NotRecognized;
            found_id = found->second;
            bits[side] = sign > 0;
          }
          if (!found_id) return Status::NotRecognized;
          ids.push_back(found_id);
        }
        if (bits[0] && bits[1]) return Status::NotRecognized;
        value.push_back(bits[0] ? -1 : bits[1] ? 1 : 0);
      }
      if (!unique.insert(value).second) return Status::NotRecognized;
      local_values[d].push_back(std::move(value));
      definition_ids[d].push_back(std::move(ids));
    }
    if (unique.size() != signatures) return Status::NotRecognized;
    std::vector<int8_t> zero(width, 0);
    if (unique.count(zero)) return Status::NotRecognized;
  }

  // Census selector clauses and provenance.
  std::vector<std::vector<uint64_t>> amo(domains.size(),
                                          std::vector<uint64_t>(signatures * signatures));
  std::map<std::vector<uint32_t>, uint64_t> forbidden;
  std::set<uint64_t> classified;
  for (const DomainWork &domain : domains) classified.insert(domain.cover->occurrence);
  for (uint32_t d = 0; d < domains.size(); ++d)
    for (auto pair : domains[d].state_pairs) {
      std::vector<int32_t> key = {-static_cast<int32_t>(pair.first),
                                  -static_cast<int32_t>(pair.second)};
      std::sort(key.begin(), key.end());
      classified.insert(clauses.at(key)->occurrence);
    }
  for (const DomainWork &domain : domains)
    for (const auto &[key, id] : domain.definitions) {
      (void)key;
      classified.insert(id);
    }

  for (const auto &[normalized, premise] : clauses) {
    if (!meter.take(uint64_t{1} + normalized.size())) return meter.status;
    bool all_negative_selectors = true;
    std::vector<std::pair<uint32_t, uint32_t>> positions;
    for (int32_t literal : normalized) {
      auto found = selector_domain.find(variable(literal));
      if (literal >= 0 || found == selector_domain.end()) {
        all_negative_selectors = false;
        break;
      }
      const uint32_t d = found->second;
      positions.emplace_back(d, domains[d].index.at(-literal));
    }
    if (!all_negative_selectors) continue;
    if (positions.size() == 2 && positions[0].first == positions[1].first) {
      const uint32_t d = positions[0].first;
      uint32_t a = positions[0].second, b = positions[1].second;
      if (a > b) std::swap(a, b);
      if (a == b || amo[d][a * signatures + b]) return Status::NotRecognized;
      amo[d][a * signatures + b] = premise->occurrence;
      amo[d][b * signatures + a] = premise->occurrence;
      classified.insert(premise->occurrence);
    } else if (positions.size() == domains.size()) {
      std::sort(positions.begin(), positions.end());
      for (uint32_t d = 0; d < positions.size(); ++d)
        if (positions[d].first != d) return Status::NotRecognized;
      std::vector<uint32_t> tuple;
      for (auto [d, index] : positions) {
        (void)d;
        tuple.push_back(index);
      }
      if (!forbidden.emplace(std::move(tuple), premise->occurrence).second)
        return Status::NotRecognized;
      classified.insert(premise->occurrence);
    } else {
      return Status::NotRecognized;
    }
  }
  for (uint32_t d = 0; d < domains.size(); ++d)
    for (uint32_t a = 0; a < signatures; ++a)
      for (uint32_t b = a + 1; b < signatures; ++b) {
        if (!meter.take(1)) return meter.status;
        if (!amo[d][a * signatures + b]) return Status::NotRecognized;
      }

  uint64_t tuple_count = 1;
  for (uint32_t d = 0; d < domains.size(); ++d) {
    if (tuple_count > limits.maximum_balance_tuples / signatures)
      return Status::LimitExceeded;
    tuple_count *= signatures;
  }

  // Find a coordinate permutation/signing for each later domain.  The first
  // local frame fixes the harmless global symmetry.  Exhaustive comparison
  // against the complete forbidden table makes this inference semantic,
  // rather than dependent on variable positions.
  std::vector<std::vector<std::vector<int8_t>>> aligned = local_values;
  std::vector<uint32_t> permutation(width);
  std::iota(permutation.begin(), permutation.end(), 0);
  uint64_t transform_count = uint64_t{1} << width;
  for (uint32_t factor = 2; factor <= width; ++factor) {
    if (transform_count > limits.maximum_alignment_trials / factor)
      return Status::LimitExceeded;
    transform_count *= factor;
  }
  uint64_t alignment_count = 1;
  for (uint32_t d = 1; d < domains.size(); ++d) {
    if (alignment_count > limits.maximum_alignment_trials / transform_count)
      return Status::LimitExceeded;
    alignment_count *= transform_count;
  }
  std::vector<std::pair<std::vector<uint32_t>, uint32_t>> transforms;
  transforms.reserve(transform_count);
  do {
    for (uint32_t mask = 0; mask < (uint32_t{1} << width); ++mask) {
      if (!meter.take(1)) return meter.status;
      transforms.emplace_back(permutation, mask);
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  uint64_t trials = 0;
  bool matched = false;
  std::vector<std::vector<uint32_t>> active_permutation(
      domains.size(), std::vector<uint32_t>(width));
  std::vector<uint32_t> active_mask(domains.size(), 0);
  std::vector<std::vector<uint32_t>> chosen_permutation;
  std::vector<uint32_t> chosen_mask;
  std::iota(active_permutation[0].begin(), active_permutation[0].end(), 0);
  std::function<void(uint32_t)> align = [&](uint32_t d) {
    if (matched || meter.status != Status::Recognized) return;
    if (d == domains.size()) {
      ++trials;
      std::vector<uint32_t> tuple(domains.size());
      for (uint64_t ordinal = 0; ordinal < tuple_count; ++ordinal) {
        if (!meter.take(1)) return;
        uint64_t cursor = ordinal;
        std::vector<int32_t> sums(width, 0);
        for (uint32_t ball = 0; ball < domains.size(); ++ball) {
          tuple[ball] = cursor % signatures;
          cursor /= signatures;
          for (uint32_t w = 0; w < width; ++w)
            sums[w] += aligned[ball][tuple[ball]][w];
        }
        const bool bad = std::any_of(sums.begin(), sums.end(),
                                     [](int32_t sum) { return sum != 0; });
        if (forbidden.count(tuple) != size_t(bad)) return;
      }
      matched = true;
      chosen_permutation = active_permutation;
      chosen_mask = active_mask;
      return;
    }
    for (const auto &[perm, mask] : transforms) {
      active_permutation[d] = perm;
      active_mask[d] = mask;
      for (uint32_t s = 0; s < signatures; ++s)
        for (uint32_t w = 0; w < width; ++w) {
          int8_t value = local_values[d][s][perm[w]];
          aligned[d][s][w] = (mask & (uint32_t{1} << w)) ? -value : value;
        }
      align(d + 1);
      if (matched || meter.status != Status::Recognized) return;
    }
  };
  align(1);
  if (meter.status != Status::Recognized) return meter.status;
  if (!matched)
    return Status::NotRecognized;
  if (classified.size() != source.size()) return Status::NotRecognized;

  Image image;
  image.balls = domains.size();
  image.width = width;
  for (uint32_t d = 0; d < domains.size(); ++d) {
    Ball ball;
    ball.cover_premise = domains[d].cover->occurrence;
    ball.exclusion_premises = amo[d];
    for (uint32_t w = 0; w < width; ++w) {
      auto pair = domains[d].state_pairs[chosen_permutation[d][w]];
      if (chosen_mask[d] & (uint32_t{1} << w)) std::swap(pair.first, pair.second);
      std::vector<int32_t> key = {-static_cast<int32_t>(pair.first),
                                  -static_cast<int32_t>(pair.second)};
      std::sort(key.begin(), key.end());
      ball.states.push_back({static_cast<int32_t>(pair.first),
                             static_cast<int32_t>(pair.second),
                             clauses.at(key)->occurrence});
    }
    for (uint32_t s = 0; s < signatures; ++s) {
      std::vector<uint64_t> ids;
      for (uint32_t w = 0; w < width; ++w) {
        const uint32_t local = chosen_permutation[d][w];
        const bool flip = chosen_mask[d] & (uint32_t{1} << w);
        ids.push_back(definition_ids[d][s][2 * local + (flip ? 1 : 0)]);
        ids.push_back(definition_ids[d][s][2 * local + (flip ? 0 : 1)]);
      }
      ball.signatures.push_back(
          {domains[d].selectors[s], aligned[d][s], std::move(ids)});
    }
    image.domains.push_back(std::move(ball));
  }
  for (const auto &[tuple, premise] : forbidden)
    image.forbidden.push_back({tuple, premise});
  image.classified_occurrences.assign(occurrences.begin(), occurrences.end());
  image.classified_premises = source;
  std::sort(image.classified_premises.begin(), image.classified_premises.end(),
            [](const Premise &a, const Premise &b) {
              return a.occurrence < b.occurrence;
            });
  image.inspected_work = meter.work;
  out = std::move(image);
  return Status::Recognized;
}

} // namespace tactix::balanced_signature_semantic

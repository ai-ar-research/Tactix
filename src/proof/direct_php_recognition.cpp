#include "proof/direct_php_recognition.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <utility>

namespace tactix::direct_php_recognition {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

struct Meter {
  const Limits &limits;
  Charge charge = nullptr;
  void *context = nullptr;
  uint64_t work = 0;
  uint64_t combinations = 0;
  uint64_t mappings = 0;
  Status status = Status::Recognized;

  bool take(uint64_t amount) {
    if (work > limits.maximum_work || amount > limits.maximum_work - work) {
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
  bool combination() {
    if (combinations == limits.maximum_row_combinations) {
      status = Status::LimitExceeded;
      return false;
    }
    ++combinations;
    return take(1);
  }
  bool mapping() {
    if (mappings == limits.maximum_mapping_trials) {
      status = Status::LimitExceeded;
      return false;
    }
    ++mappings;
    return take(1);
  }
};

struct Normalized {
  const Premise *source = nullptr;
  std::vector<int32_t> literals;
};

struct Row {
  const Normalized *clause = nullptr;
  std::vector<int32_t> variables;
};

using Edge = std::pair<uint32_t, uint32_t>;

Edge edge(uint32_t first, uint32_t second) {
  if (first > second)
    std::swap(first, second);
  return {first, second};
}

Identity identity(const Premise &premise) {
  return {premise.occurrence, premise.native_proof_id};
}

bool rows_compatible(const Row &first, const Row &second,
                     const std::map<Edge, const Premise *> &edges,
                     Meter &meter) {
  for (int32_t a : first.variables) {
    bool found = false;
    for (int32_t b : second.variables) {
      if (!meter.take(1))
        return false;
      found |= edges.count(edge(variable(a), variable(b)));
    }
    if (!found)
      return false;
  }
  for (int32_t b : second.variables) {
    bool found = false;
    for (int32_t a : first.variables) {
      if (!meter.take(1))
        return false;
      found |= edges.count(edge(variable(a), variable(b)));
    }
    if (!found)
      return false;
  }
  return true;
}

bool same_image(const Image &first, const Image &second) {
  if (first.variables != second.variables ||
      first.alos.size() != second.alos.size() ||
      first.amos.size() != second.amos.size())
    return false;
  for (size_t i = 0; i < first.alos.size(); ++i)
    if (first.alos[i].identity.occurrence !=
            second.alos[i].identity.occurrence ||
        first.alos[i].identity.native_proof_id !=
            second.alos[i].identity.native_proof_id)
      return false;
  for (size_t i = 0; i < first.amos.size(); ++i)
    if (first.amos[i].identity.occurrence !=
            second.amos[i].identity.occurrence ||
        first.amos[i].identity.native_proof_id !=
            second.amos[i].identity.native_proof_id)
      return false;
  return true;
}

bool complete_image(const std::vector<const Row *> &rows,
                    const direct_php_certificate::Matrix &matrix,
                    const std::map<Edge, const Premise *> &edges,
                    Image &image) {
  const uint32_t holes = static_cast<uint32_t>(matrix[0].size());
  image.variables = matrix;
  for (uint32_t pigeon = 0; pigeon < rows.size(); ++pigeon)
    image.alos.push_back(
        {pigeon, identity(*rows[pigeon]->clause->source)});
  std::set<uint64_t> selected;
  for (const AloPremise &alo : image.alos)
    if (!selected.insert(alo.identity.occurrence).second)
      return false;
  for (uint32_t hole = 0; hole < holes; ++hole)
    for (uint32_t first = 0; first < rows.size(); ++first)
      for (uint32_t second = first + 1; second < rows.size(); ++second) {
        const int32_t a = matrix[first][hole], b = matrix[second][hole];
        const auto found = edges.find(edge(variable(a), variable(b)));
        if (found == edges.end())
          return false;
        const Premise &premise = *found->second;
        if (!selected.insert(premise.occurrence).second)
          return false;
        image.amos.push_back({hole, first, second, a, b,
                              identity(premise)});
      }
  image.census.alo_clauses = image.alos.size();
  image.census.amo_clauses = image.amos.size();
  image.census.literals = uint64_t(rows.size()) * holes +
                          uint64_t{2} * image.amos.size();
  for (const Row *row : rows)
    image.census.premises.push_back(*row->clause->source);
  for (const AmoPremise &amo : image.amos) {
    const auto found = edges.find(
        edge(variable(amo.first_variable), variable(amo.second_variable)));
    image.census.premises.push_back(*found->second);
  }
  std::sort(image.census.premises.begin(), image.census.premises.end(),
            [](const Premise &a, const Premise &b) {
              return a.occurrence < b.occurrence;
            });
  return true;
}

void enumerate_mappings(const std::vector<const Row *> &rows,
                        const std::map<Edge, const Premise *> &edges,
                        Meter &meter, Image &first_image,
                        uint64_t &image_count) {
  const uint32_t holes = static_cast<uint32_t>(rows[0]->variables.size());
  direct_php_certificate::Matrix matrix(rows.size(),
                                         std::vector<int32_t>(holes));
  matrix[0] = rows[0]->variables;
  std::vector<bool> used(holes);

  auto assign = [&](auto &&self, uint32_t row, uint32_t hole) -> void {
    if (meter.status != Status::Recognized || image_count > 1)
      return;
    if (row == rows.size()) {
      Image image;
      if (!complete_image(rows, matrix, edges, image)) {
        meter.status = Status::Malformed;
        return;
      }
      if (!image_count) {
        first_image = std::move(image);
        image_count = 1;
      } else if (!same_image(first_image, image)) {
        image_count = 2;
      }
      return;
    }
    if (hole == holes) {
      std::fill(used.begin(), used.end(), false);
      self(self, row + 1, 0);
      return;
    }
    for (uint32_t candidate = 0; candidate < holes; ++candidate) {
      if (!meter.mapping())
        return;
      if (used[candidate])
        continue;
      const int32_t value = rows[row]->variables[candidate];
      bool compatible = true;
      for (uint32_t prior = 0; prior < row; ++prior) {
        if (!meter.take(1))
          return;
        compatible &= edges.count(
            edge(variable(value), variable(matrix[prior][hole])));
      }
      if (!compatible)
        continue;
      used[candidate] = true;
      matrix[row][hole] = value;
      self(self, row, hole + 1);
      used[candidate] = false;
      if (meter.status != Status::Recognized || image_count > 1)
        return;
    }
  };
  assign(assign, 1, 0);
}

} // namespace

Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out) {
  if (!limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || !limits.maximum_row_candidates ||
      !limits.maximum_row_combinations || !limits.maximum_mapping_trials ||
      !limits.maximum_holes)
    return Status::LimitExceeded;
  if (source.empty())
    return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;

  Meter meter{limits, charge, charge_context};
  std::set<uint64_t> occurrences, proof_ids;
  std::map<std::vector<int32_t>, Normalized> clauses;
  uint64_t literal_count = 0;
  for (const Premise &premise : source) {
    if (!premise.occurrence || !premise.native_proof_id ||
        !occurrences.insert(premise.occurrence).second ||
        !proof_ids.insert(premise.native_proof_id).second ||
        premise.literals.empty())
      return Status::Malformed;
    if (premise.literals.size() > limits.maximum_literals - literal_count)
      return Status::LimitExceeded;
    literal_count += premise.literals.size();
    if (!meter.take(uint64_t{1} + premise.literals.size()))
      return meter.status;
    std::vector<int32_t> normalized = premise.literals;
    for (int32_t literal : normalized)
      if (!literal || literal == INT32_MIN)
        return Status::Malformed;
    std::sort(normalized.begin(), normalized.end());
    if (std::adjacent_find(normalized.begin(), normalized.end()) !=
        normalized.end())
      return Status::Malformed;
    for (size_t i = 1; i < normalized.size(); ++i)
      if (variable(normalized[i - 1]) == variable(normalized[i]))
        return Status::Malformed;
    if (!clauses.emplace(normalized, Normalized{&premise, normalized}).second)
      return Status::Malformed;
  }

  std::map<Edge, const Premise *> edges;
  std::map<uint32_t, std::vector<Row>> rows_by_width;
  uint64_t row_candidates = 0;
  for (auto &[key, clause] : clauses) {
    if (!meter.take(1))
      return meter.status;
    if (key.size() == 2 && key[0] < 0 && key[1] < 0) {
      edges.emplace(edge(variable(key[0]), variable(key[1])), clause.source);
      continue;
    }
    if (key.size() > limits.maximum_holes || key.front() <= 0)
      continue;
    if (row_candidates == limits.maximum_row_candidates)
      return Status::LimitExceeded;
    ++row_candidates;
    rows_by_width[static_cast<uint32_t>(key.size())].push_back(
        Row{&clause, key});
  }

  Image recognized;
  uint64_t image_count = 0;
  for (auto &[holes, candidates] : rows_by_width) {
    if (candidates.size() < uint64_t(holes) + 1)
      continue;
    std::sort(candidates.begin(), candidates.end(),
              [](const Row &a, const Row &b) {
                return a.variables < b.variables;
              });
    std::vector<const Row *> chosen;
    std::set<uint32_t> chosen_variables;
    auto choose = [&](auto &&self, size_t begin) -> void {
      if (meter.status != Status::Recognized || image_count > 1)
        return;
      if (chosen.size() == uint64_t(holes) + 1) {
        if (!meter.combination())
          return;
        enumerate_mappings(chosen, edges, meter, recognized, image_count);
        return;
      }
      const size_t needed = size_t(holes) + 1 - chosen.size();
      for (size_t index = begin; index + needed <= candidates.size(); ++index) {
        if (!meter.take(1))
          return;
        bool compatible = true;
        for (int32_t literal : candidates[index].variables)
          if (chosen_variables.count(variable(literal))) {
            compatible = false;
            break;
          }
        if (!compatible)
          continue;
        for (const Row *prior : chosen) {
          if (!rows_compatible(*prior, candidates[index], edges, meter)) {
            compatible = false;
            break;
          }
          if (meter.status != Status::Recognized)
            return;
        }
        if (!compatible)
          continue;
        chosen.push_back(&candidates[index]);
        for (int32_t literal : candidates[index].variables)
          chosen_variables.insert(variable(literal));
        self(self, index + 1);
        for (int32_t literal : candidates[index].variables)
          chosen_variables.erase(variable(literal));
        chosen.pop_back();
        if (meter.status != Status::Recognized || image_count > 1)
          return;
      }
    };
    choose(choose, 0);
    if (meter.status != Status::Recognized)
      return meter.status;
    if (image_count > 1)
      return Status::Ambiguous;
  }
  if (!image_count)
    return Status::NotRecognized;
  recognized.inspected_work = meter.work;
  recognized.row_combinations = meter.combinations;
  recognized.mapping_trials = meter.mappings;
  out = std::move(recognized);
  return Status::Recognized;
}

} // namespace tactix::direct_php_recognition

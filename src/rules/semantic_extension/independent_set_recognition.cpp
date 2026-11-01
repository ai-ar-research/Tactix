#include "rules/semantic_extension/independent_set_recognition.hpp"
#include "kernel/types.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::independent_set_extension {
namespace {

using Clause = std::vector<int32_t>;
using ClauseIndex = std::map<Clause, std::vector<uint64_t>>;

bool spend(Charge charge, void *context, uint64_t amount, uint64_t limit,
           uint64_t &used) {
  if (amount > limit - used)
    return false;
  if (charge && !charge(context, amount))
    return false;
  used += amount;
  return true;
}

Clause canonical(Clause literals) {
  std::sort(literals.begin(), literals.end());
  return literals;
}

bool lookup(const ClauseIndex &index, const Clause &clause,
            uint64_t &occurrence) {
  const auto found = index.find(clause);
  if (found == index.end() || found->second.empty())
    return false;
  occurrence = found->second.front();
  return true;
}

uint64_t state_count(uint32_t vertices, uint32_t target) {
  uint64_t result = 0;
  for (uint32_t layer = 1; layer <= vertices; ++layer)
    result += std::min(layer, target) + 1;
  return result;
}

std::vector<std::vector<int32_t>> make_layers(uint32_t vertices,
                                               uint32_t target) {
  std::vector<std::vector<int32_t>> layers(vertices + 1);
  int32_t next = static_cast<int32_t>(vertices + 1);
  for (uint32_t layer = 1; layer <= vertices; ++layer)
    for (uint32_t state = 0; state <= std::min(layer, target); ++state)
      layers[layer].push_back(next++);
  return layers;
}

bool recognize_candidate(const std::vector<Premise> &source,
                         const ClauseIndex &index, uint32_t external_variables,
                         uint32_t vertices, uint32_t target, Charge charge,
                         void *context, uint64_t limit, uint64_t &work,
                         bool &stopped, Image &out) {
  Image image;
  image.external_variables = external_variables;
  image.vertices = vertices;
  image.target = target;
  image.layers = make_layers(vertices, target);
  image.vertex_literals.reserve(vertices);
  for (uint32_t vertex = 1; vertex <= vertices; ++vertex)
    image.vertex_literals.push_back(static_cast<int32_t>(vertex));

  std::set<Clause> required;
  auto require = [&](Clause clause, uint64_t &occurrence) {
    if (!spend(charge, context, 1 + clause.size(), limit, work)) {
      stopped = true;
      return false;
    }
    clause = canonical(std::move(clause));
    if (!lookup(index, clause, occurrence))
      return false;
    required.insert(std::move(clause));
    return true;
  };

  if (!require({1, image.layers[1][0]}, image.base_false_premise) ||
      !require({-1, image.layers[1][1]}, image.base_true_premise))
    return false;

  for (uint32_t layer = 2; layer <= vertices; ++layer) {
    const auto &previous = image.layers[layer - 1];
    const auto &current = image.layers[layer];
    for (uint32_t state = 0; state < previous.size(); ++state) {
      Transition transition;
      transition.layer = layer;
      transition.from = state;
      transition.on_false = state;
      transition.on_true = std::min(state + 1, target);
      if (!require({-previous[state], static_cast<int32_t>(layer),
                    current[transition.on_false]},
                   transition.false_premise) ||
          !require({-previous[state], -static_cast<int32_t>(layer),
                    current[transition.on_true]},
                   transition.true_premise))
        return false;
      image.transitions.push_back(transition);
    }
    for (uint32_t first = 0; first < current.size(); ++first)
      for (uint32_t second = first + 1; second < current.size(); ++second) {
        LayerExclusion exclusion{layer, first, second, 0};
        if (!require({-current[first], -current[second]},
                     exclusion.premise))
          return false;
        image.exclusions.push_back(exclusion);
      }
  }

  const auto &terminal = image.layers[vertices];
  for (uint32_t state = 0; state < target; ++state) {
    TerminalImplication implication{state, 0};
    if (!require({-terminal[state], terminal[target]}, implication.premise))
      return false;
    image.terminal_implications.push_back(implication);
  }
  if (!require(terminal, image.terminal_cover_premise))
    return false;

  std::map<std::pair<uint32_t, uint32_t>, std::vector<uint64_t>> edge_map;
  for (const auto &premise : source) {
    if (!spend(charge, context, 1 + premise.literals.size(), limit, work)) {
      stopped = true;
      return false;
    }
    Clause clause = canonical(premise.literals);
    if (required.count(clause))
      continue;
    if (clause.size() != 2 || clause[0] >= 0 || clause[1] >= 0)
      return false;
    const uint32_t first = static_cast<uint32_t>(-int64_t(clause[1]));
    const uint32_t second = static_cast<uint32_t>(-int64_t(clause[0]));
    if (!first || first >= second || second > vertices)
      return false;
    edge_map[{first, second}].push_back(premise.occurrence);
  }
  if (edge_map.empty())
    return false;
  for (auto &[ends, occurrences] : edge_map) {
    std::sort(occurrences.begin(), occurrences.end());
    image.edges.push_back({ends.first, ends.second, std::move(occurrences)});
  }
  image.classified_occurrences.reserve(source.size());
  for (const auto &premise : source)
    image.classified_occurrences.push_back(premise.occurrence);
  std::sort(image.classified_occurrences.begin(),
            image.classified_occurrences.end());
  image.inspected_work = work;
  out = std::move(image);
  return true;
}

const Transition *transition_at(const Image &image, uint32_t layer,
                                uint32_t state) {
  for (const Transition &transition : image.transitions)
    if (transition.layer == layer && transition.from == state)
      return &transition;
  return nullptr;
}

uint64_t final_exclusion(const Image &image, uint32_t state) {
  for (const LayerExclusion &exclusion : image.exclusions)
    if (exclusion.layer == image.vertices && exclusion.first == state &&
        exclusion.second == image.target)
      return exclusion.premise;
  return 0;
}

} // namespace

bool recognize(const std::vector<Premise> &source,
               uint32_t external_variables, const Limits &limits,
               Charge charge, void *charge_context, Image &out) {
  out = {};
  if (!external_variables || external_variables > INT32_MAX ||
      !limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || limits.maximum_vertices < 2 ||
      !limits.maximum_target || source.empty() ||
      source.size() > limits.maximum_clauses)
    return false;

  uint64_t work = 0;
  uint64_t literals = 0;
  std::set<uint64_t> occurrence_ids;
  ClauseIndex index;
  for (const Premise &premise : source) {
    if (!premise.occurrence || !occurrence_ids.insert(premise.occurrence).second ||
        !spend(charge, charge_context, 1 + premise.literals.size(),
               limits.maximum_work, work))
      return false;
    if (premise.literals.size() > limits.maximum_literals - literals)
      return false;
    literals += premise.literals.size();
    Clause clause = premise.literals;
    for (int32_t literal : clause) {
      if (!literal || literal == INT32_MIN ||
          external_variable_of(literal) > external_variables)
        return false;
    }
    std::sort(clause.begin(), clause.end());
    if (std::adjacent_find(clause.begin(), clause.end()) != clause.end())
      return false;
    for (int32_t literal : clause)
      if (std::binary_search(clause.begin(), clause.end(), -literal))
        return false;
    index[clause].push_back(premise.occurrence);
  }
  for (auto &[clause, occurrences] : index) {
    (void)clause;
    std::sort(occurrences.begin(), occurrences.end());
  }

  bool found = false;
  Image recognized;
  const uint32_t maximum_vertices =
      std::min(limits.maximum_vertices, external_variables);
  for (uint32_t vertices = 2; vertices <= maximum_vertices; ++vertices) {
    const uint32_t maximum_target =
        std::min({vertices, limits.maximum_target, external_variables});
    for (uint32_t target = 1; target <= maximum_target; ++target) {
      if (!spend(charge, charge_context, 1, limits.maximum_work, work))
        return false;
      if (uint64_t(vertices) + state_count(vertices, target) !=
          external_variables)
        continue;
      Image candidate;
      bool stopped = false;
      if (!recognize_candidate(source, index, external_variables, vertices,
                               target, charge, charge_context,
                               limits.maximum_work, work, stopped, candidate)) {
        if (stopped)
          return false;
        continue;
      }
      if (found)
        return false;
      found = true;
      recognized = std::move(candidate);
    }
  }
  if (!found)
    return false;
  recognized.inspected_work = work;
  out = std::move(recognized);
  return true;
}

bool derive_cover_certificate(const Image &image,
                              const std::vector<uint32_t> &cover_vertices,
                              const CertificateLimits &limits, Charge charge,
                              void *charge_context, CoverCertificate &out) {
  out = {};
  if (image.vertices < 2 || !image.target ||
      image.target > image.vertices || image.layers.size() != image.vertices + 1 ||
      !image.base_false_premise || !image.base_true_premise ||
      cover_vertices.size() != image.vertices - image.target + 1 ||
      !limits.maximum_additions || !limits.maximum_literal_occurrences ||
      !limits.maximum_work || image.target - 1 >= 63)
    return false;

  uint64_t work = 0;
  std::vector<uint32_t> cover = cover_vertices;
  std::sort(cover.begin(), cover.end());
  if (std::adjacent_find(cover.begin(), cover.end()) != cover.end() ||
      cover.empty() || cover.front() == 0 || cover.back() > image.vertices)
    return false;
  std::vector<uint8_t> covered(image.vertices + 1, 0);
  for (uint32_t vertex : cover)
    covered[vertex] = 1;
  std::vector<uint32_t> free;
  for (uint32_t vertex = 1; vertex <= image.vertices; ++vertex)
    if (!covered[vertex])
      free.push_back(vertex);
  if (free.size() != image.target - 1)
    return false;

  const uint64_t leaves = uint64_t{1} << free.size();
  const uint64_t additions = leaves * 2 - 1;
  if (additions > limits.maximum_additions ||
      additions > std::numeric_limits<uint32_t>::max())
    return false;

  CoverCertificate certificate;
  certificate.cover_vertices = cover;
  certificate.steps.reserve(static_cast<size_t>(additions));
  std::vector<uint32_t> current;
  current.reserve(static_cast<size_t>(leaves));
  for (uint64_t mask = 0; mask < leaves; ++mask) {
    if (!spend(charge, charge_context, image.vertices + image.target + 3,
               limits.maximum_work, work))
      return false;
    std::vector<uint8_t> selected(image.vertices + 1, 0);
    for (size_t bit = 0; bit < free.size(); ++bit)
      selected[free[bit]] = (mask >> bit) & 1u;

    CertificateStep step;
    step.kind = CertificateStepKind::ForbiddenAssignment;
    step.clause.reserve(image.vertices);
    for (uint32_t vertex = 1; vertex <= image.vertices; ++vertex)
      step.clause.push_back(selected[vertex] ? -static_cast<int32_t>(vertex)
                                             : static_cast<int32_t>(vertex));
    uint32_t state = selected[1] ? 1 : 0;
    step.premise_occurrences.push_back(selected[1]
                                           ? image.base_true_premise
                                           : image.base_false_premise);
    for (uint32_t layer = 2; layer <= image.vertices; ++layer) {
      const Transition *transition = transition_at(image, layer, state);
      if (!transition)
        return false;
      step.premise_occurrences.push_back(
          selected[layer] ? transition->true_premise
                          : transition->false_premise);
      state = selected[layer] ? transition->on_true : transition->on_false;
    }
    if (state >= image.target || state >= image.terminal_implications.size())
      return false;
    step.premise_occurrences.push_back(
        image.terminal_implications[state].premise);
    const uint64_t exclusion = final_exclusion(image, state);
    if (!step.premise_occurrences.back() || !exclusion)
      return false;
    step.premise_occurrences.push_back(exclusion);
    if (step.clause.size() >
        limits.maximum_literal_occurrences - certificate.literal_occurrences)
      return false;
    certificate.literal_occurrences += step.clause.size();
    current.push_back(static_cast<uint32_t>(certificate.steps.size()));
    certificate.steps.push_back(std::move(step));
  }

  for (uint32_t pivot : free) {
    if (current.size() & 1u)
      return false;
    std::vector<uint32_t> next;
    next.reserve(current.size() / 2);
    for (size_t pair = 0; pair < current.size(); pair += 2) {
      if (!spend(charge, charge_context, image.vertices + 1,
                 limits.maximum_work, work))
        return false;
      const uint32_t left = current[pair];
      const uint32_t right = current[pair + 1];
      const auto &a = certificate.steps[left].clause;
      const auto &b = certificate.steps[right].clause;
      CertificateStep step;
      step.kind = CertificateStepKind::Resolution;
      step.left = left;
      step.right = right;
      step.pivot = static_cast<int32_t>(pivot);
      for (int32_t literal : a)
        if (external_variable_of(literal) != pivot)
          step.clause.push_back(literal);
      std::vector<int32_t> expected;
      for (int32_t literal : b)
        if (external_variable_of(literal) != pivot)
          expected.push_back(literal);
      if (step.clause != expected)
        return false;
      const bool opposite =
          (std::find(a.begin(), a.end(), static_cast<int32_t>(pivot)) != a.end() &&
           std::find(b.begin(), b.end(), -static_cast<int32_t>(pivot)) != b.end()) ||
          (std::find(a.begin(), a.end(), -static_cast<int32_t>(pivot)) != a.end() &&
           std::find(b.begin(), b.end(), static_cast<int32_t>(pivot)) != b.end());
      if (!opposite || step.clause.size() >
                           limits.maximum_literal_occurrences -
                               certificate.literal_occurrences)
        return false;
      certificate.literal_occurrences += step.clause.size();
      next.push_back(static_cast<uint32_t>(certificate.steps.size()));
      certificate.steps.push_back(std::move(step));
    }
    current = std::move(next);
  }
  if (current.size() != 1 || current.front() + 1 != certificate.steps.size())
    return false;
  certificate.conclusion = certificate.steps.back().clause;
  std::vector<int32_t> expected;
  for (uint32_t vertex : cover)
    expected.push_back(static_cast<int32_t>(vertex));
  if (certificate.conclusion != expected)
    return false;
  certificate.work = work;
  out = std::move(certificate);
  return true;
}

} // namespace tactix::independent_set_extension

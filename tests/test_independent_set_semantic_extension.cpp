#undef NDEBUG
#include "rules/semantic_extension/independent_set_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <set>
#include <utility>
#include <vector>

using namespace tactix::independent_set_extension;

namespace {

struct Encoding {
  uint32_t vertices = 0;
  uint32_t target = 0;
  uint32_t variables = 0;
  std::vector<std::vector<int32_t>> layers;
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  std::vector<Premise> clauses;
};

Encoding make_encoding(uint32_t vertices, uint32_t target,
                       std::vector<std::pair<uint32_t, uint32_t>> edges) {
  Encoding result;
  result.vertices = vertices;
  result.target = target;
  result.edges = std::move(edges);
  result.layers.resize(vertices + 1);
  int32_t next = static_cast<int32_t>(vertices + 1);
  for (uint32_t layer = 1; layer <= vertices; ++layer)
    for (uint32_t state = 0; state <= std::min(layer, target); ++state)
      result.layers[layer].push_back(next++);
  result.variables = static_cast<uint32_t>(next - 1);
  uint64_t occurrence = 1;
  const auto add = [&](std::initializer_list<int32_t> literals) {
    result.clauses.push_back({occurrence++, std::vector<int32_t>(literals)});
  };
  const auto add_vector = [&](const std::vector<int32_t> &literals) {
    result.clauses.push_back({occurrence++, literals});
  };
  for (auto [first, second] : result.edges)
    add({-static_cast<int32_t>(first), -static_cast<int32_t>(second)});
  add({1, result.layers[1][0]});
  add({-1, result.layers[1][1]});
  for (uint32_t layer = 2; layer <= vertices; ++layer) {
    const auto &previous = result.layers[layer - 1];
    const auto &current = result.layers[layer];
    for (uint32_t state = 0; state < previous.size(); ++state) {
      add({-previous[state], static_cast<int32_t>(layer), current[state]});
      add({-previous[state], -static_cast<int32_t>(layer),
           current[std::min(state + 1, target)]});
    }
    for (uint32_t first = 0; first < current.size(); ++first)
      for (uint32_t second = first + 1; second < current.size(); ++second)
        add({-current[first], -current[second]});
  }
  const auto &terminal = result.layers[vertices];
  for (uint32_t state = 0; state < target; ++state)
    add({-terminal[state], terminal[target]});
  add_vector(terminal);
  return result;
}

Limits recognition_limits() {
  return {10000, 100000, 10000000, 64, 64};
}

CertificateLimits certificate_limits() {
  return {100000, 10000000, 10000000};
}

struct Meter {
  uint64_t remaining = UINT64_MAX;
  uint64_t calls = 0;
};

bool charge(void *context, uint64_t amount) {
  auto &meter = *static_cast<Meter *>(context);
  ++meter.calls;
  if (amount > meter.remaining)
    return false;
  meter.remaining -= amount;
  return true;
}

bool clause_satisfied(const std::vector<int32_t> &clause, uint64_t assignment) {
  for (int32_t literal : clause) {
    const uint32_t variable = static_cast<uint32_t>(literal < 0
                                                        ? -int64_t(literal)
                                                        : literal);
    const bool value = (assignment >> (variable - 1)) & 1u;
    if (value == (literal > 0))
      return true;
  }
  return false;
}

bool formula_satisfied(const Encoding &encoding, uint64_t assignment) {
  for (const Premise &premise : encoding.clauses)
    if (!clause_satisfied(premise.literals, assignment))
      return false;
  return true;
}

bool projected_theory_satisfied(const Encoding &encoding,
                                uint64_t vertex_assignment) {
  uint32_t selected = 0;
  for (uint32_t vertex = 1; vertex <= encoding.vertices; ++vertex)
    selected += (vertex_assignment >> (vertex - 1)) & 1u;
  if (selected < encoding.target)
    return false;
  for (auto [first, second] : encoding.edges)
    if (((vertex_assignment >> (first - 1)) & 1u) &&
        ((vertex_assignment >> (second - 1)) & 1u))
      return false;
  return true;
}

uint64_t deterministic_completion(const Encoding &encoding,
                                  uint64_t vertex_assignment) {
  uint64_t result = vertex_assignment;
  uint32_t count = 0;
  for (uint32_t layer = 1; layer <= encoding.vertices; ++layer) {
    count = std::min(encoding.target,
                     count + uint32_t((vertex_assignment >> (layer - 1)) & 1u));
    const uint32_t state_variable =
        static_cast<uint32_t>(encoding.layers[layer][count]);
    result |= uint64_t{1} << (state_variable - 1);
  }
  return result;
}

std::vector<std::vector<int32_t>> clauses_of(const Encoding &encoding) {
  std::vector<std::vector<int32_t>> result;
  for (const Premise &premise : encoding.clauses)
    result.push_back(premise.literals);
  return result;
}

bool assign(std::vector<int8_t> &values, int32_t literal) {
  const uint32_t variable = static_cast<uint32_t>(literal < 0
                                                      ? -int64_t(literal)
                                                      : literal);
  const int8_t wanted = literal > 0 ? 1 : -1;
  if (!values[variable]) {
    values[variable] = wanted;
    return true;
  }
  return values[variable] == wanted;
}

bool rup(const std::vector<std::vector<int32_t>> &formula,
         const std::vector<int32_t> &candidate, uint32_t variables) {
  std::vector<int8_t> values(variables + 1, 0);
  for (int32_t literal : candidate)
    if (!assign(values, -literal))
      return true;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &clause : formula) {
      bool satisfied = false;
      uint32_t unassigned = 0;
      int32_t unit = 0;
      for (int32_t literal : clause) {
        const uint32_t variable = static_cast<uint32_t>(
            literal < 0 ? -int64_t(literal) : literal);
        if (!values[variable]) {
          ++unassigned;
          unit = literal;
        } else if ((values[variable] > 0) == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied)
        continue;
      if (!unassigned)
        return true;
      if (unassigned == 1) {
        if (!assign(values, unit))
          return true;
        changed = true;
      }
    }
  }
  return false;
}

void exhaustive_semantics(const Encoding &encoding) {
  assert(encoding.variables < 24);
  for (uint64_t assignment = 0;
       assignment < (uint64_t{1} << encoding.variables); ++assignment) {
    if (!formula_satisfied(encoding, assignment))
      continue;
    const uint64_t vertex_mask =
        assignment & ((uint64_t{1} << encoding.vertices) - 1);
    assert(projected_theory_satisfied(encoding, vertex_mask));
  }
  for (uint64_t vertices = 0;
       vertices < (uint64_t{1} << encoding.vertices); ++vertices) {
    const bool theory = projected_theory_satisfied(encoding, vertices);
    const bool completed = formula_satisfied(
        encoding, deterministic_completion(encoding, vertices));
    assert(theory == completed);
  }
}

Image recognize_or_abort(const Encoding &encoding) {
  Image image;
  Meter meter;
  assert(recognize(encoding.clauses, encoding.variables, recognition_limits(),
                   charge, &meter, image));
  assert(meter.calls);
  assert(image.vertices == encoding.vertices);
  assert(image.target == encoding.target);
  assert(image.external_variables == encoding.variables);
  assert(image.classified_occurrences.size() == encoding.clauses.size());
  assert(image.edges.size() == encoding.edges.size());
  assert(image.inspected_work);
  return image;
}

void check_cover_certificate(const Encoding &encoding, const Image &image,
                             std::vector<uint32_t> cover) {
  CoverCertificate certificate;
  Meter meter;
  assert(derive_cover_certificate(image, cover, certificate_limits(), charge,
                                  &meter, certificate));
  assert(certificate.cover_vertices == cover);
  assert(certificate.conclusion.size() ==
         encoding.vertices - encoding.target + 1);
  assert(certificate.steps.size() ==
         (uint64_t{1} << encoding.target) - 1);
  assert(certificate.work && certificate.literal_occurrences);

  std::set<uint64_t> occurrences;
  for (const Premise &premise : encoding.clauses)
    occurrences.insert(premise.occurrence);
  std::vector<std::vector<int32_t>> formula = clauses_of(encoding);
  for (size_t index = 0; index < certificate.steps.size(); ++index) {
    const CertificateStep &step = certificate.steps[index];
    assert(rup(formula, step.clause, encoding.variables));
    if (step.kind == CertificateStepKind::ForbiddenAssignment) {
      assert(step.premise_occurrences.size() == encoding.vertices + 2);
      for (uint64_t occurrence : step.premise_occurrences)
        assert(occurrences.count(occurrence));
    } else {
      assert(step.left < index && step.right < index && step.pivot > 0);
      assert(step.premise_occurrences.empty());
    }
    formula.push_back(step.clause);
  }
  std::vector<int32_t> expected;
  for (uint32_t vertex : cover)
    expected.push_back(static_cast<int32_t>(vertex));
  assert(certificate.conclusion == expected);
  for (uint64_t assignment = 0;
       assignment < (uint64_t{1} << encoding.variables); ++assignment)
    if (formula_satisfied(encoding, assignment))
      assert(clause_satisfied(certificate.conclusion, assignment));
}

void recognition_near_misses(const Encoding &encoding, const Image &recognized) {
  const auto reject = [&](std::vector<Premise> clauses,
                          uint32_t variables = 0) {
    Image image = recognized;
    Meter meter;
    assert(!recognize(clauses, variables ? variables : encoding.variables,
                      recognition_limits(), charge, &meter, image));
    assert(!image.vertices && image.layers.empty() && image.edges.empty());
  };

  std::vector<Premise> missing = encoding.clauses;
  const uint64_t transition = recognized.transitions[1].true_premise;
  missing.erase(std::find_if(missing.begin(), missing.end(), [&](const auto &p) {
    return p.occurrence == transition;
  }));
  reject(std::move(missing));

  std::vector<Premise> wrong_sign = encoding.clauses;
  auto changed = std::find_if(wrong_sign.begin(), wrong_sign.end(),
                              [&](const auto &p) {
                                return p.occurrence == transition;
                              });
  assert(changed != wrong_sign.end());
  changed->literals[1] = -changed->literals[1];
  reject(std::move(wrong_sign));

  std::vector<Premise> missing_terminal_amo = encoding.clauses;
  const uint64_t terminal_amo = recognized.exclusions.back().premise;
  missing_terminal_amo.erase(
      std::find_if(missing_terminal_amo.begin(), missing_terminal_amo.end(),
                   [&](const auto &p) { return p.occurrence == terminal_amo; }));
  reject(std::move(missing_terminal_amo));

  std::vector<Premise> no_edges = encoding.clauses;
  no_edges.erase(std::remove_if(no_edges.begin(), no_edges.end(), [&](auto &p) {
                   return std::any_of(recognized.edges.begin(), recognized.edges.end(),
                                      [&](const Edge &edge) {
                                        return std::find(edge.premises.begin(),
                                                         edge.premises.end(),
                                                         p.occurrence) !=
                                               edge.premises.end();
                                      });
                 }),
                 no_edges.end());
  reject(std::move(no_edges));

  std::vector<Premise> extra = encoding.clauses;
  extra.push_back({100000, {1, 2}});
  reject(std::move(extra));

  std::vector<Premise> duplicate_id = encoding.clauses;
  duplicate_id.back().occurrence = duplicate_id.front().occurrence;
  reject(std::move(duplicate_id));

  std::vector<Premise> duplicate_literal = encoding.clauses;
  duplicate_literal.back().literals.push_back(duplicate_literal.back().literals[0]);
  reject(std::move(duplicate_literal));
  reject(encoding.clauses, encoding.variables + 1);

  {
    Image image = recognized;
    Meter meter{4, 0};
    assert(!recognize(encoding.clauses, encoding.variables,
                      recognition_limits(), charge, &meter, image));
    assert(!image.vertices && image.classified_occurrences.empty());
  }
  for (int which = 0; which < 4; ++which) {
    Limits limits = recognition_limits();
    if (which == 0)
      limits.maximum_clauses = encoding.clauses.size() - 1;
    else if (which == 1)
      limits.maximum_literals = 1;
    else if (which == 2)
      limits.maximum_vertices = encoding.vertices - 1;
    else
      limits.maximum_target = encoding.target - 1;
    Image image = recognized;
    Meter meter;
    assert(!recognize(encoding.clauses, encoding.variables, limits, charge,
                      &meter, image));
    assert(!image.vertices);
  }
}

void certificate_near_misses(const Image &image) {
  const std::vector<uint32_t> cover{1, 2, 3};
  CoverCertificate good;
  Meter unlimited;
  assert(derive_cover_certificate(image, cover, certificate_limits(), charge,
                                  &unlimited, good));
  const auto reject = [&](std::vector<uint32_t> proposed,
                          CertificateLimits limits, uint64_t allowance) {
    CoverCertificate result = good;
    Meter meter{allowance, 0};
    assert(!derive_cover_certificate(image, proposed, limits, charge, &meter,
                                     result));
    assert(result.steps.empty() && result.conclusion.empty() &&
           result.cover_vertices.empty());
  };
  reject({1, 2}, certificate_limits(), UINT64_MAX);
  reject({1, 1, 2}, certificate_limits(), UINT64_MAX);
  reject({0, 1, 2}, certificate_limits(), UINT64_MAX);
  reject({1, 2, 5}, certificate_limits(), UINT64_MAX);
  CertificateLimits low = certificate_limits();
  low.maximum_additions = good.steps.size() - 1;
  reject(cover, low, UINT64_MAX);
  low = certificate_limits();
  low.maximum_literal_occurrences = good.literal_occurrences - 1;
  reject(cover, low, UINT64_MAX);
  low = certificate_limits();
  low.maximum_work = 1;
  reject(cover, low, UINT64_MAX);
  reject(cover, certificate_limits(), 1);
}

void write_clause(FILE *file, const std::vector<int32_t> &clause) {
  for (int32_t literal : clause)
    std::fprintf(file, "%d ", literal);
  std::fputs("0\n", file);
}

// A machine-checkable microcontrol: the triangle has no independent set of
// size two.  The generated DRAT first derives all three size-two cardinality
// covers through the production certificate planner, then closes the small
// graph contradiction with ordinary RUP resolution.
void emit_unsat_microproof(const char *cnf_path, const char *proof_path) {
  const Encoding encoding =
      make_encoding(3, 2, {{1, 2}, {1, 3}, {2, 3}});
  const Image image = recognize_or_abort(encoding);
  FILE *cnf = std::fopen(cnf_path, "w");
  FILE *proof = std::fopen(proof_path, "w");
  assert(cnf && proof);
  std::fprintf(cnf, "p cnf %u %zu\n", encoding.variables,
               encoding.clauses.size());
  for (const Premise &premise : encoding.clauses)
    write_clause(cnf, premise.literals);

  std::vector<std::vector<int32_t>> formula = clauses_of(encoding);
  for (const std::vector<uint32_t> &cover :
       {std::vector<uint32_t>{1, 2}, std::vector<uint32_t>{1, 3},
        std::vector<uint32_t>{2, 3}}) {
    CoverCertificate certificate;
    Meter meter;
    assert(derive_cover_certificate(image, cover, certificate_limits(), charge,
                                    &meter, certificate));
    for (const CertificateStep &step : certificate.steps) {
      assert(rup(formula, step.clause, encoding.variables));
      write_clause(proof, step.clause);
      formula.push_back(step.clause);
    }
  }
  for (const std::vector<int32_t> &clause :
       {std::vector<int32_t>{2, -3}, std::vector<int32_t>{-3},
        std::vector<int32_t>{1}, std::vector<int32_t>{2},
        std::vector<int32_t>{}}) {
    assert(rup(formula, clause, encoding.variables));
    write_clause(proof, clause);
    formula.push_back(clause);
  }
  assert(!std::fclose(cnf));
  assert(!std::fclose(proof));
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 3) {
    emit_unsat_microproof(argv[1], argv[2]);
    return 0;
  }
  assert(argc == 1);
  const Encoding small = make_encoding(2, 1, {{1, 2}});
  const Encoding medium =
      make_encoding(3, 2, {{1, 2}, {2, 3}, {1, 3}});
  const Encoding main =
      make_encoding(4, 2, {{1, 2}, {2, 3}, {3, 4}, {1, 4}});
  const Encoding owner_shape = make_encoding(
      6, 3, {{1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 1}, {1, 4}});
  exhaustive_semantics(small);
  exhaustive_semantics(medium);
  exhaustive_semantics(main);

  const Image small_image = recognize_or_abort(small);
  const Image medium_image = recognize_or_abort(medium);
  const Image image = recognize_or_abort(main);
  const Image owner_image = recognize_or_abort(owner_shape);
  check_cover_certificate(small, small_image, {1, 2});
  check_cover_certificate(medium, medium_image, {1, 2});
  check_cover_certificate(main, image, {1, 2, 3});
  check_cover_certificate(main, image, {2, 3, 4});
  check_cover_certificate(owner_shape, owner_image, {1, 2, 3, 4});
  recognition_near_misses(main, image);
  certificate_near_misses(image);

  std::printf("test_independent_set_semantic_extension OK\n");
  return 0;
}

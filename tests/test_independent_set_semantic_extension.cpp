#include "unit_test.hpp"
#include "independent_set_test_support.hpp"
#include "clause_oracle.hpp"
#include "rules/semantic_extension/independent_set_recognition.hpp"

#include <set>
#include <utility>

using namespace tactix::independent_set_extension;

namespace {

// The shared encoding with its clauses as occurrence-numbered premises.
struct Encoding : IndependentSetEncoding {
  std::vector<Premise> premises;
};

Encoding make_encoding(uint32_t vertices, uint32_t target,
                       std::vector<std::pair<uint32_t, uint32_t>> edges) {
  Encoding result{independent_set_encoding(vertices, target, std::move(edges)), {}};
  for (const auto &clause : result.clauses)
    result.premises.push_back({result.premises.size() + 1, clause});
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

bool formula_satisfied(const Encoding &encoding, uint64_t assignment) {
  return formula_value(encoding.clauses, assignment);
}

bool rup(const std::vector<std::vector<int32_t>> &formula,
         const std::vector<int32_t> &candidate, uint32_t variables) {
  return rup_implied(formula, candidate, ClauseLiterals{}, variables);
}

Image recognize_or_abort(const Encoding &encoding) {
  Image image;
  Meter meter;
  assert(recognize(encoding.premises, encoding.variables, recognition_limits(),
                   charge, &meter, image));
  assert(meter.calls);
  assert(image.vertices == encoding.vertices);
  assert(image.target == encoding.target);
  assert(image.external_variables == encoding.variables);
  assert(image.classified_occurrences.size() == encoding.premises.size());
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
  for (const Premise &premise : encoding.premises)
    occurrences.insert(premise.occurrence);
  std::vector<std::vector<int32_t>> formula = encoding.clauses;
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
      assert(clause_value(certificate.conclusion, assignment));
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

  std::vector<Premise> missing = encoding.premises;
  const uint64_t transition = recognized.transitions[1].true_premise;
  missing.erase(std::find_if(missing.begin(), missing.end(), [&](const auto &p) {
    return p.occurrence == transition;
  }));
  reject(std::move(missing));

  std::vector<Premise> wrong_sign = encoding.premises;
  auto changed = std::find_if(wrong_sign.begin(), wrong_sign.end(),
                              [&](const auto &p) {
                                return p.occurrence == transition;
                              });
  assert(changed != wrong_sign.end());
  changed->literals[1] = -changed->literals[1];
  reject(std::move(wrong_sign));

  std::vector<Premise> missing_terminal_amo = encoding.premises;
  const uint64_t terminal_amo = recognized.exclusions.back().premise;
  missing_terminal_amo.erase(
      std::find_if(missing_terminal_amo.begin(), missing_terminal_amo.end(),
                   [&](const auto &p) { return p.occurrence == terminal_amo; }));
  reject(std::move(missing_terminal_amo));

  std::vector<Premise> no_edges = encoding.premises;
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

  std::vector<Premise> extra = encoding.premises;
  extra.push_back({100000, {1, 2}});
  reject(std::move(extra));

  std::vector<Premise> duplicate_id = encoding.premises;
  duplicate_id.back().occurrence = duplicate_id.front().occurrence;
  reject(std::move(duplicate_id));

  std::vector<Premise> duplicate_literal = encoding.premises;
  duplicate_literal.back().literals.push_back(duplicate_literal.back().literals[0]);
  reject(std::move(duplicate_literal));
  reject(encoding.premises, encoding.variables + 1);

  {
    Image image = recognized;
    Meter meter{4, 0};
    assert(!recognize(encoding.premises, encoding.variables,
                      recognition_limits(), charge, &meter, image));
    assert(!image.vertices && image.classified_occurrences.empty());
  }
  for (int which = 0; which < 4; ++which) {
    Limits limits = recognition_limits();
    if (which == 0)
      limits.maximum_clauses = encoding.premises.size() - 1;
    else if (which == 1)
      limits.maximum_literals = 1;
    else if (which == 2)
      limits.maximum_vertices = encoding.vertices - 1;
    else
      limits.maximum_target = encoding.target - 1;
    Image image = recognized;
    Meter meter;
    assert(!recognize(encoding.premises, encoding.variables, limits, charge,
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

} // namespace

int main() {
  // One edge with target one (a single forbidden assignment), a 4-cycle and
  // the owner's 6-vertex shape; every certificate step is RUP-checked and the
  // conclusion holds in every model.
  const Encoding small = make_encoding(2, 1, {{1, 2}});
  const Encoding main =
      make_encoding(4, 2, {{1, 2}, {2, 3}, {3, 4}, {1, 4}});
  const Encoding owner_shape = make_encoding(
      6, 3, {{1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 1}, {1, 4}});
  const Image image = recognize_or_abort(main);
  check_cover_certificate(small, recognize_or_abort(small), {1, 2});
  check_cover_certificate(main, image, {1, 2, 3});
  check_cover_certificate(owner_shape, recognize_or_abort(owner_shape),
                          {1, 2, 3, 4});
  recognition_near_misses(main, image);
  certificate_near_misses(image);

  std::printf("test_independent_set_semantic_extension OK\n");
  return 0;
}

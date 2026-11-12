#pragma once
// Recognition and Gaussian-bridge setup shared by the GF(3) tests.
#include "rules/semantic_extension/gf3_gaussian_bridge.hpp"

#include <cassert>
#include <cstdint>
#include <utility>

// The recognized theory of a fixture's source premises.
template <class Fixture>
inline tactix::gf3_affine::Theory recognize(const Fixture &fixture) {
  const tactix::gf3_affine::RecognitionBounds bounds{1000, 10000, 100, 100, 8, 10000};
  auto result = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(result.status == tactix::gf3_affine::RecognitionStatus::Recognized);
  return std::move(result.theory);
}

// Ample Gaussian-bridge bounds.
inline tactix::gf3_gaussian_bridge::Limits bridge_limits() {
  tactix::gf3_gaussian_bridge::Limits result;
  result.maximum_work = 1000000;
  result.maximum_source_clauses = 10000;
  result.maximum_source_literals = 100000;
  result.maximum_domains = 100;
  result.maximum_equations = 100;
  result.maximum_terms = 1000;
  result.maximum_authentication_references = 10000;
  result.maximum_references_per_equation = 1000;
  result.gaussian = {100000, 100000, 32, 100, 1000, 1000000};
  return result;
}

// The complete bridge plan of a recognized theory.
inline tactix::gf3_gaussian_bridge::Plan bridge(const tactix::gf3_affine::Theory &theory) {
  auto result = tactix::gf3_gaussian_bridge::build(
      theory, bridge_limits(), [](uint64_t) { return true; });
  assert(result.status == tactix::gf3_gaussian_bridge::Status::Complete);
  return std::move(result.plan);
}

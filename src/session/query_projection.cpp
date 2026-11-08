#include "session/query_projection.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

#include <algorithm>

namespace tactix::session {

QueryProjection::QueryProjection(const QueryHistory &history, uint64_t query)
    : assumptions_(history.query(query)->assumptions), history_(history), query_(query) {
  rebuild_cube();
}

ModelCheck QueryProjection::check_model(Kernel &k) {
  if (snapshot_.invalid_model) return {ModelStatus::InvalidValue};
  if (snapshot_.restore || k.query_projection != this)
    return {ModelStatus::InvalidQuery};
  k.model_extended = false;
  kernel_extend_model_seeded(k, constraints());
  const auto model = history_.check_model(query_, [&](int32_t variable) {
    return k.model_values[internalize(variable)];
  });
  snapshot_.rejected_models += model.status == ModelStatus::FalsifiedAssumption;
  return model;
}

QueryModelAdmission QueryProjection::admit_model(Kernel &k) {
  const auto model = check_model(k);
  switch (model.status) {
  case ModelStatus::Valid: return QueryModelAdmission::Accepted;
  case ModelStatus::FalsifiedAssumption: return QueryModelAdmission::Rejected;
  case ModelStatus::InvalidQuery: return QueryModelAdmission::Restore;
  default:
    snapshot_.invalid_model = true;
    return QueryModelAdmission::Invalid;
  }
}

bool QueryProjection::contains(int32_t literal) const {
  return std::binary_search(cube_.begin(), cube_.end(), literal, external_literal_less);
}

bool QueryProjection::constrains(Var external) const {
  if (external >= INT32_MAX) return false;
  const int32_t positive = static_cast<int32_t>(external+1);
  return contains(positive) || contains(-positive);
}

bool QueryProjection::preserves_constant(Lit external) const {
  return contradictory_ || !contains(-externalize(external));
}

void QueryProjection::restrict_constant(Lit external) {
  if (snapshot_.restore) return;
  if (!preserves_constant(external)) snapshot_.restore = true;
  else ++snapshot_.preserved_restrictions;
}

void QueryProjection::rebuild_cube() {
  cube_ = assumptions_;
  std::sort(cube_.begin(), cube_.end(), external_literal_less);
  cube_.erase(std::unique(cube_.begin(), cube_.end()), cube_.end());
  contradictory_ = false;
  for (size_t i = 1; i < cube_.size(); ++i)
    contradictory_ |= cube_[i] == -cube_[i-1];
}

void QueryProjection::reconstruction_literal(Lit external) {
  ++snapshot_.reconstruction_literals;
  if (!preserves_constant(external)) snapshot_.restore = true;
}

void QueryProjection::substitute(
    Kernel &k, std::span<const std::pair<Lit, Lit>> aliases) {
  if (snapshot_.restore || aliases.empty()) return;
  std::vector<int32_t> translated = assumptions_;
  std::vector<Lit> premises;
  for (size_t i = 0; i < assumptions_.size(); ++i) {
    const Lit source = kernel_internalize(k, assumptions_[i]);
    for (const auto &[from, to] : aliases) {
      if (source != from) continue;
      const int32_t external = kernel_externalize(k, to);
      if (external != assumptions_[i]) {
        translated[i] = external;
        if (std::find(premises.begin(), premises.end(), to) == premises.end())
          premises.push_back(to);
      }
      break;
    }
  }
  if (premises.size() > UINT64_MAX-k.next_clause_id) {
    snapshot_.restore = true;
    return;
  }
  // The original or preceding translated assumption is already a checker
  // premise. Its equivalence path is still live at this boundary. Do not
  // assert these consequences in the native root or its unit-ID table.
  for (const Lit literal : premises) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) k.proof->add_rup(id, &literal, 1);
  }
  for (size_t i = 0; i < assumptions_.size(); ++i)
    snapshot_.translated_assumptions += assumptions_[i] != translated[i];
  assumptions_.swap(translated);
  rebuild_cube();
}

bool QueryProjection::preserves_restriction(const Kernel &k,
    std::span<const int8_t> constants, std::span<const Lit> images) const {
  if (snapshot_.restore) return false;
  // An inconsistent cube has no models; the query premises already RUP-refute.
  if (!contradictory())
    for (const int32_t external : assumptions_) {
      const Lit literal = kernel_internalize(k, external);
      if (literal == INVALID_LIT) continue; // SR cannot name retired identities
      const Var v = var_of(literal);
      if (v >= constants.size() || v >= images.size()) return false;
      if (constants[v]) {
        if ((constants[v] > 0) == !negated(literal)) continue;
      } else {
        Lit image = images[v];
        if (image == INVALID_LIT) continue; // identity substitution
        if (negated(literal)) image = negate(image);
        if (contains(kernel_externalize(k, image)) ||
            (k.value(image) > 0 && !k.var_level(var_of(image)))) continue;
      }
      return false;
    }
  return true;
}

void QueryProjection::restrict(Kernel &k, std::span<const int8_t> constants,
                                std::span<const Lit> images) {
  if (snapshot_.restore) return;
  if (!preserves_restriction(k, constants, images)) {
    snapshot_.restore = true;
    return;
  }
  ++snapshot_.preserved_restrictions;
}
} // namespace tactix::session

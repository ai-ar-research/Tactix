#include "rules/lucky/fermat_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace tactix {
namespace {

using U64 = uint64_t;
using U128 = unsigned __int128;

bool tail_record(const Kernel &k, size_t logical,
                 std::array<int32_t, 2> &result) {
  const size_t size = k.input_clause_tail.size();
  if (logical >= size)
    return false;
  const size_t physical =
      size < Kernel::INPUT_CLAUSE_TAIL_CAPACITY
          ? logical
          : (k.input_clause_tail_next + logical) % size;
  result = k.input_clause_tail[physical];
  return true;
}

bool comparator_source_end(const Kernel &k, uint32_t tail_limit,
                           uint32_t &source_end) {
  const size_t size = k.input_clause_tail.size();
  const size_t first = size > tail_limit ? size - tail_limit : 0;
  if (size < 2)
    return false;
  for (size_t i = size - 1; i-- > first;) {
    std::array<int32_t, 2> negative{}, positive{};
    if (!tail_record(k, i, negative) ||
        !tail_record(k, i + 1, positive))
      return false;
    if (negative[0] >= 0 || negative[1] >= 0)
      continue;
    const int32_t x = -negative[0], y = -negative[1];
    if (!((positive[0] == x && positive[1] == y) ||
          (positive[0] == y && positive[1] == x)))
      continue;
    source_end = static_cast<uint32_t>(std::min(x, y));
    return true;
  }
  return false;
}

bool has_direct_equivalence(const Kernel &k, uint32_t source,
                            uint32_t source_end) {
  const Lit negative_source =
      kernel_internalize(k, -static_cast<int>(source));
  if (negative_source == INVALID_LIT)
    return false;
  for (const WatchEntry watch : k.watches.entries(negative_source)) {
    if (!watch.binary())
      continue;
    const Lit other = watch.other();
    const int external_other = kernel_externalize(k, other);
    if (external_other <= static_cast<int>(source_end))
      continue;
    if (kernel_binary_live(k, negate(negative_source), negate(other)))
      return true;
  }
  return false;
}

bool recover_constant(const Kernel &k,
                      const strategy::FermatModelParameters &parameters,
                      uint32_t width, U64 &constant) {
  if (width < 2 || width - 1 > parameters.maximum_bits)
    return false;
  uint32_t source_end = 0;
  if (!comparator_source_end(k, parameters.comparator_tail_clauses,
                             source_end))
    return false;
  const uint64_t offset = uint64_t{2} * (width - 2);
  if (source_end <= offset)
    return false;
  const uint32_t source_start = source_end - static_cast<uint32_t>(offset);
  constant = 1;
  for (uint32_t bit = 1; bit < width - 1; ++bit) {
    const uint32_t source = source_start + 2 * bit;
    if (!has_direct_equivalence(k, source, source_end))
      constant |= U64{1} << bit;
  }
  return constant > 1 && (constant & 1);
}

U64 multiply_mod(U64 a, U64 b, U64 modulus) {
  return static_cast<U64>(U128{a} * b % modulus);
}

U64 power_mod(U64 base, U64 exponent, U64 modulus) {
  U64 result = 1;
  while (exponent) {
    if (exponent & 1)
      result = multiply_mod(result, base, modulus);
    base = multiply_mod(base, base, modulus);
    exponent >>= 1;
  }
  return result;
}

bool prime(U64 value) {
  if (value < 2)
    return false;
  static constexpr U64 small[] = {2, 3, 5, 7, 11, 13,
                                  17, 19, 23, 29, 31, 37};
  for (const U64 p : small)
    if (!(value % p))
      return value == p;
  U64 odd = value - 1, powers = 0;
  while (!(odd & 1)) {
    odd >>= 1;
    ++powers;
  }
  // This basis is deterministic over the complete unsigned 64-bit domain.
  static constexpr U64 bases[] = {2,       325,      9375,      28178,
                                  450775,  9780504,  1795265022};
  for (const U64 basis : bases) {
    if (!(basis % value))
      continue;
    U64 witness = power_mod(basis % value, odd, value);
    if (witness == 1 || witness == value - 1)
      continue;
    bool composite = true;
    for (U64 r = 1; r < powers; ++r) {
      witness = multiply_mod(witness, witness, value);
      if (witness == value - 1) {
        composite = false;
        break;
      }
    }
    if (composite)
      return false;
  }
  return true;
}

U64 pollard_rho(Kernel &k, U64 value,
                const strategy::FermatModelParameters &parameters) {
  if (!(value & 1))
    return 2;
  if (!(value % 3))
    return 3;
  for (U64 c = 1; c <= parameters.rho_constants; ++c) {
    U64 x = 2, y = 2, divisor = 1;
    const auto advance = [&](U64 at) {
      return (multiply_mod(at, at, value) + c) % value;
    };
    for (U64 iteration = 0;
         divisor == 1 && iteration < parameters.rho_iterations;
         ++iteration) {
      x = advance(x);
      y = advance(advance(y));
      divisor = std::gcd(x > y ? x - y : y - x, value);
      ++k.stats.fermat_model_factor_iterations;
      if (!(iteration & ((U64{1} << 14) - 1)) &&
          kernel_rule_interrupt(k, k.stats.fermat_model_interrupted))
        return 0;
    }
    if (divisor > 1 && divisor < value)
      return divisor;
  }
  return 0;
}

bool factor_pair(Kernel &k, U64 value,
                 const strategy::FermatModelParameters &parameters,
                 U64 &low, U64 &high) {
  if (value < 4 || prime(value))
    return false;
  U64 divisor = 0;
  static constexpr U64 small[] = {3, 5, 7, 11, 13, 17,
                                  19, 23, 29, 31, 37};
  for (const U64 p : small)
    if (!(value % p)) {
      divisor = p;
      break;
    }
  if (!divisor)
    divisor = pollard_rho(k, value, parameters);
  if (!divisor || divisor == value)
    return false;
  low = divisor;
  high = value / divisor;
  if (low > high)
    std::swap(low, high);
  return low > 1 && high > 1 && U128{low} * high == value;
}

bool assign_literal(std::vector<int8_t> &assignment, Lit literal) {
  const Var variable = var_of(literal);
  const int8_t value = negated(literal) ? -1 : 1;
  if (assignment[variable] && assignment[variable] != value)
    return false;
  assignment[variable] = value;
  return true;
}

bool set_external(const Kernel &k, std::vector<int8_t> &assignment,
                  uint32_t dimacs, bool value) {
  if (!dimacs || dimacs > k.external_to_internal.size())
    return false;
  const Var internal = k.external_to_internal[dimacs - 1];
  if (internal == INVALID_VAR || internal >= assignment.size())
    return false;
  return assign_literal(assignment, make_lit(internal, !value));
}

template <class It>
int propagate_clause(std::vector<int8_t> &assignment, It begin, It end) {
  Lit unit = INVALID_LIT;
  uint32_t unassigned = 0;
  for (It it = begin; it != end; ++it) {
    const Lit literal = *it;
    const int8_t value = assignment[var_of(literal)];
    if (!value) {
      unit = literal;
      ++unassigned;
    } else if ((value > 0) != negated(literal)) {
      return 0;
    }
  }
  if (!unassigned)
    return -1;
  if (unassigned == 1)
    return assign_literal(assignment, unit) ? 1 : -1;
  return 0;
}

bool complete_assignment(Kernel &k, std::vector<int8_t> &assignment) {
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const int value = k.value(make_lit(variable, false));
    if (value &&
        !assign_literal(assignment, make_lit(variable, value < 0)))
      return false;
  }
  // A productive full scan fixes at least one new variable, so this feedback
  // loop is intrinsically bounded by the current variable population.
  for (uint64_t round = 0; round <= k.nvars; ++round) {
    ++k.stats.fermat_model_completion_rounds;
    bool changed = false;
    uint64_t visited = 0;
    for (Lit literal = 0; literal < 2 * k.nvars; ++literal)
      for (const WatchEntry watch : k.watches.entries(literal)) {
        if (!watch.binary() || literal > watch.other())
          continue;
        const Lit pair[2] = {literal, watch.other()};
        const int result =
            propagate_clause(assignment, std::begin(pair), std::end(pair));
        if (result < 0)
          return false;
        changed |= result > 0;
        if (!(++visited & ((U64{1} << 20) - 1)) &&
            kernel_rule_interrupt(k, k.stats.fermat_model_interrupted))
          return false;
      }
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage()) {
        const int result =
            propagate_clause(assignment, clause->begin(), clause->end());
        if (result < 0)
          return false;
        changed |= result > 0;
      }
      ref += words;
    }
    if (!changed)
      return true;
  }
  return false;
}

std::vector<uint8_t> boolean_assignment(const std::vector<int8_t> &values) {
  std::vector<uint8_t> result(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    result[i] = values[i] > 0;
  return result;
}

} // namespace

RuleEligibility fermat_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", k.propagate_head == k.trail.size())
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult fermat_model_apply(Kernel &k) {
  assert(fermat_model_guard(k));
  ++k.stats.fermat_model_calls;
  const strategy::FermatModelParameters parameters =
      strategy::FermatModelParameters::from_kernel(k);
  if (!parameters.valid() || !k.input_fermat_long_shape ||
      k.input_long_clause_count != 2 || k.input_fermat_width < 11 ||
      k.input_fermat_width - 1 > parameters.maximum_bits)
    return ApplyResult::NoOp;

  U64 constant = 0;
  if (!recover_constant(k, parameters, k.input_fermat_width, constant))
    return ApplyResult::NoOp;
  ++k.stats.fermat_model_recognized;

  U64 low = 0, high = 0;
  if (!factor_pair(k, constant, parameters, low, high))
    return ApplyResult::NoOp;
  if (((low + high) & 1) || high < low)
    return ApplyResult::NoOp;
  const U64 a = (low + high) / 2;
  const U64 b = (high - low) / 2;
  const uint32_t width = k.input_fermat_width;
  if (width < 64 && (a >= (U64{1} << width) || b >= (U64{1} << width)))
    return ApplyResult::NoOp;
  if (U128{a} * a < U128{b} * b ||
      U128{a} * a - U128{b} * b != constant)
    return ApplyResult::NoOp;

  ++k.stats.fermat_model_candidates;
  std::vector<int8_t> signed_assignment(k.nvars, 0);
  for (uint32_t bit = 0; bit < width; ++bit) {
    if (!set_external(k, signed_assignment, bit + 1, (a >> bit) & 1) ||
        !set_external(k, signed_assignment, width + bit + 1,
                      (b >> bit) & 1))
      return ApplyResult::NoOp;
  }
  if (!complete_assignment(k, signed_assignment))
    return ApplyResult::NoOp;
  std::vector<uint8_t> assignment = boolean_assignment(signed_assignment);
  if (!lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.fermat_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix

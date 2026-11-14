#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include "engine/subsolver.hpp"
#include "proof/proof.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/substitution_redundancy/detection_support.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace tactix {
namespace {

using sr_detail::finish;
using sr_detail::stop_requested;

enum class ValidationStatus : uint8_t {
  Valid,
  Inapplicable,
  Interrupted,
  ResourceExhausted,
};

struct ValidationResult {
  ValidationStatus status = ValidationStatus::Inapplicable;
  uint64_t ticks = 0;
};

struct ParsedWitness {
  std::vector<int8_t> constants;
  std::vector<Lit> images;
  std::vector<Lit> emitted_true;
  std::vector<DsrLiteralMapping> emitted_mappings;
};

// Syntax only; `active` records whether every variable is still live.
bool valid_internal_literal(const Kernel &k, Lit lit, bool &active) {
  if (lit == INVALID_LIT || var_of(lit) >= k.nvars)
    return false;
  active &= !kernel_var_retired(k, var_of(lit));
  return true;
}

bool normalize_candidate(const Kernel &k, const std::vector<Lit> &input,
                         std::vector<Lit> &output, bool &active) {
  output.clear();
  output.reserve(input.size());
  for (const Lit lit : input) {
    if (!valid_internal_literal(k, lit, active))
      return false;
    if (std::find(output.begin(), output.end(), negate(lit)) != output.end())
      return false;
    if (std::find(output.begin(), output.end(), lit) == output.end())
      output.push_back(lit);
  }
  return !output.empty();
}

bool parse_witness(const Kernel &k, Lit pivot,
                   const SubstitutionRedundancyInstance &instance,
                   ParsedWitness &result, bool &active) {
  result.constants.assign(k.nvars, 0);
  result.images.assign(k.nvars, INVALID_LIT);
  result.emitted_true.clear();
  result.emitted_mappings.clear();

  const auto set_true = [&](Lit lit, bool implicit) {
    if (!valid_internal_literal(k, lit, active))
      return false;
    const Var var = var_of(lit);
    const int8_t value = negated(lit) ? -1 : 1;
    if (result.images[var] != INVALID_LIT)
      return false;
    if (result.constants[var])
      return var == var_of(pivot) && lit == pivot;
    result.constants[var] = value;
    if (var == var_of(pivot))
      return lit == pivot;
    if (!implicit)
      result.emitted_true.push_back(lit);
    return true;
  };

  if (!set_true(pivot, true))
    return false;
  for (const Lit lit : instance.witness)
    if (!set_true(lit, false))
      return false;

  std::unordered_set<Var> sources;
  result.emitted_mappings.reserve(instance.substitution.size());
  for (const auto &[source, target] : instance.substitution) {
    if (!valid_internal_literal(k, source, active) ||
        !valid_internal_literal(k, target, active))
      return false;
    const Var var = var_of(source);
    if (!sources.insert(var).second)
      return false;
    // An explicit pivot mapping overrides DSR's implicit pivot-true (SST
    // cuts map the representative to another orbit point); no other
    // true-mapped source may have an image.
    if (result.constants[var]) {
      if (var != var_of(pivot) ||
          result.constants[var] != (negated(pivot) ? -1 : 1))
        return false;
      result.constants[var] = 0;
    }
    const Lit image = negated(source) ? negate(target) : target;
    if (image == make_lit(var, false))
      return false;
    result.images[var] = image;
    // DSR sources are positive literals: accept either polarity and emit the
    // positive source (Trestle rejects signed sources).
    result.emitted_mappings.push_back({make_lit(var, false), image});
  }
  return true;
}

bool clause_already_true(const Kernel &k, const std::vector<Lit> &clause) {
  for (const Lit lit : clause)
    if (k.value(lit) > 0)
      return true;
  return false;
}

bool kernel_contains_clause(const Kernel &k, const std::vector<Lit> &clause) {
  if (clause.empty())
    return k.inconsistent;
  if (clause.size() == 1)
    return k.unit_ids[var_of(clause.front())] && k.value(clause.front()) > 0;
  if (clause.size() == 2)
    return kernel_binary_live(k, clause[0], clause[1]);

  std::vector<Lit> wanted = clause;
  std::sort(wanted.begin(), wanted.end());
  for (const auto [ref, stored] : arena_records(k.arena)) {
    if (!stored->garbage() && stored->size == wanted.size()) {
      std::vector<Lit> present(stored->begin(), stored->end());
      std::sort(present.begin(), present.end());
      if (present == wanted)
        return true;
    }
  }
  return false;
}

// Re-authenticate a unique-literal AMO pair against the live formula: (~a) is
// a RAT pivot of (~a | ~b) when the only clause containing a is irredundant
// and contains b. Prefer an unassigned endpoint.
bool authenticate_unique_literal_amo(const Kernel &k,
                                     std::vector<Lit> &candidate) {
  if (candidate.size() != 2) return false;

  const auto endpoint_is_rat = [&](size_t pivot_index) {
    if (k.value(candidate[pivot_index])) return false;
    const Lit positive = negate(candidate[pivot_index]);
    const Lit partner = negate(candidate[1 - pivot_index]);
    uint64_t occurrences = 0;
    bool sole_source = false;

    for (Var var = 0; var < k.nvars; ++var) {
      Lit unit = INVALID_LIT;
      if (k.unit_ids[var])
        unit = k.value(make_lit(var, false)) > 0 ? make_lit(var, false)
                                                 : make_lit(var, true);
      else if (var < k.dsr_root_values.size() && k.dsr_root_values[var])
        unit = make_lit(var, k.dsr_root_values[var] < 0);
      if (unit == positive) ++occurrences;
    }

    std::unordered_set<uint64_t> binaries;
    for (const WatchEntry watch : k.watches.entries(positive)) {
      if (!watch.binary()) continue;
      const Lit other = watch.other();
      if (!binaries.insert(kernel_binary_key(positive, other)).second)
        continue;
      ++occurrences;
    }

    for (const auto [reference, clause] : arena_records(k.arena)) {
      if (clause->garbage() ||
          std::find(clause->begin(), clause->end(), positive) == clause->end())
        continue;
      ++occurrences;
      sole_source = !clause->redundant() &&
                    std::find(clause->begin(), clause->end(), partner) !=
                        clause->end();
    }
    return occurrences == 1 && sole_source;
  };

  if (endpoint_is_rat(0)) return true;
  if (!endpoint_is_rat(1)) return false;
  std::swap(candidate[0], candidate[1]);
  return true;
}

class ValidationFormula {
public:
  ValidationFormula(const Kernel &k, uint64_t limit,
                    const volatile sig_atomic_t *stop)
      : limit_(limit), stop_(stop) {
    solver_.reset(k.nvars, SubSearchMode::Fixed);
    for (Var var = 0; var < k.nvars && available(); ++var) {
      const Lit positive = make_lit(var, false);
      const int value = k.value(positive);
      if (!value)
        continue;
      const Lit unit = value > 0 ? positive : negate(positive);
      add(&unit, 1);
    }
    std::unordered_set<uint64_t> binaries;
    for (Lit lit = 0; lit < 2 * k.nvars && available(); ++lit)
      for (const WatchEntry watch : k.watches.entries(lit)) {
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (!binaries.insert(kernel_binary_key(lit, other)).second)
          continue;
        const Lit clause[2] = {lit, other};
        add(clause, 2);
        if (!available())
          break;
      }
    for (const auto [ref, clause] : arena_records(k.arena)) {
      if (!available())
        break;
      if (!clause->garbage())
        add(clause->begin(), clause->size);
    }
  }

  ValidationStatus status() const {
    if (interrupted_)
      return ValidationStatus::Interrupted;
    if (exhausted_)
      return ValidationStatus::ResourceExhausted;
    return ValidationStatus::Valid;
  }

  uint64_t ticks() const { return ticks_; }

  SubPropagationResult refutes(const std::vector<Lit> &assumptions,
                               const std::vector<Lit> &image) {
    std::vector<SubLit> query;
    query.reserve(assumptions.size() + image.size());
    query.insert(query.end(), assumptions.begin(), assumptions.end());
    for (const Lit lit : image)
      query.push_back(negate(lit));
    const uint64_t before = solver_.ticks();
    const uint64_t remaining = limit_ == UINT64_MAX ? UINT64_MAX
                               : ticks_ >= limit_ ? 0
                                                 : limit_ - ticks_;
    const SubPropagationResult result =
        solver_.propagate_assumptions_only(query, remaining, stop_);
    ticks_ += solver_.ticks() - before;
    if (result == SubPropagationResult::Interrupted)
      interrupted_ = true;
    else if (result == SubPropagationResult::Budget)
      exhausted_ = true;
    return result;
  }

private:
  bool available() {
    if (stop_ && *stop_) {
      interrupted_ = true;
      return false;
    }
    if (ticks_ == limit_) {
      exhausted_ = true;
      return false;
    }
    return true;
  }

  void add(const Lit *lits, uint32_t size) {
    if (limit_ != UINT64_MAX && size > limit_ - ticks_) {
      ticks_ = limit_;
      exhausted_ = true;
      return;
    }
    ticks_ += size;
    solver_.add_clause(std::vector<SubLit>(lits, lits + size));
  }

  SubSolver solver_;
  uint64_t limit_;
  const volatile sig_atomic_t *stop_;
  uint64_t ticks_ = 0;
  bool interrupted_ = false;
  bool exhausted_ = false;
};

ValidationStatus transformed_image(const Lit *lits, uint32_t size,
                                   const ParsedWitness &witness,
                                   std::vector<Lit> &image, bool &changed) {
  image.clear();
  image.reserve(size);
  changed = false;
  for (uint32_t i = 0; i < size; ++i) {
    const Lit lit = lits[i];
    const int8_t constant = witness.constants[var_of(lit)];
    if (constant) {
      changed = true;
      if ((constant > 0) != negated(lit))
        return ValidationStatus::Valid;
      continue;
    }
    const Lit positive_image = witness.images[var_of(lit)];
    const Lit mapped = positive_image == INVALID_LIT
                           ? lit
                           : (negated(lit) ? negate(positive_image)
                                           : positive_image);
    changed |= mapped != lit;
    if (std::find(image.begin(), image.end(), negate(mapped)) != image.end())
      return ValidationStatus::Valid;
    if (std::find(image.begin(), image.end(), mapped) == image.end())
      image.push_back(mapped);
  }
  return ValidationStatus::Inapplicable;
}

// A generated certificate describes the discovery snapshot; propagated root
// units are part of the checker's formula too. The fast path holds only if
// the witness maps every root unit to itself or to a true unit.
bool certified_root_closure_preserved(const Kernel &k,
                                      const ParsedWitness &witness) {
  std::vector<Lit> image;
  for (Var var = 0; var < k.nvars; ++var) {
    const Lit positive = make_lit(var, false);
    const int value = k.value(positive);
    if (!value)
      continue;
    const Lit unit = value > 0 ? positive : negate(positive);
    bool changed = false;
    if (transformed_image(&unit, 1, witness, image, changed) ==
        ValidationStatus::Valid)
      continue;
    if (!changed)
      continue;
    if (image.size() != 1 || k.value(image.front()) <= 0)
      return false;
  }
  return true;
}

ValidationResult validate_semantics(
    const Kernel &k, const std::vector<Lit> &candidate,
    const ParsedWitness &witness,
    const SubstitutionRedundancyParameters &parameters,
    const volatile sig_atomic_t *stop) {
  ValidationFormula formula(k, parameters.maximum_validation_ticks, stop);
  if (formula.status() != ValidationStatus::Valid)
    return {formula.status(), formula.ticks()};

  std::vector<Lit> assumptions;
  assumptions.reserve(candidate.size());
  for (const Lit lit : candidate)
    assumptions.push_back(negate(lit));
  std::vector<Lit> image;
  const auto check = [&](const Lit *lits, uint32_t size,
                         bool candidate_clause = false) {
    bool changed = false;
    if (transformed_image(lits, size, witness, image, changed) ==
        ValidationStatus::Valid)
      return ValidationStatus::Valid;
    if (!changed && !candidate_clause)
      return ValidationStatus::Valid;
    switch (formula.refutes(assumptions, image)) {
    case SubPropagationResult::Conflict:
      return ValidationStatus::Valid;
    case SubPropagationResult::Interrupted:
      return ValidationStatus::Interrupted;
    case SubPropagationResult::Budget:
      return ValidationStatus::ResourceExhausted;
    case SubPropagationResult::Consistent:
      return ValidationStatus::Inapplicable;
    }
    return ValidationStatus::Inapplicable;
  };

  // SR requires F & !C |-UP (F & C)|sigma: an unchanged clause of F is
  // automatic, an unchanged candidate needs a real RUP derivation.
  const ValidationStatus repaired =
      check(candidate.data(), candidate.size(), true);
  if (repaired != ValidationStatus::Valid)
    return {repaired, formula.ticks()};

  for (Var var = 0; var < k.nvars; ++var) {
    if (!k.value(make_lit(var, false)))
      continue;
    const Lit positive = make_lit(var, false);
    const Lit unit = k.value(positive) > 0 ? positive : negate(positive);
    const ValidationStatus checked = check(&unit, 1);
    if (checked != ValidationStatus::Valid)
      return {checked, formula.ticks()};
  }
  std::unordered_set<uint64_t> binaries;
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (!binaries.insert(kernel_binary_key(lit, other)).second)
        continue;
      const Lit clause[2] = {lit, other};
      const ValidationStatus checked = check(clause, 2);
      if (checked != ValidationStatus::Valid)
        return {checked, formula.ticks()};
    }
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (!clause->garbage()) {
      const ValidationStatus checked = check(clause->begin(), clause->size);
      if (checked != ValidationStatus::Valid)
        return {checked, formula.ticks()};
    }
  }
  return {ValidationStatus::Valid, formula.ticks()};
}

bool increment_digits(std::vector<uint32_t> &digits, uint32_t base) {
  for (size_t i = 0; i < digits.size(); ++i) {
    if (++digits[i] < base)
      return true;
    digits[i] = 0;
  }
  return false;
}

uint32_t candidate_size(const SubstitutionRedundancyGeneralCursor &cursor) {
  return static_cast<uint32_t>(std::count_if(
      cursor.candidate_digits.begin(), cursor.candidate_digits.end(),
      [](uint32_t digit) { return digit != 0; }));
}

void reset_general_cursor(const Kernel &k,
                          SubstitutionRedundancyGeneralCursor &cursor) {
  cursor = {};
  cursor.owner = &k;
  cursor.arena_generation = k.arena_generation;
  cursor.touch_epoch = k.touch_epoch;
  for (Var var = 0; var < k.nvars; ++var)
    if (!kernel_var_retired(k, var))
      cursor.variables.push_back(var);
  cursor.candidate_digits.assign(cursor.variables.size(), 0);
  cursor.witness_digits.assign(cursor.variables.size(), 0);
  cursor.substitution_digits.assign(cursor.variables.size(), 0);
  cursor.initialized = true;
  cursor.complete = cursor.variables.empty() ||
                    !increment_digits(cursor.candidate_digits, 3);
}

void materialize_general_instance(
    const SubstitutionRedundancyGeneralCursor &cursor,
    SubstitutionRedundancyInstance &instance) {
  instance = {};
  for (size_t i = 0; i < cursor.variables.size(); ++i) {
    const Var var = cursor.variables[i];
    if (cursor.candidate_digits[i])
      instance.clause.push_back(
          make_lit(var, cursor.candidate_digits[i] == 2));
    if (cursor.witness_digits[i])
      instance.witness.push_back(make_lit(var, cursor.witness_digits[i] == 2));
    const uint32_t mapped = cursor.substitution_digits[i];
    if (mapped) {
      const size_t target = (mapped - 1) / 2;
      instance.substitution.emplace_back(
          make_lit(var, false),
          make_lit(cursor.variables[target], (mapped - 1) & 1));
    }
  }
  if (cursor.pivot_choice < instance.clause.size())
    std::rotate(instance.clause.begin(),
                instance.clause.begin() + cursor.pivot_choice,
                instance.clause.begin() + cursor.pivot_choice + 1);
}

void advance_general_cursor(SubstitutionRedundancyGeneralCursor &cursor) {
  if (++cursor.pivot_choice < candidate_size(cursor))
    return;
  cursor.pivot_choice = 0;
  const uint32_t substitution_base =
      1 + 2 * static_cast<uint32_t>(cursor.variables.size());
  if (increment_digits(cursor.substitution_digits, substitution_base))
    return;
  if (increment_digits(cursor.witness_digits, 3))
    return;
  if (increment_digits(cursor.candidate_digits, 3))
    return;
  cursor.complete = true;
}

} // namespace

SubstitutionRedundancyGeneralResult substitution_redundancy_next_general(
    const Kernel &k, const SubstitutionRedundancyParameters &parameters,
    SubstitutionRedundancyGeneralCursor &cursor,
    SubstitutionRedundancyInstance &instance, volatile sig_atomic_t *stop) {
  SubstitutionRedundancyGeneralResult result;
  if (!parameters.valid() ||
      parameters.producer !=
          SubstitutionRedundancyProducer::GeneralComplete)
    return result;
  if (!cursor.initialized || cursor.owner != &k ||
      cursor.arena_generation != k.arena_generation ||
      cursor.touch_epoch != k.touch_epoch)
    reset_general_cursor(k, cursor);
  while (!cursor.complete) {
    if (stop_requested(k, stop))
      return finish(result, SubstitutionRedundancyGeneralStatus::Interrupted);
    if (result.candidates == parameters.maximum_general_candidates_per_advance)
      return finish(result, SubstitutionRedundancyGeneralStatus::LimitReached);
    ++result.candidates;
    materialize_general_instance(cursor, instance);
    std::vector<Lit> candidate;
    ParsedWitness witness;
    bool active = true;
    if (normalize_candidate(k, instance.clause, candidate, active) &&
        parse_witness(k, candidate.front(), instance, witness, active) && active &&
        !clause_already_true(k, candidate)) {
      const ValidationResult checked = validate_semantics(
          k, candidate, witness, parameters, stop ? stop : k.stop);
      if (checked.status == ValidationStatus::Interrupted)
        return finish(result, SubstitutionRedundancyGeneralStatus::Interrupted);
      if (checked.status == ValidationStatus::ResourceExhausted)
        return finish(result,
                      SubstitutionRedundancyGeneralStatus::ResourceExhausted);
      if (checked.status == ValidationStatus::Valid) {
        advance_general_cursor(cursor);
        return finish(result, SubstitutionRedundancyGeneralStatus::Found);
      }
    }
    advance_general_cursor(cursor);
  }
  return result;
}

RuleEligibility substitution_redundancy_guard(const Kernel &k) {
  return root_level_guard(k);
}

ApplyResult substitution_redundancy_apply(Kernel &) {
  return ApplyResult::NoOp;
}

static SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_detailed_impl(
    Kernel &k, const SubstitutionRedundancyInstance &instance,
    const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop, bool accept_generated_certificate) {
  SubstitutionRedundancyApplicationResult outcome;
  if (!substitution_redundancy_guard(k) || !parameters.valid())
    return outcome;

  std::vector<Lit> candidate;
  ParsedWitness witness;
  const bool unique_literal_amo =
      accept_generated_certificate &&
      instance.unique_literal_amo_certificate;
  const bool follows_unique_literal_amo =
      !unique_literal_amo && k.stats.sr_unique_literal_amo_binaries;
  bool active = true;
  if (!normalize_candidate(k, instance.clause, candidate, active))
    return outcome;
  // A generated ULC pair has the implicit pivot-only witness, accepted only on
  // the generated path in exactly this form; no witness images are built.
  if (unique_literal_amo) {
    if (candidate.size() != 2 || !instance.witness.empty() ||
        !instance.substitution.empty())
      return outcome;
    if (!authenticate_unique_literal_amo(k, candidate))
      return finish(outcome,
                    SubstitutionRedundancyApplicationStatus::Inapplicable);
    // Authentication may pick the other pivot: recheck the query domain.
    if (parameters.graph_symmetry.pointwise_root_stabilizer &&
        k.query_projection && !k.query_projection->preserves_constant(
            kernel_external_lit(k, candidate.front())))
      return finish(outcome,
                    SubstitutionRedundancyApplicationStatus::Inapplicable);
  } else if (!parse_witness(k, candidate.front(), instance, witness, active))
    return outcome;

  // Retirement is checked after syntax so it never masks a malformed request.
  if (!active)
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::Inapplicable);

  // A generated tuple must respect the selected query domain before any
  // proof or native mutation.
  if (accept_generated_certificate && !unique_literal_amo &&
      parameters.graph_symmetry.pointwise_root_stabilizer &&
      k.query_projection && !k.query_projection->preserves_restriction(
          k, witness.constants, witness.images))
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::Inapplicable);

  ++k.stats.sr_applications;
  if (stop_requested(k, stop))
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::Interrupted);
  if (inprocess_reach_root(k)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::Unsat;
    outcome.rule_result = ApplyResult::Unsat;
    return outcome;
  }
  if (clause_already_true(k, candidate) || kernel_contains_clause(k, candidate))
    return finish(outcome, SubstitutionRedundancyApplicationStatus::NoChange);
  // The general serializer needs one temporary external literal, with or
  // without proof output.
  if (plan_dsr_witness(candidate.data(), candidate.size(),
                       witness.emitted_true.data(), witness.emitted_true.size(),
                       witness.emitted_mappings.data(),
                       witness.emitted_mappings.size())
          .form == DsrWitnessForm::Extension &&
      k.external_to_internal.size() >= static_cast<uint32_t>(INT32_MAX))
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::ResourceExhausted);

  const bool certified_unique_literal_amo = unique_literal_amo;
  const bool certified =
      certified_unique_literal_amo ||
      (accept_generated_certificate &&
       (instance.symmetry_chain_certificate ||
        instance.orbitopal_prefix_certificate) &&
       certified_root_closure_preserved(k, witness));
#ifndef NDEBUG
  // A certificate only skips the formula scan; the tuple must still be valid.
  if (certified && !certified_unique_literal_amo)
    assert(validate_semantics(k, candidate, witness, parameters,
                              stop ? stop : k.stop)
               .status != ValidationStatus::Inapplicable);
#endif
  if (!certified) {
    const ValidationResult checked = validate_semantics(
        k, candidate, witness, parameters, stop ? stop : k.stop);
    outcome.validation_ticks = checked.ticks;
    k.stats.sr_validation_ticks += checked.ticks;
    if (checked.status != ValidationStatus::Valid) {
      outcome.status = checked.status == ValidationStatus::Interrupted
                           ? SubstitutionRedundancyApplicationStatus::Interrupted
                       : checked.status == ValidationStatus::ResourceExhausted
                           ? SubstitutionRedundancyApplicationStatus::ResourceExhausted
                           : SubstitutionRedundancyApplicationStatus::Inapplicable;
      return outcome;
    }
  }

  std::vector<Lit> kept;
  kept.reserve(candidate.size());
  for (const Lit lit : candidate)
    if (k.value(lit) >= 0)
      kept.push_back(lit);
  if (kept != candidate && kernel_contains_clause(k, kept))
    return finish(outcome, SubstitutionRedundancyApplicationStatus::NoChange);
  // A no-op after root simplification needs no SR line, so it is classified
  // before refusing a proof format that cannot write one.
  if (k.proof && k.proof->startup_pb_written())
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat);
  const uint64_t required_ids = kept.size() == candidate.size() ? 1 : 2;
  if (k.next_clause_id > UINT64_MAX - required_ids)
    return finish(outcome,
                  SubstitutionRedundancyApplicationStatus::ResourceExhausted);

  const uint64_t sr_id = ++k.next_clause_id;
  if (k.proof) {
    if (certified_unique_literal_amo) {
      k.proof->add_rat(sr_id, candidate.data(), candidate.size());
    } else if (!k.proof->add_sr(
                   sr_id, candidate.data(), candidate.size(),
                   witness.emitted_true.data(), witness.emitted_true.size(),
                   witness.emitted_mappings.data(),
                   witness.emitted_mappings.size(),
                   static_cast<uint32_t>(k.external_to_internal.size()),
                   /*propagates=*/kept.size() == 1)) {
      --k.next_clause_id;
      return finish(outcome,
                    SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat);
    }
  }
  outcome.witness_literals =
      certified_unique_literal_amo
          ? 0
          : witness.emitted_true.size() +
                2 * witness.emitted_mappings.size() + 1;
  k.stats.sr_witness_literals += outcome.witness_literals;

  uint64_t committed_id = sr_id;
  if (kept.size() != candidate.size()) {
    committed_id = ++k.next_clause_id;
    if (k.proof) {
      k.proof->add_rup(committed_id, kept.data(), kept.size());
      k.proof->del(k.next_clause_id, sr_id, candidate.data(), candidate.size());
    }
  }

  k.projection_changed = true;
  if (k.query_projection) {
    if (certified_unique_literal_amo)
      k.query_projection->restrict_constant(kernel_external_lit(k, candidate.front()));
    else k.query_projection->restrict(k, witness.constants, witness.images);
  }
  if (kept.empty()) {
    k.inconsistent = true;
    outcome.status = SubstitutionRedundancyApplicationStatus::Unsat;
    outcome.rule_result = ApplyResult::Unsat;
    return outcome;
  }
  if (kept.size() == 1) {
    k.unit_ids[var_of(kept[0])] = committed_id;
    kernel_assign_unit(k, kept[0]);
  } else if (kept.size() == 2) {
    // Satsuma publishes ULC pairs in its preprocessed DIMACS, so they are
    // added without arming the "new material" schedulers.
    kernel_add_binary_clause(k, kept[0], kept[1], committed_id,
                             /*mark_added_material=*/!unique_literal_amo);
    if (unique_literal_amo)
      ++k.stats.sr_unique_literal_amo_binaries;
  } else {
    kernel_add_large_clause(k, kept.data(), kept.size(), true, 0, committed_id);
  }
  outcome.status = SubstitutionRedundancyApplicationStatus::Applied;
  outcome.rule_result = ApplyResult::Ok;
  ++k.stats.sr_applied;
  if (follows_unique_literal_amo) {
    ++k.stats.sr_post_amo_nonhelper_applied;
    if (kept.size() == 1 &&
        parameters.producer == SubstitutionRedundancyProducer::GraphSymmetry)
      ++k.stats.sr_post_amo_graph_root_units;
  }
  // A certified prefix admits its next member against the propagated root
  // state; the last member's conflict surfaces here too.
  if (accept_generated_certificate &&
      instance.orbitopal_prefix_certificate && inprocess_reach_root(k)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::Unsat;
    outcome.rule_result = ApplyResult::Unsat;
  }
  return outcome;
}

SubstitutionRedundancyApplicationResult substitution_redundancy_apply_detailed(
    Kernel &k, const SubstitutionRedundancyInstance &instance,
    const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop) {
  return substitution_redundancy_apply_detailed_impl(k, instance, parameters,
                                                      stop, false);
}

SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_orbitopal_prefix(
    Kernel &k, const SubstitutionRedundancyInstance &instance,
    const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop) {
  return substitution_redundancy_apply_detailed_impl(k, instance, parameters,
                                                      stop, true);
}

TACTIX_REGISTER_RULE(SubstitutionRedundancy, RuleTier::Inprocess,
                     substitution_redundancy_guard,
                     substitution_redundancy_apply);

} // namespace tactix

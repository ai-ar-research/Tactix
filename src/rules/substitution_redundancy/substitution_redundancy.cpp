#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include "engine/subsolver.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace tactix {
namespace {

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

bool valid_internal_literal(const Kernel &k, Lit lit) {
  return lit != INVALID_LIT && var_of(lit) < k.nvars &&
         !kernel_var_retired(k, var_of(lit));
}

bool normalize_candidate(const Kernel &k, const std::vector<Lit> &input,
                         std::vector<Lit> &output) {
  output.clear();
  output.reserve(input.size());
  for (const Lit lit : input) {
    if (!valid_internal_literal(k, lit))
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
                   ParsedWitness &result) {
  result.constants.assign(k.nvars, 0);
  result.images.assign(k.nvars, INVALID_LIT);
  result.emitted_true.clear();
  result.emitted_mappings.clear();

  const auto set_true = [&](Lit lit, bool implicit) {
    if (!valid_internal_literal(k, lit))
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
    if (!valid_internal_literal(k, source) ||
        !valid_internal_literal(k, target))
      return false;
    const Var var = var_of(source);
    if (result.constants[var] || !sources.insert(var).second)
      return false;
    const Lit image = negated(source) ? negate(target) : target;
    if (image == make_lit(var, false))
      return false;
    result.images[var] = image;
    // A DSR substitution-map source is a variable, encoded as its positive
    // literal. Accept either polarity on the typed semantic surface, then
    // emit the canonical source and the image of that positive literal.
    // dsr-trim accepts signed sources, but Trestle deliberately rejects them.
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
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *stored = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(stored);
    if (!stored->garbage() && stored->size == wanted.size()) {
      std::vector<Lit> present(stored->begin(), stored->end());
      std::sort(present.begin(), present.end());
      if (present == wanted)
        return true;
    }
    ref += words;
  }
  return false;
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
    for (Ref ref = 0; ref < k.arena.size() && available();) {
      const Clause *clause = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(clause);
      if (!clause->garbage())
        add(clause->begin(), clause->size);
      ref += words;
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
  const auto check = [&](const Lit *lits, uint32_t size) {
    bool changed = false;
    if (transformed_image(lits, size, witness, image, changed) ==
        ValidationStatus::Valid)
      return ValidationStatus::Valid;
    if (!changed)
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
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      const ValidationStatus checked = check(clause->begin(), clause->size);
      if (checked != ValidationStatus::Valid)
        return {checked, formula.ticks()};
    }
    ref += words;
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
  if (!parameters.valid() || !parameters.general_producer)
    return result;
  if (!cursor.initialized || cursor.owner != &k ||
      cursor.arena_generation != k.arena_generation ||
      cursor.touch_epoch != k.touch_epoch)
    reset_general_cursor(k, cursor);
  while (!cursor.complete) {
    if ((stop && *stop) || (k.stop && *k.stop)) {
      result.status = SubstitutionRedundancyGeneralStatus::Interrupted;
      return result;
    }
    if (result.candidates == parameters.maximum_general_candidates_per_advance) {
      result.status = SubstitutionRedundancyGeneralStatus::LimitReached;
      return result;
    }
    ++result.candidates;
    materialize_general_instance(cursor, instance);
    std::vector<Lit> candidate;
    ParsedWitness witness;
    if (normalize_candidate(k, instance.clause, candidate) &&
        parse_witness(k, candidate.front(), instance, witness) &&
        !clause_already_true(k, candidate)) {
      const ValidationResult checked = validate_semantics(
          k, candidate, witness, parameters, stop ? stop : k.stop);
      if (checked.status == ValidationStatus::Interrupted) {
        result.status = SubstitutionRedundancyGeneralStatus::Interrupted;
        return result;
      }
      if (checked.status == ValidationStatus::ResourceExhausted) {
        result.status = SubstitutionRedundancyGeneralStatus::ResourceExhausted;
        return result;
      }
      if (checked.status == ValidationStatus::Valid) {
        advance_general_cursor(cursor);
        result.status = SubstitutionRedundancyGeneralStatus::Found;
        return result;
      }
    }
    advance_general_cursor(cursor);
  }
  return result;
}

RuleEligibility substitution_redundancy_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

ApplyResult substitution_redundancy_apply(Kernel &) {
  return ApplyResult::NoOp;
}

SubstitutionRedundancyApplicationResult substitution_redundancy_apply_detailed(
    Kernel &k, const SubstitutionRedundancyInstance &instance,
    const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop) {
  SubstitutionRedundancyApplicationResult outcome;
  if (!substitution_redundancy_guard(k) || !parameters.valid())
    return outcome;

  std::vector<Lit> candidate;
  ParsedWitness witness;
  if (!normalize_candidate(k, instance.clause, candidate) ||
      !parse_witness(k, candidate.front(), instance, witness))
    return outcome;

  ++k.stats.sr_applications;
  if ((stop && *stop) || (k.stop && *k.stop)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::Interrupted;
    return outcome;
  }
  if (inprocess_reach_root(k)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::Unsat;
    outcome.rule_result = ApplyResult::Unsat;
    return outcome;
  }
  if (clause_already_true(k, candidate) || kernel_contains_clause(k, candidate)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::NoChange;
    return outcome;
  }
  if (k.proof && !k.proof->dsr()) {
    outcome.status =
        SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat;
    return outcome;
  }

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

  std::vector<Lit> kept;
  kept.reserve(candidate.size());
  for (const Lit lit : candidate)
    if (k.value(lit) >= 0)
      kept.push_back(lit);
  if (kept != candidate && kernel_contains_clause(k, kept)) {
    outcome.status = SubstitutionRedundancyApplicationStatus::NoChange;
    return outcome;
  }
  const uint64_t required_ids = kept.size() == candidate.size() ? 1 : 2;
  if (k.next_clause_id > UINT64_MAX - required_ids) {
    outcome.status =
        SubstitutionRedundancyApplicationStatus::ResourceExhausted;
    return outcome;
  }

  const uint64_t sr_id = ++k.next_clause_id;
  if (k.proof &&
      !k.proof->add_sr(sr_id, candidate.data(), candidate.size(),
                       witness.emitted_true.data(), witness.emitted_true.size(),
                       witness.emitted_mappings.data(),
                       witness.emitted_mappings.size())) {
    --k.next_clause_id;
    outcome.status =
        SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat;
    return outcome;
  }
  outcome.witness_literals = witness.emitted_true.size() +
                             2 * witness.emitted_mappings.size() + 1;
  k.stats.sr_witness_literals += outcome.witness_literals;

  uint64_t committed_id = sr_id;
  if (kept.size() != candidate.size()) {
    committed_id = ++k.next_clause_id;
    if (k.proof) {
      k.proof->add_rup(committed_id, kept.data(), kept.size(), nullptr, 0);
      k.proof->del(k.next_clause_id, sr_id, candidate.data(), candidate.size());
    }
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
    kernel_add_binary_clause(k, kept[0], kept[1], committed_id);
  } else {
    kernel_add_large_clause(k, kept.data(), kept.size(), true, 0, committed_id);
  }
  outcome.status = SubstitutionRedundancyApplicationStatus::Applied;
  outcome.rule_result = ApplyResult::Ok;
  ++k.stats.sr_applied;
  return outcome;
}

ApplyResult substitution_redundancy_apply(
    Kernel &k, const SubstitutionRedundancyInstance &instance,
    const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop) {
  return substitution_redundancy_apply_detailed(k, instance, parameters, stop)
      .rule_result;
}

ApplyResult substitution_redundancy_apply_orbitopal(
    Kernel &kernel, const SubstitutionRedundancyParameters &parameters,
    volatile sig_atomic_t *stop) {
  std::vector<SubstitutionRedundancyInstance> instances;
  const OrbitopalEnumerationResult enumeration =
      substitution_redundancy_enumerate_orbitopal(
          kernel, parameters,
          [&](const SubstitutionRedundancyInstance &instance) {
            instances.push_back(instance);
            return true;
          },
          stop);
  if (enumeration.status == OrbitopalEnumerationStatus::Interrupted)
    return ApplyResult::NoOp;
  bool progress = false;
  for (const SubstitutionRedundancyInstance &instance : instances) {
    if ((stop && *stop) || (kernel.stop && *kernel.stop))
      break;
    const ApplyResult applied =
        substitution_redundancy_apply(kernel, instance, parameters, stop);
    if (applied == ApplyResult::Unsat)
      return applied;
    progress |= applied == ApplyResult::Ok;
  }
  return progress ? ApplyResult::Ok : ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(SubstitutionRedundancy, RuleTier::Inprocess,
                     substitution_redundancy_guard,
                     substitution_redundancy_apply);

} // namespace tactix

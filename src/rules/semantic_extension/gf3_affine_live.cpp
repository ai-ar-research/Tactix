#include "rules/semantic_extension/gf3_affine_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace tactix {
namespace {

namespace gf3 = gf3_affine;
using ClauseKey = std::vector<int32_t>;

struct Meter {
  const Kernel &kernel;
  uint64_t limit = 0;
  uint64_t used = 0;
  bool interrupted = false;
  bool exhausted = false;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (used > limit || amount > limit - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
};

ClauseKey normalized(ClauseKey clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool root_guard(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && kernel.trail.empty() && !kernel.dense &&
         !kernel.probing.recording && kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX;
}

bool recognition_limits_valid(const Gf3AffineLiveRequest &request) {
  return request.maximum_recognition_work &&
         request.recognition.maximum_source_clauses &&
         request.recognition.maximum_source_literals &&
         request.recognition.maximum_domains &&
         request.recognition.maximum_equations &&
         request.recognition.maximum_equation_arity >= 2 &&
         request.recognition.maximum_tuples_per_equation;
}

bool apply_limits_valid(const Gf3AffineLiveRequest &request) {
  return recognition_limits_valid(request) && request.maximum_planning_work &&
         request.maximum_duplicate_scan_work &&
         request.maximum_revalidation_work && request.maximum_proof_work &&
         request.maximum_proof_additions &&
         request.maximum_proof_literal_occurrences &&
         request.maximum_arena_words && request.maximum_journal_bytes &&
         request.maximum_external_extent &&
         request.elimination.maximum_result_arity &&
         request.elimination.maximum_result_tuples &&
         request.elimination.maximum_additions &&
         request.elimination.maximum_addition_literals && request.action_id &&
         request.semantic_id && request.records;
}

Gf3AffineLiveStatus stopped_status(const Meter &meter,
                                   Gf3AffineLiveStatus otherwise) {
  if (meter.interrupted) return Gf3AffineLiveStatus::Interrupted;
  if (meter.exhausted) return Gf3AffineLiveStatus::ResourceExhausted;
  return otherwise;
}

struct Capture {
  Gf3AffineOwnerSnapshot snapshot;
  Gf3AffineLiveStatus status = Gf3AffineLiveStatus::NoStructure;
};

bool take_source(const Kernel &kernel, Gf3AffineSourceKind kind, Ref ref,
                 Lit first, Lit second, uint64_t native_id,
                 const Lit *literals, uint32_t size,
                 const Gf3AffineLiveRequest &request, Meter &meter,
                 Capture &capture) {
  if (capture.snapshot.sources.size() ==
          request.recognition.maximum_source_clauses ||
      size > request.recognition.maximum_source_literals -
                 capture.snapshot.source_literals) {
    capture.status = Gf3AffineLiveStatus::ResourceExhausted;
    return false;
  }
  if (!meter.charge(uint64_t{1} + size)) return false;
  Gf3AffineLiveSource source;
  source.occurrence = capture.snapshot.sources.size() + 1;
  source.kind = kind;
  source.ref = ref;
  source.first = first;
  source.second = second;
  source.native_proof_id = native_id;
  source.clause.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    const Var var = var_of(literals[index]);
    if (kernel_var_retired(kernel, var) || kernel.value(literals[index])) {
      capture.status = Gf3AffineLiveStatus::NoStructure;
      return false;
    }
    source.clause.push_back(kernel_externalize(kernel, literals[index]));
  }
  capture.snapshot.source_literals += size;
  capture.snapshot.sources.push_back(std::move(source));
  return true;
}

bool capture_and_recognize(const Kernel &kernel,
                           const Gf3AffineLiveRequest &request, Meter &meter,
                           Capture &capture) {
  capture.snapshot.external_extent = kernel.external_to_internal.size();
  capture.snapshot.arena_generation = kernel.arena_generation;
  capture.status = Gf3AffineLiveStatus::NoStructure;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Ref current = reference;
    const Clause *clause = clause_at(kernel.arena, reference);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - reference) return false;
    reference += words;
    if (!meter.charge(1)) return false;
    if (!clause->garbage() && !clause->redundant() &&
        !take_source(kernel, Gf3AffineSourceKind::Large, current, INVALID_LIT,
                     INVALID_LIT, clause->id(), clause->lits, clause->size,
                     request, meter, capture))
      return false;
  }
  std::set<uint64_t> binaries;
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      const uint64_t key = kernel_binary_key(first, watch.other());
      if (!binaries.insert(key).second) {
        capture.status = Gf3AffineLiveStatus::NoStructure;
        return false;
      }
      const Lit pair[] = {first, watch.other()};
      const uint64_t native_id =
          kernel.binary_ids.tracks_values()
              ? kernel_binary_id(kernel, first, watch.other())
              : 0;
      if (!take_source(kernel, Gf3AffineSourceKind::Binary, INVALID_REF,
                       first, watch.other(), native_id, pair, 2, request, meter,
                       capture))
        return false;
    }
  if (capture.snapshot.sources.empty()) return false;

  std::vector<gf3::Premise> premises;
  premises.reserve(capture.snapshot.sources.size());
  for (const Gf3AffineLiveSource &source : capture.snapshot.sources)
    premises.push_back(
        {source.occurrence, source.clause, source.native_proof_id});
  const auto charge = [&](uint64_t amount) { return meter.charge(amount); };
  gf3::RecognitionResult recognized =
      gf3::recognize(premises, request.recognition, charge);
  if (recognized.status != gf3::RecognitionStatus::Recognized) {
    capture.status =
        recognized.status == gf3::RecognitionStatus::EffortExhausted ||
                recognized.status == gf3::RecognitionStatus::BoundExceeded
            ? Gf3AffineLiveStatus::ResourceExhausted
            : Gf3AffineLiveStatus::NoStructure;
    return false;
  }
  capture.snapshot.theory = std::move(recognized.theory);
  capture.snapshot.capture_work = meter.used;
  capture.status = Gf3AffineLiveStatus::Applied;
  return true;
}

bool same_domain(const gf3::Domain &first, const gf3::Domain &second) {
  return first.values == second.values &&
         first.at_least_one == second.at_least_one &&
         first.at_most_one == second.at_most_one;
}

bool same_equation(const gf3::Equation &first, const gf3::Equation &second) {
  return first.domains == second.domains &&
         first.coefficients == second.coefficients && first.rhs == second.rhs &&
         first.forbidden_by_tuple == second.forbidden_by_tuple;
}

bool same_theory(const gf3::Theory &first, const gf3::Theory &second) {
  if (first.maximum_variable != second.maximum_variable ||
      first.source.size() != second.source.size() ||
      first.domains.size() != second.domains.size() ||
      first.equations.size() != second.equations.size())
    return false;
  for (size_t index = 0; index < first.source.size(); ++index) {
    const auto &a = first.source[index];
    const auto &b = second.source[index];
    if (a.occurrence != b.occurrence ||
        a.native_proof_id != b.native_proof_id || a.literals != b.literals)
      return false;
  }
  for (size_t index = 0; index < first.domains.size(); ++index)
    if (!same_domain(first.domains[index], second.domains[index])) return false;
  for (size_t index = 0; index < first.equations.size(); ++index)
    if (!same_equation(first.equations[index], second.equations[index]))
      return false;
  return true;
}

Gf3AffineLiveStatus retained_owner_status(
    const Gf3AffineOwnerSnapshot &owner,
    const Gf3AffineLiveRequest &request, Meter &meter, gf3::Theory &checked) {
  if (!owner.external_extent || owner.external_extent > INT32_MAX ||
      owner.sources.empty() ||
      owner.sources.size() > request.recognition.maximum_source_clauses ||
      owner.source_literals > request.recognition.maximum_source_literals ||
      owner.sources.size() != owner.theory.source.size())
    return Gf3AffineLiveStatus::Invalid;
  uint64_t literals = 0;
  std::vector<gf3::Premise> premises;
  premises.reserve(owner.sources.size());
  for (size_t index = 0; index < owner.sources.size(); ++index) {
    const Gf3AffineLiveSource &source = owner.sources[index];
    const gf3::Premise &premise = owner.theory.source[index];
    if (!meter.charge(uint64_t{1} + source.clause.size()))
      return stopped_status(meter, Gf3AffineLiveStatus::Invalid);
    if (source.occurrence != index + 1 ||
        premise.occurrence != source.occurrence ||
        premise.native_proof_id != source.native_proof_id ||
        premise.literals != source.clause || source.clause.empty() ||
        source.clause.size() >
            request.recognition.maximum_source_literals - literals)
      return Gf3AffineLiveStatus::Invalid;
    literals += source.clause.size();
    premises.push_back(premise);
  }
  if (literals != owner.source_literals) return Gf3AffineLiveStatus::Invalid;
  const auto charge = [&](uint64_t amount) { return meter.charge(amount); };
  gf3::RecognitionResult recognized =
      gf3::recognize(premises, request.recognition, charge);
  if (recognized.status != gf3::RecognitionStatus::Recognized) {
    if (recognized.status == gf3::RecognitionStatus::EffortExhausted ||
        recognized.status == gf3::RecognitionStatus::BoundExceeded)
      return stopped_status(meter, Gf3AffineLiveStatus::ResourceExhausted);
    return stopped_status(meter, Gf3AffineLiveStatus::Invalid);
  }
  if (!same_theory(owner.theory, recognized.theory))
    return Gf3AffineLiveStatus::Invalid;
  checked = std::move(recognized.theory);
  return Gf3AffineLiveStatus::Applied;
}

bool current_clause(const Kernel &kernel, const Lit *literals, uint32_t size,
                    const ClauseKey &expected) {
  if (size != expected.size()) return false;
  ClauseKey external;
  external.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    if (kernel_var_retired(kernel, var_of(literals[index])) ||
        kernel.value(literals[index]))
      return false;
    external.push_back(kernel_externalize(kernel, literals[index]));
  }
  return normalized(std::move(external)) == normalized(expected);
}

bool revalidate_sources(const Kernel &kernel,
                        const Gf3AffineOwnerSnapshot &owner, Meter &meter) {
  if (owner.arena_generation != kernel.arena_generation) return false;
  std::map<Ref, const Clause *> large;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Ref current = reference;
    const Clause *clause = clause_at(kernel.arena, reference);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - reference) return false;
    reference += words;
    if (!meter.charge(uint64_t{1} + clause->size)) return false;
    if (!clause->garbage() && !clause->redundant())
      large.emplace(current, clause);
  }
  // Reconstruct the exact live binary image in one bounded scan.  Looking up
  // each retained binary through kernel_binary_live would hide its watch-list
  // traversal from the action effort meter.
  std::map<uint64_t, uint64_t> binaries;
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      const uint64_t key = kernel_binary_key(first, watch.other());
      const uint64_t native_id =
          kernel.binary_ids.tracks_values()
              ? kernel_binary_id(kernel, first, watch.other())
              : 0;
      if (!binaries.emplace(key, native_id).second) return false;
    }
  for (const Gf3AffineLiveSource &source : owner.sources) {
    if (!meter.charge(uint64_t{1} + source.clause.size())) return false;
    if (source.kind == Gf3AffineSourceKind::Large) {
      const auto found = large.find(source.ref);
      if (found == large.end() ||
          found->second->id() != source.native_proof_id ||
          !current_clause(kernel, found->second->lits, found->second->size,
                          source.clause))
        return false;
    } else {
      if (source.first == INVALID_LIT || source.second == INVALID_LIT ||
          source.first == source.second)
        return false;
      const auto found =
          binaries.find(kernel_binary_key(source.first, source.second));
      if (found == binaries.end() || found->second != source.native_proof_id)
        return false;
      const Lit pair[] = {source.first, source.second};
      if (!current_clause(kernel, pair, 2, source.clause)) return false;
    }
  }
  return true;
}

bool collect_live_clauses(const Kernel &kernel, Meter &meter,
                          std::set<ClauseKey> &clauses) {
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > kernel.arena.size() - reference) return false;
    reference += words;
    if (!meter.charge(uint64_t{1} + clause->size)) return false;
    if (clause->garbage()) continue;
    ClauseKey external;
    external.reserve(clause->size);
    for (Lit literal : *clause)
      external.push_back(kernel_externalize(kernel, literal));
    clauses.insert(normalized(std::move(external)));
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      clauses.insert(normalized(
          {kernel_externalize(kernel, first),
           kernel_externalize(kernel, watch.other())}));
    }
  return true;
}

struct Journal {
  uint64_t maximum = 0;
  uint64_t work = 0;
  std::string bytes;

  bool append(const std::string &text) {
    if (text.size() > UINT64_MAX - work) {
      work = UINT64_MAX;
      return false;
    }
    work += text.size();
    if (bytes.size() > maximum || text.size() > maximum - bytes.size())
      return false;
    bytes += text;
    return true;
  }
  bool integers(const std::vector<int32_t> &values) {
    for (size_t index = 0; index < values.size(); ++index)
      if (!append((index ? "," : "") + std::to_string(values[index])))
        return false;
    return true;
  }
  bool unsigneds(const std::vector<uint32_t> &values) {
    for (size_t index = 0; index < values.size(); ++index)
      if (!append((index ? "," : "") + std::to_string(values[index])))
        return false;
    return true;
  }
  bool bytes_array(const std::vector<uint8_t> &values) {
    for (size_t index = 0; index < values.size(); ++index)
      if (!append((index ? "," : "") +
                  std::to_string(static_cast<unsigned>(values[index]))))
        return false;
    return true;
  }
};

const gf3::Premise *premise(const gf3::Theory &theory, uint64_t occurrence) {
  if (!occurrence || occurrence > theory.source.size()) return nullptr;
  const gf3::Premise &result = theory.source[occurrence - 1];
  return result.occurrence == occurrence ? &result : nullptr;
}

bool render_journal(const Gf3AffineLiveRequest &request,
                    const Gf3AffineOwnerSnapshot &owner,
                    const gf3::PairEliminationPlan &plan,
                    const std::vector<uint64_t> &published_ids,
                    uint64_t proof_work, Journal &journal) {
  const std::string ids = "\"action\":" + std::to_string(request.action_id) +
                          ",\"semantic\":" +
                          std::to_string(request.semantic_id);
  if (!journal.append("{" + ids +
                      ",\"kind\":\"gf3-affine-begin\",\"first_equation\":" +
                      std::to_string(request.first_equation) +
                      ",\"second_equation\":" +
                      std::to_string(request.second_equation) +
                      ",\"recognition\":\"complete-forbidden-tuples\","
                      "\"proof\":\"rup\",\"propagation\":"
                      "\"deferred-controller\"}\n"))
    return false;

  std::set<uint64_t> relevant;
  std::set<uint32_t> domains;
  for (uint32_t equation_index :
       {request.first_equation, request.second_equation}) {
    if (equation_index >= owner.theory.equations.size()) return false;
    const gf3::Equation &equation = owner.theory.equations[equation_index];
    domains.insert(equation.domains.begin(), equation.domains.end());
    for (uint64_t occurrence : equation.forbidden_by_tuple)
      if (occurrence) relevant.insert(occurrence);
  }
  for (uint32_t domain_index : domains) {
    if (domain_index >= owner.theory.domains.size()) return false;
    const gf3::Domain &domain = owner.theory.domains[domain_index];
    relevant.insert(domain.at_least_one);
    for (uint64_t occurrence : domain.at_most_one)
      if (occurrence) relevant.insert(occurrence);
  }
  for (uint64_t occurrence : relevant) {
    const gf3::Premise *source = premise(owner.theory, occurrence);
    if (!source ||
        !journal.append("{" + ids +
                        ",\"kind\":\"gf3-affine-source\",\"occurrence\":" +
                        std::to_string(occurrence) +
                        ",\"native_proof_id\":" +
                        std::to_string(source->native_proof_id) +
                        ",\"clause\":[") ||
        !journal.integers(source->literals) || !journal.append("]}\n"))
      return false;
  }
  for (uint32_t equation_index :
       {request.first_equation, request.second_equation}) {
    const gf3::Equation &equation = owner.theory.equations[equation_index];
    if (!journal.append("{" + ids +
                        ",\"kind\":\"gf3-affine-equation\",\"index\":" +
                        std::to_string(equation_index) +
                        ",\"domains\":[") ||
        !journal.unsigneds(equation.domains) ||
        !journal.append("],\"coefficients\":[") ||
        !journal.bytes_array(equation.coefficients) ||
        !journal.append("],\"rhs\":" + std::to_string(equation.rhs) +
                        "}\n"))
      return false;
  }
  if (!journal.append("{" + ids +
                      ",\"kind\":\"gf3-affine-consequence\",\"domains\":[") ||
      !journal.unsigneds(plan.consequence.domains) ||
      !journal.append("],\"coefficients\":[") ||
      !journal.bytes_array(plan.consequence.coefficients) ||
      !journal.append("],\"rhs\":" +
                      std::to_string(plan.consequence.rhs) +
                      ",\"projection\":\"identity-original-variables\","
                      "\"reconstruction_witness_required\":false}\n"))
    return false;
  if (published_ids.size() != plan.additions.size()) return false;
  for (size_t index = 0; index < plan.additions.size(); ++index) {
    const gf3::Addition &addition = plan.additions[index];
    if (!journal.append("{" + ids +
                        ",\"kind\":\"gf3-affine-addition\",\"plan_index\":" +
                        std::to_string(index) +
                        ",\"published_id\":" +
                        std::to_string(published_ids[index]) +
                        ",\"duplicate\":" +
                        (published_ids[index] ? "false" : "true") +
                        ",\"clause\":[") ||
        !journal.integers(addition.clause) ||
        !journal.append("],\"antecedents\":["))
      return false;
    for (size_t antecedent = 0; antecedent < addition.antecedents.size();
         ++antecedent) {
      const gf3::SourceReference &source = addition.antecedents[antecedent];
      if (!journal.append((antecedent ? "," : "") +
                          std::string("{\"occurrence\":") +
                          std::to_string(source.occurrence) +
                          ",\"native_proof_id\":" +
                          std::to_string(source.native_proof_id) + "}"))
        return false;
    }
    if (!journal.append("]}\n")) return false;
  }
  size_t published = 0;
  for (uint64_t id : published_ids) published += id != 0;
  return journal.append(
      "{" + ids +
      ",\"kind\":\"gf3-affine-complete\",\"planned_additions\":" +
      std::to_string(plan.additions.size()) +
      ",\"published_additions\":" + std::to_string(published) +
      ",\"proof_work\":" + std::to_string(proof_work) +
      ",\"projection\":\"identity-original-variables\","
      "\"reconstruction_witness_required\":false}\n");
}

Gf3AffineLiveStatus recognition_failure(const Meter &meter,
                                        Gf3AffineLiveStatus otherwise) {
  return stopped_status(meter, otherwise);
}

Gf3AffineLiveStatus plan_failure(const Meter &meter,
                                 const gf3::PairEliminationPlan &plan) {
  if (meter.interrupted) return Gf3AffineLiveStatus::Interrupted;
  if (meter.exhausted || plan.status == gf3::PlanStatus::BoundExceeded ||
      plan.status == gf3::PlanStatus::EffortExhausted)
    return Gf3AffineLiveStatus::ResourceExhausted;
  switch (plan.status) {
  case gf3::PlanStatus::NoConsequence:
    return Gf3AffineLiveStatus::NoConsequence;
  case gf3::PlanStatus::UnsupportedOverlap:
  case gf3::PlanStatus::IncompleteTheory:
    return Gf3AffineLiveStatus::NoStructure;
  case gf3::PlanStatus::InvalidRequest:
    return Gf3AffineLiveStatus::Invalid;
  case gf3::PlanStatus::Planned:
    return Gf3AffineLiveStatus::Applied;
  case gf3::PlanStatus::BoundExceeded:
  case gf3::PlanStatus::EffortExhausted:
    return Gf3AffineLiveStatus::ResourceExhausted;
  }
  return Gf3AffineLiveStatus::Invalid;
}

} // namespace

Gf3AffineOwnerCaptureResult gf3_affine_live_capture_owner(
    Kernel &kernel, const Gf3AffineLiveRequest &request,
    Gf3AffineOwnerSnapshot &output) {
  Gf3AffineOwnerCaptureResult result;
  if (!recognition_limits_valid(request)) return result;
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? Gf3AffineLiveStatus::Interrupted
                        : Gf3AffineLiveStatus::Inapplicable;
    return result;
  }
  Meter meter{kernel, request.maximum_recognition_work};
  Capture capture;
  if (!capture_and_recognize(kernel, request, meter, capture)) {
    result.status = recognition_failure(meter, capture.status);
    result.recognition_work = meter.used;
    return result;
  }
  result.status = Gf3AffineLiveStatus::Applied;
  result.source_clauses = capture.snapshot.sources.size();
  result.source_literals = capture.snapshot.source_literals;
  result.recognition_work = meter.used;
  result.equations = capture.snapshot.theory.equations.size();
  output = std::move(capture.snapshot);
  return result;
}

Gf3AffineLiveResult gf3_affine_live_apply(
    Kernel &kernel, const Gf3AffineLiveRequest &request) {
  Gf3AffineLiveResult result;
  if (!apply_limits_valid(request)) return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Gf3AffineLiveStatus::UnsupportedProof;
    return result;
  }
  if (!root_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? Gf3AffineLiveStatus::Interrupted
                        : Gf3AffineLiveStatus::Inapplicable;
    return result;
  }
  if (kernel.external_to_internal.size() > request.maximum_external_extent) {
    result.status = Gf3AffineLiveStatus::ResourceExhausted;
    return result;
  }

  Meter recognition_meter{kernel, request.maximum_recognition_work};
  Gf3AffineOwnerSnapshot owned;
  const Gf3AffineOwnerSnapshot *owner = request.retained_owner;
  gf3::Theory checked;
  const uint64_t scan_begin = recognition_meter.used;
  if (!owner) {
    Capture capture;
    if (!capture_and_recognize(kernel, request, recognition_meter, capture)) {
      result.source_scan_work = recognition_meter.used - scan_begin;
      result.recognition_work = recognition_meter.used;
      result.status = recognition_failure(recognition_meter, capture.status);
      return result;
    }
    owned = std::move(capture.snapshot);
    owner = &owned;
    checked = owner->theory;
  } else {
    Gf3AffineLiveStatus retained_status = Gf3AffineLiveStatus::Invalid;
    if (owner->external_extent <= kernel.external_to_internal.size())
      retained_status = retained_owner_status(
          *owner, request, recognition_meter, checked);
    if (retained_status != Gf3AffineLiveStatus::Applied) {
      result.source_scan_work = recognition_meter.used - scan_begin;
      result.recognition_work = recognition_meter.used;
      result.status = retained_status;
      return result;
    }
  }
  result.source_scan_work = recognition_meter.used - scan_begin;
  result.recognition_work = recognition_meter.used;
  result.source_clauses = owner->sources.size();
  result.source_literals = owner->source_literals;
  result.equations = checked.equations.size();

  Meter planning_meter{kernel, request.maximum_planning_work};
  const auto planning_charge =
      [&](uint64_t amount) { return planning_meter.charge(amount); };
  gf3::PairEliminationPlan plan = gf3::plan_pair_elimination(
      checked, request.first_equation, request.second_equation,
      request.elimination, planning_charge);
  result.planning_work = planning_meter.used;
  result.planned_additions = plan.additions.size();
  if (plan.status != gf3::PlanStatus::Planned) {
    result.status = plan_failure(planning_meter, plan);
    return result;
  }
  result.consequence = plan.consequence;

  Meter duplicate_meter{kernel, request.maximum_duplicate_scan_work};
  std::set<ClauseKey> live;
  if (!collect_live_clauses(kernel, duplicate_meter, live)) {
    result.duplicate_scan_work = duplicate_meter.used;
    result.status = stopped_status(duplicate_meter,
                                   Gf3AffineLiveStatus::ResourceExhausted);
    return result;
  }
  result.duplicate_scan_work = duplicate_meter.used;
  std::vector<size_t> missing;
  std::vector<uint64_t> published_ids(plan.additions.size(), 0);
  for (size_t index = 0; index < plan.additions.size(); ++index)
    if (live.count(normalized(plan.additions[index].clause)))
      ++result.duplicate_additions;
    else
      missing.push_back(index);
  if (missing.empty()) {
    result.status = Gf3AffineLiveStatus::Duplicate;
    return result;
  }

  uint64_t proof_work = 0, proof_literals = 0, arena_words = 0;
  for (size_t index : missing) {
    const gf3::Addition &addition = plan.additions[index];
    const uint64_t amount = uint64_t{1} + addition.clause.size() +
                            addition.antecedents.size();
    if (amount > request.maximum_proof_work - proof_work ||
        addition.clause.size() >
            request.maximum_proof_literal_occurrences - proof_literals) {
      result.status = Gf3AffineLiveStatus::ResourceExhausted;
      return result;
    }
    proof_work += amount;
    proof_literals += addition.clause.size();
    if (addition.clause.size() >= 3) {
      const uint64_t words = addition.clause.size() + CLAUSE_HEADER_WORDS;
      if (words > request.maximum_arena_words - arena_words) {
        result.status = Gf3AffineLiveStatus::ResourceExhausted;
        return result;
      }
      arena_words += words;
    }
  }
  result.proof_work = proof_work;
  result.proof_literal_occurrences = proof_literals;
  result.arena_words = arena_words;
  if (missing.size() > request.maximum_proof_additions ||
      missing.size() > request.elimination.maximum_additions ||
      kernel.next_clause_id > uint64_t(INT64_MAX) - missing.size() ||
      kernel.arena.size() >= INVALID_REF ||
      arena_words >= INVALID_REF - kernel.arena.size()) {
    result.status = Gf3AffineLiveStatus::ResourceExhausted;
    return result;
  }

  uint64_t next_id = kernel.next_clause_id;
  for (size_t index : missing) published_ids[index] = ++next_id;
  Journal journal{request.maximum_journal_bytes, 0, {}};
  if (!render_journal(request, *owner, plan, published_ids, proof_work,
                      journal)) {
    result.journal_work = journal.work;
    result.status = Gf3AffineLiveStatus::ResourceExhausted;
    return result;
  }
  result.journal_work = journal.work;

  Meter revalidation_meter{kernel, request.maximum_revalidation_work};
  if (!revalidate_sources(kernel, *owner, revalidation_meter)) {
    result.revalidation_work = revalidation_meter.used;
    result.status = stopped_status(revalidation_meter,
                                   Gf3AffineLiveStatus::Stale);
    return result;
  }
  result.revalidation_work = revalidation_meter.used;
  if (kernel_terminating(kernel)) {
    result.status = Gf3AffineLiveStatus::Interrupted;
    return result;
  }

  // Commit starts here. Every recoverable proof, capacity, journal, source,
  // duplicate, and interruption check has completed without mutation.
  kernel.close_startup_certificate();
  result.published_clause_ids.reserve(missing.size());
  for (size_t index : missing) {
    const gf3::Addition &addition = plan.additions[index];
    std::vector<Lit> literals;
    literals.reserve(addition.clause.size());
    for (int32_t external : addition.clause) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
        std::abort();
      literals.push_back(literal);
    }
    const uint64_t id = ++kernel.next_clause_id;
    if (id != published_ids[index]) std::abort();
    if (kernel.proof)
      kernel.proof->add_rup(id, literals.data(), literals.size(), nullptr, 0);
    if (literals.size() == 2)
      kernel_add_binary_clause(kernel, literals[0], literals[1], id);
    else
      kernel_add_large_clause(kernel, literals.data(), literals.size(), true,
                              literals.size(), id);
    result.published_clause_ids.push_back(id);
  }
  if (std::fwrite(journal.bytes.data(), 1, journal.bytes.size(),
                  request.records) != journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();

  result.status = Gf3AffineLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.emitted_additions = missing.size();
  result.journal_bytes = journal.bytes.size();
  result.first_published_clause_id = result.published_clause_ids.front();
  result.last_published_clause_id = result.published_clause_ids.back();
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  result.reconstruction_witness_required = false;
  std::set<int32_t> semantic;
  for (uint32_t domain : plan.consequence.domains)
    semantic.insert(checked.domains[domain].values.begin(),
                    checked.domains[domain].values.end());
  result.semantic_variables.assign(semantic.begin(), semantic.end());
  return result;
}

} // namespace tactix

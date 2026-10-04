#include "rules/semantic_extension/affine_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace tactix {
namespace {
using affine_semantic_extension::External;
using affine_semantic_extension::ProofEvent;
using affine_semantic_extension::ProofPlan;

std::vector<External> key(std::vector<External> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

struct Journal {
  uint64_t limit;
  std::string bytes;
  bool append(const std::string &text) {
    if (text.size() > limit - bytes.size()) return false;
    bytes += text;
    return true;
  }
  bool clause(const std::string &prefix, const std::vector<External> &literals) {
    std::string line = prefix;
    for (size_t i = 0; i < literals.size(); ++i)
      line += (i ? "," : "") + std::to_string(literals[i]);
    line += "]}\n";
    return append(line);
  }
};

struct Replay {
  std::vector<uint64_t> ids;
  std::vector<uint64_t> deleted;
  std::vector<uint64_t> target_ids;
  uint64_t last_id = 0;
};

bool plan_replay(uint64_t first_id, const ProofPlan &plan, Replay &replay) {
  Replay result;
  result.last_id = first_id;
  result.ids.resize(plan.events.size());
  result.deleted.resize(plan.events.size());
  std::map<std::vector<External>, std::vector<uint64_t>> active;
  for (size_t i = 0; i < plan.events.size(); ++i) {
    const ProofEvent &event = plan.events[i];
    std::vector<External> clause(event.clause.lits.begin(),
                                 event.clause.lits.begin() + event.clause.size);
    auto &population = active[key(std::move(clause))];
    if (event.addition) {
      if (result.last_id == UINT64_MAX) return false;
      const uint64_t id = ++result.last_id;
      result.ids[i] = id;
      population.push_back(id);
    } else {
      if (population.empty()) return false;
      result.deleted[i] = population.back();
      population.pop_back();
    }
  }
  for (const auto &target : plan.targets) {
    const auto found = active.find(key(target));
    if (found == active.end() || found->second.empty()) return false;
    result.target_ids.push_back(found->second.back());
  }
  replay = std::move(result);
  return true;
}

bool snapshot_guard(const Kernel &k, const AffineLiveRequest &request) {
  return request.records && request.action_id && request.semantic_id &&
         request.maximum_snapshot_literals && request.maximum_snapshot_work &&
         request.maximum_proof_additions && request.maximum_journal_bytes &&
         !k.inconsistent && !k.level && !k.conflict_valid && !k.pending.valid &&
         !k.dense && !k.probing.recording && k.large_watches_connected &&
         kernel_propagation_fixpoint(k) &&
         !(k.proof && (k.proof->lrat() || k.proof->pb()));
}

const Clause *large_source_at(const Kernel &k, Ref wanted) {
  if (wanted == INVALID_REF || wanted >= k.arena.size()) return nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (ref == wanted) return clause;
    if (ref > wanted) return nullptr;
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > k.arena.size() - ref) return nullptr;
    ref += words;
  }
  return nullptr;
}

bool snapshot_payload_counters(const AffineLiveSnapshot &snapshot,
                               const AffineLiveRequest &request,
                               uint64_t &literals, uint64_t &work) {
  literals = work = 0;
  if (snapshot.premises.size() != snapshot.sources.size() ||
      snapshot.premises.size() > request.image.maximum_clauses)
    return false;
  for (size_t i = 0; i < snapshot.premises.size(); ++i) {
    const auto &premise = snapshot.premises[i];
    const auto &source = snapshot.sources[i];
    if (premise.occurrence != i + 1 || source.occurrence != i + 1 ||
        premise.literals.size() != source.clause.size() ||
        premise.literals.size() > request.maximum_snapshot_literals - literals)
      return false;
    const uint64_t charge = uint64_t(premise.literals.size()) + 1;
    if (charge > request.maximum_snapshot_work - work ||
        premise.literals != source.clause)
      return false;
    literals += premise.literals.size();
    work += charge;
  }
  return true;
}

bool same_clause(const Kernel &k, const Lit *literals, uint32_t size,
                 const std::vector<External> &external) {
  if (size != external.size()) return false;
  std::vector<External> current;
  current.reserve(size);
  for (uint32_t i = 0; i < size; ++i) {
    if (kernel_var_retired(k, var_of(literals[i]))) return false;
    current.push_back(kernel_externalize(k, literals[i]));
  }
  return key(std::move(current)) == key(external);
}

bool clause_covered(const Kernel &k, const std::vector<External> &clause) {
  for (External external : clause) {
    const Lit literal = kernel_internalize(k, external);
    if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal)))
      return false;
    if (k.value(literal) > 0) return true;
  }
  if (clause.empty()) return false;
  if (clause.size() == 1)
    return k.value(kernel_internalize(k, clause[0])) > 0;
  if (clause.size() == 2)
    return kernel_binary_live(k, kernel_internalize(k, clause[0]),
                              kernel_internalize(k, clause[1]));
  const auto wanted = key(clause);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *candidate = clause_at(k.arena, ref);
    ref += clause_actual_words(candidate);
    if (candidate->garbage() || candidate->size != clause.size()) continue;
    std::vector<External> current;
    current.reserve(candidate->size);
    for (Lit literal : *candidate)
      current.push_back(kernel_externalize(k, literal));
    if (key(std::move(current)) == wanted) return true;
  }
  return false;
}

bool append_journal(const AffineLiveRequest &request,
                    const AffineLiveSnapshot &snapshot,
                    const affine_semantic_extension::Image &image,
                    const affine_semantic_extension::Consequence &choice,
                    const ProofPlan &plan, const Replay &replay,
                    const std::vector<bool> &installed,
                    const std::vector<bool> &delete_uninstalled,
                    const AffineLiveResult &result, uint64_t root_conflict_id,
                    Journal &journal) {
  const std::string ids = "\"action\":" + std::to_string(request.action_id) +
                          ",\"semantic\":" +
                          std::to_string(request.semantic_id);
  if (!journal.append("{" + ids +
      ",\"kind\":\"affine-begin\",\"consequence\":" +
      std::to_string(request.consequence) +
      ",\"maximum_proof_additions\":" +
      std::to_string(request.maximum_proof_additions) +
      ",\"maximum_proof_deletions\":" +
      std::to_string(request.maximum_proof_deletions) +
      ",\"propagation\":\"deferred-controller\"" +
      ",\"projection\":\"identity-existing-variables\"}\n")) return false;
  std::set<uint64_t> selected(choice.premises.begin(), choice.premises.end());
  for (uint64_t occurrence : selected) {
    if (!occurrence || occurrence > snapshot.sources.size()) return false;
    const auto &source = snapshot.sources[occurrence - 1];
    const std::string prefix = "{" + ids +
        ",\"kind\":\"affine-source\",\"occurrence\":" +
        std::to_string(occurrence) + ",\"source_kind\":\"" +
        (source.kind == AffineSourceKind::Large ? "large" : "binary") +
        "\",\"source_id\":" + std::to_string(source.clause_id) +
        ",\"clause\":[";
    if (!journal.clause(prefix, source.clause)) return false;
  }
  for (size_t source_index : choice.sources) {
    if (source_index >= image.equations.size()) return false;
    const auto &equation = image.equations[source_index];
    std::string line = "{" + ids +
        ",\"kind\":\"affine-equation\",\"index\":" +
        std::to_string(source_index) + ",\"rhs\":" +
        (equation.rhs ? "true" : "false") + ",\"variables\":[";
    for (size_t i = 0; i < equation.variables.size(); ++i)
      line += (i ? "," : "") + std::to_string(equation.variables[i]);
    line += "],\"premises\":[";
    for (size_t i = 0; i < equation.premises.size(); ++i)
      line += (i ? "," : "") + std::to_string(equation.premises[i]);
    line += "]}\n";
    if (!journal.append(line)) return false;
  }
  std::string consequence = "{" + ids +
      ",\"kind\":\"affine-consequence\",\"rhs\":" +
      (choice.rhs ? "true" : "false") + ",\"variables\":[";
  for (size_t i = 0; i < choice.variables.size(); ++i)
    consequence += (i ? "," : "") + std::to_string(choice.variables[i]);
  consequence += "],\"sources\":[";
  for (size_t i = 0; i < choice.sources.size(); ++i)
    consequence += (i ? "," : "") + std::to_string(choice.sources[i]);
  consequence += "]}\n";
  if (!journal.append(consequence)) return false;
  for (size_t i = 0; i < plan.events.size(); ++i) {
    const auto &event = plan.events[i];
    std::vector<External> clause(event.clause.lits.begin(),
                                 event.clause.lits.begin() + event.clause.size);
    const std::string prefix = "{" + ids + ",\"kind\":\"" +
        (event.addition ? "affine-proof-rup" : "affine-proof-delete") +
        "\",\"id\":" +
        std::to_string(event.addition ? replay.ids[i] : replay.deleted[i]) +
        ",\"clause\":[";
    if (!journal.clause(prefix, clause)) return false;
  }
  for (size_t i = 0; i < plan.targets.size(); ++i) {
    if (!installed[i]) continue;
    const std::string prefix = "{" + ids +
        ",\"kind\":\"affine-publish\",\"id\":" +
        std::to_string(replay.target_ids[i]) + ",\"clause\":[";
    if (!journal.clause(prefix, plan.targets[i])) return false;
  }
  if (root_conflict_id && !journal.append(
          "{" + ids +
          ",\"kind\":\"affine-root-conflict\",\"id\":" +
          std::to_string(root_conflict_id) + ",\"clause\":[]}\n"))
    return false;
  for (size_t i = 0; i < plan.targets.size(); ++i) {
    if (!delete_uninstalled[i]) continue;
    const std::string prefix = "{" + ids +
        ",\"kind\":\"affine-proof-delete-uninstalled\",\"id\":" +
        std::to_string(replay.target_ids[i]) + ",\"clause\":[";
    if (!journal.clause(prefix, plan.targets[i])) return false;
  }
  return journal.append("{" + ids +
      ",\"kind\":\"affine-complete\",\"snapshot_clauses\":" +
      std::to_string(result.snapshot_clauses) +
      ",\"equations\":" + std::to_string(result.equations) +
      ",\"consequences\":" + std::to_string(result.consequences) +
      ",\"preflight_additions\":" +
      std::to_string(result.preflight_additions) +
      ",\"preflight_deletions\":" +
      std::to_string(result.preflight_deletions) +
      ",\"additions\":" + std::to_string(result.additions) +
      ",\"deletions\":" + std::to_string(result.deletions) +
      ",\"published\":" + std::to_string(result.published) +
      ",\"propagation_pending\":" +
      (result.propagation_pending ? "true" : "false") + "}\n");
}

} // namespace

bool capture_affine_live_snapshot(Kernel &k, const AffineLiveRequest &request,
                                  AffineLiveSnapshot &output) {
  AffineLiveSnapshot result;
  result.arena_generation = k.arena_generation;
  auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k) ||
        amount > request.maximum_snapshot_work - result.work)
      return false;
    result.work += amount;
    return true;
  };
  auto take = [&](AffineSourceKind kind, Ref ref, Lit first, Lit second,
                  uint64_t clause_id, const Lit *literals, uint32_t size) {
    if (result.premises.size() == request.image.maximum_clauses ||
        size > request.maximum_snapshot_literals - result.literals ||
        !charge(size + 1)) return false;
    AffineLiveSource source;
    source.occurrence = result.sources.size() + 1;
    source.kind = kind;
    source.ref = ref;
    source.first = first;
    source.second = second;
    source.clause_id = clause_id;
    source.clause.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      if (kernel_var_retired(k, var_of(literals[i]))) return false;
      source.clause.push_back(kernel_externalize(k, literals[i]));
    }
    result.literals += size;
    result.premises.push_back({source.occurrence, source.clause});
    result.sources.push_back(std::move(source));
    return true;
  };
  for (Ref ref = 0; ref < k.arena.size();) {
    const Ref current = ref;
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (!charge(1)) return false;
    if (!clause->garbage() && clause->size <= request.image.maximum_arity &&
        !take(AffineSourceKind::Large, current, INVALID_LIT, INVALID_LIT,
              clause->id(), clause->lits, clause->size)) return false;
  }
  std::set<uint64_t> binaries;
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!charge(1)) return false;
      if (!watch.binary() || first >= watch.other()) continue;
      if (!binaries.insert(kernel_binary_key(first, watch.other())).second)
        continue;
      const Lit pair[] = {first, watch.other()};
      if (!take(AffineSourceKind::Binary, INVALID_REF, first, watch.other(),
                kernel_binary_id(k, first, watch.other()), pair, 2))
        return false;
    }
  output = std::move(result);
  return true;
}

bool affine_live_sources_valid(const Kernel &k,
                               const AffineLiveSnapshot &snapshot,
                               const std::vector<uint64_t> &occurrences) {
  if (snapshot.arena_generation != k.arena_generation) return false;
  std::set<uint64_t> unique;
  for (uint64_t occurrence : occurrences) {
    if (!occurrence || occurrence > snapshot.sources.size()) return false;
    if (!unique.insert(occurrence).second) continue;
    const AffineLiveSource &source = snapshot.sources[occurrence - 1];
    if (source.occurrence != occurrence) return false;
    if (source.kind == AffineSourceKind::Large) {
      const Clause *clause = large_source_at(k, source.ref);
      if (!clause) return false;
      if (clause->garbage() || clause->id() != source.clause_id ||
          !same_clause(k, clause->lits, clause->size, source.clause))
        return false;
    } else if (source.kind == AffineSourceKind::Binary) {
      if (source.first == INVALID_LIT || source.second == INVALID_LIT ||
          var_of(source.first) >= k.nvars || var_of(source.second) >= k.nvars ||
          kernel_var_retired(k, var_of(source.first)) ||
          kernel_var_retired(k, var_of(source.second)) ||
          !kernel_binary_live(k, source.first, source.second) ||
          kernel_binary_id(k, source.first, source.second) != source.clause_id)
        return false;
      const Lit pair[] = {source.first, source.second};
      if (!same_clause(k, pair, 2, source.clause)) return false;
    } else return false;
  }
  return true;
}

AffineLiveResult derive_affine_live(Kernel &k,
                                    const AffineLiveRequest &request) {
  AffineLiveResult result;
  if (!snapshot_guard(k, request) || kernel_terminating(k)) return result;
  AffineLiveSnapshot owned;
  const AffineLiveSnapshot *snapshot = request.snapshot;
  if (!snapshot) {
    if (!capture_affine_live_snapshot(k, request, owned)) return result;
    snapshot = &owned;
  }
  // Ref identity is generation-scoped. Reject a cached view before doing any
  // semantic work; selected large sources are boundary-checked again just
  // before proof publication.
  if (snapshot->arena_generation != k.arena_generation) return result;
  uint64_t payload_literals = 0, payload_work = 0;
  if (!snapshot_payload_counters(*snapshot, request, payload_literals,
                                 payload_work))
    return result;
  result.snapshot_clauses = snapshot->premises.size();
  result.snapshot_literals = payload_literals;
  // A freshly captured view reports its full arena/watch scan. Reusing a
  // caller-owned view charges a deterministic replay of every cached payload;
  // its stored counters are advisory and cannot bypass this action's limits.
  result.snapshot_work = request.snapshot ? payload_work : snapshot->work;
  if (result.snapshot_work > request.maximum_snapshot_work) return result;

  affine_semantic_extension::Image image;
  uint64_t recognition_work = 0;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k) ||
        amount > request.image.maximum_elimination_work - recognition_work)
      return false;
    recognition_work += amount;
    return true;
  };
  result.recognition = affine_semantic_extension::recognize(
      snapshot->premises, request.image, charge, image);
  result.recognition_work = recognition_work;
  if (result.recognition != affine_semantic_extension::Status::Complete)
    return result;
  result.equations = image.equations.size();
  result.consequences = image.consequences.size();
  if (request.consequence >= image.consequences.size()) return result;
  const auto &choice = image.consequences[request.consequence];
  result.selected_sources = choice.sources.size();
  result.selected_premises = choice.premises.size();
  result.semantic_variables = choice.variables;
  result.rhs = choice.rhs;
  result.contradiction = choice.variables.empty() && choice.rhs;
  if (!affine_live_sources_valid(k, *snapshot, choice.premises)) return result;

  ProofPlan plan;
  if (!affine_semantic_extension::build_proof_plan(
          image, request.consequence, request.maximum_proof_additions, plan))
    return result;
  if (plan.additions > request.maximum_proof_additions) return result;
  std::vector<bool> missing;
  uint64_t arena_words = 0;
  for (const auto &target : plan.targets) {
    const bool needed = !clause_covered(k, target);
    missing.push_back(needed);
    if (needed && target.size() > 2) {
      if (target.size() + CLAUSE_HEADER_WORDS > INVALID_REF - arena_words)
        return result;
      arena_words += target.size() + CLAUSE_HEADER_WORDS;
    }
  }
  if (std::none_of(missing.begin(), missing.end(), [](bool value) { return value; }))
    return result;
  if (arena_words >= INVALID_REF - k.arena.size()) return result;

  Replay replay;
  if (!plan_replay(k.next_clause_id, plan, replay)) return result;
  bool false_unit = false;
  std::vector<bool> installed(plan.targets.size(), false);
  std::vector<bool> delete_uninstalled(plan.targets.size(), false);
  uint64_t planned_published = 0, cleanup_deletions = 0;
  bool propagation_pending = false;
  for (size_t i = 0; i < plan.targets.size(); ++i) {
    if (!missing[i]) {
      delete_uninstalled[i] = true;
      ++cleanup_deletions;
      continue;
    }
    const auto &target = plan.targets[i];
    if (target.empty()) continue;
    if (target.size() == 1) {
      const Lit literal = kernel_internalize(k, target[0]);
      if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal)))
        return result;
      if (k.value(literal) < 0) {
        if (k.proof && !k.unit_ids[var_of(literal)]) return result;
        false_unit = true;
        // The derived unit is proof-only: it supports the preflighted empty
        // clause and is retired after that terminal addition.
        delete_uninstalled[i] = true;
        ++cleanup_deletions;
        continue;
      }
      propagation_pending |= !k.value(literal);
    } else {
      for (External external : target) {
        const Lit literal = kernel_internalize(k, external);
        if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal)))
          return result;
        propagation_pending |= k.value(literal) < 0;
      }
    }
    installed[i] = true;
    ++planned_published;
  }
  const uint64_t total_additions = plan.additions + uint64_t(false_unit);
  if (total_additions > request.maximum_proof_additions ||
      replay.last_id > uint64_t(INT64_MAX) - uint64_t(false_unit)) return result;
  if (cleanup_deletions > UINT64_MAX - plan.deletions) return result;
  const uint64_t total_deletions = plan.deletions + cleanup_deletions;
  if (total_deletions > request.maximum_proof_deletions) return result;
  result.preflight_additions = total_additions;
  result.preflight_deletions = total_deletions;
  Journal journal{request.maximum_journal_bytes, {}};
  AffineLiveResult journal_result = result;
  journal_result.additions = total_additions;
  journal_result.deletions = total_deletions;
  journal_result.published = planned_published;
  journal_result.propagation_pending = !false_unit && propagation_pending;
  if (!append_journal(request, *snapshot, image, choice, plan, replay,
                      installed, delete_uninstalled, journal_result,
                      false_unit ? replay.last_id + 1 : 0, journal))
    return result;
  if (kernel_terminating(k) ||
      !affine_live_sources_valid(k, *snapshot, choice.premises)) return result;

  k.close_startup_certificate();
  for (size_t i = 0; i < plan.events.size(); ++i) {
    const auto &event = plan.events[i];
    const External *literals = event.clause.lits.data();
    if (event.addition) {
      if (++k.next_clause_id != replay.ids[i]) std::abort();
      if (k.proof && !k.proof->add_external_rup(
                         k.next_clause_id, literals, event.clause.size))
        std::abort();
    } else if (k.proof && !k.proof->del_external(
                                  k.next_clause_id, replay.deleted[i],
                                  literals, event.clause.size))
      std::abort();
  }
  for (size_t i = 0; i < plan.targets.size(); ++i) {
    if (!installed[i]) continue;
    const auto &target = plan.targets[i];
    std::vector<Lit> literals;
    literals.reserve(target.size());
    for (External external : target) {
      const Lit literal = kernel_internalize(k, external);
      if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal)))
        std::abort();
      literals.push_back(literal);
    }
    const uint64_t id = replay.target_ids[i];
    if (literals.size() == 1) {
      if (!k.value(literals[0])) {
        k.unit_ids[var_of(literals[0])] = id;
        kernel_assign_unit(k, literals[0]);
      }
    } else if (literals.size() == 2) {
      kernel_add_binary_clause(k, literals[0], literals[1], id);
    } else {
      kernel_add_large_clause(k, literals.data(), literals.size(), true,
                              literals.size(), id);
    }
  }
  if (result.contradiction) k.inconsistent = true;
  if (false_unit) {
    if (++k.next_clause_id > uint64_t(INT64_MAX)) std::abort();
    if (k.proof && !k.proof->add_external_rup(k.next_clause_id, nullptr, 0))
      std::abort();
    k.inconsistent = true;
  }
  if (k.proof)
    for (size_t i = 0; i < plan.targets.size(); ++i) {
      if (!delete_uninstalled[i]) continue;
      const auto &target = plan.targets[i];
      if (!k.proof->del_external(
              k.next_clause_id, replay.target_ids[i],
              target.empty() ? nullptr : target.data(), target.size()))
        std::abort();
    }
  result.additions = total_additions;
  result.deletions = total_deletions;
  result.published = planned_published;
  result.result = k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
  result.propagation_pending =
      !k.inconsistent && !kernel_propagation_fixpoint(k);
  if (result.propagation_pending != journal_result.propagation_pending)
    std::abort();
  if (std::fwrite(journal.bytes.data(), 1, journal.bytes.size(), request.records) !=
          journal.bytes.size() ||
      std::fflush(request.records) || std::ferror(request.records)) std::abort();
  return result;
}

} // namespace tactix

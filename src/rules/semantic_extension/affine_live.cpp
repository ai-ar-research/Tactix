#include "rules/semantic_extension/affine_live.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <bit>
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
  uint64_t work = 0;
  bool append(const std::string &text) {
    if (text.size() > UINT64_MAX - work) {
      work = UINT64_MAX;
      return false;
    }
    work += text.size();
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

bool request_valid(const AffineLiveRequest &request) {
  return request.records && request.action_id && request.semantic_id &&
         request.image.maximum_arity <= 8 &&
         request.image.maximum_intermediate_width <= 16 &&
         request.image.maximum_output_width <= 4;
}

bool request_has_work(const AffineLiveRequest &request) {
  return request.image.maximum_clauses && request.image.maximum_groups &&
         request.image.maximum_patterns &&
         request.image.maximum_elimination_work &&
         request.image.maximum_arity &&
         request.image.maximum_intermediate_width &&
         request.image.maximum_sources && request.maximum_snapshot_literals &&
         request.maximum_snapshot_work && request.maximum_proof_additions &&
         request.maximum_journal_bytes;
}

bool state_applicable(const Kernel &k) {
  return !k.inconsistent && !k.level && !k.conflict_valid && !k.pending.valid &&
         !k.dense && !k.probing.recording && k.large_watches_connected &&
         kernel_propagation_fixpoint(k);
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

enum class PayloadStatus { Complete, Invalid, ResourceExhausted };

PayloadStatus snapshot_payload_counters(const AffineLiveSnapshot &snapshot,
                                        const AffineLiveRequest &request,
                                        uint64_t &literals, uint64_t &work) {
  literals = work = 0;
  if (snapshot.premises.size() != snapshot.sources.size() ||
      snapshot.premises.size() > request.image.maximum_clauses)
    return snapshot.premises.size() > request.image.maximum_clauses
               ? PayloadStatus::ResourceExhausted
               : PayloadStatus::Invalid;
  for (size_t i = 0; i < snapshot.premises.size(); ++i) {
    const auto &premise = snapshot.premises[i];
    const auto &source = snapshot.sources[i];
    if (premise.occurrence != i + 1 || source.occurrence != i + 1 ||
        premise.literals.size() != source.clause.size() ||
        premise.literals != source.clause)
      return PayloadStatus::Invalid;
    if (premise.literals.size() >
        request.maximum_snapshot_literals - literals)
      return PayloadStatus::ResourceExhausted;
    const uint64_t charge = uint64_t(premise.literals.size()) + 1;
    if (charge > request.maximum_snapshot_work - work)
      return PayloadStatus::ResourceExhausted;
    literals += premise.literals.size();
    work += charge;
  }
  return PayloadStatus::Complete;
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

bool proof_plan_supported(
    const affine_semantic_extension::Image &image,
    const affine_semantic_extension::Consequence &choice) {
  if (choice.variables.size() > 4 || choice.sources.size() < 2)
    return false;
  std::set<External> variables;
  for (size_t source : choice.sources) {
    if (source >= image.equations.size()) return false;
    const auto &equation = image.equations[source];
    if (equation.premises.size() != equation.premise_clauses.size())
      return false;
    for (const auto &clause : equation.premise_clauses) {
      if (clause.size() > 8) return false;
      for (External literal : clause) {
        if (!literal || literal == INT32_MIN) return false;
        variables.insert(literal < 0 ? -literal : literal);
      }
    }
  }
  return variables.size() <= 16;
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

namespace {

enum class CaptureStatus { Complete, Invalid, ResourceExhausted, Interrupted };

CaptureStatus capture_snapshot(Kernel &k, const AffineLiveRequest &request,
                               AffineLiveSnapshot &output,
                               AffineLiveResult *effort) {
  AffineLiveSnapshot result;
  result.arena_generation = k.arena_generation;
  CaptureStatus failure = CaptureStatus::Invalid;
  const auto finish = [&](CaptureStatus status) {
    if (effort) {
      effort->snapshot_clauses = result.premises.size();
      effort->snapshot_literals = result.literals;
      effort->snapshot_work = result.work;
    }
    if (status == CaptureStatus::Complete) output = std::move(result);
    return status;
  };
  auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k)) {
      failure = CaptureStatus::Interrupted;
      return false;
    }
    if (amount > request.maximum_snapshot_work - result.work) {
      failure = CaptureStatus::ResourceExhausted;
      return false;
    }
    result.work += amount;
    return true;
  };
  uint64_t reserved_bytes = sizeof(AffineLiveSnapshot);
  auto take = [&](AffineSourceKind kind, Ref ref, Lit first, Lit second,
                  uint64_t clause_id, const Lit *literals, uint32_t size) {
    if (result.premises.size() == request.image.maximum_clauses ||
        size > request.maximum_snapshot_literals - result.literals) {
      failure = CaptureStatus::ResourceExhausted;
      return false;
    }
    if (!charge(size + 1)) return false;
    const uint64_t bytes = 4 * (sizeof(AffineLiveSource) + sizeof(affine_semantic_extension::Premise)) +
        4 * uint64_t(size) * sizeof(int32_t);
    if (request.maximum_snapshot_bytes && (reserved_bytes > request.maximum_snapshot_bytes ||
        bytes > request.maximum_snapshot_bytes - reserved_bytes)) {
      failure = CaptureStatus::ResourceExhausted; return false;
    }
    reserved_bytes += bytes;
    AffineLiveSource source;
    source.occurrence = result.sources.size() + 1;
    source.kind = kind;
    source.ref = ref;
    source.first = first;
    source.second = second;
    source.clause_id = clause_id;
    source.proof_id_exact = kind != AffineSourceKind::Binary || k.binary_ids.tracks_values();
    source.clause.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      if (kernel_var_retired(k, var_of(literals[i]))) {
        failure = CaptureStatus::Invalid;
        return false;
      }
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
    if (!charge(1)) return finish(failure);
    bool eligible=true;
    if(request.discovered_through_clause_id && clause->id()<=request.discovered_through_clause_id) {
      if(!charge(1+(request.pending_large_ids?std::bit_width(request.pending_large_ids->size()):0))) return finish(failure);
      eligible=request.pending_large_ids&&std::binary_search(request.pending_large_ids->begin(),request.pending_large_ids->end(),clause->id());
    }
    if (eligible && !clause->garbage() && (request.full_root_view || clause->size <= request.image.maximum_arity) &&
        !take(AffineSourceKind::Large, current, INVALID_LIT, INVALID_LIT,
              clause->id(), clause->lits, clause->size))
      return finish(failure);
  }
  std::set<uint64_t> binaries;
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!charge(1)) return finish(failure);
      if (!watch.binary() || first >= watch.other()) continue;
      if(request.known_binary_bodies) {
        if(!charge(3+std::bit_width(request.known_binary_bodies->size())+
            (request.pending_binary_bodies?std::bit_width(request.pending_binary_bodies->size()):0))) return finish(failure);
        const auto identity=affine_binary_body_identity(kernel_externalize(k,first),kernel_externalize(k,watch.other()));
        if(std::binary_search(request.known_binary_bodies->begin(),request.known_binary_bodies->end(),identity)&&
            !(request.pending_binary_bodies&&std::binary_search(request.pending_binary_bodies->begin(),request.pending_binary_bodies->end(),identity))) continue;
      }
      const auto native_key=kernel_binary_key(first,watch.other());
      if(request.maximum_snapshot_bytes) {
        if(!charge(2*(1+std::bit_width(binaries.size()))))return finish(failure);
        if(binaries.contains(native_key))continue;
        // The four source-metadata populations also cover this unique-pair
        // tree node (three links/color/alignment) and its allocator overhead.
        // Reserve the next source envelope before allocating the node.
        const uint64_t bytes=4*(sizeof(AffineLiveSource)+sizeof(affine_semantic_extension::Premise))+8*sizeof(int32_t);
        if(reserved_bytes>request.maximum_snapshot_bytes||bytes>request.maximum_snapshot_bytes-reserved_bytes) {
          failure=CaptureStatus::ResourceExhausted;return finish(failure);
        }
        binaries.insert(native_key);
      } else if(!binaries.insert(native_key).second)continue;
      const Lit pair[] = {first, watch.other()};
      if (!take(AffineSourceKind::Binary, INVALID_REF, first, watch.other(),
                kernel_binary_id(k, first, watch.other()), pair, 2))
        return finish(failure);
    }
  if (request.full_root_view) {
    for (Var v = 0; v < k.nvars; ++v) {
      if (!charge(1)) return finish(failure);
      const Lit positive = make_lit(v, false);
      if (kernel_var_retired(k,v) || !k.values[positive] || k.assigned[v].level) continue;
      const Lit literal = k.values[positive] > 0 ? positive : negate(positive);
      const uint64_t unit_id = k.unit_ids[v];
      const bool recorded = k.proof && k.proof->dsr() && k.proof->stream_identity() &&
          k.dsr_root_proof==k.proof && k.dsr_root_stream_identity==k.proof->stream_identity() &&
          v < k.dsr_root_values.size() &&
          k.dsr_root_values[v] == (negated(literal) ? -1 : 1);
      if(!unit_id&&!recorded) {
        const auto &assigned=k.assigned[v];
        if(assigned.reason==UNIT_REASON||assigned.reason==DECISION_REASON)continue;
        if(assigned.binary) {
          if(assigned.reason>=2*k.nvars||!kernel_binary_live(k,literal,assigned.reason))continue;
          const auto identity=kernel_binary_key(literal,assigned.reason);
          if(!charge(2*(1+std::bit_width(binaries.size()))))return finish(failure);
          if(binaries.contains(identity))continue;
          const uint64_t bytes=4*(sizeof(AffineLiveSource)+sizeof(affine_semantic_extension::Premise))+8*sizeof(int32_t);
          if(request.maximum_snapshot_bytes&&(reserved_bytes>request.maximum_snapshot_bytes||bytes>request.maximum_snapshot_bytes-reserved_bytes)) {
            failure=CaptureStatus::ResourceExhausted;return finish(failure);
          }
          binaries.insert(identity);
          const Lit pair[]={literal,assigned.reason};
          if(!take(AffineSourceKind::Binary,INVALID_REF,literal,assigned.reason,
              kernel_binary_id(k,literal,assigned.reason),pair,2))return finish(failure);
        } else {
          if(assigned.reason>=k.arena.size())continue;
          if(!charge(1+result.sources.size()))return finish(failure);
          const Clause *reason=clause_at(k.arena,assigned.reason);
          if(reason->garbage())continue;
          bool present=false;
          for(const auto &source:result.sources)
            if(source.kind==AffineSourceKind::Large&&source.ref==assigned.reason) {present=true;break;}
          if(present)continue;
          if(!take(AffineSourceKind::Large,assigned.reason,INVALID_LIT,INVALID_LIT,
              reason->id(),reason->lits,reason->size))return finish(failure);
        }
        result.sources.back().recognition_candidate=false;
        continue;
      }
      if (!take(AffineSourceKind::RootUnit, INVALID_REF, literal, INVALID_LIT,
                unit_id, &literal, 1)) return finish(failure);
      result.sources.back().proof_id_exact = unit_id != 0;
    }
  }
  return finish(CaptureStatus::Complete);
}

} // namespace

bool capture_affine_live_snapshot(Kernel &k, const AffineLiveRequest &request,
                                  AffineLiveSnapshot &output) {
  return capture_snapshot(k, request, output, nullptr) ==
         CaptureStatus::Complete;
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
    } else if (source.kind == AffineSourceKind::RootUnit) {
      const Lit literal = source.first;
      if (literal == INVALID_LIT || var_of(literal) >= k.nvars ||
          k.values[literal] <= 0 || k.assigned[var_of(literal)].level ||
          !same_clause(k, &literal, 1, source.clause)) return false;
      if (source.proof_id_exact) {
        if (!source.clause_id || k.unit_ids[var_of(literal)] != source.clause_id) return false;
      } else if (!k.proof || !k.proof->dsr() || !k.proof->stream_identity() ||
          k.dsr_root_proof!=k.proof || k.dsr_root_stream_identity!=k.proof->stream_identity() ||
          var_of(literal) >= k.dsr_root_values.size() ||
          k.dsr_root_values[var_of(literal)] != (negated(literal) ? -1 : 1)) return false;
    } else return false;
  }
  return true;
}

AffineLiveResult derive_affine_live(Kernel &k,
                                    const AffineLiveRequest &request) {
  AffineLiveResult result;
  if (!request_valid(request)) return result;
  if (!request_has_work(request)) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  if (k.proof && (k.proof->lrat() || k.proof->pb())) {
    result.status = AffineLiveStatus::UnsupportedProof;
    return result;
  }
  if (!state_applicable(k)) {
    result.status = AffineLiveStatus::Inapplicable;
    return result;
  }
  if (kernel_terminating(k)) {
    result.status = AffineLiveStatus::Interrupted;
    return result;
  }
  AffineLiveSnapshot owned;
  const AffineLiveSnapshot *snapshot = request.snapshot;
  if (!snapshot) {
    const CaptureStatus captured = capture_snapshot(k, request, owned, &result);
    if (captured != CaptureStatus::Complete) {
      result.status = captured == CaptureStatus::Interrupted
                          ? AffineLiveStatus::Interrupted
                      : captured == CaptureStatus::ResourceExhausted
                          ? AffineLiveStatus::ResourceExhausted
                          : AffineLiveStatus::Invalid;
      return result;
    }
    snapshot = &owned;
  }
  // Ref identity is generation-scoped. Reject a cached view before doing any
  // semantic work; selected large sources are boundary-checked again just
  // before proof publication.
  if (snapshot->arena_generation != k.arena_generation) {
    result.status = AffineLiveStatus::Stale;
    return result;
  }
  uint64_t payload_literals = 0, payload_work = 0;
  const PayloadStatus payload = snapshot_payload_counters(
      *snapshot, request, payload_literals, payload_work);
  result.snapshot_clauses = snapshot->premises.size();
  result.snapshot_literals = payload_literals;
  if (payload != PayloadStatus::Complete) {
    result.snapshot_work = request.snapshot ? payload_work : snapshot->work;
    result.status = payload == PayloadStatus::ResourceExhausted
                        ? AffineLiveStatus::ResourceExhausted
                        : AffineLiveStatus::Invalid;
    return result;
  }
  // A freshly captured view reports its full arena/watch scan. Reusing a
  // caller-owned view charges a deterministic replay of every cached payload;
  // its stored counters are advisory and cannot bypass this action's limits.
  result.snapshot_work = request.snapshot ? payload_work : snapshot->work;
  if (result.snapshot_work > request.maximum_snapshot_work) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }

  affine_semantic_extension::Image image;
  uint64_t recognition_work = 0;
  bool recognition_interrupted = false, recognition_exhausted = false;
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k)) {
      recognition_interrupted = true;
      return false;
    }
    if (amount >
        request.image.maximum_elimination_work - recognition_work) {
      recognition_exhausted = true;
      return false;
    }
    recognition_work += amount;
    return true;
  };
  result.recognition = affine_semantic_extension::recognize(
      snapshot->premises, request.image, charge, image);
  result.recognition_work = recognition_work;
  if (result.recognition != affine_semantic_extension::Status::Complete) {
    using affine_semantic_extension::Status;
    if (recognition_interrupted || result.recognition == Status::Interrupted)
      result.status = recognition_exhausted
                          ? AffineLiveStatus::ResourceExhausted
                          : AffineLiveStatus::Interrupted;
    else if (result.recognition == Status::Exhausted)
      result.status = AffineLiveStatus::ResourceExhausted;
    else if (result.recognition == Status::NoStructure)
      result.status = AffineLiveStatus::NoStructure;
    else
      result.status = AffineLiveStatus::Invalid;
    return result;
  }
  result.equations = image.equations.size();
  result.consequences = image.consequences.size();
  if (request.consequence >= image.consequences.size()) {
    result.status = AffineLiveStatus::NoStructure;
    return result;
  }
  const auto &choice = image.consequences[request.consequence];
  result.selected_sources = choice.sources.size();
  result.selected_premises = choice.premises.size();
  result.semantic_variables = choice.variables;
  result.rhs = choice.rhs;
  result.contradiction = choice.variables.empty() && choice.rhs;
  if (!affine_live_sources_valid(k, *snapshot, choice.premises)) {
    result.status = AffineLiveStatus::Stale;
    return result;
  }

  ProofPlan plan;
  if (!proof_plan_supported(image, choice)) {
    result.status = AffineLiveStatus::UnsupportedProof;
    return result;
  }
  if (!affine_semantic_extension::build_proof_plan(
          image, request.consequence, request.maximum_proof_additions, plan)) {
    // After the structural support check above, the bounded planning sink can
    // fail only when it has filled the caller's complete addition quota.
    result.proof_addition_work = request.maximum_proof_additions;
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  result.proof_addition_work = plan.additions;
  if (plan.additions > request.maximum_proof_additions) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  std::vector<bool> missing;
  uint64_t arena_words = 0;
  for (const auto &target : plan.targets) {
    const bool needed = !clause_covered(k, target);
    missing.push_back(needed);
    if (needed && target.size() > 2) {
      if (target.size() + CLAUSE_HEADER_WORDS > INVALID_REF - arena_words) {
        result.status = AffineLiveStatus::ResourceExhausted;
        return result;
      }
      arena_words += target.size() + CLAUSE_HEADER_WORDS;
    }
  }
  if (std::none_of(missing.begin(), missing.end(),
                   [](bool value) { return value; })) {
    result.status = AffineLiveStatus::NoStructure;
    return result;
  }
  if (arena_words >= INVALID_REF - k.arena.size()) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }

  if (plan.additions > UINT64_MAX - k.next_clause_id) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
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
      if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal))) {
        result.status = AffineLiveStatus::Stale;
        return result;
      }
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
        if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal))) {
          result.status = AffineLiveStatus::Stale;
          return result;
        }
        propagation_pending |= k.value(literal) < 0;
      }
    }
    installed[i] = true;
    ++planned_published;
  }
  const uint64_t total_additions = plan.additions + uint64_t(false_unit);
  if (total_additions > request.maximum_proof_additions ||
      replay.last_id > uint64_t(INT64_MAX) - uint64_t(false_unit)) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  if (cleanup_deletions > UINT64_MAX - plan.deletions) {
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  const uint64_t total_deletions = plan.deletions + cleanup_deletions;
  if (total_deletions > request.maximum_proof_deletions) {
    result.preflight_additions = total_additions;
    result.preflight_deletions = total_deletions;
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
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
                      false_unit ? replay.last_id + 1 : 0, journal)) {
    result.journal_work = journal.work;
    result.status = AffineLiveStatus::ResourceExhausted;
    return result;
  }
  result.journal_work = journal.work;
  if (kernel_terminating(k)) {
    result.status = AffineLiveStatus::Interrupted;
    return result;
  }
  if (!affine_live_sources_valid(k, *snapshot, choice.premises)) {
    result.status = AffineLiveStatus::Stale;
    return result;
  }

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
  result.journal_bytes = journal.bytes.size();
  result.status = AffineLiveStatus::Applied;
  return result;
}

} // namespace tactix

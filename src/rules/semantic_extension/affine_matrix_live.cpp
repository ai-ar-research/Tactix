#include "rules/semantic_extension/affine_matrix_live.hpp"

#include "kernel/clause.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <set>

namespace tactix::affine_semantic_extension {
namespace {

struct Work {
  uint64_t used = 0;
  uint64_t maximum = 0;
  Kernel &kernel;
  bool interrupted = false;
  bool exhausted = false;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel)) {
      interrupted = true;
      return false;
    }
    if (used > maximum || amount > maximum - used) {
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }
};

bool valid(const MatrixLiveRequest &request) {
  return request.owner && request.matrix && request.exact_sources &&
         request.event && request.owner_identity &&
         request.transaction.maximum_work &&
         request.transaction.maximum_proof_additions &&
         request.transaction.maximum_proof_deletions &&
         request.transaction.maximum_helper_extent &&
         request.maximum_revalidation_work && request.maximum_journal_bytes &&
         request.maximum_external_extent && request.action_id &&
         request.semantic_id && request.records;
}

bool applicable(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.conflict_valid &&
         !kernel.pending.valid && !kernel.dense && !kernel.probing.recording &&
         kernel.large_watches_connected && kernel_propagation_fixpoint(kernel);
}

bool same_equation(const Equation &first, const Equation &second) {
  return first.variables == second.variables && first.rhs == second.rhs &&
         first.premises == second.premises &&
         first.premise_clauses == second.premise_clauses;
}

bool same_event(const MatrixEvent &first, const MatrixEvent &second) {
  return first.kind == second.kind && first.row == second.row &&
         first.implied_variable == second.implied_variable &&
         first.implied_value == second.implied_value &&
         first.reason == second.reason;
}

bool same_request(const MatrixLiveRequest &first,
                  const MatrixLiveRequest &second) {
  return first.owner == second.owner && first.matrix == second.matrix &&
         first.exact_sources == second.exact_sources &&
         first.event == second.event &&
         first.owner_identity == second.owner_identity &&
         first.transaction == second.transaction &&
         first.maximum_revalidation_work == second.maximum_revalidation_work &&
         first.maximum_journal_bytes == second.maximum_journal_bytes &&
         first.maximum_external_extent == second.maximum_external_extent &&
         first.action_id == second.action_id &&
         first.semantic_id == second.semantic_id &&
         first.records == second.records &&
         first.compact_provenance == second.compact_provenance &&
         first.retained_source_provenance ==
             second.retained_source_provenance &&
         first.proof_cache == second.proof_cache &&
         first.pbp_parity_cache == second.pbp_parity_cache &&
         first.premise_leases == second.premise_leases &&
         first.maximum_persistent_bytes == second.maximum_persistent_bytes;
}

bool snapshot_trail(const Kernel &kernel,
                    std::vector<MatrixAssignment> &assignments,
                    std::vector<uint32_t> *levels) {
  assignments.clear();
  if (levels) levels->clear();
  assignments.reserve(kernel.trail.size());
  if (levels) levels->reserve(kernel.trail.size());
  for (Lit literal : kernel.trail) {
    const int32_t external = kernel_externalize(kernel, literal);
    if (!external || external == INT32_MIN) return false;
    const int32_t variable = external < 0 ? -external : external;
    assignments.push_back({variable, external > 0});
    const Assigned &assigned = kernel.assigned[var_of(literal)];
    if (assigned.trail + 1 != assignments.size()) return false;
    if (levels) levels->push_back(assigned.level);
  }
  return true;
}

bool snapshot_support(const Kernel &kernel,
                      const std::vector<External> &support,
                      std::vector<MatrixAssignment> &assignments) {
  assignments.clear();
  assignments.reserve(support.size());
  for (const External variable : support) {
    if (variable <= 0 || variable == INT32_MIN) return false;
    const int8_t value =
        kernel_external_value(kernel, internalize(variable));
    if (value) assignments.push_back({variable, value > 0});
  }
  return true;
}

bool same_clause_body(const std::vector<int32_t> &first,
                      const std::vector<int32_t> &second) {
  // A prepared matrix transaction accepts only bounded parity equations, so
  // comparing their premise clauses directly avoids two heap copies and sorts
  // per live premise while retaining exact multiset identity.
  return first.size() == second.size() &&
         std::is_permutation(first.begin(), first.end(), second.begin());
}

bool validate_selected_sources(
    const Kernel &kernel, const AffineLiveSnapshot &owner,
    const std::vector<Equation> &current_sources,
    const std::vector<size_t> &source_equations,
    const std::vector<Equation> &source_identity, Work &work,
    const std::vector<uint64_t> *leases, std::vector<uint64_t> &occurrences, std::vector<uint64_t> &clause_ids) {
  occurrences.clear();
  clause_ids.clear();
  if (source_equations.size() != source_identity.size())
    return false;
  std::set<uint64_t> unique;
  for (size_t at = 0; at < source_equations.size(); ++at) {
    if (!work.charge(1)) return false;
    const size_t ordinal = source_equations[at];
    if (ordinal >= current_sources.size()) return false;
    const Equation &identity = source_identity[at];
    if (leases) {
      if (!work.charge(1 + identity.premise_clauses.size())) return false;
      for (const auto &clause : identity.premise_clauses)
        if (!work.charge(1 + uint64_t(clause.size()) * clause.size())) return false;
    }
    if (leases && (!kernel.proof || ordinal >= leases->size() ||
        !kernel.proof->premise_group_matches((*leases)[ordinal], identity.premise_clauses)))
      return false;
    if (!same_equation(current_sources[ordinal], identity) ||
        identity.premises.size() != identity.premise_clauses.size())
      return false;
    for (size_t i = 0; i < identity.premises.size(); ++i) {
      if (!work.charge(2 + identity.premise_clauses[i].size())) return false;
      const uint64_t occurrence = identity.premises[i];
      if (!occurrence || occurrence > owner.sources.size() ||
          !unique.insert(occurrence).second)
        return false;
      const AffineLiveSource &source = owner.sources[occurrence - 1];
      if (source.occurrence != occurrence || !source.clause_id ||
          !same_clause_body(source.clause, identity.premise_clauses[i]))
        return false;
      occurrences.push_back(occurrence);
      clause_ids.push_back(source.clause_id);
    }
  }
  if (!work.charge(occurrences.size())) return false;
  return leases || affine_live_sources_valid(kernel, owner, occurrences);
}

bool reason_order(const Kernel &kernel, const MatrixEvent &event,
                  std::vector<Lit> &output) {
  output.clear();
  if (event.kind == MatrixEventKind::None) return false;
  // A contradiction with no assigned support is the globally valid empty
  // clause.  The transaction still has to derive it from the authenticated
  // source equations before publication.
  if (event.reason.empty())
    return event.kind == MatrixEventKind::Conflict;
  std::set<Lit> unique;
  size_t open = SIZE_MAX;
  for (int32_t external : event.reason) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)) ||
        !unique.insert(literal).second || kernel.value(literal) > 0)
      return false;
    if (!kernel.value(literal)) {
      if (open != SIZE_MAX) return false;
      open = output.size();
    }
    output.push_back(literal);
  }
  if (event.kind == MatrixEventKind::Implication) {
    const int32_t implied = event.implied_value ? event.implied_variable
                                                : -event.implied_variable;
    if (open == SIZE_MAX || event.reason.back() != implied ||
        kernel_externalize(kernel, output[open]) != implied)
      return false;
  } else if (event.kind == MatrixEventKind::Conflict) {
    if (open != SIZE_MAX) return false;
  } else {
    return false;
  }
  if (output.size() == 1) return kernel.level == 0 && open == 0;

  const auto before = [&](Lit first, Lit second) {
    return kernel.assigned[var_of(first)].trail <
           kernel.assigned[var_of(second)].trail;
  };
  if (open != SIZE_MAX) {
    std::swap(output[0], output[open]);
    const auto earliest = std::min_element(output.begin() + 1, output.end(),
                                           before);
    std::iter_swap(output.begin() + 1, earliest);
  } else {
    const auto earliest = std::min_element(output.begin(), output.end(), before);
    std::iter_swap(output.begin(), earliest);
    const auto second = std::min_element(output.begin() + 1, output.end(), before);
    std::iter_swap(output.begin() + 1, second);
  }
  return true;
}

bool make_journal(const MatrixLiveRequest &request,
                  const AffineMatrixTransaction &transaction,
                  const MatrixEvent &event,
                  const std::vector<uint64_t> &occurrences,
                  const std::vector<uint64_t> &source_clause_ids,
                  uint32_t level, uint32_t starting_extent,
                  std::string &journal,
                  uint64_t &journal_work) {
  journal.clear();
  journal_work = 0;
  const auto append = [&](const std::string &text) {
    if (text.size() > UINT64_MAX - journal_work)
      journal_work = UINT64_MAX;
    else
      journal_work += text.size();
    if (journal.size() > request.maximum_journal_bytes ||
        text.size() > request.maximum_journal_bytes - journal.size())
      return false;
    journal += text;
    return true;
  };
  const auto list = [&](const auto &values) {
    if (!append("[")) return false;
    for (size_t i = 0; i < values.size(); ++i)
      if (!append((i ? "," : "") + std::to_string(values[i]))) return false;
    return append("]");
  };
  if (!append("{\"kind\":\"affine-matrix-reason\",\"action_id\":" +
              std::to_string(request.action_id) + ",\"semantic_id\":" +
              std::to_string(request.semantic_id) + ",\"owner\":" +
              std::to_string(request.owner_identity) + ",\"event\":\"" +
              (event.kind == MatrixEventKind::Implication ? "implication"
                                                          : "conflict") +
              "\",\"level\":" + std::to_string(level) +
              ",\"reason\":"))
    return false;
  if (!list(event.reason)) return false;
  if (transaction.proof_cache_hit()) {
    if (!append(",\"sources_ref\":{\"cache_entry\":" +
                std::to_string(transaction.proof_cache_entry().entry_id) +
                ",\"proof_epoch\":" +
                std::to_string(transaction.proof_cache_entry().proof_epoch) +
                "}"))
      return false;
  } else if (transaction.compact_pbp() &&
             request.retained_source_provenance) {
    if (!append(",\"source_ordinals\":") ||
        !list(transaction.source_equations()) ||
        !append(",\"sources_ref\":{\"retained_owner\":" +
                std::to_string(request.owner_identity) + "}"))
      return false;
  } else {
    if (!append(",\"source_ordinals\":") ||
        !list(transaction.source_equations()) || !append(",\"sources\":["))
      return false;
    size_t premise_at = 0;
    for (size_t source_at = 0;
         source_at < transaction.source_identity().size(); ++source_at) {
      const Equation &source = transaction.source_identity()[source_at];
      if (!append((source_at ? "," : "") +
                  std::string("{\"ordinal\":") +
                  std::to_string(transaction.source_equations()[source_at]) +
                  ",\"variables\":") ||
          !list(source.variables) ||
          !append(",\"rhs\":" + std::to_string(source.rhs) +
                  ",\"premises\":["))
        return false;
      for (size_t i = 0; i < source.premises.size(); ++i, ++premise_at) {
        if (premise_at >= occurrences.size() ||
            premise_at >= source_clause_ids.size() ||
            !append((i ? "," : "") +
                    std::string("{\"occurrence\":") +
                    std::to_string(occurrences[premise_at]) +
                    ",\"clause_id\":" +
                    std::to_string(source_clause_ids[premise_at]) +
                    ",\"clause\":") ||
            !list(source.premise_clauses[i]) || !append("}"))
          return false;
      }
      if (!append("]}")) return false;
    }
    if (premise_at != occurrences.size() ||
        premise_at != source_clause_ids.size() || !append("]"))
      return false;
  }
  if (request.compact_provenance) {
    if (!append(",\"proof_events_omitted\":true")) return false;
  } else {
    if (!append(",\"proof_events\":[")) return false;
    for (size_t i = 0; i < transaction.proof_events().size(); ++i) {
      const MatrixProofEvent &proof = transaction.proof_events()[i];
      const char *kind = proof.kind == MatrixProofEvent::Kind::Rat ? "rat"
                         : proof.kind == MatrixProofEvent::Kind::Rup ? "rup"
                                                                    : "delete";
      if (!append((i ? "," : "") + std::string("{\"kind\":\"") + kind +
                  "\",\"id\":" + std::to_string(proof.id) +
                  ",\"deleted_id\":" + std::to_string(proof.deleted_id) +
                  ",\"install\":" + std::to_string(proof.install) +
                  ",\"clause\":") ||
          !list(proof.clause) || !append("}"))
        return false;
    }
    if (!append("]")) return false;
  }
  if (!append(",\"proof_mode\":\"" +
              std::string(transaction.compact_pbp()
                              ? "pbp-polynomial"
                              : "clausal-parity-tree") +
              "\""))
    return false;
  const char *cache_status = transaction.proof_cache_hit()
                                 ? "hit"
                             : transaction.proof_cache_resume()
                                 ? (transaction.proof_cache_insert()
                                        ? "resume-insert" : "resume-unretained")
                             : transaction.proof_cache_insert() ? "insert"
                                                                : "disabled";
  if (!append(",\"proof_cache\":{\"status\":\"" +
              std::string(cache_status) + "\",\"entry_id\":" +
              std::to_string(transaction.proof_cache_entry().entry_id) +
              ",\"generation\":" +
              std::to_string(transaction.proof_cache_generation()) +
              ",\"proof_epoch\":" +
              std::to_string(transaction.proof_cache_entry().proof_epoch) +
              ",\"parent_entry\":" +
              std::to_string(transaction.proof_cache_entry().parent_entry_id)) ||
      (!transaction.proof_cache_hit() &&
       (!append(",\"support_ids\":") ||
        !list(transaction.proof_cache_entry().support_ids))) ||
      !append("},\"helpers\":"))
    return false;
  const uint64_t helper_first = transaction.helper_extent()
                                    ? uint64_t{starting_extent} + 1
                                    : 0;
  const uint64_t helper_last = transaction.helper_extent()
                                   ? uint64_t{starting_extent} +
                                         transaction.helper_extent()
                                   : 0;
  if (!append("{\"first\":" +
              std::to_string(helper_first) + ",\"last\":" +
              std::to_string(helper_last) + ",\"count\":" +
              std::to_string(transaction.helper_extent()) +
              ",\"meaning\":\"" +
              std::string(transaction.compact_pbp()
                              ? "proof-only PB threshold carries; "
                              : "proof-only balanced-XOR identities; ") +
              (transaction.proof_cache_insert()
                   ? std::string("persistent cache support, never branchable")
                   : std::string("retired or previously cached, never branchable")) +
              "\"},"
              "\"proof_additions\":" +
              std::to_string(transaction.additions()) +
              ",\"proof_deletions\":" +
              std::to_string(transaction.deletions()) +
              ",\"helper_extent\":" +
              std::to_string(transaction.helper_extent()) +
              ",\"planner_work\":" + std::to_string(transaction.work()) +
              ",\"arena_words\":" +
              std::to_string(transaction.arena_words()) + "}\n"))
    return false;
  return true;
}

MatrixLiveStatus transaction_status(MatrixTransactionStatus status) {
  switch (status) {
  case MatrixTransactionStatus::Prepared:
    return MatrixLiveStatus::Prepared;
  case MatrixTransactionStatus::Invalid:
    return MatrixLiveStatus::Invalid;
  case MatrixTransactionStatus::Stale:
    return MatrixLiveStatus::Stale;
  case MatrixTransactionStatus::UnsupportedProof:
    return MatrixLiveStatus::UnsupportedProof;
  case MatrixTransactionStatus::ResourceExhausted:
    return MatrixLiveStatus::ResourceExhausted;
  case MatrixTransactionStatus::Interrupted:
    return MatrixLiveStatus::Interrupted;
  }
  return MatrixLiveStatus::Invalid;
}

bool proof_supported(const Kernel &kernel) {
  return !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->terminal_pbp());
}

bool proof_unchanged(const Kernel &kernel, bool had_proof, Proof *proof,
                     ProofFormat format) {
  if ((kernel.proof != nullptr) != had_proof || kernel.proof != proof)
    return false;
  return !kernel.proof || kernel.proof->format() == format;
}

bool install_reason(Kernel &kernel, const std::vector<Lit> &physical,
                    uint64_t id, bool &implication_consumed) {
  implication_consumed = false;
  if (physical.empty()) {
    kernel.inconsistent = true;
    return true;
  }
  if (physical.size() == 1) {
    if (kernel.level || kernel.value(physical[0])) return false;
    kernel.unit_ids[var_of(physical[0])] = id;
    kernel_assign_unit(kernel, physical[0]);
    return true;
  }
  const bool consume_implication =
      kernel.level && !kernel.value(physical[0]);
  if (physical.size() == 2) {
    kernel_add_binary_clause(kernel, physical[0], physical[1], id);
    if (consume_implication) {
      if (kernel.value(physical[1]) >= 0) return false;
      // The proof-visible clause is committed before this assignment. The
      // reason is already unit at the propagated fixpoint, so consume it as
      // the ordinary binary propagator would instead of rewinding and
      // rescanning an otherwise unchanged trail prefix.
      kernel_assign_binary(kernel, physical[0], physical[1]);
      implication_consumed = true;
    }
  } else {
    const Ref reason = kernel_add_large_clause(
        kernel, physical.data(), physical.size(), true, 0, id);
    if (consume_implication) {
      for (size_t i = 1; i < physical.size(); ++i)
        if (kernel.value(physical[i]) >= 0) return false;
      // reason_order puts the unique open literal first. Direct assignment
      // retains exactly the installed clause as its analysis reason and leaves
      // only the new literal's watches for the next Propagate action.
      kernel_assign_ref(kernel, physical[0], reason);
      implication_consumed = true;
    }
  }
  if (kernel.level && !implication_consumed)
    kernel_rearm_theory_clause(kernel, physical.data(), physical.size());
  return true;
}

} // namespace

MatrixLiveResult prepare_affine_matrix_live(
    Kernel &kernel, const MatrixLiveRequest &request,
    AffineMatrixLivePrepared &output) {
  MatrixLiveResult result;
  if (!valid(request)) return result;
  if (!proof_supported(kernel)) {
    result.status = MatrixLiveStatus::UnsupportedProof;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = kernel_terminating(kernel) ? MatrixLiveStatus::Interrupted
                                               : MatrixLiveStatus::Inapplicable;
    return result;
  }

  AffineMatrixLivePrepared candidate;
  // The one-shot adapter marks its private output before entering prepare.
  // It publishes on this same stack without an intervening mutation boundary,
  // so snapshots used only by a later, separately callable publish are not
  // retained or copied on that path.
  candidate.immediate_ = output.immediate_;
  candidate.request_ = request;
  candidate.starting_clause_id_ = kernel.next_clause_id;
  candidate.arena_generation_ = kernel.arena_generation;
  candidate.starting_nvars_ = kernel.nvars;
  if (kernel.external_to_internal.size() > UINT32_MAX) {
    result.status = MatrixLiveStatus::ResourceExhausted;
    return result;
  }
  candidate.starting_extent_ = kernel.external_to_internal.size();
  candidate.starting_level_ = kernel.level;
  candidate.had_proof_ = kernel.proof;
  candidate.proof_ = kernel.proof;
  if (kernel.proof) candidate.proof_format_ = kernel.proof->format();
  candidate.records_position_ = std::ftell(request.records);
  std::vector<MatrixAssignment> support_trail;
  if (candidate.records_position_ < 0 ||
      !snapshot_trail(kernel, candidate.trail_,
                      candidate.immediate_ ? nullptr
                                           : &candidate.trail_levels_) ||
      !snapshot_support(kernel, request.matrix->admitted_variables(),
                        support_trail) ||
      (candidate.trail_ != request.matrix->trail() &&
       support_trail != request.matrix->trail())) {
    result.status = MatrixLiveStatus::Stale;
    return result;
  }
  candidate.event_ = *request.event;
  if (!candidate.immediate_) candidate.owner_ = *request.owner;
  if (!reason_order(kernel, candidate.event_, candidate.physical_reason_)) {
    result.status = MatrixLiveStatus::UnsupportedOutcome;
    return result;
  }

  const PbpParityReasonPlan *compact_plan = nullptr;
  if (kernel.proof && kernel.proof->streaming_pbp() &&
      request.pbp_parity_cache) {
    bool complete = true;
    candidate.pbp_equations_.reserve(
        candidate.event_.row.source_equations.size());
    for (const size_t ordinal : candidate.event_.row.source_equations) {
      if (ordinal >= request.exact_sources->size()) {
        complete = false;
        break;
      }
      const Equation &source = (*request.exact_sources)[ordinal];
      PbpParityEquation equation;
      equation.variables.assign(source.variables.begin(),
                                source.variables.end());
      equation.rhs = source.rhs;
      candidate.pbp_equations_.push_back(std::move(equation));
    }
    if (complete && kernel.proof->plan_pbp_parity_reason(
                        candidate.pbp_equations_,
                        candidate.event_.reason.empty()
                            ? nullptr : candidate.event_.reason.data(),
                        candidate.event_.reason.size(),
                        candidate.starting_extent_,
                        *request.pbp_parity_cache, candidate.pbp_plan_)) {
      if (request.maximum_persistent_bytes &&
          candidate.pbp_plan_.retained_bytes >
              request.maximum_persistent_bytes) {
        result.status = MatrixLiveStatus::ResourceExhausted;
        return result;
      }
      compact_plan = &candidate.pbp_plan_;
    } else {
      candidate.pbp_equations_.clear();
    }
  }

  bool planner_interrupted = false;
  const MatrixTransactionStatus planned = prepare_affine_matrix_transaction(
      *request.matrix, request.owner_identity, *request.exact_sources,
      candidate.event_, candidate.starting_extent_, candidate.starting_clause_id_,
      request.transaction,
      [&](uint64_t) {
        if (kernel_terminating(kernel)) planner_interrupted = true;
        return !planner_interrupted;
      },
      candidate.transaction_, compact_plan ? nullptr : request.proof_cache,
      kernel.proof ? kernel.proof->persistent_external_epoch() : 0,
      compact_plan);
  result.planner_work = candidate.transaction_.work();
  if (planned != MatrixTransactionStatus::Prepared) {
    result.status = transaction_status(planned);
    return result;
  }

  if (request.maximum_persistent_bytes && request.proof_cache &&
      candidate.transaction_.proof_cache_insert()) {
    __uint128_t bytes = request.proof_cache->retained_bytes() +
        (kernel.proof ? kernel.proof->persistent_external_bytes() : 0);
    const auto &entry = candidate.transaction_.proof_cache_entry();
    bytes += 2 * sizeof(MatrixProofCacheEntry) +
        2 * (entry.source_equations.size() * sizeof(size_t) +
             entry.support_ids.size() * sizeof(uint64_t) +
             entry.coordinates.size() * sizeof(External) +
             entry.tree.variables.size() * sizeof(External) +
             entry.tree.definitions.size() *
                 sizeof(parity_certificate::ActiveDefinition));
    for (const auto &definition : entry.tree.definitions)
      bytes += 2 * definition.clauses.size() *
               sizeof(parity_certificate::SmallClause);
    for (const auto &equation : entry.source_identity) {
      bytes += 2 * (sizeof(Equation) + equation.variables.size() * sizeof(External) +
                    equation.premises.size() * sizeof(uint64_t));
      for (const auto &clause : equation.premise_clauses)
        bytes += 2 * (sizeof(std::vector<External>) + clause.size() * sizeof(External));
    }
    for (const auto &event : candidate.transaction_.proof_events())
      if (event.kind != MatrixProofEvent::Kind::Delete && !event.install)
        bytes += 2 * (sizeof(std::vector<External>) + sizeof(uint64_t) +
                     event.clause.size() * sizeof(External));
    if (bytes > request.maximum_persistent_bytes) {
      result.status = MatrixLiveStatus::ResourceExhausted; return result;
    }
  }
  Work validation{0, request.maximum_revalidation_work, kernel};
  if (!validate_selected_sources(
          kernel, candidate.immediate_ ? *request.owner : candidate.owner_,
          *request.exact_sources,
          candidate.transaction_.source_equations(),
          candidate.transaction_.source_identity(), validation,
          request.premise_leases, candidate.source_occurrences_,
          candidate.source_clause_ids_) ||
      (candidate.transaction_.proof_cache_resume() &&
       !validate_selected_sources(
           kernel, candidate.immediate_ ? *request.owner : candidate.owner_,
           *request.exact_sources,
           candidate.transaction_.resume_source_equations(),
           candidate.transaction_.resume_source_identity(), validation,
           request.premise_leases, candidate.resume_source_occurrences_,
           candidate.resume_source_clause_ids_))) {
    result.revalidation_work = validation.used;
    result.status = validation.interrupted
                        ? MatrixLiveStatus::Interrupted
                    : validation.exhausted
                        ? MatrixLiveStatus::ResourceExhausted
                        : MatrixLiveStatus::Stale;
    return result;
  }
  result.revalidation_work = validation.used;
  if (uint64_t(candidate.starting_extent_) +
              candidate.transaction_.helper_extent() >
          request.maximum_external_extent ||
      candidate.transaction_.arena_words() >=
          INVALID_REF - kernel.arena.size()) {
    result.status = MatrixLiveStatus::ResourceExhausted;
    return result;
  }
  if (!make_journal(request, candidate.transaction_, candidate.event_,
                    candidate.source_occurrences_, candidate.source_clause_ids_,
                    candidate.starting_level_, candidate.starting_extent_,
                    candidate.journal_,
                    result.journal_work)) {
    result.status = MatrixLiveStatus::ResourceExhausted;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = MatrixLiveStatus::Interrupted;
    return result;
  }

  candidate.ready_ = true;
  result.status = MatrixLiveStatus::Prepared;
  result.selected_sources = candidate.transaction_.source_equations().size();
  result.selected_premises = candidate.source_occurrences_.size();
  result.preflight_additions = candidate.transaction_.additions();
  result.preflight_deletions = candidate.transaction_.deletions();
  result.helper_extent = candidate.transaction_.helper_extent();
  result.arena_words = candidate.transaction_.arena_words();
  result.implication = candidate.event_.kind == MatrixEventKind::Implication;
  result.conflict = candidate.event_.kind == MatrixEventKind::Conflict;
  result.preflight_journal_bytes = candidate.journal_.size();
  result.proof_cache_hit = candidate.transaction_.proof_cache_hit();
  result.proof_cache_insert = candidate.transaction_.proof_cache_insert();
  result.proof_cache_resume = candidate.transaction_.proof_cache_resume();
  result.compact_pbp = candidate.transaction_.compact_pbp();
  candidate.prepared_ = result;
  output = std::move(candidate);
  return result;
}

MatrixLiveResult publish_affine_matrix_live(
    Kernel &kernel, const MatrixLiveRequest &request,
    AffineMatrixLivePrepared &prepared) {
  MatrixLiveResult result = prepared.prepared_;
  result.result = ApplyResult::NoOp;
  result.journal_bytes = 0;
  if (!prepared.ready_ || !valid(request) ||
      !same_request(request, prepared.request_)) {
    result.status = MatrixLiveStatus::Invalid;
    return result;
  }
  if (prepared.spent_) {
    result.status = MatrixLiveStatus::AlreadyApplied;
    return result;
  }
  if (!proof_supported(kernel)) {
    result.status = MatrixLiveStatus::UnsupportedProof;
    return result;
  }
  if (!applicable(kernel)) {
    result.status = kernel_terminating(kernel) ? MatrixLiveStatus::Interrupted
                                               : MatrixLiveStatus::Inapplicable;
    return result;
  }
  if (kernel.next_clause_id != prepared.starting_clause_id_ ||
      kernel.arena_generation != prepared.arena_generation_ ||
      kernel.nvars != prepared.starting_nvars_ ||
      kernel.external_to_internal.size() != prepared.starting_extent_ ||
      kernel.level != prepared.starting_level_ ||
      !proof_unchanged(kernel, prepared.had_proof_, prepared.proof_,
                       prepared.proof_format_) ||
      (request.proof_cache &&
       (request.proof_cache->generation() !=
            prepared.transaction_.proof_cache_generation() ||
        (kernel.proof &&
         kernel.proof->persistent_external_epoch() !=
             prepared.transaction_.proof_cache_entry().proof_epoch))) ||
      std::ftell(request.records) != prepared.records_position_) {
    result.status = MatrixLiveStatus::Stale;
    return result;
  }

  // The one-shot apply helper enters publish on the same stack immediately
  // after prepare, with no callback or mutation boundary between them.  Its
  // prepared snapshots are therefore still current.  The separately callable
  // two-phase API retains the complete stale-state audit.
  if (!prepared.immediate_) {
    std::vector<MatrixAssignment> trail;
    std::vector<uint32_t> levels;
    if (!snapshot_trail(kernel, trail, &levels) || trail != prepared.trail_ ||
        levels != prepared.trail_levels_ ||
        !same_event(*request.event, prepared.event_)) {
      result.status = MatrixLiveStatus::Stale;
      return result;
    }
    Work validation{0, request.maximum_revalidation_work, kernel};
    std::vector<uint64_t> occurrences;
    std::vector<uint64_t> clause_ids;
    std::vector<uint64_t> resume_occurrences;
    std::vector<uint64_t> resume_clause_ids;
    if (!request.matrix->authenticates(
            request.owner_identity, *request.exact_sources, *request.event,
            [&](uint64_t amount) { return validation.charge(amount); }) ||
        !validate_selected_sources(
            kernel, prepared.owner_, *request.exact_sources,
            prepared.transaction_.source_equations(),
            prepared.transaction_.source_identity(), validation,
            request.premise_leases, occurrences, clause_ids) ||
        (prepared.transaction_.proof_cache_resume() &&
         !validate_selected_sources(
             kernel, prepared.owner_, *request.exact_sources,
             prepared.transaction_.resume_source_equations(),
             prepared.transaction_.resume_source_identity(), validation,
             request.premise_leases, resume_occurrences,
             resume_clause_ids))) {
      result.revalidation_work = validation.used;
      result.status = validation.interrupted
                          ? MatrixLiveStatus::Interrupted
                      : validation.exhausted
                          ? MatrixLiveStatus::ResourceExhausted
                          : MatrixLiveStatus::Stale;
      return result;
    }
    if (occurrences != prepared.source_occurrences_ ||
        clause_ids != prepared.source_clause_ids_ ||
        resume_occurrences != prepared.resume_source_occurrences_ ||
        resume_clause_ids != prepared.resume_source_clause_ids_) {
      result.status = MatrixLiveStatus::Stale;
      return result;
    }
    result.revalidation_work = validation.used;
    std::vector<Lit> physical;
    if (!reason_order(kernel, prepared.event_, physical) ||
        physical != prepared.physical_reason_) {
      result.status = MatrixLiveStatus::Stale;
      return result;
    }
    if (prepared.transaction_.compact_pbp()) {
      PbpParityReasonPlan current;
      if (!kernel.proof || !request.pbp_parity_cache ||
          !kernel.proof->plan_pbp_parity_reason(
              prepared.pbp_equations_,
              prepared.event_.reason.empty() ? nullptr
                                             : prepared.event_.reason.data(),
              prepared.event_.reason.size(), prepared.starting_extent_,
              *request.pbp_parity_cache, current) ||
          current.stream_identity != prepared.pbp_plan_.stream_identity ||
          current.cache_generation != prepared.pbp_plan_.cache_generation ||
          current.proof_constraints != prepared.pbp_plan_.proof_constraints ||
          current.retained_bytes != prepared.pbp_plan_.retained_bytes ||
          current.helper_variables != prepared.pbp_plan_.helper_variables) {
        result.status = MatrixLiveStatus::Stale;
        return result;
      }
    }
  } else {
    // Keep the historical publish-phase work currency exactly.  Preparation
    // already established the selected-source identities and recorded their
    // deterministic validation charge.  Re-authenticate the matrix event,
    // then charge that recorded suffix without walking the same premise
    // population again.  If the aggregate would cross the bound, replay the
    // old suffix so the terminal partial-work report remains exact.
    Work validation{0, request.maximum_revalidation_work, kernel};
    if (!validation.charge(prepared.transaction_.authentication_work())) {
      Work replay{0, request.maximum_revalidation_work, kernel};
      const bool authenticated = request.matrix->authenticates(
          request.owner_identity, *request.exact_sources, *request.event,
          [&](uint64_t amount) { return replay.charge(amount); });
      result.revalidation_work = replay.used;
      result.status = replay.interrupted
                          ? MatrixLiveStatus::Interrupted
                      : replay.exhausted
                          ? MatrixLiveStatus::ResourceExhausted
                      : authenticated
                          ? MatrixLiveStatus::Invalid
                          : MatrixLiveStatus::Stale;
      return result;
    }
    const uint64_t selected_work = result.revalidation_work;
    if (!validation.charge(selected_work)) {
      Work replay{validation.used, request.maximum_revalidation_work, kernel};
      std::vector<uint64_t> occurrences;
      std::vector<uint64_t> clause_ids;
      std::vector<uint64_t> resume_occurrences;
      std::vector<uint64_t> resume_clause_ids;
      const bool valid_sources = validate_selected_sources(
          kernel, prepared.immediate_ ? *request.owner : prepared.owner_,
          *request.exact_sources,
          prepared.transaction_.source_equations(),
          prepared.transaction_.source_identity(), replay,
          request.premise_leases, occurrences, clause_ids) &&
          (!prepared.transaction_.proof_cache_resume() ||
           validate_selected_sources(
               kernel, prepared.immediate_ ? *request.owner : prepared.owner_,
               *request.exact_sources,
               prepared.transaction_.resume_source_equations(),
               prepared.transaction_.resume_source_identity(), replay,
               request.premise_leases, resume_occurrences,
               resume_clause_ids));
      result.revalidation_work = replay.used;
      result.status = replay.interrupted
                          ? MatrixLiveStatus::Interrupted
                      : replay.exhausted
                          ? MatrixLiveStatus::ResourceExhausted
                      : valid_sources
                          ? MatrixLiveStatus::Invalid
                          : MatrixLiveStatus::Stale;
      return result;
    }
    result.revalidation_work = validation.used;
  }
  if (uint64_t(prepared.starting_extent_) +
              prepared.transaction_.helper_extent() >
          request.maximum_external_extent ||
      prepared.transaction_.arena_words() >=
          INVALID_REF - kernel.arena.size()) {
    result.status = MatrixLiveStatus::ResourceExhausted;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = MatrixLiveStatus::Interrupted;
    return result;
  }

  kernel.close_startup_certificate();
  Var first = INVALID_VAR;
  if (!kernel_reserve_proof_variables(
          kernel, prepared.transaction_.helper_extent(), first) ||
      first != prepared.starting_extent_)
    std::abort();
  uint64_t installed_id = 0;
  std::set<uint64_t> retained_support(
      prepared.transaction_.proof_cache_entry().support_ids.begin(),
      prepared.transaction_.proof_cache_entry().support_ids.end());
  size_t retained = 0;
  if (prepared.transaction_.compact_pbp()) {
    if (!request.pbp_parity_cache || kernel.next_clause_id == UINT64_MAX)
      std::abort();
    installed_id = ++kernel.next_clause_id;
    if (kernel.proof &&
        !kernel.proof->add_prevalidated_pbp_parity_reason(
            installed_id, prepared.pbp_equations_,
            prepared.event_.reason.empty() ? nullptr
                                           : prepared.event_.reason.data(),
            prepared.event_.reason.size(), *request.pbp_parity_cache,
            prepared.transaction_.pbp_plan()))
      std::abort();
  }
  for (const MatrixProofEvent &event : prepared.transaction_.proof_events()) {
    if (event.kind == MatrixProofEvent::Kind::Delete) {
      if (event.id != kernel.next_clause_id) std::abort();
      if (kernel.proof &&
          !kernel.proof->del_external(
              event.id, event.deleted_id,
              event.clause.empty() ? nullptr : event.clause.data(),
              event.clause.size()))
        std::abort();
      continue;
    }
    if (++kernel.next_clause_id != event.id) std::abort();
    if (kernel.proof) {
      const bool emitted = event.kind == MatrixProofEvent::Kind::Rat
          ? kernel.proof->add_external_rat(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                event.clause.size())
          : kernel.proof->add_external_rup(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                event.clause.size());
      if (!emitted) std::abort();
    }
    if (retained_support.count(event.id)) {
      if (event.install || event.kind == MatrixProofEvent::Kind::Delete)
        std::abort();
      if (kernel.proof &&
          !kernel.proof->retain_external(
              event.id, event.clause.empty() ? nullptr : event.clause.data(),
              event.clause.size()))
        std::abort();
      ++retained;
    }
    if (event.install) {
      if (installed_id) std::abort();
      installed_id = event.id;
    }
  }
  if (prepared.transaction_.proof_cache_insert() &&
      retained != retained_support.size())
    std::abort();
  result.installed_reason_id = installed_id;
  bool implication_consumed = false;
  if (!installed_id ||
      !install_reason(kernel, prepared.physical_reason_, installed_id,
                      implication_consumed))
    std::abort();
  result.implication_consumed = implication_consumed;
  if (std::fwrite(prepared.journal_.data(), 1, prepared.journal_.size(),
                  request.records) != prepared.journal_.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  if (prepared.transaction_.proof_cache_insert() &&
      (!request.proof_cache ||
       !request.proof_cache->commit(
           MatrixProofCacheEntry(prepared.transaction_.proof_cache_entry()),
           prepared.transaction_.proof_cache_generation())))
    std::abort();

  prepared.spent_ = true;
  result.status = MatrixLiveStatus::Applied;
  result.result = kernel.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
  result.additions = prepared.transaction_.additions();
  result.deletions = prepared.transaction_.deletions();
  result.journal_bytes = prepared.journal_.size();
  result.propagation_pending =
      !kernel.inconsistent && !kernel_propagation_fixpoint(kernel);
  return result;
}

MatrixLiveResult apply_affine_matrix_live(Kernel &kernel,
                                          const MatrixLiveRequest &request) {
  AffineMatrixLivePrepared prepared;
  prepared.immediate_ = true;
  MatrixLiveResult result = prepare_affine_matrix_live(kernel, request, prepared);
  if (result.status != MatrixLiveStatus::Prepared) return result;
  return publish_affine_matrix_live(kernel, request, prepared);
}

} // namespace tactix::affine_semantic_extension

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
         first.records == second.records;
}

bool snapshot_trail(const Kernel &kernel,
                    std::vector<MatrixAssignment> &assignments,
                    std::vector<uint32_t> &levels) {
  assignments.clear();
  levels.clear();
  assignments.reserve(kernel.trail.size());
  levels.reserve(kernel.trail.size());
  for (Lit literal : kernel.trail) {
    const int32_t external = kernel_externalize(kernel, literal);
    if (!external || external == INT32_MIN) return false;
    const int32_t variable = external < 0 ? -external : external;
    assignments.push_back({variable, external > 0});
    const Assigned &assigned = kernel.assigned[var_of(literal)];
    if (assigned.trail + 1 != assignments.size()) return false;
    levels.push_back(assigned.level);
  }
  return true;
}

std::vector<int32_t> clause_key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool validate_selected_sources(
    const Kernel &kernel, const AffineLiveSnapshot &owner,
    const std::vector<Equation> &current_sources,
    const AffineMatrixTransaction &transaction, Work &work,
    std::vector<uint64_t> &occurrences, std::vector<uint64_t> &clause_ids) {
  occurrences.clear();
  clause_ids.clear();
  if (transaction.source_equations().size() !=
      transaction.source_identity().size())
    return false;
  std::set<uint64_t> unique;
  for (size_t at = 0; at < transaction.source_equations().size(); ++at) {
    if (!work.charge(1)) return false;
    const size_t ordinal = transaction.source_equations()[at];
    if (ordinal >= current_sources.size()) return false;
    const Equation &identity = transaction.source_identity()[at];
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
          clause_key(source.clause) !=
              clause_key(identity.premise_clauses[i]))
        return false;
      occurrences.push_back(occurrence);
      clause_ids.push_back(source.clause_id);
    }
  }
  if (!work.charge(occurrences.size())) return false;
  return affine_live_sources_valid(kernel, owner, occurrences);
}

bool reason_order(const Kernel &kernel, const MatrixEvent &event,
                  std::vector<Lit> &output) {
  output.clear();
  if (event.kind == MatrixEventKind::None || event.reason.empty()) return false;
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
  if (!list(event.reason) || !append(",\"source_ordinals\":" ) ||
      !list(transaction.source_equations()) || !append(",\"sources\":["))
    return false;
  size_t premise_at = 0;
  for (size_t source_at = 0;
       source_at < transaction.source_identity().size(); ++source_at) {
    const Equation &source = transaction.source_identity()[source_at];
    if (!append((source_at ? "," : "") +
                std::string("{\"ordinal\":") +
                std::to_string(transaction.source_equations()[source_at]) +
                ",\"variables\":" ) ||
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
                  ",\"clause\":" ) ||
          !list(source.premise_clauses[i]) || !append("}"))
        return false;
    }
    if (!append("]}")) return false;
  }
  if (premise_at != occurrences.size() ||
      premise_at != source_clause_ids.size() ||
      !append("],\"proof_events\":["))
    return false;
  for (size_t i = 0; i < transaction.proof_events().size(); ++i) {
    const MatrixProofEvent &proof = transaction.proof_events()[i];
    const char *kind = proof.kind == MatrixProofEvent::Kind::Rat ? "rat"
                       : proof.kind == MatrixProofEvent::Kind::Rup ? "rup"
                                                                  : "delete";
    if (!append((i ? "," : "") + std::string("{\"kind\":\"") + kind +
                "\",\"id\":" + std::to_string(proof.id) +
                ",\"deleted_id\":" + std::to_string(proof.deleted_id) +
                ",\"install\":" + std::to_string(proof.install) +
                ",\"clause\":" ) ||
        !list(proof.clause) || !append("}"))
      return false;
  }
  const uint64_t helper_first = transaction.helper_extent()
                                    ? uint64_t{starting_extent} + 1
                                    : 0;
  const uint64_t helper_last = transaction.helper_extent()
                                   ? uint64_t{starting_extent} +
                                         transaction.helper_extent()
                                   : 0;
  if (!append("],\"helpers\":{\"first\":" +
              std::to_string(helper_first) + ",\"last\":" +
              std::to_string(helper_last) + ",\"count\":" +
              std::to_string(transaction.helper_extent()) +
              ",\"meaning\":\"proof-only balanced-XOR identities; retired, never branchable\"},"
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
  return !kernel.proof || (!kernel.proof->lrat() && !kernel.proof->pb());
}

bool proof_unchanged(const Kernel &kernel, bool had_proof, Proof *proof,
                     ProofFormat format) {
  if ((kernel.proof != nullptr) != had_proof || kernel.proof != proof)
    return false;
  return !kernel.proof || kernel.proof->format() == format;
}

bool install_reason(Kernel &kernel, const std::vector<Lit> &physical,
                    uint64_t id) {
  if (physical.empty()) return false;
  if (physical.size() == 1) {
    if (kernel.level || kernel.value(physical[0])) return false;
    kernel.unit_ids[var_of(physical[0])] = id;
    kernel_assign_unit(kernel, physical[0]);
    return true;
  }
  if (physical.size() == 2)
    kernel_add_binary_clause(kernel, physical[0], physical[1], id);
  else
    kernel_add_large_clause(kernel, physical.data(), physical.size(), true, 0,
                            id);
  if (kernel.level)
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
  if (candidate.records_position_ < 0 ||
      !snapshot_trail(kernel, candidate.trail_, candidate.trail_levels_) ||
      candidate.trail_ != request.matrix->trail()) {
    result.status = MatrixLiveStatus::Stale;
    return result;
  }
  candidate.event_ = *request.event;
  candidate.owner_ = *request.owner;
  if (!reason_order(kernel, candidate.event_, candidate.physical_reason_)) {
    result.status = MatrixLiveStatus::UnsupportedOutcome;
    return result;
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
      candidate.transaction_);
  result.planner_work = candidate.transaction_.work();
  if (planned != MatrixTransactionStatus::Prepared) {
    result.status = transaction_status(planned);
    return result;
  }

  Work validation{0, request.maximum_revalidation_work, kernel};
  if (!validate_selected_sources(
          kernel, candidate.owner_, *request.exact_sources,
          candidate.transaction_, validation, candidate.source_occurrences_,
          candidate.source_clause_ids_)) {
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
      std::ftell(request.records) != prepared.records_position_) {
    result.status = MatrixLiveStatus::Stale;
    return result;
  }

  std::vector<MatrixAssignment> trail;
  std::vector<uint32_t> levels;
  if (!snapshot_trail(kernel, trail, levels) || trail != prepared.trail_ ||
      levels != prepared.trail_levels_ || !same_event(*request.event, prepared.event_)) {
    result.status = MatrixLiveStatus::Stale;
    return result;
  }
  Work validation{0, request.maximum_revalidation_work, kernel};
  std::vector<uint64_t> occurrences;
  std::vector<uint64_t> clause_ids;
  if (!request.matrix->authenticates(
          request.owner_identity, *request.exact_sources, *request.event,
          [&](uint64_t amount) { return validation.charge(amount); }) ||
      !validate_selected_sources(
          kernel, prepared.owner_, *request.exact_sources,
          prepared.transaction_, validation, occurrences, clause_ids)) {
    result.revalidation_work = validation.used;
    result.status = validation.interrupted
                        ? MatrixLiveStatus::Interrupted
                    : validation.exhausted
                        ? MatrixLiveStatus::ResourceExhausted
                        : MatrixLiveStatus::Stale;
    return result;
  }
  if (occurrences != prepared.source_occurrences_ ||
      clause_ids != prepared.source_clause_ids_) {
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
    if (event.install) {
      if (installed_id) std::abort();
      installed_id = event.id;
    }
  }
  if (!installed_id ||
      !install_reason(kernel, prepared.physical_reason_, installed_id))
    std::abort();
  if (std::fwrite(prepared.journal_.data(), 1, prepared.journal_.size(),
                  request.records) != prepared.journal_.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();

  prepared.spent_ = true;
  result.status = MatrixLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.additions = prepared.transaction_.additions();
  result.deletions = prepared.transaction_.deletions();
  result.journal_bytes = prepared.journal_.size();
  result.propagation_pending = !kernel_propagation_fixpoint(kernel);
  return result;
}

MatrixLiveResult apply_affine_matrix_live(Kernel &kernel,
                                          const MatrixLiveRequest &request) {
  AffineMatrixLivePrepared prepared;
  MatrixLiveResult result = prepare_affine_matrix_live(kernel, request, prepared);
  if (result.status != MatrixLiveStatus::Prepared) return result;
  return publish_affine_matrix_live(kernel, request, prepared);
}

} // namespace tactix::affine_semantic_extension

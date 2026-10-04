// Transactional live-kernel admission and publication for the normalized
// direct-PHP certificate.  This header deliberately registers no rule or
// policy action: callers decide when a terminal refutation may be attempted.
#pragma once

#include "kernel/kernel.hpp"
#include "proof/direct_php_recognition.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tactix::direct_php_live {

namespace certificate = direct_php_certificate;
namespace recognition = direct_php_recognition;

enum class SourceKind : uint8_t { Large, Binary };
enum class SourceIdentityKind : uint8_t { NativeProof, CaptureLocal };

struct Source {
  uint64_t occurrence = 0;
  // CaptureLocal values are exact replay tokens, never checker line ids.
  // They occur only after non-LRAT input finalization releases binary ids.
  uint64_t native_proof_id = 0;
  SourceIdentityKind identity_kind = SourceIdentityKind::NativeProof;
  SourceKind kind = SourceKind::Large;
  Ref ref = INVALID_REF;
  Lit first = INVALID_LIT;
  Lit second = INVALID_LIT;
  std::vector<int32_t> clause;

  friend bool operator==(const Source &, const Source &) = default;
};

struct Snapshot {
  std::vector<recognition::Premise> premises;
  std::vector<Source> sources;
  uint64_t arena_generation = 0;
  uint64_t next_clause_id = 0;
  uint32_t nvars = 0;
  uint32_t external_extent = 0;
  uint64_t literals = 0;
  uint64_t work = 0;

  friend bool operator==(const Snapshot &a, const Snapshot &b) {
    if (a.sources != b.sources || a.arena_generation != b.arena_generation ||
        a.next_clause_id != b.next_clause_id || a.nvars != b.nvars ||
        a.external_extent != b.external_extent || a.literals != b.literals ||
        a.work != b.work || a.premises.size() != b.premises.size())
      return false;
    for (size_t i = 0; i < a.premises.size(); ++i) {
      const auto &x = a.premises[i], &y = b.premises[i];
      if (x.occurrence != y.occurrence || x.native_proof_id != y.native_proof_id ||
          x.literals != y.literals)
        return false;
    }
    return true;
  }
};

struct SnapshotLimits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
};

struct Request {
  SnapshotLimits snapshot;
  recognition::Limits recognition;
  certificate::Limits certificate;
};

enum class Status : uint8_t {
  Invalid,
  Inapplicable,
  NotRecognized,
  Malformed,
  Ambiguous,
  ResourceExhausted,
  Interrupted,
  UnsupportedProof,
  Stale,
  Admitted,
  Prepared,
  Applied,
};

struct Result {
  Status status = Status::Invalid;
  uint64_t snapshot_clauses = 0;
  uint64_t snapshot_literals = 0;
  uint64_t snapshot_work = 0;
  uint64_t recognition_work = 0;
  uint64_t additions = 0;
  uint64_t deletions = 0;
  uint64_t fresh_variables = 0;
};

class Admission {
public:
  bool ready() const { return ready_; }
  const Snapshot &snapshot() const { return snapshot_; }
  const recognition::Image &image() const { return image_; }
  const certificate::Plan &plan() const { return plan_; }
  const Request &request() const { return request_; }
  const Result &result() const { return result_; }

private:
  friend Result admit(Kernel &, const Request &, Admission &);
  friend class Transaction;
  bool ready_ = false;
  Request request_;
  Snapshot snapshot_;
  recognition::Image image_;
  certificate::Plan plan_;
  Result result_;
};

struct ReplayEvent {
  certificate::EventKind kind = certificate::EventKind::AddRup;
  certificate::EventRole role = certificate::EventRole::Conclusion;
  uint32_t layer = 0;
  std::vector<int32_t> clause;
  uint64_t id = 0;
  uint64_t deleted_id = 0;
  bool deleted_id_is_capture_local = false;
  uint64_t source_occurrence = 0;
};

class Transaction {
public:
  bool ready() const { return ready_; }
  const std::vector<ReplayEvent> &events() const { return events_; }
  const certificate::Accounting &accounting() const { return accounting_; }
  int32_t first_fresh() const { return first_fresh_; }

private:
  friend Result prepare(Kernel &, const Admission &, Transaction &);
  friend Result publish(Kernel &, const Transaction &);
  bool ready_ = false;
  Request request_;
  Snapshot snapshot_;
  uint64_t starting_clause_id_ = 0;
  uint64_t arena_generation_ = 0;
  uint32_t starting_nvars_ = 0;
  uint32_t starting_extent_ = 0;
  size_t starting_reconstruction_ = 0;
  Proof *starting_proof_ = nullptr;
  int32_t first_fresh_ = 0;
  certificate::Accounting accounting_;
  std::vector<ReplayEvent> events_;
  Result result_;
};

namespace detail {

enum class CaptureStatus { Complete, Invalid, Exhausted, Interrupted };

inline bool applicable(const Kernel &k) {
  return !k.inconsistent && !k.level && !k.conflict_valid && !k.pending.valid &&
         k.trail.empty() && !k.dense && !k.probing.recording &&
         k.large_watches_connected && kernel_propagation_fixpoint(k);
}

inline bool valid(const Request &r) {
  return r.snapshot.maximum_clauses && r.snapshot.maximum_literals &&
         r.snapshot.maximum_work && r.recognition.maximum_clauses &&
         r.recognition.maximum_literals && r.recognition.maximum_work &&
         r.recognition.maximum_row_candidates &&
         r.recognition.maximum_row_combinations &&
         r.recognition.maximum_mapping_trials && r.recognition.maximum_holes;
}

inline std::vector<int32_t> key(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

inline bool valid_clause(const std::vector<int32_t> &clause) {
  std::set<int32_t> literals;
  for (int32_t literal : clause) {
    if (!literal || literal == INT32_MIN || literals.count(-literal) ||
        !literals.insert(literal).second)
      return false;
  }
  return true;
}

inline CaptureStatus capture(Kernel &k, const SnapshotLimits &limits,
                             Snapshot &output, Result *effort = nullptr) {
  Snapshot local;
  local.arena_generation = k.arena_generation;
  local.next_clause_id = k.next_clause_id;
  local.nvars = k.nvars;
  if (k.external_to_internal.size() > UINT32_MAX)
    return CaptureStatus::Invalid;
  local.external_extent = static_cast<uint32_t>(k.external_to_internal.size());
  CaptureStatus failure = CaptureStatus::Invalid;
  const auto finish = [&](CaptureStatus status) {
    if (effort) {
      effort->snapshot_clauses = local.sources.size();
      effort->snapshot_literals = local.literals;
      effort->snapshot_work = local.work;
    }
    if (status == CaptureStatus::Complete)
      output = std::move(local);
    return status;
  };
  const auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k)) {
      failure = CaptureStatus::Interrupted;
      return false;
    }
    if (local.work > limits.maximum_work ||
        amount > limits.maximum_work - local.work) {
      failure = CaptureStatus::Exhausted;
      return false;
    }
    local.work += amount;
    return true;
  };
  const auto take = [&](SourceKind kind, Ref ref, Lit first, Lit second,
                        uint64_t id, const Lit *literals, uint32_t size) {
    // Proofless/DRAT/DSR input finalization deliberately releases binary
    // clause ids: those formats delete by literal multiset and no other
    // kernel consumer needs the values.  Direct-PHP still needs a stable,
    // unique identity for exact recognition/replay.  Admit an absent binary
    // id here and assign a capture-local identity below; large clauses always
    // retain their native id, while LRAT/PB were rejected before capture.
    if (!id && (kind != SourceKind::Binary || k.binary_ids.tracks_values())) {
      failure = CaptureStatus::Invalid;
      return false;
    }
    if (local.sources.size() == limits.maximum_clauses ||
        local.literals > limits.maximum_literals ||
        size > limits.maximum_literals - local.literals) {
      failure = CaptureStatus::Exhausted;
      return false;
    }
    if (!charge(uint64_t(size) + 1))
      return false;
    Source source;
    source.native_proof_id = id;
    source.kind = kind;
    source.ref = ref;
    source.first = first;
    source.second = second;
    source.clause.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      if (var_of(literals[i]) >= k.nvars ||
          kernel_var_retired(k, var_of(literals[i]))) {
        failure = CaptureStatus::Invalid;
        return false;
      }
      source.clause.push_back(kernel_externalize(k, literals[i]));
    }
    if (!valid_clause(source.clause)) {
      failure = CaptureStatus::Invalid;
      return false;
    }
    local.literals += size;
    local.sources.push_back(std::move(source));
    return true;
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    const Ref current = ref;
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!words || words > k.arena.size() - ref)
      return finish(CaptureStatus::Invalid);
    ref += words;
    if (!charge(1))
      return finish(failure);
    if (!clause->garbage() &&
        !take(SourceKind::Large, current, INVALID_LIT, INVALID_LIT,
              clause->id(), clause->lits, clause->size))
      return finish(failure);
  }
  std::set<uint64_t> binary_keys;
  for (Lit first = 0; first < 2 * k.nvars; ++first) {
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!charge(1))
        return finish(failure);
      if (!watch.binary() || first >= watch.other())
        continue;
      const uint64_t binary_key = kernel_binary_key(first, watch.other());
      if (!binary_keys.insert(binary_key).second)
        return finish(CaptureStatus::Invalid);
      const Lit pair[] = {first, watch.other()};
      if (!take(SourceKind::Binary, INVALID_REF, first, watch.other(),
                k.binary_ids.tracks_values()
                    ? kernel_binary_id(k, first, watch.other()) : 0,
                pair, 2))
        return finish(failure);
    }
  }

  std::set<uint64_t> used_ids;
  for (const Source &source : local.sources)
    if (source.native_proof_id && !used_ids.insert(source.native_proof_id).second)
      return finish(CaptureStatus::Invalid);
  uint64_t synthetic = UINT64_MAX;
  for (Source &source : local.sources) {
    if (source.native_proof_id) continue;
    while (used_ids.count(synthetic)) {
      if (!synthetic--) return finish(CaptureStatus::Invalid);
    }
    source.native_proof_id = synthetic;
    source.identity_kind = SourceIdentityKind::CaptureLocal;
    used_ids.insert(synthetic);
    if (synthetic) --synthetic;
  }

  std::sort(local.sources.begin(), local.sources.end(),
            [](const Source &a, const Source &b) {
              if (a.native_proof_id != b.native_proof_id)
                return a.native_proof_id < b.native_proof_id;
              if (a.clause != b.clause)
                return a.clause < b.clause;
              return a.kind < b.kind;
            });
  local.premises.reserve(local.sources.size());
  for (size_t i = 0; i < local.sources.size(); ++i) {
    Source &source = local.sources[i];
    source.occurrence = i + 1;
    local.premises.push_back(
        {source.occurrence, source.clause, source.native_proof_id});
  }
  return finish(CaptureStatus::Complete);
}

struct RecognitionPoll {
  Kernel *kernel = nullptr;
};

inline bool poll(void *opaque, uint64_t) {
  auto *context = static_cast<RecognitionPoll *>(opaque);
  return context && context->kernel && !kernel_terminating(*context->kernel);
}

inline Status recognition_status(recognition::Status status) {
  switch (status) {
  case recognition::Status::Recognized: return Status::Admitted;
  case recognition::Status::NotRecognized: return Status::NotRecognized;
  case recognition::Status::Malformed: return Status::Malformed;
  case recognition::Status::Ambiguous: return Status::Ambiguous;
  case recognition::Status::LimitExceeded: return Status::ResourceExhausted;
  case recognition::Status::Interrupted: return Status::Interrupted;
  }
  return Status::Invalid;
}

inline bool exact_source(const Snapshot &snapshot,
                         const recognition::Identity &identity,
                         const std::vector<int32_t> &clause,
                         const Source *&source) {
  if (!identity.occurrence || identity.occurrence > snapshot.sources.size())
    return false;
  const Source &candidate = snapshot.sources[identity.occurrence - 1];
  if (candidate.occurrence != identity.occurrence ||
      candidate.native_proof_id != identity.native_proof_id ||
      key(candidate.clause) != key(clause))
    return false;
  source = &candidate;
  return true;
}

} // namespace detail

inline Result admit(Kernel &kernel, const Request &request, Admission &output) {
  Result result;
  if (!detail::valid(request))
    return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Status::UnsupportedProof;
    return result;
  }
  if (!detail::applicable(kernel)) {
    result.status = Status::Inapplicable;
    return result;
  }
  Snapshot snapshot;
  const detail::CaptureStatus captured =
      detail::capture(kernel, request.snapshot, snapshot, &result);
  if (captured != detail::CaptureStatus::Complete) {
    result.status = captured == detail::CaptureStatus::Interrupted
                        ? Status::Interrupted
                    : captured == detail::CaptureStatus::Exhausted
                        ? Status::ResourceExhausted
                        : Status::Invalid;
    return result;
  }
  recognition::Image image;
  detail::RecognitionPoll context{&kernel};
  const recognition::Status recognized = recognition::recognize(
      snapshot.premises, request.recognition, detail::poll, &context, image);
  result.recognition_work = image.inspected_work;
  if (recognized != recognition::Status::Recognized) {
    result.status = detail::recognition_status(recognized);
    return result;
  }
  if (snapshot.external_extent >= uint32_t(INT32_MAX)) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  const int32_t first_fresh = static_cast<int32_t>(snapshot.external_extent + 1);
  certificate::Plan plan;
  const certificate::Status planned = certificate::plan(
      image.variables, first_fresh, request.certificate, plan);
  if (planned != certificate::Status::Ok) {
    result.status = planned == certificate::Status::LimitExceeded ||
                            planned == certificate::Status::ArithmeticOverflow ||
                            planned == certificate::Status::FreshNamespaceCollision
                        ? Status::ResourceExhausted
                        : Status::Invalid;
    return result;
  }
  if (plan.accounting.fresh_variables > UINT32_MAX ||
      uint64_t(snapshot.external_extent) + plan.accounting.fresh_variables >
          uint64_t(INT32_MAX)) {
    result.status = Status::ResourceExhausted;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    return result;
  }
  Snapshot revalidated;
  const detail::CaptureStatus recaptured =
      detail::capture(kernel, request.snapshot, revalidated);
  if (recaptured == detail::CaptureStatus::Interrupted) {
    result.status = Status::Interrupted;
    return result;
  }
  if (recaptured != detail::CaptureStatus::Complete ||
      !(revalidated == snapshot)) {
    result.status = Status::Stale;
    return result;
  }
  Admission local;
  local.ready_ = true;
  local.request_ = request;
  local.snapshot_ = std::move(snapshot);
  local.image_ = std::move(image);
  local.plan_ = std::move(plan);
  result.status = Status::Admitted;
  result.additions = local.plan_.accounting.additions;
  result.deletions = local.plan_.accounting.deletions;
  result.fresh_variables = local.plan_.accounting.fresh_variables;
  local.result_ = result;
  output = std::move(local);
  return result;
}

inline Result prepare(Kernel &kernel, const Admission &admission,
                      Transaction &output) {
  Result result = admission.result();
  if (!admission.ready()) {
    result.status = Status::Invalid;
    return result;
  }
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Status::UnsupportedProof;
    return result;
  }
  if (!detail::applicable(kernel)) {
    result.status = Status::Inapplicable;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    return result;
  }
  Snapshot current;
  const detail::CaptureStatus captured =
      detail::capture(kernel, admission.request().snapshot, current);
  if (captured == detail::CaptureStatus::Interrupted) {
    result.status = Status::Interrupted;
    return result;
  }
  if (captured != detail::CaptureStatus::Complete ||
      !(current == admission.snapshot())) {
    result.status = Status::Stale;
    return result;
  }

  Transaction local;
  local.request_ = admission.request();
  local.snapshot_ = admission.snapshot();
  local.starting_clause_id_ = kernel.next_clause_id;
  local.arena_generation_ = kernel.arena_generation;
  local.starting_nvars_ = kernel.nvars;
  local.starting_extent_ = static_cast<uint32_t>(kernel.external_to_internal.size());
  local.starting_reconstruction_ = kernel.reconstruction.size();
  local.starting_proof_ = kernel.proof;
  local.first_fresh_ = admission.plan().first_fresh;
  local.accounting_ = admission.plan().accounting;
  local.events_.reserve(admission.plan().events.size());
  if (local.starting_clause_id_ > uint64_t(INT64_MAX) ||
      local.accounting_.additions >
          uint64_t(INT64_MAX) - local.starting_clause_id_) {
    result.status = Status::ResourceExhausted;
    return result;
  }

  struct OriginalIdentity {
    uint64_t token = 0;
    uint64_t occurrence = 0;
    bool capture_local = false;
  };
  std::map<std::vector<int32_t>, OriginalIdentity> originals;
  for (const recognition::AmoPremise &amo : admission.image().amos) {
    const std::vector<int32_t> clause =
        {-amo.first_variable, -amo.second_variable};
    const Source *source = nullptr;
    if (!detail::exact_source(admission.snapshot(), amo.identity, clause, source) ||
        originals.count(detail::key(clause))) {
      result.status = Status::Invalid;
      return result;
    }
    originals.emplace(detail::key(clause),
                      OriginalIdentity{
                          source->native_proof_id, source->occurrence,
                          source->identity_kind ==
                              SourceIdentityKind::CaptureLocal});
  }
  std::map<std::vector<int32_t>, std::vector<uint64_t>> generated;
  uint64_t next_id = local.starting_clause_id_;
  for (const certificate::Event &planned : admission.plan().events) {
    if (!detail::valid_clause(planned.clause) ||
        (planned.kind == certificate::EventKind::AddRat &&
         planned.clause.empty())) {
      result.status = Status::Invalid;
      return result;
    }
    ReplayEvent event;
    event.kind = planned.kind;
    event.role = planned.role;
    event.layer = planned.layer;
    event.clause = planned.clause;
    const auto clause_key = detail::key(event.clause);
    if (event.kind != certificate::EventKind::Delete) {
      event.id = ++next_id;
      generated[clause_key].push_back(event.id);
    } else if (event.role == certificate::EventRole::RetireOriginalAmo) {
      const auto found = originals.find(clause_key);
      if (found == originals.end()) {
        result.status = Status::Invalid;
        return result;
      }
      event.deleted_id = found->second.token;
      event.source_occurrence = found->second.occurrence;
      event.deleted_id_is_capture_local = found->second.capture_local;
      originals.erase(found);
    } else {
      auto found = generated.find(clause_key);
      if (found == generated.end() || found->second.empty()) {
        result.status = Status::Invalid;
        return result;
      }
      event.deleted_id = found->second.back();
      found->second.pop_back();
    }
    local.events_.push_back(std::move(event));
  }
  if (!originals.empty() || local.events_.empty() ||
      local.events_.back().kind != certificate::EventKind::AddRup ||
      local.events_.back().role != certificate::EventRole::Conclusion ||
      !local.events_.back().clause.empty() ||
      local.events_.size() != local.accounting_.events ||
      next_id - local.starting_clause_id_ != local.accounting_.additions) {
    result.status = Status::Invalid;
    return result;
  }
  local.ready_ = true;
  result.status = Status::Prepared;
  local.result_ = result;
  output = std::move(local);
  return result;
}

inline Result publish(Kernel &kernel, const Transaction &transaction) {
  Result result = transaction.result_;
  if (!transaction.ready_) {
    result.status = Status::Invalid;
    return result;
  }
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) {
    result.status = Status::UnsupportedProof;
    return result;
  }
  if (!detail::applicable(kernel)) {
    result.status = Status::Inapplicable;
    return result;
  }
  if (kernel_terminating(kernel)) {
    result.status = Status::Interrupted;
    return result;
  }
  if (kernel.next_clause_id != transaction.starting_clause_id_ ||
      kernel.arena_generation != transaction.arena_generation_ ||
      kernel.nvars != transaction.starting_nvars_ ||
      kernel.external_to_internal.size() != transaction.starting_extent_ ||
      kernel.reconstruction.size() != transaction.starting_reconstruction_ ||
      kernel.proof != transaction.starting_proof_) {
    result.status = Status::Stale;
    return result;
  }
  Snapshot current;
  const detail::CaptureStatus captured = detail::capture(
      kernel, transaction.request_.snapshot, current);
  if (captured == detail::CaptureStatus::Interrupted) {
    result.status = Status::Interrupted;
    return result;
  }
  if (captured != detail::CaptureStatus::Complete ||
      !(current == transaction.snapshot_)) {
    result.status = Status::Stale;
    return result;
  }

  // No interruption point exists below this boundary: proof bytes and their
  // monotonically reserved namespace cannot be rolled back.
  kernel.close_startup_certificate();
  Var reserved = INVALID_VAR;
  if (!kernel_reserve_proof_variables(
          kernel, static_cast<uint32_t>(transaction.accounting_.fresh_variables),
          reserved) ||
      uint64_t(reserved) + 1 != uint64_t(transaction.first_fresh_))
    std::abort();
  for (const ReplayEvent &event : transaction.events_) {
    if (event.kind == certificate::EventKind::Delete) {
      if (kernel.proof) {
        if (event.source_occurrence) {
          // Original AMOs are live kernel clauses, so retain their ordinary
          // checker-population classification. Capture-local replay tokens
          // are erased here: DSR/DRAT delete by literal multiset, while LRAT
          // was rejected before admission.
          std::vector<Lit> internal;
          internal.reserve(event.clause.size());
          for (int32_t external : event.clause) {
            const Lit literal = kernel_internalize(kernel, external);
            if (literal == INVALID_LIT)
              std::abort();
            internal.push_back(literal);
          }
          kernel.proof->del(kernel.next_clause_id,
                            event.deleted_id_is_capture_local
                                ? 0 : event.deleted_id,
                            internal.data(),
                            static_cast<uint32_t>(internal.size()));
        } else if (!kernel.proof->del_external(
                       kernel.next_clause_id, event.deleted_id,
                       event.clause.empty() ? nullptr : event.clause.data(),
                       static_cast<uint32_t>(event.clause.size()))) {
          std::abort();
        }
      }
      continue;
    }
    if (++kernel.next_clause_id != event.id)
      std::abort();
    if (kernel.proof) {
      const bool emitted = event.kind == certificate::EventKind::AddRat
          ? kernel.proof->add_external_rat(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                static_cast<uint32_t>(event.clause.size()))
          : kernel.proof->add_external_rup(
                event.id, event.clause.empty() ? nullptr : event.clause.data(),
                static_cast<uint32_t>(event.clause.size()));
      if (!emitted)
        std::abort();
    }
  }
  kernel.inconsistent = true;
  result.status = Status::Applied;
  return result;
}

inline Result apply(Kernel &kernel, const Request &request) {
  Admission admission;
  Result result = admit(kernel, request, admission);
  if (result.status != Status::Admitted)
    return result;
  Transaction transaction;
  result = prepare(kernel, admission, transaction);
  if (result.status != Status::Prepared)
    return result;
  return publish(kernel, transaction);
}

} // namespace tactix::direct_php_live

// Observation seam for raw, checker and solver clause populations.
#pragma once

#include "kernel/types.hpp"
#include "kernel/checker_names.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace tactix {

struct Kernel;

// Callback-local view of solver literals, proof-only DIMACS literals and native
// DSR's prefix/suffix helpers, exposing external identities without a buffer;
// `internal_to_external` may be null only for the identity map.
struct PopulationClauseView {
  const Lit *internal = nullptr;
  const int32_t *external = nullptr;
  const std::vector<Var> *internal_to_external = nullptr;
  uint32_t body_size = 0;
  // When set, expose this body literal first, keeping the others' order: SR
  // lines can repivot the checker clause before witness metadata, which the
  // observer never sees.
  uint32_t first_body_index = std::numeric_limits<uint32_t>::max();
  int32_t prefix = 0;
  int32_t suffix = 0;

  uint32_t size() const noexcept {
    return body_size + static_cast<uint32_t>(prefix != 0) +
           static_cast<uint32_t>(suffix != 0);
  }

  int32_t at(uint32_t index) const noexcept {
    if (prefix) {
      if (!index)
        return prefix;
      --index;
    }
    if (index < body_size) {
      if (first_body_index < body_size) {
        if (!index)
          index = first_body_index;
        else if (index <= first_body_index)
          --index;
      }
      if (external)
        return external[index];
      Lit literal = internal[index];
      if (internal_to_external) {
        const Var variable = var_of(literal);
        if (variable >= internal_to_external->size())
          return 0;
        literal = make_lit((*internal_to_external)[variable],
                           negated(literal));
      }
      return externalize(literal);
    }
    return suffix;
  }

  static PopulationClauseView internal_lits(
      const Lit *literals, uint32_t count,
      const std::vector<Var> *map = nullptr) noexcept {
    PopulationClauseView result;
    result.internal = literals;
    result.internal_to_external = map;
    result.body_size = count;
    return result;
  }

  static PopulationClauseView external_lits(const int32_t *literals,
                                             uint32_t count) noexcept {
    PopulationClauseView result;
    result.external = literals;
    result.body_size = count;
    return result;
  }
};

enum class RawInputEventKind : uint8_t { Clause, Outcome, Finished };

enum RawInputDisposition : uint32_t {
  RawNone = 0,
  RawAdmitted = 1u << 0,
  RawDropped = 1u << 1,
  RawRepeatedLiteral = 1u << 2,
  RawTautology = 1u << 3,
  RawRootSatisfied = 1u << 4,
  RawRootShortened = 1u << 5,
  RawDuplicate = 1u << 6,
  RawEmpty = 1u << 7,
  RawUnit = 1u << 8,
  RawBinary = 1u << 9,
  RawLarge = 1u << 10,
  RawContradiction = 1u << 11,
  RawAlreadyInconsistent = 1u << 12,
};

enum class PopulationSourceClassification : uint8_t {
  PendingInputClassification,
  Eligible,
  Ineligible,
};

struct RawInputPopulationEvent {
  RawInputEventKind kind = RawInputEventKind::Clause;
  uint64_t raw_input_id = 0;
  uint32_t disposition = RawNone;
  bool normalized_available = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::PendingInputClassification;
  PopulationClauseView raw;
  PopulationClauseView normalized;
};

enum class CheckerRoute : uint8_t {
  None,
  Dsr,
  Pbp,
};

enum class CheckerNaming : uint8_t { None, LiteralMultiset };

enum class CheckerPopulationEventKind : uint8_t {
  ObserverBound,
  ObserverUnbound,
  ObserverUnboundIncomplete,
  RouteOpened,
  RouteCommitted,
  ClausalRouteDiscarded,
  InputSeed,
  InputFinished,
  Add,
  DeleteRequested,
  StageAdd,
  StageDelete,
  StageDeleteRequested,
  PendingCommitBegin,
  PendingDeletesCommit,
  PendingCommitEnd,
};

enum class CheckerClauseRole : uint8_t {
  None,
  Input,
  Rup,
  Rat,
  SubstitutionRedundancy,
  Empty,
  DerivedUnit,
  ExternalRup,
  ExternalRat,
  DsrSplitHelper,
  DsrSubstitutionHelper,
};

struct CheckerPopulationEvent {
  CheckerPopulationEventKind kind = CheckerPopulationEventKind::Add;
  CheckerClauseRole role = CheckerClauseRole::None;
  CheckerRoute route = CheckerRoute::None;
  CheckerNaming naming = CheckerNaming::None;
  uint64_t line_id = 0;
  uint64_t checker_id = 0;
  uint64_t transaction_id = 0;
  uint32_t certificate_offset = 0; // logical external coordinates -> query names
  // Borrowed per callback; ordinary clauses also use this gap map.
  std::span<const DsrHelperNames> checker_helpers;
  bool clause_is_named = false;
  bool proof_only = false;
  PopulationSourceClassification source_classification =
      PopulationSourceClassification::Ineligible;
  PopulationClauseView clause;

  int32_t checker_literal_at(uint32_t index) const noexcept {
    const int32_t literal = clause.at(index);
    if (clause_is_named || !literal) return literal;
    const uint32_t variable = external_variable_of(literal);
    uint32_t helpers = 0;
    const auto after = std::lower_bound(checker_helpers.begin(), checker_helpers.end(), variable,
        [](const DsrHelperNames &entry, uint32_t v) { return entry.extent < v; });
    if (after != checker_helpers.begin()) helpers = std::prev(after)->helpers;
    const int32_t name = static_cast<int32_t>(uint64_t(variable)+certificate_offset+helpers);
    return literal < 0 ? -name : name;
  }
};

enum class SolverPopulationEventKind : uint8_t {
  Add,
  Delete,
  UnitMaterialized,
  MetadataUpdate,
  TransactionBegin,
  StageAdd,
  StageDelete,
  TransactionCommit,
};

enum class SolverClauseKind : uint8_t { Empty, Unit, Binary, Large };

struct SolverPopulationEvent {
  SolverPopulationEventKind kind = SolverPopulationEventKind::Add;
  SolverClauseKind clause_kind = SolverClauseKind::Empty;
  uint64_t logical_id = 0;
  uint32_t copies = 1;
  bool redundant = false;
  uint64_t transaction_id = 0;
  PopulationClauseView clause;
};

enum class SolverReplacementPhase : uint8_t { Begin, End };
enum class SolverReplacementOutcome : uint8_t {
  Pending,
  Refuted,
  Unit,
  Binary,
  Large,
};

struct SolverReplacementEvent {
  SolverReplacementPhase phase = SolverReplacementPhase::Begin;
  SolverReplacementOutcome outcome = SolverReplacementOutcome::Pending;
  uint64_t predecessor_id = 0;
  uint64_t replacement_id = 0;
  PopulationClauseView predecessor;
  PopulationClauseView replacement;
};

enum class WeakeningFamily : uint8_t { Bve, Bce, Condition, Cce, Substitute };
enum class WeakeningEventKind : uint8_t { Prepare, Commit, Cancel, Reconstruction };

// Prepare borrows the full source clause before deletion and the signed repair
// cube; Commit follows the solver deletion. Binary ids come from observed
// additions.
struct WeakeningPopulationEvent {
  WeakeningEventKind kind = WeakeningEventKind::Prepare;
  WeakeningFamily family = WeakeningFamily::Bve;
  uint64_t transaction_id = 0;
  uint64_t source_id = 0; // exact for large clauses; ignored for binaries
  uint64_t reconstruction_offset = 0;
  uint64_t reconstruction_words = 0;
  bool binary = false;
  bool all_binary_copies = false;
  PopulationClauseView clause;
  PopulationClauseView witness;
};

// Callbacks never throw or retain a view; null costs one test.
class PopulationObserver {
public:
  virtual ~PopulationObserver() = default;
  virtual PopulationObserver *forwarded_observer() const noexcept { return nullptr; }
  virtual void observe_binding(const Kernel &, bool /*bound*/) noexcept {}
  virtual void observe(const RawInputPopulationEvent &) noexcept {}
  virtual void observe(const CheckerPopulationEvent &) noexcept {}
  virtual void observe(const SolverPopulationEvent &) noexcept {}
  virtual void observe(const SolverReplacementEvent &) noexcept {}
  virtual bool records_weakening() const noexcept { return false; }
  virtual void observe(const WeakeningPopulationEvent &) noexcept {}
};

// An immutable forwarding link that interceptors cannot bypass.
class ForwardingPopulationObserver : public PopulationObserver {
  PopulationObserver *const previous_;
protected:
  explicit ForwardingPopulationObserver(PopulationObserver *previous) : previous_(previous) {}
  virtual void intercept(const RawInputPopulationEvent &) noexcept {}
  virtual void intercept(const CheckerPopulationEvent &) noexcept {}
  virtual void intercept(const SolverPopulationEvent &) noexcept {}
  virtual void intercept(const SolverReplacementEvent &) noexcept {}
  virtual void intercept(const WeakeningPopulationEvent &) noexcept {}
public:
  PopulationObserver *forwarded_observer() const noexcept final { return previous_; }
  bool records_weakening() const noexcept final {
    return previous_ && previous_->records_weakening();
  }
  void observe(const RawInputPopulationEvent &e) noexcept final {
    intercept(e); if (previous_) previous_->observe(e);
  }
  void observe(const CheckerPopulationEvent &e) noexcept final {
    intercept(e); if (previous_) previous_->observe(e);
  }
  void observe(const SolverPopulationEvent &e) noexcept final {
    intercept(e); if (previous_) previous_->observe(e);
  }
  void observe(const SolverReplacementEvent &e) noexcept final {
    intercept(e); if (previous_) previous_->observe(e);
  }
  void observe(const WeakeningPopulationEvent &e) noexcept final {
    intercept(e); if (previous_) previous_->observe(e);
  }
};

inline bool population_observer_contains(const PopulationObserver *chain,
                                         const PopulationObserver *observer) noexcept {
  for (; chain; chain = chain->forwarded_observer())
    if (chain == observer) return true;
  return false;
}

uint64_t kernel_prepare_weakening(Kernel &, WeakeningFamily,
    PopulationClauseView clause, PopulationClauseView witness,
    uint64_t source_id, bool binary, bool all_binary_copies = false,
    uint64_t reconstruction_words = 0) noexcept;
void kernel_finish_weakening(Kernel &, uint64_t token, bool committed) noexcept;
// Record a producer's appended reconstruction lemma.
void kernel_record_reconstruction(Kernel &, WeakeningFamily, uint64_t begin) noexcept;

// Owns only observer staging; never changes solver state.
class ScopedWeakeningCapture {
public:
  ScopedWeakeningCapture(Kernel &, WeakeningFamily, const Lit *clause,
      uint32_t size, Lit witness, uint64_t source_id, bool binary,
      bool all_binary_copies = false) noexcept;
  ScopedWeakeningCapture(Kernel &, WeakeningFamily, PopulationClauseView clause,
      PopulationClauseView witness, uint64_t source_id, bool binary,
      bool all_binary_copies, uint64_t reconstruction_words) noexcept;
  ~ScopedWeakeningCapture();
  void commit() noexcept;
  ScopedWeakeningCapture(const ScopedWeakeningCapture &) = delete;
  ScopedWeakeningCapture &operator=(const ScopedWeakeningCapture &) = delete;
private:
  Kernel &kernel_;
  uint64_t token_ = 0;
};

// Install a non-owning observer; returns the previous binding.
PopulationObserver *kernel_bind_population_observer(
    Kernel &, PopulationObserver *) noexcept;

// Reconcile an assigned Proof pointer with the kernel binding.
void kernel_sync_population_observer(Kernel &) noexcept;

// Settles pending transactions for the selected proof route while the observer
// lives, then restores the previous binding; an unpublished streaming-PBP
// prefix reports an incomplete unbind. Construct after Kernel and Proof unless
// their destruction order already ends Proof first.
class ScopedPopulationObserverBinding {
public:
  ScopedPopulationObserverBinding(Kernel &, PopulationObserver &) noexcept;
  ~ScopedPopulationObserverBinding();
  ScopedPopulationObserverBinding(const ScopedPopulationObserverBinding &) =
      delete;
  ScopedPopulationObserverBinding &
  operator=(const ScopedPopulationObserverBinding &) = delete;

private:
  Kernel *kernel_ = nullptr;
  PopulationObserver *previous_ = nullptr;
};

} // namespace tactix

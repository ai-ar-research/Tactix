// Proof backend (plan §5.2): rules emit typed logical events. Native DSR is
// the canonical stream; witnessless DSR additions cover the existing RUP/RAT
// vocabulary and direct SR additions carry a substitution witness.
// Streaming + buffered: never held in memory (§6).
#pragma once

#include "kernel/types.hpp"
#include "proof/dsr_to_pbp.hpp"
#include "population/population_observer.hpp"

#include <array>
#include <compare>
#include <functional>
#include <map>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

enum class ProofFormat { DsrBinary, DsrAscii, Pbp, PbpStreaming };

// One route for a proof that cannot be written: say why and stop, with no verdict.
[[noreturn]] void proof_error(const char *what);

struct DsrLiteralMapping {
  Lit from = INVALID_LIT;
  Lit to = INVALID_LIT;
};

// Preserve the semantic pivot override using an encoding shared by dsr-trim
// and Trestle. Classify images relative to !C: an unmapped pivot is Direct,
// a true image permits Repivot, all-false images permit Rup, one open image
// permits Lemma, and multiple open images use a temporary Extension.
// Every form preserves the X7 application; see docs/dev/calculus.md section 6d.
enum class DsrWitnessForm : uint8_t { Direct, Repivot, Rup, Lemma, Extension };

struct DsrWitnessPlan {
  DsrWitnessForm form = DsrWitnessForm::Direct;
  uint32_t pivot = 0;      // clause index of the emitted pivot
  Lit lemma = INVALID_LIT; // Lemma: open image appended to the clause
};

// One already-authenticated complete XOR encoding, named only by its semantic
// equation.  Streaming PBP turns the bounded CNF row into two persistent
// cutting-planes bounds, then combines cached bounds for later live reasons.  Premise authentication and lifetime remain the caller's job.
struct PbpParityEquation {
  std::vector<int32_t> variables;
  bool rhs = false;

  friend bool operator==(const PbpParityEquation &,
                         const PbpParityEquation &) = default;
  friend std::strong_ordering operator<=>(const PbpParityEquation &first,
                                           const PbpParityEquation &second) {
    if (const auto order = first.variables <=> second.variables; order != 0)
      return order;
    return first.rhs <=> second.rhs;
  }
};

class ProofWriter; // proof/proof_writer.hpp, private to src/proof

struct PbpParityBounds {
  uint64_t lower = 0;
  uint64_t upper = 0;
  std::vector<int32_t> carries;
  // Which numbering wrote lower and upper. A conversion restarts the numbering and
  // a witness deletes the rows it touches, so a row from another numbering is written
  // again before its next use.
  uint64_t numbering = 0;
};

class PbpParityBoundsCache {
public:
  uint64_t generation() const noexcept { return generation_; }
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  size_t size() const noexcept { return entries_.size(); }
  uint64_t retained_bytes() const noexcept;

private:
  friend class Proof;
  friend class ProofWriter;
  static uint64_t empty_retained_bytes() noexcept;
  uint64_t stream_identity_ = 0;
  uint64_t generation_ = 0;
  std::map<PbpParityEquation, PbpParityBounds> entries_;
  // Exact cached value of the historical retained_bytes() scan.  The logical
  // base deliberately excludes this representation-only counter so existing
  // resource boundaries and proof trajectories do not move.
  uint64_t retained_bytes_ = 0;
};

struct PbpParityReasonPlan {
  uint64_t stream_identity = 0;
  uint64_t cache_generation = 0;
  uint64_t proof_constraints = 0;
  uint64_t retained_bytes = 0;
  uint32_t starting_external_extent = 0;
  uint32_t helper_variables = 0;
  std::vector<int32_t> odd_variables;
  bool valid = false;
};

// `lits[0]` is the semantic pivot; `mappings` use positive sources.
DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings);

// Automatic proof selection (plan §5). The ledger owns every kernel-visible
// decision, as in every mode, and the writer counts the whole-run streaming PBP;
// a native DSR copy of the same proof events is the proof. A startup PB refutation,
// frequent parity reasons, or an event the copy cannot follow converts it to
// PBP. Otherwise the DSR copy is the proof at the empty clause.
enum class AutomaticProofReason : uint8_t {
  Pending,              // the DSR copy is still live
  DsrSelected,          // the empty clause kept the DSR copy
  StartupPbRefutation,  // Lucky parity / count-preservation terminal PB
  ParityReason,         // compact GF(2) reason (affine root closure, search)
  CopyDeclined,         // the DSR copy rejected an event PBP accepted
  CopyWriteFailed,      // the DSR stage could not be written
  DestinationUnsupported, // not a regular file: no copy was opened
  StageUnavailable,     // no stage file could be created
  ForcedSwitch,         // TACTIX_AUTO_PROOF_SWITCH_AT (validation only)
};
const char *automatic_proof_reason_name(AutomaticProofReason) noexcept;

struct LazyPbpReport {
  // off, dsr (still DSR), converted, dsr-kept, startup-pb, fallback, or why the
  // run stayed eager (off-*).
  const char *state = "off";
  uint64_t dsr_bytes = 0; // DSR bytes converted at the switch
  uint64_t pbp_bytes = 0; // PBP text the conversion wrote, or that DSR replaced
  uint64_t nanos = 0;     // time spent converting
};

class Proof {
public:
  Proof(); // defined out of line: ProofWriter is complete only in src/proof
  ~Proof();

  bool open(const char *path, ProofFormat format);
  // The DSR writer alone, for its own tests. A production kernel never holds
  // one: open() always gives the kernel a streaming ledger.
  bool open_dsr_writer(const char *path, ProofFormat format);
  // Automatic selection: streaming PBP at `path` plus a binary (or ASCII) DSR
  // copy. `stage_path`, when given, names the copy's caller-owned stage file;
  // otherwise a sibling of `path` (renamed into place) or an anonymous file
  // (copied into place) is used. Returns false only when `path` cannot be
  // opened; without a usable stage the run is whole-run PBP.
  // With `dsr_only`, the run writes DSR to `path` and never converts: the search
  // and the kernel's view of the proof are those of every other run (N2).
  bool open_automatic(const char *path, bool binary_dsr,
                      const char *stage_path = nullptr,
                      bool dsr_only = false);
  // A run with no proof: the ledger every proof mode's search answers to,
  // with no writer.
  bool open_nowhere();
  // A proof is written. Read only to name the published route (api.cpp).
  bool writes() const noexcept { return writer_ != nullptr; }
  bool automatic() const noexcept;
  // A startup parity refutation; the run ends on it. `write_pb` writes it to the
  // PBP stream it is given, and a DSR-only proof spells it in clauses instead.
  // False, with nothing written, when the stream is already published.
  bool refute_parity(const std::vector<PbpParityEquation> &equations,
                     const std::function<bool(FILE *)> &write_pb);
  // A startup counting refutation; the run ends on it. `write_pb` writes it to the
  // PBP stream it is given, and is not called when no proof is being written. A
  // DSR-only proof writes nothing yet: it takes `input`, the original DIMACS
  // literals, and is pending until a follow-on solve writes the DSR
  // (frontend/dsr_follow_on.hpp). False, with nothing written or taken, when the
  // stream is already published.
  bool refute_by_counting(const std::function<bool(FILE *)> &write_pb,
                          std::vector<int32_t> &input);
  bool dsr_follow_on_pending() const noexcept;
  const std::vector<int32_t> &follow_on_input() const noexcept;
  // Open `f`, a fresh proof, as DSR-only on this proof's untouched destination.
  bool open_dsr_follow_on(Proof &f);
  // After `f`'s run reported UNSAT: true when its DSR is complete, which then
  // names this proof's route.
  bool settle_dsr_follow_on(const Proof &f);
  AutomaticProofReason automatic_reason() const noexcept;
  // The selection is final once the copy is discarded or kept.
  bool automatic_dsr_live() const noexcept;
  void release_lazy_event() noexcept;
  // Whether the run paid the DSR-to-PBP conversion (see LazyPbpReport).
  const LazyPbpReport &lazy_pbp_report() const noexcept;
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  // Whole-run PBP privately stages input normalization. At the original-input
  // action boundary the caller may commit a complete native PB refutation.
  // The first subsequent clausal event publishes the streaming-PBP header.
  void finish_input_transaction();
  bool startup_pb_available() const {
    return live_stream() && !input_transaction_ && !published_;
  }
  bool commit_clause_stream();
  FILE *begin_pb();
  bool finish_pb();
  // A complete startup PB refutation owns the stream, which takes no further clause
  // events. No search follows it.
  bool startup_pb_written() const { return terminal_; }
  // Delete by specification for the rest of the run: a proof attached after
  // replayed input cannot name the live kernel's clauses by their ids (see
  // attach_replayed_input_proof).
  void disable_pbp_id_deletions();
  // Published proof feature, used only to choose an independent final checker;
  // null without a proof. The runner also scans the finished certificate and
  // rejects disagreement.
  const char *verification_route() const;
  void bind_variable_map(const std::vector<Var> *map);
  void bind_external_variable_map(const std::vector<Var> *map);
  void bind_population_observer(PopulationObserver *) noexcept;
  // Settle only a route whose bytes already have a committed destination.
  // An unpublished streaming-PBP prefix is incomplete until a clausal event
  // commits its header.
  bool settle_population_observer() noexcept;
  PopulationObserver *population_observer() const noexcept {
    return population_observer_;
  }
  // --- proof-delta vocabulary (subset live in phase 2) ---
  // add-RUP. Native DSR stages consecutive RUP units so a following deletion block can
  // be ordered before the batch with a shared proof-only split.
  void add_rup(uint64_t id, const Lit *lits, uint32_t size);
  // add-RAT: `lits` MUST be pivot-first (the checker infers RAT on the first
  // literal). Factor is the first RAT-emitting rule (plan §5.2 add-RAT
  // primitive).
  void add_rat(uint64_t id, const Lit *lits, uint32_t size);
  // Add a substitution-redundant clause. `lits[0]` is the DSR pivot,
  // `true_lits` are mapped to true, and each mapping maps its source literal
  // to its target literal. `external_variables` bounds every external variable
  // in the proof-visible formula, including retired variables. The general
  // encoding temporarily uses the next external variable and deletes every
  // clause containing it before returning, so later solver extensions may use
  // that identity. Returns false for terminal PBP or exhaustion of the external
  // literal representation, before emitting any clause.
  // `propagates`: the clause is unit at the root. No witnessed line may
  // propagate when read, so it takes the general encoding and enters by RUP
  // (docs/dev/implementation-notes.md, native DSR checking).
  bool add_sr(uint64_t id, const Lit *lits, uint32_t size,
              const Lit *true_lits, uint32_t ntrue,
              const DsrLiteralMapping *mappings, uint32_t nmappings,
              uint32_t external_variables, bool propagates = false);
  void add_empty(uint64_t id);
  // root-level propagation-derived unit: explicit in DSR and streaming PBP
  void add_derived_unit(uint64_t id, Lit unit);
  // delete: DSR writes the literals; streaming PBP names the constraint
  void del(uint64_t line_id, uint64_t del_id, const Lit *lits,
           uint32_t size);

  // Proof-only extension clauses use DIMACS literals whose variables need not
  // exist in the solver kernel.  This keeps terminal extended-resolution
  // certificates streaming without allocating millions of inactive kernel
  // variables.
  bool add_external_rup(uint64_t id, const int32_t *lits, uint32_t size);
  bool add_external_rat(uint64_t id, const int32_t *lits, uint32_t size);
  // Compact live GF(2) consequence for streaming PBP.  Planning is pure and
  // bounded; publication rechecks the exact stream/cache generation before
  // writing.  `reason` is one entailed clause (or the empty root conflict),
  // and every equation's complete CNF premises must still be checker-visible.
  bool plan_pbp_parity_reason(
      const std::vector<PbpParityEquation> &, const int32_t *reason,
      uint32_t reason_size, uint32_t starting_external_extent,
      const PbpParityBoundsCache &, PbpParityReasonPlan &) const;
  // Commit a plan whose exact equations/reason and stream/cache generation
  // were validated by the caller immediately before its mutation boundary.
  // This deliberately does not repeat the allocation-heavy pure planner.
  bool add_prevalidated_pbp_parity_reason(
      uint64_t kernel_clause_id, const std::vector<PbpParityEquation> &,
      const int32_t *reason, uint32_t reason_size,
      PbpParityBoundsCache &, const PbpParityReasonPlan &);
  bool del_external(uint64_t line_id, uint64_t del_id,
                    const int32_t *lits, uint32_t size);
  // Keep an already-emitted proof-only clause available for later RUP steps.
  // The registry is a checker-population boundary. add_sr deletes every registered
  // clause before a witnessed line, then advances the epoch so clients cannot
  // reuse stale support.
  bool retain_external(uint64_t id, const int32_t *lits, uint32_t size);
  uint64_t persistent_external_epoch() const {
    return persistent_external_epoch_;
  }
  uint64_t persistent_external_bytes() const;

  // Exact, already authenticated checker premises. These are not duplicate
  // additions: native deletion may proceed while the original checker copy
  // remains. Every deletion primitive honors the same literal-body lease.
  // Registration is transactional and caller-metered. Witnessed SR explicitly
  // retires groups whose variables it changes before emitting its witness.
  struct DeferredDeletionLimits {
    uint64_t maximum_pending_deletions;
    uint64_t maximum_pending_literal_work;
    constexpr DeferredDeletionLimits(uint64_t deletions = 0, uint64_t work = 0)
        : maximum_pending_deletions(deletions), maximum_pending_literal_work(work) {}
  };
  using PremiseGroup = std::vector<std::vector<int32_t>>;
  // Read-only reservation for the physical helper deletions queued after a
  // new lease transaction. Every shared owner's common minimum is respected.
  bool can_defer_premise_group_deletions(const std::vector<PremiseGroup> &,
      DeferredDeletionLimits, const std::function<bool(uint64_t)> &) const;
  bool retain_premise_groups(const std::vector<PremiseGroup> &,
      uint64_t maximum_groups, uint64_t maximum_literals,
      uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
      std::vector<uint64_t> &handles, DeferredDeletionLimits limits = {});
  bool premise_group_active(uint64_t) const;
  bool premise_group_matches(uint64_t, const PremiseGroup &) const;
  uint64_t premise_epoch() const { return premise_epoch_; }
  uint64_t premise_literals() const { return premise_literals_; }
  uint64_t premise_bytes() const { return premise_bytes_; }
  // Cumulative serializer overhead, not outstanding occupancy.
  uint64_t premise_deletions_deferred() const { return premise_deferred_; }
  uint64_t premise_pending_deletions() const { return premise_pending_deletions_; }
  uint64_t premise_pending_literal_work() const { return premise_pending_literal_work_; }
  uint64_t premise_maintenance_work() const { return premise_maintenance_work_; }
  void release_premise_groups(const std::vector<uint64_t> &, uint64_t line_id);

  void flush(); // signal-safe enough: fflush on the buffered stream

  // Streaming PBP: root units forced by a propagation that ended in a root
  // conflict are staged instead of written. The staged batch is dropped when
  // the very next proof event is the empty clause: each unit is RUP over the
  // same database, which VeriPB propagates while checking that clause. Any
  // other event first writes the batch exactly as it would have been written.
  // Solver state (unit ids, root tables) is unaffected either way.
  void begin_deferred_root_units() noexcept;
  void end_deferred_root_units() noexcept;

  // Streaming-PBP deletion accounting for tests (no proof effect).
  struct PbpDeletionCounters {
    uint64_t by_id = 0;        // every `del id` line
    uint64_t by_anonymous = 0; // ... resolved by the id-0 literal registry
    uint64_t by_rebind = 0;    // ... restated-input originals
    uint64_t spec_unit = 0, spec_binary = 0, spec_wide = 0;
    uint64_t anonymous_registered = 0;
    uint64_t rebinds_recorded = 0;
    uint64_t deferred_batches = 0, deferred_units = 0;
    uint64_t dropped_batches = 0, dropped_units = 0;
    uint32_t abandon_reason = 0; // first reason; 0 = never abandoned
    uint32_t abandon_width = 0;
    uint64_t abandon_line = 0; // derived-constraint count at abandonment
  };
  const PbpDeletionCounters &pbp_deletion_counters() const noexcept;

  // A tautological (or duplicate-carrying) lemma says nothing a checker can
  // use, and a RUP check of it passes without touching the formula. Rules must
  // therefore never hand one to the proof;
  // the emission sites test this and skip the inference instead (the
  // D-CONG-SKIP discipline: losing an inference is sound, emitting an
  // uncheckable line is not). Debug builds assert on it in `add_rup`/
  // `add_rat` so any future rule that regresses is caught by the debug fuzz
  // legs rather than by a gigabyte-scale tier proof.
  static bool tautological(const Lit *lits, uint32_t size);

  // Register one exact checker input premise.  Observer delivery is always
  // compiled; the optional proof shadow consumes the same event in diagnostic
  // builds.
  void seed_population(uint64_t id, const Lit *lits, uint32_t size);

#ifdef TACTIX_ENABLE_PROOF_SHADOW
  // Diagnostic N2 oracle; production pins compile none of this. A deletion
  // must be backed by one checker-visible copy, named by its literals. The
  // frontend seeds and arms the oracle only
  // after it has registered the complete input and its restatements. See
  // benchmarks/reports/2026-08-05-input-clause-normalization-n2.md.
  // The oracle lives in the writer: a run with no proof has none.
  void arm_population_shadow();
  bool population_shadow_available() const { return true; }
  bool population_contains(uint64_t id, const Lit *lits, uint32_t size) const;
  uint64_t unbacked_deletions() const;
  bool population_shadow() const;
#else
  void arm_population_shadow() {}
  bool population_shadow_available() const { return false; }
  bool population_contains(uint64_t, const Lit *, uint32_t) const {
    return true;
  }
  uint64_t unbacked_deletions() const { return 0; }
  bool population_shadow() const { return false; }
#endif

private:
  friend class ProofWriter;

  // --- The search ledger: every answer the search reads, the same in every
  // mode. The writer (writer_) exists only when a proof is written. ---
  struct StagedUnit {
    uint64_t id;
    Lit lit;
  };
  bool live_stream() const noexcept {
    return open_ && streaming_ && !terminal_;
  }
  // Draws the stream identity, then attaches `writer` (null: no proof).
  bool start(std::unique_ptr<ProofWriter> writer, bool streaming);
  // Explicit DSR on `destination` (owned); see open_automatic.
  bool open_dsr_only(FILE *destination, bool binary);
  // The first clausal event publishes the stream; false once it is terminal.
  bool commit();
  // Write (or, before the empty clause, drop) every staged root unit.
  void settle_staged(bool drop);
  void rup_event(uint64_t id, const Lit *lits, uint32_t size,
                 CheckerClauseRole role);
  void del_event(uint64_t line_id, uint64_t del_id, const Lit *lits,
                 uint32_t size);
  bool del_external_event(uint64_t line_id, uint64_t del_id,
                          const int32_t *lits, uint32_t size);
  void check_lifecycle() const; // Debug: the writer agrees with the ledger
  static bool external_tautological(const int32_t *lits, uint32_t size);
  // A startup PB refutation ends the clausal stream and its staged units.
  void make_terminal() noexcept;
  // The ledger bodies of the public events that take a writer hold.
  bool sr_event(uint64_t id, const Lit *lits, uint32_t size,
                const Lit *true_lits, uint32_t ntrue,
                const DsrLiteralMapping *mappings, uint32_t nmappings,
                uint32_t external_variables, bool propagates);
  bool external_rup_event(uint64_t id, const int32_t *lits, uint32_t size,
                          CheckerClauseRole role);
  bool external_rat_event(uint64_t id, const int32_t *lits, uint32_t size);
  bool retain_external_event(uint64_t id, const int32_t *lits, uint32_t size);
  bool retain_premise_groups_event(const std::vector<PremiseGroup> &,
      uint64_t maximum_groups, uint64_t maximum_literals,
      uint64_t maximum_bytes, const std::function<bool(uint64_t)> &charge,
      std::vector<uint64_t> &handles, DeferredDeletionLimits limits);
  void release_premise_groups_event(const std::vector<uint64_t> &,
                                    uint64_t line_id);

  // Lifecycle. A streaming ledger (the kernel's, with or without a writer)
  // opens inside the private input transaction and is published by its first
  // clausal event; a startup PB refutation makes it terminal and the empty
  // clause finishes it. A DSR writer's own ledger is always published.
  bool open_ = false, streaming_ = false, input_transaction_ = false;
  bool published_ = false, terminal_ = false, finished_ = false;
  bool deferring_ = false;        // staging root units (begin_deferred_root_units)
  bool replaying_leased_ = false; // a lease release replays deferred deletions
  bool copy_role_ = false;        // the automatic copy: events already journaled
  std::vector<StagedUnit> staged_; // root units of a conflicting root propagation
  uint64_t stream_identity_ = 0;
  const std::vector<Var> *variable_map_ = nullptr;
  const std::vector<Var> *external_variable_map_ = nullptr;
  PopulationObserver *population_observer_ = nullptr; // non-owning
  // The checker-visible literal for an internal literal under the bound map.
  Lit checker_lit(Lit l) const {
    if (!variable_map_)
      return l;
    assert(var_of(l) < variable_map_->size());
    return make_lit((*variable_map_)[var_of(l)], negated(l));
  }

  // Persistent external clauses (retain_external).
  struct PersistentExternalClause {
    uint64_t id = 0;
    std::vector<int32_t> lits;
  };
  std::vector<PersistentExternalClause> persistent_external_;
  uint64_t persistent_external_epoch_ = 0;
  void flush_persistent_external(uint64_t line_id);

  // Premise leases (retain_premise_groups).
  struct LeasedPremise {
    uint64_t users = 0, deferred = 0;
  };
  // Retained bytes charged per leased premise clause; release refunds them.
  static constexpr uint64_t premise_storage_bytes(size_t size) {
    return (4 * sizeof(PremiseGroup) + 16 * sizeof(void *) +
            sizeof(LeasedPremise)) + 2 * uint64_t(size) * sizeof(int32_t);
  }
  std::map<std::vector<int32_t>, LeasedPremise> leased_premises_;
  std::map<uint64_t, PremiseGroup> premise_groups_;
  uint64_t next_premise_group_ = 0, premise_epoch_ = 0;
  uint64_t premise_literals_ = 0, premise_bytes_ = 0, premise_deferred_ = 0;
  uint64_t premise_pending_deletions_ = 0, premise_pending_literal_work_ = 0;
  uint64_t premise_pending_deletion_cap_ = UINT64_MAX, premise_pending_work_cap_ = UINT64_MAX;
  uint32_t premise_maximum_width_ = 0;
  uint64_t premise_maintenance_work_ = 0;
  // Occurrences of each external variable in premise_groups_, so a line whose
  // pivot and witness touch no lease need not scan them.
  std::vector<uint32_t> premise_variable_uses_;
  void count_premise_variables(const PremiseGroup &, bool add);
  bool defer_leased_deletion(const int32_t *, uint32_t);
  void invalidate_premises_for_sr(uint64_t, const Lit *, uint32_t,
      const Lit *, uint32_t, const DsrLiteralMapping *, uint32_t);

  std::unique_ptr<ProofWriter> writer_;
};

} // namespace tactix

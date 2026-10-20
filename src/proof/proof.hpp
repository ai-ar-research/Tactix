// Proof backend (plan §5.2): rules emit typed logical events. Native DSR is
// the canonical stream; witnessless DSR additions cover the existing RUP/RAT
// vocabulary and direct SR additions carry a substitution witness. DRAT and
// native LRAT remain compatibility serializers for the pre-existing events.
// Streaming + buffered: never held in memory (§6).
#pragma once

#include "kernel/types.hpp"
#include "population/population_observer.hpp"

#include <array>
#include <compare>
#include <functional>
#include <map>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

enum class ProofFormat { DsrBinary, DsrAscii, DratBinary, DratAscii, Lrat,
                         Automatic, Pbp, PbpStreaming };

struct DsrLiteralMapping {
  Lit from = INVALID_LIT;
  Lit to = INVALID_LIT;
};

// Preserve the semantic pivot override using an encoding shared by dsr-trim
// and Trestle. Classify images relative to !C: an unmapped pivot is Direct,
// a true image permits Repivot, all-false images permit Rup, one open image
// permits Lemma, and multiple open images use a temporary Extension.
// Every form preserves the X7 application; see docs/calculus.md section 6d.
enum class DsrWitnessForm : uint8_t { Direct, Repivot, Rup, Lemma, Extension };

struct DsrWitnessPlan {
  DsrWitnessForm form = DsrWitnessForm::Direct;
  uint32_t pivot = 0;      // clause index of the emitted pivot
  Lit lemma = INVALID_LIT; // Lemma: open image appended to the clause
};

// One already-authenticated complete XOR encoding, named only by its semantic
// equation.  Streaming PBP can turn the bounded CNF row into two persistent
// cutting-planes bounds once, then combine cached bounds for later live
// reasons.  Premise authentication and lifetime remain the caller's job.
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

struct PbpParityBounds {
  uint64_t lower = 0;
  uint64_t upper = 0;
  std::vector<int32_t> carries;
};

class PbpParityBoundsCache {
public:
  uint64_t generation() const noexcept { return generation_; }
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  size_t size() const noexcept { return entries_.size(); }
  uint64_t retained_bytes() const noexcept;

private:
  friend class Proof;
  uint64_t stream_identity_ = 0;
  uint64_t generation_ = 0;
  std::map<PbpParityEquation, PbpParityBounds> entries_;
};

struct PbpParityReasonPlan {
  uint64_t stream_identity = 0;
  uint64_t cache_generation = 0;
  uint64_t proof_constraints = 0;
  uint64_t retained_bytes = 0;
  uint32_t starting_external_extent = 0;
  uint32_t helper_variables = 0;
  bool valid = false;
};

// `lits[0]` is the semantic pivot; `mappings` use positive sources.
DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings);

class Proof {
public:
  ~Proof();

  bool open(const char *path, ProofFormat format);
  uint64_t stream_identity() const noexcept { return stream_identity_; }
  // Automatic output privately stages only input-normalization DSR events.
  // At the original-input action boundary the caller may commit a complete
  // PB refutation. The first subsequent clausal event commits DSR instead.
  // Neither choice can be undone, and no published prefix is overwritten.
  void finish_input_transaction();
  bool startup_pb_available() const { return automatic_pending_ && !input_transaction_; }
  bool commit_dsr();
  FILE *begin_pb();
  bool finish_pb();
  bool pb() const {
    return format_ == ProofFormat::Pbp ||
           format_ == ProofFormat::PbpStreaming;
  }
  // The startup-only terminal PBP route owns a complete refutation and cannot
  // accept subsequent typed clause events.  Explicit streaming PBP can.
  bool terminal_pbp() const { return format_ == ProofFormat::Pbp; }
  bool streaming_pbp() const { return format_ == ProofFormat::PbpStreaming; }
  bool lrat() const { return format_ == ProofFormat::Lrat; }
  ProofFormat format() const { return format_; }
  bool dsr() const {
    return format_ == ProofFormat::DsrBinary ||
           format_ == ProofFormat::DsrAscii;
  }
  bool supports_sr() const { return dsr() || streaming_pbp(); }
  // Published proof feature, used only to choose an independent final checker.
  // The runner also scans the finished certificate and rejects disagreement.
  const char *verification_route() const {
    if (pb()) return "pbp";
    if (dsr()) return has_sr_witness_ ? "dsr-trestle" : "dsr-cake_lpr";
    if (lrat()) return "lrat";
    return "drat";
  }
  bool binary() const {
    return format_ == ProofFormat::DsrBinary ||
           format_ == ProofFormat::DratBinary;
  }
  void bind_variable_map(const std::vector<Var> *map) { variable_map_ = map; }
  void bind_external_variable_map(const std::vector<Var> *map) {
    external_variable_map_ = map;
  }
  void bind_population_observer(PopulationObserver *) noexcept;
  // Settle only a route whose bytes already have a committed destination.
  // Automatic must remain undecided; callers learn that its staged stream was
  // incomplete through ObserverUnboundIncomplete instead of accidentally
  // selecting DSR while merely removing an observer.
  bool settle_population_observer() noexcept;
  PopulationObserver *population_observer() const noexcept {
    return population_observer_;
  }
  // Frontend-only lineage event: a live solver clause starts citing a new
  // checker ID after LRAT restatement/replay.  Both checker clauses remain in
  // the population; this must not be represented as add/delete.
  void observe_checker_id_rebind(uint64_t predecessor_id,
                                 uint64_t checker_id, const Lit *lits,
                                 uint32_t size) noexcept;

  // --- proof-delta vocabulary (subset live in phase 2) ---
  // add-RUP: LRAT uses `id` and `chain` (ids in checker-propagation order).
  // Native DSR stages consecutive RUP units so a following deletion block can
  // be ordered before the batch with a shared proof-only split.
  void add_rup(uint64_t id, const Lit *lits, uint32_t size,
               const uint64_t *chain, uint32_t nchain);
  // add-RAT: `lits` MUST be pivot-first (the RAT pivot is the first literal, both
  // for LRAT and for the checker's first-literal inference on DRAT). LRAT writes
  // `<id> <lits> 0 <hints> 0` where `hints` is the flat SIGNED antecedent stream:
  // optional leading positive RUP antecedents, then for each resolution partner D
  // (a clause containing ¬pivot) a negative `-cid(D)` followed by the positive RUP
  // hints discharging the resolvent. DRAT writes `a <lits> 0` (identical to a
  // plain add — the RAT-ness is inferred by the checker from the first literal).
  // Factor is the first RAT-emitting rule (plan §5.2 add-RAT primitive).
  void add_rat(uint64_t id, const Lit *lits, uint32_t size,
               const int64_t *hints, uint32_t nhints);
  // Add a substitution-redundant clause. `lits[0]` is the DSR pivot,
  // `true_lits` are mapped to true, and each mapping maps its source literal
  // to its target literal. `external_variables` bounds every external variable
  // in the proof-visible formula, including retired variables. The general
  // encoding temporarily uses the next external variable and deletes every
  // clause containing it before returning, so later solver extensions may use
  // that identity. Returns false for compatibility formats or exhaustion of
  // the external literal representation, before emitting any clause.
  // `propagates`: the clause is unit at the root. No witnessed line may
  // propagate when read, so it takes the general encoding and enters by RUP
  // (docs/implementation-notes.md, native DSR checking).
  bool add_sr(uint64_t id, const Lit *lits, uint32_t size,
              const Lit *true_lits, uint32_t ntrue,
              const DsrLiteralMapping *mappings, uint32_t nmappings,
              uint32_t external_variables, bool propagates = false);
  void add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain);
  // root-level propagation-derived unit: explicit in LRAT (each level-0
  // assignment needs an id for later chains), implied/omitted in DSR/DRAT
  void add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                        uint32_t nchain);
  // delete: DRAT writes the literals; LRAT the clause id
  void del(uint64_t line_id, uint64_t del_id, const Lit *lits,
           uint32_t size);

  // Proof-only extension clauses use DIMACS literals whose variables need not
  // exist in the solver kernel.  This keeps terminal extended-resolution
  // certificates streaming without allocating millions of inactive kernel
  // variables.  Native LRAT has no witnessless extension vocabulary, so these
  // operations decline it before writing any bytes.
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
  bool add_pbp_parity_reason(
      uint64_t kernel_clause_id, const std::vector<PbpParityEquation> &,
      const int32_t *reason, uint32_t reason_size,
      PbpParityBoundsCache &, const PbpParityReasonPlan &);
  bool del_external(uint64_t line_id, uint64_t del_id,
                    const int32_t *lits, uint32_t size);
  // Keep an already-emitted proof-only clause available for later RUP steps.
  // The registry is a checker-population boundary.  On DSR routes add_sr
  // deletes every registered clause before emitting a witnessed line, then
  // advances the epoch so clients cannot reuse stale support.  DRAT has no
  // witnessed entry: retained clauses remain legal until the proof ends.
  bool retain_external(uint64_t id, const int32_t *lits, uint32_t size);
  uint64_t persistent_external_epoch() const {
    return persistent_external_epoch_;
  }
  size_t persistent_external_clauses() const {
    return persistent_external_.size();
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

  // A tautological (or duplicate-carrying) lemma is CHECKER-DIVERGENT: DRAT
  // accepts it for free (negating it self-conflicts, so RUP passes without
  // touching the formula) while every LRAT checker walks the hint chain,
  // finds the hints already satisfied, and REJECTS the proof. Rules must
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
  // must be backed by one checker-visible copy under the format's naming rule:
  // literals for DRAT, IDs for LRAT. The frontend seeds and arms the oracle only
  // after it has registered the complete input and its restatements. See
  // benchmarks/reports/2026-08-05-input-clause-normalization-n2.md.
  void arm_population_shadow() { armed_ = true; }
  bool population_shadow_available() const { return true; }
  bool population_contains(uint64_t id, const Lit *lits, uint32_t size) const;
  uint64_t unbacked_deletions() const { return unbacked_deletions_; }
  bool population_shadow() const { return armed_; }
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
  // Consecutive RUP units and their following deletion block form one split
  // transaction. Two RUP helpers per unit use a shared fresh variable; each
  // prior pair supplies its unit under either assumption on that variable.
  // Deletion bytes are spooled; helpers are retired before a later SR step.
  void flush_pending_dsr_units();
  void flush_persistent_external(uint64_t line_id);
  CheckerRoute population_route() const noexcept;
  CheckerNaming population_naming() const noexcept;
  void observe_checker(CheckerPopulationEventKind, CheckerClauseRole,
                       uint64_t line_id, uint64_t checker_id,
                       PopulationClauseView, bool proof_only = false,
                       uint64_t predecessor_id = 0,
                       uint64_t transaction_id = 0,
                       PopulationSourceClassification =
                           PopulationSourceClassification::Ineligible) noexcept;
  uint32_t external_variable_bound() const;
  struct PendingDsrUnit {
    int32_t external;
    uint64_t id;
    CheckerClauseRole role;
  };
  std::vector<PendingDsrUnit> pending_units_;
  std::unordered_set<int32_t> pending_unit_literals_;
  std::unordered_set<uint64_t> pending_unit_ids_;
  uint32_t pending_max_external_ = 0;
  uint32_t proof_external_max_ = 0;
  struct PersistentExternalClause {
    uint64_t id = 0;
    std::vector<int32_t> lits;
  };
  std::vector<PersistentExternalClause> persistent_external_;
  uint64_t persistent_external_epoch_ = 0;
  struct LeasedPremise {
    uint64_t users = 0, deferred = 0;
  };
  std::map<std::vector<int32_t>, LeasedPremise> leased_premises_;
  std::map<uint64_t, PremiseGroup> premise_groups_;
  uint64_t next_premise_group_ = 0, premise_epoch_ = 0;
  uint64_t stream_identity_ = 0;
  uint64_t premise_literals_ = 0, premise_bytes_ = 0, premise_deferred_ = 0;
  uint64_t premise_pending_deletions_ = 0, premise_pending_literal_work_ = 0;
  uint64_t premise_pending_deletion_cap_ = UINT64_MAX, premise_pending_work_cap_ = UINT64_MAX;
  uint32_t premise_maximum_width_ = 0;
  uint64_t premise_maintenance_work_ = 0;
  bool defer_leased_deletion(const int32_t *, uint32_t);
  void invalidate_premises_for_sr(uint64_t, const Lit *, uint32_t,
      const Lit *, uint32_t, const DsrLiteralMapping *, uint32_t);
  // Binary deletion blocks are usually only a few bytes. Keep them in a
  // bounded buffer and spill exceptional blocks to a temporary file.
  std::vector<unsigned char> pending_deletion_bytes_;
  FILE *pending_deletions_ = nullptr;
  bool spooling_deletions_ = false;
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::string population_key(uint64_t id, const Lit *lits, uint32_t size) const;
  void population_add(uint64_t id, const Lit *lits, uint32_t size);
  void population_del(uint64_t id, const Lit *lits, uint32_t size);
  std::unordered_map<std::string, uint32_t> population_;
  uint64_t unbacked_deletions_ = 0;
  bool armed_ = false;
#endif
  void put_byte(unsigned char byte);
  void flush_binary();
  void put_lit(Lit l);
  void put_external_lit(Lit l);
  void put_external_dimacs(int32_t literal);
  void put_pbp_external_dimacs(int32_t literal);
  void put_pbp_lit(Lit literal);
  void put_pbp_clause(const char *rule, const Lit *lits, uint32_t size);
  void put_pbp_external_clause(const char *rule, const int32_t *lits,
                               uint32_t size);
  void finish_streaming_pbp_unsat();
  void publish_streaming_pbp_header();
  void put_u64(uint64_t v);
  void end_line();
  FILE *file_ = nullptr;
  FILE *automatic_output_ = nullptr;
  FILE *pbp_output_ = nullptr;
  bool automatic_pending_ = false;
  bool input_transaction_ = false;
  bool has_sr_witness_ = false;
  bool streaming_pbp_finished_ = false;
  uint64_t pbp_original_constraints_ = 0;
  uint64_t pbp_derived_constraints_ = 0;
  ProofFormat format_ = ProofFormat::DsrBinary;
  std::array<unsigned char, 1 << 20> binary_buffer_{};
  size_t binary_buffer_size_ = 0;
  const std::vector<Var> *variable_map_ = nullptr;
  const std::vector<Var> *external_variable_map_ = nullptr;
  PopulationObserver *population_observer_ = nullptr; // non-owning
  CheckerClauseRole population_role_ = CheckerClauseRole::Rup;
  uint64_t pending_population_transaction_ = 0;
  uint64_t next_population_transaction_ = 0;
};

} // namespace tactix

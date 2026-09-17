// Proof backend (plan §5.2): rules emit typed logical events. Native DSR is
// the canonical stream; witnessless DSR additions cover the existing RUP/RAT
// vocabulary and direct SR additions carry a substitution witness. DRAT and
// native LRAT remain compatibility serializers for the pre-existing events.
// Streaming + buffered: never held in memory (§6).
#pragma once

#include "kernel/types.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace tactix {

enum class ProofFormat { DsrBinary, DsrAscii, DratBinary, DratAscii, Lrat };

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

// `lits[0]` is the semantic pivot; `mappings` use positive sources.
DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings);

class Proof {
public:
  ~Proof();

  bool open(const char *path, ProofFormat format);
  bool lrat() const { return format_ == ProofFormat::Lrat; }
  bool dsr() const {
    return format_ == ProofFormat::DsrBinary ||
           format_ == ProofFormat::DsrAscii;
  }
  bool binary() const {
    return format_ == ProofFormat::DsrBinary ||
           format_ == ProofFormat::DratBinary;
  }
  void bind_variable_map(const std::vector<Var> *map) { variable_map_ = map; }

  // --- proof-delta vocabulary (subset live in phase 2) ---
  // add-RUP: `id` and the antecedent `chain` (ids, checker-propagation
  // order) are used by LRAT only; DRAT writes the literals
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
  bool add_sr(uint64_t id, const Lit *lits, uint32_t size,
              const Lit *true_lits, uint32_t ntrue,
              const DsrLiteralMapping *mappings, uint32_t nmappings,
              uint32_t external_variables);
  void add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain);
  // root-level propagation-derived unit: explicit in LRAT (each level-0
  // assignment needs an id for later chains), implied/omitted in DSR/DRAT
  void add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                        uint32_t nchain);
  // delete: DRAT writes the literals; LRAT the clause id
  void del(uint64_t line_id, uint64_t del_id, const Lit *lits,
           uint32_t size);

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

#ifdef TACTIX_ENABLE_PROOF_SHADOW
  // N2 population oracle (diagnostic build feature, CMake
  // TACTIX_ENABLE_PROOF_SHADOW; production pins compile none of this).
  //
  // A checker retires exactly ONE clause per `d` line, selected the way that
  // format names a clause: DRAT by the literal multiset, LRAT by the id. So
  // every deletion must be backed by a copy the checker still holds. An
  // unbacked one is not a harmless no-op — under DRAT it retires whatever OTHER
  // live clause carries the same literals, leaving the solver with a clause the
  // checker lost, after which the first lemma resolving through it fails RUP.
  // That divergence is silent in the certificate (drat-trim only warns when the
  // surplus deletion happens to match nothing at all), so it has to be caught
  // where it is emitted. `seed` registers the clauses the checker starts with.
  void seed_population(uint64_t id, const Lit *lits, uint32_t size);
  // The oracle can only judge a run whose whole starting population it saw, so
  // it stays inert until the frontend has registered every original file
  // clause and completed input restatement. A kernel assembled directly in a
  // unit test never arms it.
  void arm_population_shadow() { armed_ = true; }
  bool population_shadow_available() const { return true; }
  bool population_contains(uint64_t id, const Lit *lits, uint32_t size) const;
  uint64_t unbacked_deletions() const { return unbacked_deletions_; }
  bool population_shadow() const { return armed_; }
#else
  void seed_population(uint64_t, const Lit *, uint32_t) {}
  void arm_population_shadow() {}
  bool population_shadow_available() const { return false; }
  bool population_contains(uint64_t, const Lit *, uint32_t) const {
    return true;
  }
  uint64_t unbacked_deletions() const { return 0; }
  bool population_shadow() const { return false; }
#endif

private:
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
  void put_u64(uint64_t v);
  void end_line();
  FILE *file_ = nullptr;
  ProofFormat format_ = ProofFormat::DsrBinary;
  std::array<unsigned char, 1 << 20> binary_buffer_{};
  size_t binary_buffer_size_ = 0;
  const std::vector<Var> *variable_map_ = nullptr;
};

} // namespace tactix

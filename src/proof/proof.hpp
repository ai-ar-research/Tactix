// Proof backend (plan §5.2): the closed five-primitive vocabulary is the
// only side channel rules may use. DRAT (binary/ascii) serializes
// add-RUP/delete ignoring hints; LRAT (ascii, native — plan O4) writes
// the clause id and the antecedent chain the rules capture during
// conflict analysis. Streaming + buffered: never held in memory (§6).
#pragma once

#include "kernel/types.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

enum class ProofFormat { DratBinary, DratAscii, Lrat };

class Proof {
public:
  ~Proof();

  bool open(const char *path, ProofFormat format);
  bool lrat() const { return format_ == ProofFormat::Lrat; }
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
  void add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain);
  // root-level propagation-derived unit: explicit in LRAT (each level-0
  // assignment needs an id for later chains), implied/omitted in DRAT
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

private:
  void put_lit(Lit l);
  void put_u64(uint64_t v);
  void end_line();
  FILE *file_ = nullptr;
  ProofFormat format_ = ProofFormat::DratBinary;
  const std::vector<Var> *variable_map_ = nullptr;
};

} // namespace tactix

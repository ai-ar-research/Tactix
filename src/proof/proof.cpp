#include "proof/proof.hpp"

#include <cassert>
#include <cinttypes>

namespace tactix {

Proof::~Proof() {
  if (file_) {
    fflush(file_);
    fclose(file_);
  }
}

bool Proof::open(const char *path, ProofFormat format) {
  file_ = fopen(path, "wb");
  format_ = format;
  if (file_)
    setvbuf(file_, nullptr, _IOFBF, 1 << 20);
  return file_ != nullptr;
}

// Binary DRAT literal encoding (drat-trim format): map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void Proof::put_lit(Lit l) {
  if (variable_map_) {
    assert(var_of(l) < variable_map_->size());
    l = make_lit((*variable_map_)[var_of(l)], negated(l));
  }
  const int e = externalize(l);
  if (format_ == ProofFormat::DratBinary) {
    uint32_t u = (static_cast<uint32_t>(e < 0 ? -e : e) << 1) | (e < 0);
    while (u >= 128) {
      fputc(128 | (u & 127), file_);
      u >>= 7;
    }
    fputc(u, file_);
  } else {
    fprintf(file_, "%d ", e);
  }
}

void Proof::put_u64(uint64_t v) { fprintf(file_, "%" PRIu64 " ", v); }

void Proof::end_line() {
  if (format_ == ProofFormat::DratBinary)
    fputc(0, file_);
  else
    fputs("0\n", file_);
}

// Duplicate/complementary-literal scan. Small clauses only reach here, so the
// quadratic form beats any scratch allocation; it runs in debug builds only.
bool Proof::tautological(const Lit *lits, uint32_t size) {
  for (uint32_t i = 0; i < size; i++)
    for (uint32_t j = i + 1; j < size; j++)
      if (lits[i] == lits[j] || lits[i] == negate(lits[j]))
        return true;
  return false;
}

void Proof::add_rup(uint64_t id, const Lit *lits, uint32_t size,
                    const uint64_t *chain, uint32_t nchain) {
  assert(!tautological(lits, size)); // see Proof::tautological
  // Under LRAT a non-empty clause with NO antecedents is uncheckable: the
  // checker has nothing to propagate and rejects the line (`<id> <lits> 0 0`).
  // Rules must skip the inference instead of emitting one; a trivially-true
  // lemma whose replay produces an empty chain is the way this happens.
  assert(!(format_ == ProofFormat::Lrat && size > 0 && nchain == 0));
#ifndef NDEBUG
  // 0 is LRAT's line terminator, never an antecedent, so a zero hint TRUNCATES
  // the line instead of corrupting it — the checker sees a shorter chain and
  // rejects a lemma that the emitter believed it had justified. Caught here
  // because the hand-built chains (Congruence's RUP replay, Substitute's
  // rewrite, Sweep's general path) never pass through `kernel_lrat_chain`,
  // whose own assert covers the reason-walk chains.
  if (format_ == ProofFormat::Lrat)
    for (uint32_t i = 0; i < nchain; i++)
      assert(chain[i] != 0 && "LRAT RUP chain contains the line terminator");
#endif
  if (!file_)
    return;
  if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nchain; i++)
      put_u64(chain[i]);
    fputs("0\n", file_);
    return;
  }
  if (format_ == ProofFormat::DratBinary)
    fputc('a', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

void Proof::add_rat(uint64_t id, const Lit *lits, uint32_t size,
                    const int64_t *hints, uint32_t nhints) {
  assert(!tautological(lits, size)); // see Proof::tautological
#ifndef NDEBUG
  if (format_ == ProofFormat::Lrat) // same terminator hazard as add_rup
    for (uint32_t i = 0; i < nhints; i++)
      assert(hints[i] != 0 && "LRAT RAT hints contain the line terminator");
#endif
  if (!file_)
    return;
  if (format_ == ProofFormat::Lrat) {
    put_u64(id);
    for (uint32_t i = 0; i < size; i++)
      put_lit(lits[i]);
    fputs("0 ", file_);
    for (uint32_t i = 0; i < nhints; i++)
      fprintf(file_, "%" PRId64 " ", hints[i]);
    fputs("0\n", file_);
    return;
  }
  // DRAT (binary/ascii): a plain add with the pivot ordered first; the RAT
  // justification is inferred by the checker (first-literal RAT). Byte-identical
  // to add_rup's DRAT path, so DRAT output is unchanged.
  if (format_ == ProofFormat::DratBinary)
    fputc('a', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

void Proof::add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain) {
  add_rup(id, nullptr, 0, chain, nchain);
}

void Proof::add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                             uint32_t nchain) {
  if (!file_ || format_ != ProofFormat::Lrat)
    return; // DRAT: propagation-derived units are implied, never written
  add_rup(id, &unit, 1, chain, nchain);
}

void Proof::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size) {
  if (!file_)
    return;
  if (format_ == ProofFormat::Lrat) {
    put_u64(line_id);
    fputs("d ", file_);
    put_u64(del_id);
    fputs("0\n", file_);
    return;
  }
  fputc('d', file_);
  if (format_ == ProofFormat::DratAscii)
    fputc(' ', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

void Proof::flush() {
  if (file_)
    fflush(file_);
}

} // namespace tactix

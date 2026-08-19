#include "proof/proof.hpp"
#ifdef TACTIX_ENABLE_RULE_JOURNAL
#include "diagnostics/rule_journal.hpp"
#endif

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#ifdef TACTIX_ENABLE_PROOF_SHADOW
#include <algorithm>
#include <execinfo.h>
#endif

namespace tactix {

Proof::~Proof() {
  if (file_) {
    flush_binary();
    fflush(file_);
    fclose(file_);
  }
}

bool Proof::open(const char *path, ProofFormat format) {
  file_ = fopen(path, "wb");
  format_ = format;
  if (file_)
    setvbuf(file_, nullptr,
            format == ProofFormat::DratBinary ? _IONBF : _IOFBF, 1 << 20);
  return file_ != nullptr;
}

void Proof::put_byte(unsigned char byte) {
  binary_buffer_[binary_buffer_size_++] = byte;
  if (binary_buffer_size_ == binary_buffer_.size())
    flush_binary();
}

void Proof::flush_binary() {
  if (!file_ || !binary_buffer_size_)
    return;
  const size_t written =
      fwrite(binary_buffer_.data(), 1, binary_buffer_size_, file_);
  if (written != binary_buffer_size_)
    std::abort();
  binary_buffer_size_ = 0;
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
      put_byte(static_cast<unsigned char>(128 | (u & 127)));
      u >>= 7;
    }
    put_byte(static_cast<unsigned char>(u));
  } else {
    fprintf(file_, "%d ", e);
  }
}

void Proof::put_u64(uint64_t v) { fprintf(file_, "%" PRIu64 " ", v); }

void Proof::end_line() {
  if (format_ == ProofFormat::DratBinary)
    put_byte(0);
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
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_rup(id, chain, nchain);
#endif
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
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    return;
  }
  if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
}

void Proof::add_rat(uint64_t id, const Lit *lits, uint32_t size,
                    const int64_t *hints, uint32_t nhints) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_rat(id, hints, nhints);
#endif
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
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    return;
  }
  // DRAT (binary/ascii): a plain add with the pivot ordered first; the RAT
  // justification is inferred by the checker (first-literal RAT). Byte-identical
  // to add_rup's DRAT path, so DRAT output is unchanged.
  if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
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
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_del(del_id, lits, size);
#endif
    return;
  }
  if (format_ == ProofFormat::DratBinary)
    put_byte('d');
  else
    fputc('d', file_);
  if (format_ == ProofFormat::DratAscii)
    fputc(' ', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_del(del_id, lits, size);
#endif
}

void Proof::flush() {
  if (file_) {
    flush_binary();
    fflush(file_);
  }
}

#ifdef TACTIX_ENABLE_PROOF_SHADOW
// Key a clause exactly the way the target format's checker matches a deletion:
// by the sorted external literal multiset under DRAT, by the clause id under
// native LRAT. Keying DRAT by id would miss the whole defect class, because the
// hazard is precisely that two different ids can carry the same literals.
std::string Proof::population_key(uint64_t id, const Lit *lits,
                                  uint32_t size) const {
  std::string key;
  if (format_ == ProofFormat::Lrat) {
    key.assign(reinterpret_cast<const char *>(&id), sizeof id);
    return key;
  }
  std::vector<int> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; i++) {
    Lit l = lits[i];
    if (variable_map_)
      l = make_lit((*variable_map_)[var_of(l)], negated(l));
    external.push_back(externalize(l));
  }
  std::sort(external.begin(), external.end());
  key.assign(reinterpret_cast<const char *>(external.data()),
             external.size() * sizeof(int));
  return key;
}

void Proof::population_add(uint64_t id, const Lit *lits, uint32_t size) {
  population_[population_key(id, lits, size)]++;
}

void Proof::population_del(uint64_t id, const Lit *lits, uint32_t size) {
  if (!armed_)
    return;
  const auto found = population_.find(population_key(id, lits, size));
  if (found != population_.end() && found->second) {
    if (!--found->second)
      population_.erase(found);
    return;
  }
  unbacked_deletions_++;
  fprintf(stderr, "PROOF SHADOW: unbacked deletion #%" PRIu64 " of [%" PRIu64
                  "]",
          unbacked_deletions_, id);
  for (uint32_t i = 0; i < size; i++) {
    Lit l = lits[i];
    if (variable_map_)
      l = make_lit((*variable_map_)[var_of(l)], negated(l));
    fprintf(stderr, " %d", externalize(l));
  }
  fputs(" 0\n", stderr);
  void *frames[32];
  const int depth = backtrace(frames, 32);
  backtrace_symbols_fd(frames, depth, fileno(stderr));
  fflush(stderr);
  // Abort on the FIRST one: the emitting rule is what this build exists to
  // name, and every later line is downstream of an already-diverged checker.
  std::abort();
}

void Proof::seed_population(uint64_t id, const Lit *lits, uint32_t size) {
  population_add(id, lits, size);
}
#endif

} // namespace tactix

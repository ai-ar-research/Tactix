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

DsrWitnessPlan plan_dsr_witness(const Lit *lits, uint32_t size,
                                const Lit *true_lits, uint32_t ntrue,
                                const DsrLiteralMapping *mappings,
                                uint32_t nmappings) {
  DsrWitnessPlan plan;
  if (!size)
    return plan;
  const Var pivot_var = var_of(lits[0]);
  const DsrLiteralMapping *pivot_mapping = nullptr;
  for (uint32_t i = 0; i < nmappings && !pivot_mapping; ++i)
    if (var_of(mappings[i].from) == pivot_var)
      pivot_mapping = &mappings[i];
  if (!pivot_mapping)
    return plan;

  enum class Image : uint8_t { True, False, Open };
  const auto in_clause = [&](Lit lit) {
    for (uint32_t i = 0; i < size; ++i)
      if (lits[i] == lit)
        return true;
    return false;
  };
  const auto image_of = [&](uint32_t at, Lit &image) {
    const Lit lit = lits[at];
    for (uint32_t i = 0; i < nmappings; ++i)
      if (var_of(mappings[i].from) == var_of(lit)) {
        image = negated(lit) ? negate(mappings[i].to) : mappings[i].to;
        return in_clause(negate(image)) ? Image::True
               : in_clause(image)       ? Image::False
                                        : Image::Open;
      }
    for (uint32_t i = 0; i < ntrue; ++i)
      if (var_of(true_lits[i]) == var_of(lit))
        return true_lits[i] == lit ? Image::True : Image::False;
    return Image::False; // an identity image of a clause literal
  };

  uint32_t open = 0;
  for (uint32_t at = 0; at < size; ++at) {
    Lit image = INVALID_LIT;
    switch (image_of(at, image)) {
    case Image::True:
      plan.form = DsrWitnessForm::Repivot;
      plan.pivot = at;
      plan.lemma = INVALID_LIT;
      return plan;
    case Image::Open:
      if (!open++) {
        plan.pivot = at;
        plan.lemma = image;
      }
      break;
    case Image::False:
      break;
    }
  }
  plan.form = !open        ? DsrWitnessForm::Rup
              : open == 1 ? DsrWitnessForm::Lemma
                          : DsrWitnessForm::Extension;
  if (plan.form != DsrWitnessForm::Lemma) {
    plan.pivot = 0;
    plan.lemma = INVALID_LIT;
  }
  return plan;
}

Proof::~Proof() {
  flush();
  if (file_ && std::fclose(file_))
    std::abort();
}

bool Proof::open(const char *path, ProofFormat format) {
  if (file_ || automatic_output_ || format == ProofFormat::Pbp)
    return false;
  file_ = fopen(path, "wb");
  format_ = format;
  if (file_ && format == ProofFormat::Automatic) {
    automatic_output_ = file_;
    file_ = std::tmpfile();
    if (!file_) {
      std::fclose(automatic_output_);
      automatic_output_ = nullptr;
      return false;
    }
    format_ = ProofFormat::DsrBinary;
    automatic_pending_ = input_transaction_ = true;
  }
  if (file_)
    setvbuf(file_, nullptr, binary() ? _IONBF : _IOFBF, 1 << 20);
  return file_ != nullptr;
}

bool Proof::commit_dsr() {
  if (!automatic_pending_)
    return !pb();
  // Preserve the ordinary binary serializer's unbuffered stdio contract:
  // binary_buffer_ owns buffering and every fwrite must observe I/O failure.
  // The destination has not yet received any bytes, so setvbuf is still legal.
  if (std::setvbuf(automatic_output_, nullptr, _IONBF, 0))
    std::abort();
  flush_binary();
  if (std::fflush(file_) || std::fseek(file_, 0, SEEK_SET))
    std::abort();
  std::array<unsigned char, 65536> bytes{};
  for (;;) {
    const size_t count = std::fread(bytes.data(), 1, bytes.size(), file_);
    if (count && std::fwrite(bytes.data(), 1, count, automatic_output_) != count)
      std::abort();
    if (count < bytes.size()) {
      if (std::ferror(file_))
        std::abort();
      break;
    }
  }
  std::fclose(file_);
  file_ = automatic_output_;
  automatic_output_ = nullptr;
  automatic_pending_ = input_transaction_ = false;
  return true;
}

FILE *Proof::begin_pb() {
  if (!startup_pb_available())
    return nullptr;
  // The input DSR transaction was never published. Its facts are justified
  // afresh from original CNF by the complete, prevalidated PB derivation.
  std::fclose(file_);
  binary_buffer_size_ = 0;
  file_ = automatic_output_;
  automatic_output_ = nullptr;
  automatic_pending_ = false;
  format_ = ProofFormat::Pbp;
  return file_;
}

bool Proof::finish_pb() {
  return pb() && file_ && !std::ferror(file_) && std::fflush(file_) == 0;
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

// Binary DSR/DRAT literal encoding: map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void Proof::put_lit(Lit l) {
  if (variable_map_) {
    assert(var_of(l) < variable_map_->size());
    l = make_lit((*variable_map_)[var_of(l)], negated(l));
  }
  put_external_lit(l);
}

void Proof::put_external_lit(Lit l) {
  put_external_dimacs(externalize(l));
}

void Proof::put_external_dimacs(int32_t e) {
  assert(e && e != INT32_MIN);
  if (binary()) {
    const uint32_t magnitude =
        static_cast<uint32_t>(e < 0 ? -int64_t(e) : int64_t(e));
    uint32_t u = (magnitude << 1) | (e < 0);
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
  if (binary())
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
  if (!input_transaction_ && !commit_dsr())
    std::abort(); // a terminal PB stream cannot accept a clausal suffix
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
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
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
  if (!input_transaction_ && !commit_dsr())
    std::abort();
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
  // DSR/DRAT: a witnessless addition with the pivot ordered first. The checker
  // first tries RUP and then infers first-literal RAT. DRAT output is unchanged.
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
}

bool Proof::add_sr(uint64_t id, const Lit *lits, uint32_t size,
                   const Lit *true_lits, uint32_t ntrue,
                   const DsrLiteralMapping *mappings, uint32_t nmappings,
                   uint32_t external_variables, bool propagates) {
  if (!input_transaction_ && !commit_dsr())
    return false;
#ifndef TACTIX_ENABLE_PROOF_SHADOW
  (void)id;
#endif
  assert(size > 0);
  assert(lits);
  assert(!tautological(lits, size));
  assert(!ntrue || true_lits);
  assert(!nmappings || mappings);

#ifndef NDEBUG
  const Lit pivot = lits[0];
  for (uint32_t i = 0; i < ntrue; ++i) {
    assert(var_of(true_lits[i]) != var_of(pivot));
    for (uint32_t j = 0; j < i; ++j)
      assert(var_of(true_lits[i]) != var_of(true_lits[j]));
  }
  for (uint32_t i = 0; i < nmappings; ++i) {
    assert(mappings[i].from != INVALID_LIT);
    assert(mappings[i].to != INVALID_LIT);
    assert(mappings[i].from != mappings[i].to ||
           var_of(mappings[i].from) == var_of(pivot));
    for (uint32_t j = 0; j < ntrue; ++j)
      assert(var_of(mappings[i].from) != var_of(true_lits[j]));
    for (uint32_t j = 0; j < i; ++j)
      assert(var_of(mappings[i].from) != var_of(mappings[j].from));
  }
#endif

  if (!file_)
    return true;
  if (!dsr())
    return false;

  const DsrWitnessPlan plan =
      plan_dsr_witness(lits, size, true_lits, ntrue, mappings, nmappings);
  // A pivot without an explicit mapping has the default image true; the
  // general encoding moves the pivot to the auxiliary, so it states that image.
  const bool default_pivot = plan.form == DsrWitnessForm::Direct;
  if (plan.form == DsrWitnessForm::Extension ||
      (propagates && plan.form != DsrWitnessForm::Rup)) {
    if (external_variables >= static_cast<uint32_t>(INT32_MAX))
      return false;
    const Lit auxiliary = make_lit(external_variables, false);
    const auto temporary = [&](bool deletion, bool negative) {
      if (format_ == ProofFormat::DsrBinary)
        put_byte(deletion ? 2 : 1);
      else if (deletion)
        fputs("d ", file_);
      put_external_lit(negative ? negate(auxiliary) : auxiliary);
      for (uint32_t i = 0; i < size; ++i)
        put_lit(lits[i]);
    };
    // The first clause is RAT on a fresh variable. Under the negation of
    // the second clause, the original X7 obligations hold; the first clause's
    // image is exactly the already-validated candidate image C|sigma.
    temporary(false, true);
    end_line();
    temporary(false, false);
    put_external_lit(auxiliary);
    if (default_pivot)
      put_lit(lits[0]);
    for (uint32_t i = 0; i < ntrue; ++i)
      put_lit(true_lits[i]);
    if (nmappings) {
      put_external_lit(auxiliary);
      for (uint32_t i = 0; i < nmappings; ++i) {
        put_lit(mappings[i].from);
        put_lit(mappings[i].to);
      }
    }
    end_line();
    if (format_ == ProofFormat::DsrBinary)
      put_byte(1);
    for (uint32_t i = 0; i < size; ++i)
      put_lit(lits[i]);
    end_line();
    // These are nonunit clauses, so the checkers honour both deletions.
    // No live clause retains the auxiliary identity after this transaction.
    temporary(true, false);
    end_line();
    temporary(true, true);
    end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
    population_add(id, lits, size);
#endif
    return true;
  }
  const Lit emitted_pivot = lits[plan.pivot];
  const auto put_addition = [&](bool lemma) {
    if (format_ == ProofFormat::DsrBinary)
      put_byte(1);
    put_lit(emitted_pivot);
    for (uint32_t i = 0; i < size; ++i)
      if (i != plan.pivot)
        put_lit(lits[i]);
    if (lemma)
      put_lit(plan.lemma);
  };
  const auto put_lemma_deletion = [&] {
    if (format_ == ProofFormat::DsrBinary)
      put_byte(2);
    else
      fputs("d ", file_);
    put_lit(emitted_pivot);
    for (uint32_t i = 0; i < size; ++i)
      if (i != plan.pivot)
        put_lit(lits[i]);
    put_lit(plan.lemma);
    end_line();
  };

  if (plan.form == DsrWitnessForm::Lemma) {
    put_addition(true);
    end_line();
  }
  put_addition(false);
  if (plan.form != DsrWitnessForm::Rup) {
    const Var dropped = plan.form == DsrWitnessForm::Direct
                            ? INVALID_VAR
                            : var_of(emitted_pivot);
    uint32_t kept_mappings = 0;
    for (uint32_t i = 0; i < nmappings; ++i)
      kept_mappings += var_of(mappings[i].from) != dropped;
    put_lit(emitted_pivot); // begin witness; the pivot is mapped true
    for (uint32_t i = 0; i < ntrue; ++i)
      if (var_of(true_lits[i]) != dropped)
        put_lit(true_lits[i]);
    if (kept_mappings) {
      put_lit(emitted_pivot); // separate true literals from literal mappings
      for (uint32_t i = 0; i < nmappings; ++i)
        if (var_of(mappings[i].from) != dropped) {
          put_lit(mappings[i].from);
          put_lit(mappings[i].to);
        }
    }
  }
  end_line();
  if (plan.form == DsrWitnessForm::Lemma)
    put_lemma_deletion();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_add(id, lits, size);
#endif
  return true;
}

void Proof::add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain) {
  add_rup(id, nullptr, 0, chain, nchain);
}

void Proof::add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain,
                             uint32_t nchain) {
  if (!file_)
    return;
  if (format_ == ProofFormat::Lrat) {
    add_rup(id, &unit, 1, chain, nchain);
    return;
  }
  // Native DSR keeps root units explicit so they remain available after a
  // reason clause is deleted and later direct-SR checks see the same root
  // formula as dsr-trim/Trestle. DRAT retains its historical implicit units.
  if (dsr())
    add_rup(id, &unit, 1, nullptr, 0);
}

void Proof::del(uint64_t line_id, uint64_t del_id, const Lit *lits,
                uint32_t size) {
  if (!input_transaction_ && !commit_dsr())
    std::abort();
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
  if (format_ == ProofFormat::DsrBinary)
    put_byte(2);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('d');
  else
    fputc('d', file_);
  if (format_ == ProofFormat::DsrAscii ||
      format_ == ProofFormat::DratAscii)
    fputc(' ', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  population_del(del_id, lits, size);
#endif
}

namespace {

bool external_tautological(const int32_t *lits, uint32_t size) {
  for (uint32_t i = 0; i < size; ++i) {
    if (!lits[i] || lits[i] == INT32_MIN)
      return true;
    for (uint32_t j = i + 1; j < size; ++j)
      if (lits[i] == lits[j] || lits[i] == -lits[j])
        return true;
  }
  return false;
}

} // namespace

bool Proof::add_external_rup(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  if (!input_transaction_ && !commit_dsr())
    return false;
  if (lrat())
    return false;
  assert(!size || lits);
  if (external_tautological(lits, size))
    return false;
  if (!file_)
    return true;
  if (format_ == ProofFormat::DsrBinary)
    put_byte(1);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('a');
  for (uint32_t i = 0; i < size; ++i)
    put_external_dimacs(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::vector<Lit> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    external.push_back(internalize(lits[i]));
  const auto *saved = variable_map_;
  variable_map_ = nullptr;
  population_add(id, external.data(), size);
  variable_map_ = saved;
#else
  (void)id;
#endif
  return true;
}

bool Proof::add_external_rat(uint64_t id, const int32_t *lits,
                             uint32_t size) {
  // DSR/DRAT infer RAT from the first literal after RUP fails, so their byte
  // stream is identical to an external RUP addition.
  return add_external_rup(id, lits, size);
}

bool Proof::del_external(uint64_t line_id, uint64_t del_id,
                         const int32_t *lits, uint32_t size) {
  if (!input_transaction_ && !commit_dsr())
    return false;
  (void)line_id;
  if (lrat())
    return false;
  assert(!size || lits);
  if (!file_)
    return true;
  if (format_ == ProofFormat::DsrBinary)
    put_byte(2);
  else if (format_ == ProofFormat::DratBinary)
    put_byte('d');
  else
    fputs("d ", file_);
  for (uint32_t i = 0; i < size; ++i)
    put_external_dimacs(lits[i]);
  end_line();
#ifdef TACTIX_ENABLE_PROOF_SHADOW
  std::vector<Lit> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; ++i)
    external.push_back(internalize(lits[i]));
  const auto *saved = variable_map_;
  variable_map_ = nullptr;
  population_del(del_id, external.data(), size);
  variable_map_ = saved;
#else
  (void)del_id;
#endif
  return true;
}

void Proof::flush() {
  if (automatic_pending_)
    commit_dsr();
  if (file_) {
    flush_binary();
    // Text serializers use stdio buffering. Both a previous short write and
    // this publication flush must succeed before a caller may report UNSAT.
    if (std::ferror(file_) || std::fflush(file_))
      std::abort();
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

bool Proof::population_contains(uint64_t id, const Lit *lits,
                                uint32_t size) const {
  const auto found = population_.find(population_key(id, lits, size));
  return found != population_.end() && found->second;
}

void Proof::population_del(uint64_t id, const Lit *lits, uint32_t size) {
  const auto found = population_.find(population_key(id, lits, size));
  if (found != population_.end() && found->second) {
    if (!--found->second)
      population_.erase(found);
    return;
  }
  if (!armed_)
    return; // unknown pre-seeding deltas remain outside the strict oracle
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

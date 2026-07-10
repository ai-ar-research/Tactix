#include "proof/proof.hpp"

#include <cstdlib>

namespace tactix {

Proof::~Proof() {
  if (file_) {
    fflush(file_);
    fclose(file_);
  }
}

bool Proof::open(const char *path, bool binary) {
  file_ = fopen(path, "wb");
  binary_ = binary;
  if (file_)
    setvbuf(file_, nullptr, _IOFBF, 1 << 20);
  return file_ != nullptr;
}

// Binary DRAT literal encoding (drat-trim format): map external literal e
// to unsigned u = (|e| << 1) | (e < 0), then 7-bit varint chunks,
// high bit = continuation.
void Proof::put_lit(Lit l) {
  const int e = externalize(l);
  if (binary_) {
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

void Proof::end_line() {
  if (binary_)
    fputc(0, file_);
  else
    fputs("0\n", file_);
}

void Proof::add_rup(const Lit *lits, uint32_t size) {
  if (!file_)
    return;
  if (binary_)
    fputc('a', file_);
  for (uint32_t i = 0; i < size; i++)
    put_lit(lits[i]);
  end_line();
}

void Proof::add_empty() { add_rup(nullptr, 0); }

void Proof::del(const Lit *lits, uint32_t size) {
  if (!file_)
    return;
  fputc('d', file_);
  if (!binary_)
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

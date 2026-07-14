// Proof backend (plan §5.2): the closed five-primitive vocabulary is the
// only side channel rules may use. Phase 1 implements DRAT serialization of
// add-RUP and delete (add-RAT shares the DRAT encoding; restore and
// push-witness join with the reconstruction stack in phase 3).
// Streaming + buffered: the proof is never held in memory (plan §6).
#pragma once

#include "kernel/types.hpp"

#include <cstdio>

namespace tactix {

class Proof {
public:
  ~Proof();

  // opens the proof file; binary DRAT by default (competition format)
  bool open(const char *path, bool binary);

  // --- proof-delta vocabulary (subset live in phase 1) ---
  void add_rup(const Lit *lits, uint32_t size); // also used for add-RAT
  void add_empty();
  void del(const Lit *lits, uint32_t size);

  void flush(); // signal-safe enough: fflush on the buffered stream

private:
  void put_lit(Lit l);
  void end_line();
  FILE *file_ = nullptr;
  bool binary_ = true;
};

} // namespace tactix

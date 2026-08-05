// Streaming DIMACS ingestion (plan §6): tolerant of comments anywhere,
// arbitrary whitespace, missing final newline/zero; grows past a lying
// header. Shared clause finalization (dedupe, tautology skip, unit/empty
// handling) used by both the file parser and the IPASIR-style C API.
// NOTE on the missing-final-ZERO tolerance: lrat-check DROPS such a trailing
// clause and cake_lpr refuses the file, so a proof citing its id cannot verify
// (2026-08-05 report §2.1). The input is malformed and no checker accepts it,
// so this is an input-validation question, not an emission one — recorded, not
// silently relied on. A missing final NEWLINE is valid DIMACS and is fine.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

// returns nullptr on success, a static error string otherwise. Calls
// finish_input before returning.
const char *parse_dimacs(FILE *, Kernel &);

// finalize one clause given as internal literals (scratch is consumed):
// dedupes, drops tautologies, assigns units, records empty clauses
void ingest_clause(Kernel &, std::vector<Lit> &scratch);

// End of input: settle the proof id space (N2). Input clause `i` MUST carry
// proof id `i` because that is how the checker numbers the file, so no derived
// id may be consumed until the last input clause is in. Emits the deferred
// input-normalization lemmas and any refutation found during input, and is a
// no-op once there is nothing pending. MUST be called after the last
// ingest_clause and before any rule runs; parse_dimacs does it for the file
// path, api.cpp for the C API.
void finish_input(Kernel &);

} // namespace tactix

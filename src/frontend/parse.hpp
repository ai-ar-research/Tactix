// Streaming DIMACS ingestion (plan §6): tolerant of comments anywhere,
// arbitrary whitespace and a missing final newline or zero; grows past a lying
// header. Shared clause finalization (dedupe, tautology skip, unit/empty
// handling) used by both the file parser and the IPASIR-style C API.
//
// A missing final newline is valid DIMACS. A missing final zero is not: no
// checker accepts such a file (lrat-check drops the trailing clause, cake_lpr
// refuses), so a proof citing its id cannot verify. Tolerating it is an
// input-validation choice, never something an emitted proof may rely on. See
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2.1.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

// Returns nullptr on success, a static error string otherwise. Calls
// finish_input before returning.
const char *parse_dimacs(FILE *, Kernel &);

// Finalize one clause given as internal literals (scratch is consumed):
// dedupes, drops tautologies, assigns units, records empty clauses.
void ingest_clause(Kernel &, std::vector<Lit> &scratch);

// End of input: settle the proof id space (N2). Input clause `i` must carry
// proof id `i`, because that is how the checker numbers the file, so no derived
// id may be consumed until the last input clause is in. Emits the deferred
// input-normalization lemmas and any refutation found during input, and is a
// no-op once nothing is pending. Must be called after the last ingest_clause
// and before any rule runs; parse_dimacs does it for the file path, api.cpp for
// the C API.
void finish_input(Kernel &);

} // namespace tactix
